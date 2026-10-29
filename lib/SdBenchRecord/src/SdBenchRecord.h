#pragma once
#include <stddef.h>
#include <stdint.h>
#include <cstring>

namespace SdBenchRecord {
constexpr size_t RECORD_BYTES = 512;
constexpr unsigned MAX_SLOT = 9999;
constexpr char MAGIC[] = "SMART_MEDICATION_SD_BENCH_V1\n";
inline uint8_t byteAt(unsigned slot, size_t offset) {
    const size_t magicSize = sizeof(MAGIC)-1;
    if (offset < magicSize) return static_cast<uint8_t>(MAGIC[offset]);
    if (offset < magicSize+4) {
        constexpr unsigned divisors[]{1000,100,10,1};
        return '0'+(slot/divisors[offset-magicSize])%10;
    }
    if (offset==magicSize+4 || offset==RECORD_BYTES-1 || offset%64==63) return '\n';
    return 'A'+(slot+offset)%26;
}
inline bool matches(unsigned slot, size_t offset, const uint8_t* data, size_t length) {
    if (slot>MAX_SLOT || offset>RECORD_BYTES || length>RECORD_BYTES-offset || (!data && length)) return false;
    for (size_t i=0;i<length;++i) if (data[i]!=byteAt(slot,offset+i)) return false;
    return true;
}
inline bool complete(size_t readBytes, size_t fileBytes) {
    return readBytes==RECORD_BYTES && fileBytes==RECORD_BYTES;
}
enum class Action { NONE, HELP, MOUNT, UNMOUNT, STATUS, WRITE, VERIFY, INVALID };
struct Command { Action action; unsigned slot; };
inline bool space(char c) { return c==' ' || c=='\t'; }
inline Command parse(const char* line) {
    while(space(*line)) ++line;
    if(!*line) return {Action::NONE,0};
    const char* end=line;
    while(*end && !space(*end)) ++end;
    const size_t length=end-line;
    auto matchesWord=[&](const char* name) { return std::strlen(name)==length && !std::strncmp(line,name,length); };
    const Action action=matchesWord("help")?Action::HELP:matchesWord("mount")?Action::MOUNT:
        matchesWord("unmount")?Action::UNMOUNT:matchesWord("status")?Action::STATUS:
        matchesWord("write")?Action::WRITE:matchesWord("verify")?Action::VERIFY:Action::INVALID;
    while(space(*end)) ++end;
    if(action!=Action::WRITE && action!=Action::VERIFY)
        return {*end?Action::INVALID:action,0};
    unsigned slot=0,digits=0;
    while(*end>='0' && *end<='9') {
        if(++digits>4) return {Action::INVALID,0};
        slot=slot*10+*end++-'0';
    }
    while(space(*end)) ++end;
    return {digits && !*end ? action : Action::INVALID,slot};
}
} // namespace SdBenchRecord
