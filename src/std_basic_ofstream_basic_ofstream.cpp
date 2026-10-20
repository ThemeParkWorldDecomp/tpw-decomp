// std::basic_ofstream<wchar_t>::basic_ofstream() 0x10221f20 (unit v5755).
// Mac layout: +0 virtual-base pointer, +4 vtable word, +8 filebuf, +0x58 the ios_base part (the virtual base).
// The host object uses native pointers: the slot sits at +8, the filebuf at +0x10 and the ios part at +0x38 (as basic_ifstream's).
#include <cstdint>

#include "online.h"
#include "std_basic_ifstream.h"

extern char gIosBaseVt[];         // 0x103bd034
extern uint32_t gOfsWVtbl[4];     // 0x102b1f50 (TOC -0x5eac)
extern uint32_t gIfsWVtblE[4];    // 0x102b1f20 (TOC -0x5eb0)

namespace ignore_callee {

void OfstreamCtor(IgnoreStream* stream, int32_t flag) {
    uint8_t* base = reinterpret_cast<uint8_t*>(stream);
    IgnoreIos* ios = reinterpret_cast<IgnoreIos*>(base + 0x38);
    // The virtual-base pointer and the ios_base vtable words are stored only when the flag (a short) is set.
    if (static_cast<int16_t>(flag) != 0) {
        stream->mIos = ios;
        ios->mVtable = gIosBaseVt;
        ios->mVtable = gIfsWVtblE;
    }
    WostreamCtor(stream, 0, base + 0x10);                 // basic_ostream(streambuf* = this + 8), flag 0
    stream->mSlot = gOfsWVtbl;
    stream->mIos->mVtable = reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc;
    stream->mIos->mBaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(ios) - reinterpret_cast<intptr_t>(stream->mIos));
    ifstream_callee::FilebufCtor(base + 0x10, nullptr);   // basic_filebuf(FILE* = NULL)
}

}  // namespace ignore_callee
