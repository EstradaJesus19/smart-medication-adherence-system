#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <EventLog.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

static_assert(ARDUINO_RUNNING_CORE==1, "This build requires the Arduino loop on core1 and SD worker on core0");
namespace QueuedSdLogger {
constexpr unsigned CAPACITY=64;
enum class State : uint8_t { STARTING, READY, DRAINING, STOPPED, FAULT };
enum class Failure : uint8_t { NONE, QUEUE, TASK, MOUNT, DIRECTORY, OPEN, HEADER, ENCODE, WRITE, SYNC, CLOSE };
enum class Injection : uint8_t { NONE, WRITE, SYNC, FULL };
struct Stats {
    uint64_t boot=0,attempted=0,accepted=0,committed=0,dropped=0,uncertain=0,lastCommitted=0,lastUncertain=0;
    uint32_t generation=1,delayMs=0,maxWriteUs=0,maxSyncUs=0,maxStorageUs=0;
    State state=State::STARTING;
    Failure failure=Failure::NONE;
    int error=0;
    bool run=true,inflight=false;
    Injection injection=Injection::NONE;
    char path[64]{};
};
inline portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
inline Stats stats;
inline QueueHandle_t queue=nullptr;
inline StaticQueue_t queueControl;
inline uint8_t queueStorage[CAPACITY*sizeof(EventLog::Event)];
inline TaskHandle_t task=nullptr;
// SD and SPI are exclusively owned by the worker. No control-loop SD calls.
inline SPIClass storageSpi(VSPI);
inline Stats snapshot() {
    portENTER_CRITICAL(&mux); const Stats copy=stats; portEXIT_CRITICAL(&mux); return copy;
}
inline void fail(Failure why,int error) {
    portENTER_CRITICAL(&mux); stats.state=State::FAULT; stats.failure=why; stats.error=error; stats.inflight=false; portEXIT_CRITICAL(&mux);
}
inline void state(State value) {
    portENTER_CRITICAL(&mux); stats.state=value; portEXIT_CRITICAL(&mux);
}
inline const char* stateName(State s) {
    switch(s) { case State::STARTING:return "STARTING";case State::READY:return "READY";
    case State::DRAINING:return "DRAINING";case State::STOPPED:return "STOPPED";case State::FAULT:return "FAULT"; } return "UNKNOWN";
}
inline const char* failureName(Failure f) {
    switch(f) { case Failure::NONE:return "none";case Failure::QUEUE:return "queue_init";case Failure::TASK:return "task_init";
    case Failure::MOUNT:return "mount";case Failure::DIRECTORY:return "directory";case Failure::OPEN:return "open";
    case Failure::HEADER:return "header";case Failure::ENCODE:return "encode";case Failure::WRITE:return "write";
    case Failure::SYNC:return "sync";case Failure::CLOSE:return "close"; } return "unknown";
}
inline unsigned pending() { return queue?uxQueueMessagesWaiting(queue):0; }
inline uint64_t enqueue(EventLog::Event event) {
    portENTER_CRITICAL(&mux);
    event.sequence=++stats.attempted;
    const bool accepting=stats.run && queue;
    portEXIT_CRITICAL(&mux);
    const bool sent=accepting && xQueueSend(queue,&event,0)==pdTRUE;
    portENTER_CRITICAL(&mux);
    if(sent) ++stats.accepted; else ++stats.dropped;
    portEXIT_CRITICAL(&mux);
    return event.sequence;
}
inline void stop() { portENTER_CRITICAL(&mux); stats.run=false; portEXIT_CRITICAL(&mux); }
inline bool retry() {
    portENTER_CRITICAL(&mux);
    const bool allowed=task && (stats.state==State::FAULT || stats.state==State::STOPPED);
    if(allowed) { stats.run=true; ++stats.generation; stats.state=State::STARTING; }
    portEXIT_CRITICAL(&mux); return allowed;
}
inline void delay(uint32_t ms) { portENTER_CRITICAL(&mux); stats.delayMs=ms; portEXIT_CRITICAL(&mux); }
inline void inject(Injection value) { portENTER_CRITICAL(&mux); stats.injection=value; portEXIT_CRITICAL(&mux); }
inline Injection takeInjection() {
    portENTER_CRITICAL(&mux); const auto value=stats.injection; stats.injection=Injection::NONE; portEXIT_CRITICAL(&mux); return value;
}
inline void storageTime(uint32_t elapsed) {
    portENTER_CRITICAL(&mux); if(elapsed>stats.maxStorageUs) stats.maxStorageUs=elapsed; portEXIT_CRITICAL(&mux);
}
// Worker-only POSIX adapter; preserve short/failed writes without replaying.
struct Backend {
    int fd;
    Injection injection;
    Failure errorPhase=Failure::NONE;
    int error=0;
    int write(const char* bytes,size_t length) {
        if(injection==Injection::WRITE || injection==Injection::FULL) {
            errorPhase=Failure::WRITE; error=injection==Injection::FULL?ENOSPC:EIO; return -1;
        }
        const uint32_t start=micros();
        const ssize_t n=::write(fd,bytes,length);
        const int saved=n<0?errno:0;
        const uint32_t elapsed=micros()-start;
        portENTER_CRITICAL(&mux); if(elapsed>stats.maxWriteUs) stats.maxWriteUs=elapsed; portEXIT_CRITICAL(&mux);
        if(n!=static_cast<ssize_t>(length)) { errorPhase=Failure::WRITE; error=saved?saved:EIO; }
        return static_cast<int>(n);
    }
    bool sync() {
        if(injection==Injection::SYNC) { errorPhase=Failure::SYNC; error=EIO; return false; }
        const uint32_t start=micros();
        const int result=::fsync(fd),saved=result?errno:0;
        const uint32_t elapsed=micros()-start;
        portENTER_CRITICAL(&mux); if(elapsed>stats.maxSyncUs) stats.maxSyncUs=elapsed; portEXIT_CRITICAL(&mux);
        if(result) { errorPhase=Failure::SYNC; error=saved; }
        return result==0;
    }
};
inline bool closeFile(int& fd) {
    if(fd<0) return true;
    const int old=fd; fd=-1;
    if(::close(old)!=0) { const int saved=errno; fail(Failure::CLOSE,saved); return false; }
    return true;
}
inline void discardQueue() {
    EventLog::Event event;
    while(xQueueReceive(queue,&event,0)==pdTRUE) {
        portENTER_CRITICAL(&mux); ++stats.dropped; portEXIT_CRITICAL(&mux);
    }
}
inline bool openSegment(int& fd) {
    const uint32_t started=micros();
    if(!SD.begin(13,storageSpi,1000000,"/sd",2,false) || SD.cardType()==CARD_NONE) {
        fail(Failure::MOUNT,EIO); SD.end(); storageTime(micros()-started); return false;
    }
    if(::mkdir("/sd/MEDLOG",0700)!=0 && errno!=EEXIST) {
        const int saved=errno; fail(Failure::DIRECTORY,saved); SD.end(); storageTime(micros()-started); return false;
    }
    struct stat info{};
    if(::stat("/sd/MEDLOG",&info)!=0 || !S_ISDIR(info.st_mode)) {
        fail(Failure::DIRECTORY,ENOTDIR); SD.end(); storageTime(micros()-started); return false;
    }
    const auto s=snapshot();
    char path[64];
    for(unsigned segment=0;segment<100;++segment) {
        snprintf(path,sizeof(path),"/sd/MEDLOG/B%016llx_%02u.CSV",static_cast<unsigned long long>(s.boot),segment);
        fd=::open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
        if(fd>=0 || errno!=EEXIST) break;
    }
    if(fd<0) { const int saved=errno; fail(Failure::OPEN,saved); SD.end(); storageTime(micros()-started); return false; }
    portENTER_CRITICAL(&mux); std::strncpy(stats.path,path,sizeof(stats.path)-1); portEXIT_CRITICAL(&mux);
    Backend backend{fd,Injection::NONE};
    if(!EventLog::commit(backend,EventLog::HEADER,sizeof(EventLog::HEADER)-1)) {
        closeFile(fd); fail(Failure::HEADER,backend.error); SD.end(); storageTime(micros()-started); return false;
    }
    portENTER_CRITICAL(&mux); stats.failure=Failure::NONE; stats.error=0; stats.state=State::READY; portEXIT_CRITICAL(&mux);
    storageTime(micros()-started); return true;
}
inline void worker(void*) {
    digitalWrite(13,HIGH); pinMode(13,OUTPUT);
    storageSpi.begin(18,19,23,13);
    int fd=-1;
    uint32_t generation=0;
    for(;;) {
        auto s=snapshot();
        if(s.generation!=generation) {
            generation=s.generation;
            closeFile(fd); SD.end();
            if(s.run) { state(State::STARTING); openSegment(fd); }
            s=snapshot();
        }
        if(!s.run) {
            if(fd<0) {
                discardQueue(); SD.end(); state(State::STOPPED);
                vTaskDelay(pdMS_TO_TICKS(20)); continue;
            }
            state(State::DRAINING);
        }
        if(fd<0 || s.state==State::FAULT) {
            vTaskDelay(pdMS_TO_TICKS(20)); continue;
        }
        EventLog::Event event;
        if(xQueueReceive(queue,&event,pdMS_TO_TICKS(10))==pdTRUE) {
            portENTER_CRITICAL(&mux); stats.inflight=true; portEXIT_CRITICAL(&mux);
            const uint32_t sleepMs=snapshot().delayMs;
            if(sleepMs) vTaskDelay(pdMS_TO_TICKS(sleepMs));
            const uint32_t started=micros();
            const auto current=snapshot();
            char line[EventLog::LINE_BYTES];
            const size_t length=EventLog::encode(event,current.boot,current.dropped,current.uncertain,line,sizeof(line));
            Backend backend{fd,takeInjection()};
            const bool ok=length && EventLog::commit(backend,line,length);
            storageTime(micros()-started);
            portENTER_CRITICAL(&mux);
            stats.inflight=false;
            if(ok) { ++stats.committed; stats.lastCommitted=event.sequence; }
            else { ++stats.uncertain; stats.lastUncertain=event.sequence; }
            portEXIT_CRITICAL(&mux);
            if(!ok) {
                closeFile(fd); SD.end();
                fail(length?backend.errorPhase:Failure::ENCODE,length?backend.error:EINVAL);
            }
            vTaskDelay(1); // Yield to core0 idle/watchdog even under a burst.
        } else if(!snapshot().run) {
            const bool closed=closeFile(fd); SD.end();
            if(closed) state(State::STOPPED);
        }
    }
}
inline void begin() {
    portENTER_CRITICAL(&mux);
    stats.boot=(static_cast<uint64_t>(esp_random())<<32)|esp_random();
    portEXIT_CRITICAL(&mux);
    queue=xQueueCreateStatic(CAPACITY,sizeof(EventLog::Event),queueStorage,&queueControl);
    if(!queue) { fail(Failure::QUEUE,ENOMEM); return; }
    // ESP-IDF stack size argument is bytes. Loop task remains on core1.
    if(xTaskCreatePinnedToCore(worker,"sd_logger",8192,nullptr,1,&task,0)!=pdPASS) {
        task=nullptr; fail(Failure::TASK,ENOMEM);
    }
}
inline void printStatus() {
    const auto s=snapshot();
    Serial.printf("# logger=%s fault=%s errno=%d boot=%016llx pending=%u inflight=%s capacity=%u\n",
        stateName(s.state),failureName(s.failure),s.error,static_cast<unsigned long long>(s.boot),pending(),s.inflight?"yes":"no",CAPACITY);
    Serial.printf("# attempted=%llu accepted=%llu synced=%llu dropped=%llu uncertain=%llu last_synced_seq=%llu last_uncertain_seq=%llu\n",
        static_cast<unsigned long long>(s.attempted),static_cast<unsigned long long>(s.accepted),static_cast<unsigned long long>(s.committed),
        static_cast<unsigned long long>(s.dropped),static_cast<unsigned long long>(s.uncertain),
        static_cast<unsigned long long>(s.lastCommitted),static_cast<unsigned long long>(s.lastUncertain));
    Serial.printf("# log_path=%s max_storage_us=%lu max_write_us=%lu max_sync_us=%lu injected_delay_ms=%lu\n",
        s.path,static_cast<unsigned long>(s.maxStorageUs),static_cast<unsigned long>(s.maxWriteUs),
        static_cast<unsigned long>(s.maxSyncUs),static_cast<unsigned long>(s.delayMs));
    Serial.println("# Queued is not synced; sync API success is not guaranteed power-loss durability; counts are RAM-only");
}
} // namespace QueuedSdLogger
