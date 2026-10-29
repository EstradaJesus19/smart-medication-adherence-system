#include <EventLog.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <climits>
struct Backend {
    int mode=0,writes=0,syncs=0;
    std::string bytes;
    int write(const char* text,size_t n) {
        ++writes;
        if(mode==1) return -1;
        const size_t count=mode==2?n-1:n;
        bytes.append(text,count); return static_cast<int>(count);
    }
    bool sync() { ++syncs; return mode!=3; }
};
int main(int argc,char** argv) {
    using namespace EventLog;
    assert(crc32("123456789",9)==0xcbf43926U);
    Event event; event.sequence=1; std::strcpy(event.name,"BOOT"); event.origin=Origin::LOGGER;
    char first[LINE_BYTES],line[LINE_BYTES];
    const size_t firstLength=encode(event,0x123456789abcdef0ULL,0,0,first,sizeof(first));
    assert(firstLength && first[firstLength-1]=='\n');
    assert(std::strstr(first,"1,123456789abcdef0,1,0,0,untrusted,logger,BOOT,0,0,0,0,IDLE,0,0,-,")==first);
    event.sequence=2; event.uptime=1000; event.rtc=80000000; event.quality=Quality::VALID;
    event.origin=Origin::CORE; event.compartment=1; event.doseAt=event.sessionAt=80000000;
    event.mask=5; event.state=1; std::strcpy(event.name,"DOSE_STARTED");
    const size_t length=encode(event,0x123456789abcdef0ULL,0,0,line,sizeof(line)); assert(length);
    Backend success; assert(commit(success,line,length) && success.writes==1 && success.syncs==1);
    for(int mode=1;mode<=3;++mode) {
        Backend fail; fail.mode=mode;
        assert(!commit(fail,line,length) && fail.writes==1);
        assert(fail.syncs==(mode==3?1:0)); // No false acknowledgment or retry.
    }
    Backend empty; assert(!commit(empty,line,0) && !empty.writes && !empty.syncs);
    char tiny[12]; assert(!encode(event,1,0,0,tiny,sizeof(tiny)));
    assert(!encode(event,1,0,0,nullptr,100));
    auto bad=event; std::strcpy(bad.name,"BAD,EVENT"); assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; std::memset(bad.name,'X',sizeof(bad.name)); assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; std::strcpy(bad.detail,"INJECTED\nROW"); assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; bad.sequence=0; assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; bad.compartment=5; assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; bad.mask=16; assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; bad.state=4; assert(!encode(bad,1,0,0,line,sizeof(line)));
    bad=event; bad.quality=Quality::STALE;
    assert(encode(bad,1,3,2,line,sizeof(line)) && std::strstr(line,",0,stale,"));
    auto largest=event;
    largest.sequence=UINT64_MAX; largest.uptime=largest.rtc=largest.doseAt=largest.sessionAt=UINT32_MAX;
    std::memset(largest.name,'A',sizeof(largest.name)-1);
    std::memset(largest.detail,'Z',sizeof(largest.detail)-1);
    largest.state=2;
    assert(encode(largest,UINT64_MAX,UINT64_MAX,UINT64_MAX,line,sizeof(line)));
    if(argc==2) {
        FILE* file=std::fopen(argv[1],"wb"); assert(file);
        assert(std::fwrite(HEADER,1,sizeof(HEADER)-1,file)==sizeof(HEADER)-1);
        assert(std::fwrite(first,1,firstLength,file)==firstLength);
        const size_t row=encode(event,0x123456789abcdef0ULL,0,0,line,sizeof(line));
        assert(std::fwrite(line,1,row,file)==row && !std::fclose(file));
    }
    std::puts("Event log checks passed: CRC, schema, trust marking, bounds, short/error/sync failures without retry or false success");
}
