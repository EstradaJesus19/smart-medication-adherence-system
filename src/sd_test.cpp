#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <SdBenchRecord.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

namespace {
constexpr int CS_PIN=13, SCK_PIN=18, MISO_PIN=19, MOSI_PIN=23, BUZZER_PIN=14;
constexpr uint32_t SPI_HZ=1000000;
constexpr char DIRECTORY[]="/sd/SD_BENCH";
bool mounted=false,writeFault=false;
char line[64]{};
unsigned used=0;
bool overflow=false;
int lastVerified=-1;
uint32_t mountUs=0,maxOperationUs=0;

void track(uint32_t us) { if(us>maxOperationUs) maxOperationUs=us; }
void ioError(const char* operation,int error) {
    writeFault=true;
    Serial.printf("# SD_IO_ERROR operation=%s errno=%d (%s); writes inhibited until unmount/mount\n",
        operation,error,std::strerror(error));
}
void pathFor(unsigned slot,char* path,size_t length) {
    snprintf(path,length,"%s/T%04u.TXT",DIRECTORY,slot);
}
void status() {
    const auto type=mounted?SD.cardType():CARD_NONE;
    Serial.printf("# mounted=%s type=%s card_bytes=%llu spi_hz=%lu write_fault=%s last_verified_slot=%d\n",
        mounted?"yes":"no",type==CARD_SDHC?"SDHC":type==CARD_SD?"SD":type==CARD_MMC?"MMC":"NONE",
        static_cast<unsigned long long>(mounted?SD.cardSize():0),static_cast<unsigned long>(SPI_HZ),
        writeFault?"yes":"no",lastVerified);
    Serial.printf("# mount_us=%lu max_operation_us=%lu (blocking bench measurements, not worst-case guarantees)\n",
        static_cast<unsigned long>(mountUs),static_cast<unsigned long>(maxOperationUs));
    Serial.println("# Mounted status is cached, not a live card-presence detector; no RTC/OLED access");
}
void mountCard() {
    if(mounted) { Serial.println("# Already mounted; use unmount then mount to retry"); return; }
    const uint32_t start=micros();
    // Never auto-format an unsupported, unformatted or damaged card.
    mounted=SD.begin(CS_PIN,SPI,SPI_HZ,"/sd",2,false);
    if(mounted && SD.cardType()==CARD_NONE) { SD.end(); mounted=false; }
    mountUs=micros()-start; track(mountUs);
    if(!mounted) {
        SD.end();
        Serial.println("# SD_MOUNT_FAILED: no card, wiring/power fault or unsupported/damaged filesystem; no format attempted");
    } else {
        writeFault=false;
        Serial.println("# SD_MOUNT_OK: no test files written automatically");
    }
    status();
}
void unmountCard() {
    const uint32_t start=micros();
    SD.end(); mounted=false;
    digitalWrite(CS_PIN,HIGH);
    track(micros()-start);
    Serial.println("# SD_UNMOUNTED; power OFF before removing/inserting the card");
}
bool verify(unsigned slot) {
    lastVerified=-1;
    if(!mounted) { Serial.println("# VERIFY_FAIL: card not mounted"); return false; }
    char path[40]; pathFor(slot,path,sizeof(path));
    const uint32_t start=micros();
    const int fd=::open(path,O_RDONLY);
    if(fd<0) {
        const int error=errno;
        if(error==ENOENT) Serial.printf("# VERIFY_FAIL slot=%u file not found\n",slot);
        else ioError("verify_open",error);
        track(micros()-start); return false;
    }
    struct stat info{};
    bool ok=true;
    if(::fstat(fd,&info)!=0) { const int error=errno; ioError("verify_stat",error); ok=false; }
    else if(!S_ISREG(info.st_mode) || info.st_size!=SdBenchRecord::RECORD_BYTES) {
        Serial.println("# VERIFY_FAIL: not a regular 512-byte test record (left unchanged)"); ok=false;
    }
    size_t bytes=0;
    uint8_t buffer[64];
    while(ok && bytes<SdBenchRecord::RECORD_BYTES) {
        const ssize_t got=::read(fd,buffer,sizeof(buffer));
        if(got<0 && errno==EINTR) continue;
        if(got<=0) {
            if(got<0) { const int error=errno; ioError("verify_read",error); }
            else Serial.println("# VERIFY_FAIL: unexpected end of file");
            ok=false; break;
        }
        if(!SdBenchRecord::matches(slot,bytes,buffer,static_cast<size_t>(got))) {
            Serial.println("# VERIFY_FAIL: byte mismatch (left unchanged)"); ok=false;
        }
        bytes+=static_cast<size_t>(got);
    }
    if(::close(fd)!=0) { const int error=errno; ioError("verify_close",error); ok=false; }
    ok=ok && SdBenchRecord::complete(bytes,static_cast<size_t>(info.st_size));
    const uint32_t elapsed=micros()-start; track(elapsed);
    if(ok) lastVerified=static_cast<int>(slot);
    Serial.printf("# VERIFY_%s slot=%u bytes_checked=%u verify_us=%lu\n",
        ok?"PASS":"FAIL",slot,static_cast<unsigned>(bytes),static_cast<unsigned long>(elapsed));
    return ok;
}
void writeNew(unsigned slot) {
    if(!mounted) { Serial.println("# WRITE_FAIL: card not mounted"); return; }
    if(writeFault) { Serial.println("# WRITE_INHIBITED: inspect error; unmount/mount before retry"); return; }
    lastVerified=-1;
    const uint32_t totalStart=micros();
    struct stat info{};
    // Create only our dedicated directory; never format or delete anything.
    if(::mkdir(DIRECTORY,0700)!=0) {
        const int error=errno;
        if(error!=EEXIST) { ioError("mkdir",error); track(micros()-totalStart); return; }
    }
    if(::stat(DIRECTORY,&info)!=0 || !S_ISDIR(info.st_mode)) {
        Serial.println("# WRITE_FAIL: SD_BENCH is unavailable/not a directory; nothing overwritten");
        writeFault=true; track(micros()-totalStart); return;
    }
    char path[40]; pathFor(slot,path,sizeof(path));
    uint8_t record[SdBenchRecord::RECORD_BYTES];
    for(size_t i=0;i<sizeof(record);++i) record[i]=SdBenchRecord::byteAt(slot,i);
    uint32_t start=micros();
    // O_EXCL rejects an existing path atomically; FILE_WRITE would truncate it.
    const int fd=::open(path,O_WRONLY|O_CREAT|O_EXCL,0600);
    const int openError=fd<0?errno:0;
    const uint32_t openUs=micros()-start;
    if(fd<0) {
        if(openError==EEXIST) Serial.printf("# FILE_EXISTS: /SD_BENCH/T%04u.TXT unchanged; choose another slot\n",slot);
        else ioError("create_exclusive",openError);
        track(micros()-totalStart); return;
    }
    start=micros();
    const ssize_t written=::write(fd,record,sizeof(record));
    const int writeError=written<0?errno:0;
    const uint32_t writeUs=micros()-start;
    bool ok=written==static_cast<ssize_t>(sizeof(record));
    if(!ok) {
        if(written<0) ioError("write",writeError);
        else { writeFault=true; Serial.println("# WRITE_FAIL: short write; partial file retained, not retried"); }
    }
    start=micros();
    const int syncResult=::fsync(fd);
    const int syncError=syncResult!=0?errno:0;
    const uint32_t syncUs=micros()-start;
    if(syncResult!=0) { ioError("sync",syncError); ok=false; }
    start=micros();
    if(::close(fd)!=0) { const int error=errno; ioError("close",error); ok=false; }
    const uint32_t closeUs=micros()-start;
    // No retries, deletion or repair: leave evidence of interrupted/failed writes.
    const bool readback=verify(slot);
    ok=ok && readback;
    if(!ok) writeFault=true;
    const uint32_t totalUs=micros()-totalStart; track(totalUs);
    Serial.printf("# WRITE_%s slot=%u requested=512 returned=%ld open_us=%lu write_us=%lu sync_us=%lu close_us=%lu total_us=%lu\n",
        ok?"PASS":"FAIL",slot,static_cast<long>(written),static_cast<unsigned long>(openUs),
        static_cast<unsigned long>(writeUs),static_cast<unsigned long>(syncUs),
        static_cast<unsigned long>(closeUs),static_cast<unsigned long>(totalUs));
    Serial.println("# Readback PASS is not proof of power-loss durability; verify again after a clean power cycle");
}
void help() {
    Serial.println("# mount | unmount | status | write N | verify N | help (N=0..9999)");
    Serial.println("# write N creates /SD_BENCH/TNNNN.TXT, then syncs/closes/reopens/verifies 512 known bytes");
    Serial.println("# Existing paths refused; no overwrite/delete/format command; no automatic file writes");
    Serial.println("# Standalone BLOCKING bench: no reminders, buttons, OLED updates or RTC/NVS writes");
    Serial.println("# CS13 SCK18 MOSI23 MISO19; module VCC=verified 5V, internal rail=3.3V; keep expansion rail 3.3V");
}
void command() {
    const auto cmd=SdBenchRecord::parse(line);
    using A=SdBenchRecord::Action;
    switch(cmd.action) {
    case A::NONE: break;
    case A::HELP: help(); break;
    case A::MOUNT: mountCard(); break;
    case A::UNMOUNT: unmountCard(); break;
    case A::STATUS: status(); break;
    case A::WRITE: writeNew(cmd.slot); break;
    case A::VERIFY: verify(cmd.slot); break;
    case A::INVALID: Serial.println("# INVALID_COMMAND: no action; use help"); break;
    }
}
} // namespace
void setup() {
    digitalWrite(BUZZER_PIN,LOW); pinMode(BUZZER_PIN,OUTPUT);
    for(int pin:{26,27,32,33}) { digitalWrite(pin,LOW); pinMode(pin,OUTPUT); }
    digitalWrite(CS_PIN,HIGH); pinMode(CS_PIN,OUTPUT);
    Serial.begin(115200);
    SPI.begin(SCK_PIN,MISO_PIN,MOSI_PIN,CS_PIN);
    Serial.println("# Standalone sd_test: buzzer/LEDs off; any retained OLED image is stale");
    help(); mountCard();
}
void loop() {
    for(unsigned i=0;i<64 && Serial.available();++i) {
        const char byte=static_cast<char>(Serial.read());
        if(byte=='\r') continue;
        if(byte=='\n') {
            if(overflow) Serial.println("# INVALID_COMMAND: long line/control byte discarded; no action");
            else { line[used]='\0'; command(); }
            used=0; overflow=false;
        } else if((static_cast<unsigned char>(byte)<32 && byte!='\t') || static_cast<unsigned char>(byte)>126) overflow=true;
        else if(!overflow && used+1<sizeof(line)) line[used++]=byte;
        else overflow=true;
    }
    yield();
}
