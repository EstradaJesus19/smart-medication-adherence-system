#pragma once
#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstring>

namespace EventLog {
constexpr size_t LINE_BYTES=384;
constexpr char HEADER[]="v,boot_id,seq,uptime_ms,rtc_s2000,rtc_quality,origin,event,compartment,scheduled_at_s2000,session_at_s2000,due_mask,state,dropped,uncertain,detail,crc32\n";
enum class Quality : uint8_t { VALID, UNTRUSTED, STALE };
enum class Origin : uint8_t { CORE, SCHEDULER, CONTROL, TEST, LOGGER };
struct Event {
    uint64_t sequence=0;
    uint32_t uptime=0,rtc=0,doseAt=0,sessionAt=0;
    char name[40]{},detail[40]{};
    uint8_t compartment=0,mask=0,state=0;
    Quality quality=Quality::UNTRUSTED;
    Origin origin=Origin::CORE;
};
inline uint32_t crc32(const char* text,size_t length) {
    uint32_t crc=0xffffffffU;
    for(size_t i=0;i<length;++i) {
        crc^=static_cast<uint8_t>(text[i]);
        for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U)));
    }
    return ~crc;
}
inline const char* qualityName(Quality q) {
    switch(q) { case Quality::VALID:return "valid"; case Quality::UNTRUSTED:return "untrusted"; case Quality::STALE:return "stale"; }
    return "invalid";
}
inline const char* originName(Origin o) {
    switch(o) { case Origin::CORE:return "core"; case Origin::SCHEDULER:return "scheduler";
    case Origin::CONTROL:return "control"; case Origin::TEST:return "test"; case Origin::LOGGER:return "logger"; }
    return "invalid";
}
inline const char* stateName(uint8_t s) {
    switch(s) { case 0:return "IDLE";case 1:return "REMINDER";case 2:return "WAITING_FOR_CLOSURE";case 3:return "REFILL"; }
    return "UNKNOWN";
}
inline size_t encode(const Event& e,uint64_t boot,uint64_t dropped,uint64_t uncertain,char* out,size_t capacity) {
    size_t nameLength=0;
    while(nameLength<sizeof(e.name) && e.name[nameLength]) {
        const char c=e.name[nameLength++];
        if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_')) return 0;
    }
    if(!nameLength || nameLength==sizeof(e.name) || !e.sequence || e.compartment>4 || e.mask>15 || e.state>3 ||
       static_cast<unsigned>(e.quality)>2 || static_cast<unsigned>(e.origin)>4 || !out || capacity<12) return 0;
    size_t detailLength=0;
    while(detailLength<sizeof(e.detail) && e.detail[detailLength]) {
        const char c=e.detail[detailLength++];
        if(!((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_' || c==':' || c=='-' || c=='.')) return 0;
    }
    if(detailLength==sizeof(e.detail)) return 0;
    const int length=std::snprintf(out,capacity,"1,%016llx,%llu,%lu,%lu,%s,%s,%s,%u,%lu,%lu,%u,%s,%llu,%llu,%s",
        static_cast<unsigned long long>(boot),static_cast<unsigned long long>(e.sequence),
        static_cast<unsigned long>(e.uptime),static_cast<unsigned long>(e.quality==Quality::VALID?e.rtc:0),
        qualityName(e.quality),originName(e.origin),e.name,static_cast<unsigned>(e.compartment),
        static_cast<unsigned long>(e.doseAt),static_cast<unsigned long>(e.sessionAt),static_cast<unsigned>(e.mask),stateName(e.state),
        static_cast<unsigned long long>(dropped),static_cast<unsigned long long>(uncertain),detailLength?e.detail:"-");
    if(length<0 || static_cast<size_t>(length)+11>capacity) return 0;
    const int tail=std::snprintf(out+length,capacity-length,",%08lx\n",static_cast<unsigned long>(crc32(out,length)));
    return tail==10 ? static_cast<size_t>(length)+10 : 0;
}
// No retry of a partial/uncertain row. Callers retain the file and start a new
// segment after explicit recovery. Success means full write + sync API success.
template<class Backend> bool commit(Backend& backend,const char* bytes,size_t length) {
    if(!length || backend.write(bytes,length)!=static_cast<int>(length)) return false;
    return backend.sync();
}
} // namespace EventLog
