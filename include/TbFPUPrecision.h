// TbFPUPrecision.h: bullfrog library FPU flag guards (original TbFPUPrecision.cpp, named
// by its "Missing assembler" strings; functions at 0x10016070..0x10016234).
//
// PARTIAL: TbFPUFlag only (TbFPUSinglePrecision derives from it; its destructor is at
// 0x1006ab38). The PC build forces x87 control bits while an object lives; the Mac port
// has no assembler for the FPU accesses, so each one only reports itself with DebugStr.
#pragma once
#include <cstdint>

class TbFPUFlag {
public:
    // Inline; its out-of-line copy is at 0x1006abdc (after WinScrn.cpp's static
    // initialiser). Vtable 0x10073a90.
    virtual ~TbFPUFlag() { Reset(); }

    // 0 if the FPU flags have every bit of mSet and none of mClear, otherwise 1 (after
    // trying to set them again).
    int32_t CheckFlags();                          // 0x10016070
    // Restores the saved bits (if any were forced) and forgets them.
    void Reset();                                  // 0x1001611c
    // Forces `set` on and `clear` off, keeping the original state of each forced bit in
    // mSaved.
    void SetFlags(uint16_t set, uint16_t clear);   // 0x100161b4

    uint16_t mSaved;   // 0x04 (vptr at 0x00)
    uint16_t mSet;     // 0x06
    uint16_t mClear;   // 0x08

private:
    // The FPU accesses; on the Mac each only calls DebugStr ("Missing assembler").
    static uint16_t DoFlagGet();
    static void DoFlagSet();
};

// Derived guard; only its destructor (0x1006ab38) is translated. Vtable 0x10073a84
// (replaced by TbFPUFlag's 0x10073a90 while the base part is destroyed).
class TbFPUSinglePrecision : public TbFPUFlag {
public:
    ~TbFPUSinglePrecision() override;              // 0x1006ab38
};
