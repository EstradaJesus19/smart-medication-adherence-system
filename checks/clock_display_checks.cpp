#include <ClockDisplay.h>
#include <DailyScheduler.h>
#include <MedicationSystem.h>
#include <SessionAlert.h>
#include <cassert>
#include <cstdio>
#include <cstring>
using ClockDisplay::Transfer;
SessionAlert audio;
uint32_t nowMs=0;
namespace MedicationSystem {
void report(const char* name,int,uint32_t) { audio.event(name,nowMs); }
}
int main() {
    ClockDisplay::Calendar c{};
    assert(ClockDisplay::calendar(0,c) && c.year==2000 && c.month==1 && c.day==1);
    assert(ClockDisplay::calendar(59*86400U,c) && c.month==2 && c.day==29);
    assert(ClockDisplay::calendar(60*86400U,c) && c.month==3 && c.day==1);
    assert(ClockDisplay::calendar(366*86400U,c) && c.year==2001 && c.month==1 && c.day==1);
    assert(ClockDisplay::calendar(3155759999UL,c) && c.year==2099 && c.month==12 && c.day==31 && c.hour==23 && c.minute==59 && c.second==59);
    assert(!ClockDisplay::calendar(3155760000UL,c));
    assert(ClockDisplay::trusted(true,UINT32_MAX-50,100));
    assert(!ClockDisplay::trusted(false,0,1) && !ClockDisplay::trusted(true,0,1001));
    uint8_t frame[1024],received[1024]{};
    for (unsigned i=0;i<1024;++i) frame[i]=static_cast<uint8_t>(i/7);
    Transfer t; t.begin(); unsigned calls=0,bytes=0; bool on=false;
    auto writer=[&](uint8_t control,const uint8_t* data,size_t size) {
        ++calls; assert(size<=16);
        if (control==0x40) { std::memcpy(received+bytes,data,size); bytes+=size; }
        else { assert(control==0); if(size==1 && data[0]==0xaf) { assert(bytes==1024); on=true; } }
        return true;
    };
    while(!t.idle()) {
        assert(!t.request());
        const auto before=calls; t.service(frame,writer); assert(calls==before+1);
    }
    assert(on && calls==68 && bytes==1024 && t.frames()==1 && !std::memcmp(frame,received,1024));
    t.service(frame,writer); assert(calls==68); // Idle performs no bus work.
    assert(t.request()); bytes=0; calls=0;
    while(!t.idle()) t.service(frame,writer);
    assert(calls==65 && bytes==1024 && t.frames()==2);
    for(unsigned fail=0;fail<68;++fail) {
        Transfer f; f.begin(); unsigned transactions=0;
        while(!f.failed()) f.service(frame,[&](uint8_t,const uint8_t*,size_t size) {
            assert(size<=16); return transactions++!=fail;
        });
        const auto before=transactions;
        for(unsigned i=0;i<100;++i) f.service(frame,[&](uint8_t,const uint8_t*,size_t) { ++transactions; return true; });
        assert(transactions==before && !f.request()); // No automatic retry storm.
        f.begin(); assert(!f.failed());
        unsigned recovered=0;
        while(!f.idle()) f.service(frame,[&](uint8_t,const uint8_t*,size_t) { ++recovered; return true; });
        assert(recovered==68 && f.frames()==1);
    }
    // Exercise a scheduler/core/audio session while display transfers share each loop.
    using namespace MedicationSystem;
    const bool closed[4]{},due[4]{true,false,true,false};
    assert(initialize(closed,Settings{})); audio.begin(false,0); audio.sample(false,30);
    DailyScheduler scheduler; assert(scheduler.add(1,5) && scheduler.arm(0,0,true));
    Transfer display; display.begin(); unsigned busCalls=0;
    for(nowMs=0;nowMs<=61000;nowMs+=2) {
        const auto e=scheduler.observe(nowMs/1000,nowMs,true);
        if(e.result==DailyScheduler::Result::DUE) assert(startSession(due,e.scheduledAt,nowMs));
        if(nowMs==1200) audio.sample(true,nowMs);
        if(nowMs==1230) assert(audio.sample(true,nowMs));
        if(nowMs==2000) lidChanged(1,true,nowMs);
        update(nowMs);
        if(nowMs==4100) lidChanged(1,false,nowMs);
        if(display.idle() && nowMs%1000==0) display.request();
        display.service(frame,[&](uint8_t,const uint8_t*,size_t size) {
            ++busCalls; assert(size<=16); return nowMs<5000;
        });
        if(nowMs==6000) assert(display.failed() && scheduler.armed() && audio.muted());
    }
    assert(busCalls>100 && display.failed());
    assert(getCompartment(1)->latestDose.outcome==DoseOutcome::ACCESS_CONFIRMED);
    assert(getCompartment(3)->latestDose.outcome==DoseOutcome::MISSED);
    assert(getState()==SystemState::IDLE && !audio.active());
    std::puts("Clock display checks passed: calendar, freshness, exact frame order, one <=16-byte transaction per service, all transfer failure positions, core/scheduler/audio independence");
}
