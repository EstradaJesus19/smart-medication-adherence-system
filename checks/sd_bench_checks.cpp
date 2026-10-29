#include <SdBenchRecord.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main() {
    using namespace SdBenchRecord;
    using A=Action;
    assert(parse("").action==A::NONE && parse(" \t ").action==A::NONE);
    assert(parse("mount").action==A::MOUNT && parse("unmount").action==A::UNMOUNT);
    assert(parse("help").action==A::HELP && parse(" status ").action==A::STATUS);
    assert(parse("write 0").action==A::WRITE && parse("write 0").slot==0);
    assert(parse("verify 9999").action==A::VERIFY && parse("verify 9999").slot==9999);
    assert(parse("\twrite\t0012  ").action==A::WRITE && parse("write 0012").slot==12);
    for(const char* text:{"write","verify","write -1","write +1","write 10000","write 00000",
        "verify 0x10","write 1 2","write 1x","write 1\n","mount extra","format","erase","write0"})
        assert(parse(text).action==A::INVALID);
    for(unsigned slot:{0U,1U,12U,999U,9999U}) {
        uint8_t record[RECORD_BYTES];
        for(size_t i=0;i<RECORD_BYTES;++i) record[i]=byteAt(slot,i);
        assert(!std::memcmp(record,MAGIC,sizeof(MAGIC)-1));
        assert(matches(slot,0,record,RECORD_BYTES));
        size_t read=0;
        while(read<RECORD_BYTES) { assert(matches(slot,read,record+read,64)); read+=64; }
        assert(complete(read,RECORD_BYTES));
        assert(!complete(read-1,RECORD_BYTES) && !complete(read,RECORD_BYTES-1) && !complete(read,RECORD_BYTES+1));
        assert(!matches(slot+1,0,record,RECORD_BYTES));
        for(size_t offset=0;offset<RECORD_BYTES;++offset) {
            record[offset]^=1; assert(!matches(slot,0,record,RECORD_BYTES)); record[offset]^=1;
        }
        assert(!matches(slot,RECORD_BYTES-1,record,2));
        assert(!matches(slot,SIZE_MAX,record,1));
        assert(!matches(slot,0,nullptr,1));
        assert(!matches(10000,0,record,RECORD_BYTES));
    }
    std::puts("SD bench checks passed: exact bounded commands, slot identity, complete/short/long records, every single-byte corruption and range guards");
}
