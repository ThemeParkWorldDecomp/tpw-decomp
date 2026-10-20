// SMachineParameters: what the engine measures about the machine at start (video memory, upload speed, memory, CPU
// clock, math speed, fill rate), written to "detect.txt" and turned into a flag word by comparing against the records of
// the data section. Unit v5092 (SMachineParameters::Output, ::Suggest).
#pragma once
#include <cstdint>

// A configuration record of the data section (opaque here; their spacing in the data is 0x20 or more).
struct SCfgRecord {
    uint8_t mBytes[0x20];
};

// The records Suggest hands to Return: the Mac's data section from r2 - 0x5c74 for 0x400 bytes (contents unknown here, so
// zero; Return is not translated yet).
extern uint8_t gSMachineCfgData[0x400];

// engine data 0x100af124: how often Output ran (the first run truncates detect.txt, later ones append)
extern int32_t gSMachineOutputCount;

struct SMachineParameters {
    // Opens detect.txt and writes the measured values as text.
    void Output();                      // 0x10013d3c
    // Measures, calls Output, and builds mFlags from sixteen Return calls; answers 1.
    int32_t Suggest();                  // 0x10013eec
    // Not translated yet (0x10013c2c).
    uint32_t Return(SCfgRecord* record, int32_t mode);   // 0x10013c2c

    uint32_t mFlags;                    // 0x00
    int32_t mF04;                       // 0x04
    int32_t mF08;                       // 0x08
    int32_t mF0C;                       // 0x0c
    int32_t mVRAMSize;                  // 0x10  render system VRAM size / 1024
    float mVRAMSpeed;                   // 0x14  CTextureCache::TestUploadSpeed(0x100, 0x10)
    int32_t mRAMSize;                   // 0x18  physical memory / 1024
    int32_t mCPUFreq;                   // 0x1c  clocks per ms / 1000
    float mMathSpeed;                   // 0x20  CTransformBase::TestMathSpeed
    float mFillRate;                    // 0x24  CDrawPrimitive::GetPixelFillRate
};
