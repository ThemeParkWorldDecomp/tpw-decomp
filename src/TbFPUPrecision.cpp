// TbFPUPrecision.cpp (decompiled from the bullfrog library of SimTheme Park 2.04b3,
// functions at 0x10016070..0x10016234).
//
// DoFlagGet and DoFlagSet are inline in the original (no out-of-line copies). On the PC
// they read and load the x87 control word; what DoFlagSet would load is not recoverable
// from the Mac build, whose assembler is missing.
#include "TbFPUPrecision.h"

#include "platform/fpu.h"
#include "platform/interfacelib.h"

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
