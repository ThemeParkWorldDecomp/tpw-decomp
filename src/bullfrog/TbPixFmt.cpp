// TbPixFmt.cpp: name proven by __sinit_TbPixFmt_cpp in the Bullfrog library.

#include "platform/fpu.h"
#include "platform/interfacelib.h"
#include "TbFPUPrecision.h"
#include "TbPixFmt.h"

// TbFPUPrecision.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x10016070..0x10016234).
//
// DoFlagGet and DoFlagSet are inline in the original (no out-of-line copies). On the PC
// they read and load the x87 control word; what DoFlagSet would load is not recoverable
// from the Mac build, whose assembler is missing.

namespace {
// Pascal strings (length byte first), 0x1006c751 and 0x1006c7a1.
const unsigned char kMissingFlagGet[] =
    "\x4fMissing assembler  - TbFPUFlag::DoFlagGet TbFPUPrecision.cpp (Bullfrog library)";
const unsigned char kMissingFlagSet[] =
    "\x4fMissing assembler  - TbFPUFlag::DoFlagSet TbFPUPrecision.cpp (Bullfrog library)";
}  // namespace

uint16_t TbFPUFlag::DoFlagGet() {
    DebugStr(kMissingFlagGet);
    return PlatformFPUFlags();
}

void TbFPUFlag::DoFlagSet() {
    DebugStr(kMissingFlagSet);
}

int32_t TbFPUFlag::CheckFlags() {
    uint16_t flags = DoFlagGet();
    if ((flags & mSet) == mSet && (flags & mClear) == 0) return 0;
    DoFlagSet();
    return 1;
}

void TbFPUFlag::Reset() {
    if (mSet != 0 || mClear != 0) {
        DoFlagGet();
        DoFlagSet();
    }
    mClear = 0;
    mSet = 0;
}

void TbFPUFlag::SetFlags(uint16_t set, uint16_t clear) {
    uint16_t flags = DoFlagGet();
    uint16_t forced = mSet | mClear;
    mSet = set;
    mClear = clear;
    mSaved = (mSaved | (flags & ~forced)) & (mSet | mClear);
    DoFlagSet();
}

// TbPixFmt.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3, functions
// at 0x10016264..0x10016534).

// __sinit_TbPixFmt_cpp at 0x10016484 (inline constructors).
TbPixFormat TbPixFormat8(0, 0, 0, 0, 8, 1);
TbPixFormat TbPixFormat16(0x7c00, 0x3e0, 0x1f, 0, 16, 0);
TbPixFormat TbPixFormat24(0xff0000, 0xff00, 0xff, 0, 24, 0);
TbPixFormat TbPixFormat32(0xff0000, 0xff00, 0xff, 0xff000000u, 32, 0);

void TbMaskInfo::Set(uint32_t mask, uint32_t topBit) {
    mMask = mask;
    uint32_t top = 31;
    if (mask != 0) {
        for (uint32_t bit = 0x80000000u; (mask & bit) == 0; bit >>= 1) --top;
    }
    if (top >= topBit) {
        mShiftRight = static_cast<int16_t>(top - topBit);
        mShiftLeft = 0;
    } else {
        mShiftRight = 0;
        mShiftLeft = static_cast<int16_t>(topBit - top);
    }
}

// The original inlines TbMaskInfo::Set and does alpha first.
void TbPixelCaps::Set(const TbPixFormat& format) {
    mAlpha.Set(format.mAlphaMask, 31);
    mRed.Set(format.mRedMask, 23);
    mGreen.Set(format.mGreenMask, 15);
    mBlue.Set(format.mBlueMask, 7);
}
