// std::basic_ifstream<wchar_t>::~basic_ifstream 0x1021d5bc (324 B), unit v5201.
#include <cstdint>

#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

BasicIfstreamWchar* BasicIfstreamWchar::destroy(int16_t flag) {
    if (this == nullptr) return this;
    auto* filebuf = reinterpret_cast<BasicFilebufWchar*>(reinterpret_cast<uint8_t*>(this) + 0x10);   // Mac this+0xc
    void* locale = mLocale;                                                                          // Mac this+0x28
    void* ios = &mIosVtable;                                                                         // Mac this+0x5c
    const uint8_t* files = gIfsWFiles;

    mVtable = gIfsWVtblB;
    mVbase->mFirst = &gIfsWVtblB[3];   // vtable + 0xc
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(ios) - reinterpret_cast<const uint8_t*>(mVbase);
    // The original tests this+0xc and this+0x28 against 0 (never true for a real object).
    mFilebufVt = gIfsWVtblD;
    if (mFile != files && mFile != files + 0x54 && mFile != files + 0xa8) filebuf->close();
    mFilebufVt = gIfsWVtblC;
    ifstream_callee::LocaleDtor(locale, -1);

    mVtable = gIfsWVtblA;
    mVbase->mFirst = &gIfsWVtblA[3];
    mVbase->mBaseOffset = reinterpret_cast<const uint8_t*>(filebuf) - reinterpret_cast<const uint8_t*>(mVbase);
    if (flag != 0) {
        mIosVtable = gIfsWVtblE;
        ifstream_callee::IosBaseDtor(ios, 0);
    }
    if (flag > 0) ifstream_callee::Delete(this);
    return this;
}
