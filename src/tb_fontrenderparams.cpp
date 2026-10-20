// ltms TbFontRenderParams: the constructors, the destructor and UseAlphaWriteMode (ltms 0x10002350, 0x10002400, 0x100024ac,
// 0x1000296c). The class's other members are untranslated (include/TbFontRenderParams.h). The destructor is the class's key
// function: the constructors write the vtable pointer, so the vtable has to be emitted where the destructor is defined.
#include <new>

#include "TbFontRenderParams.h"
#include "TbResource.h"

namespace {

// The five flag bytes at +0x44..+0x48 (the first five bytes of slot 16 on the host).
uint8_t* Flags(TbFontRenderParams& params) { return reinterpret_cast<uint8_t*>(&params.mState[16]); }

}  // namespace

// 0x10002350
TbFontRenderParams::TbFontRenderParams() {
    mState[0] = 0;
    mState[2] = 3;
    mState[3] = 1;
    new (&mState[4]) TbRenderArea;
    mState[14] = 0;
    mState[15] = 0;
    uint8_t* flags = Flags(*this);
    flags[0] = 0;
    flags[1] = 0;
    flags[2] = 0;
    flags[3] = 0;
    flags[4] = 0;
    mState[18] = 0;
}

// 0x10002400: lets go of the font (its virtual Release, slot 8 on the Mac) and forgets it. The original's deleting form
// (a positive halfword flag in r4 calls operator delete on `this`) is the compiler's handling of `delete`.
TbFontRenderParams::~TbFontRenderParams() {
    TbIFont* font = reinterpret_cast<TbIFont*>(mState[0]);
    if (font != nullptr) {
        font->Release();
        mState[0] = 0;
    }
}

// 0x100024ac
TbFontRenderParams::TbFontRenderParams(const TbFontRenderParams& other) {
    for (int32_t i = 0; i < 16; ++i) mState[i] = other.mState[i];
    const uint8_t* source = reinterpret_cast<const uint8_t*>(&other.mState[16]);
    uint8_t* flags = Flags(*this);
    for (int32_t i = 0; i < 5; ++i) flags[i] = source[i];
    mState[18] = other.mState[18];
    reinterpret_cast<TbIFont*>(mState[0])->AddRef();
}

// 0x1000296c: with a font, the slot at +0x10 is cleared and the answer is 0; without, -1.
TbError TbFontRenderParams::UseAlphaWriteMode() {
    if (mState[0] != 0) {
        mState[3] = 0;
        return TbError(0);
    }
    return TbError(-1);
}
