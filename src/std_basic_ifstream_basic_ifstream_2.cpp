// std::basic_ifstream<wchar_t>::basic_ifstream() 0x10221e20 (180 B), unit v5724.
#include "std_basic_ifstream.h"

extern char gIosBaseVt[];   // 0x103bd034 data import __vt__Q23std8ios_base

BasicIfstreamWchar* BasicIfstreamWchar::construct(int16_t flag) {
    // The Mac stores the virtual-base pointer (this + 0x5c) and the ios_base vtable words only when the flag is set.
    if (flag != 0) {
        mVbase = reinterpret_cast<IfstreamWVbTable*>(&mIosVtable);
        mIosVtable = gIosBaseVt;
        mIosVtable = gIfsWVtblE;
    }
    ifstream_callee::IstreamCtor(this, 0, &mFilebufVt);   // basic_istream(streambuf* = this + 0xc), flag 0
    mVtable = gIfsWVtblB;
    mVbase->mFirst = reinterpret_cast<const uint8_t*>(gIfsWVtblB) + 0xc;
    mVbase->mBaseOffset = reinterpret_cast<intptr_t>(&mIosVtable) - reinterpret_cast<intptr_t>(mVbase);
    ifstream_callee::FilebufCtor(&mFilebufVt, nullptr);   // basic_filebuf(FILE* = NULL)
    return this;
}
