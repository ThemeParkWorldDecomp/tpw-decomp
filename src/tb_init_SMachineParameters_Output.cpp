// SMachineParameters::Output (0x10013d3c) and ::Suggest (0x10013eec), the engine's machine detection.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "SMachineParameters.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_matrix.h"
#include "tb_resource_system.h"
#include "tb_texturecache.h"

uint8_t gSMachineCfgData[0x400];     // the records at r2 - 0x5c74 .. (see the header)
int32_t gSMachineOutputCount;        // engine data 0x100af124

void SMachineParameters::Output() {
    char path[200];
    strcpy(path, "detect.txt");
    FILE* file;
    if (gSMachineOutputCount != 0) {
        file = fopen(path, "a");
    } else {
        gSMachineOutputCount = gSMachineOutputCount + 1;
        file = fopen(path, "w+b");
    }
    if (file == nullptr) return;
    // CDirectDraw + 0x21c: mDeviceInfo (0x1c) + the 512 bytes of szDriver = szDescription
    fprintf(file, "Graphics card description: %s\n", reinterpret_cast<const char*>(gei_DDraw) + 0x21c);
    fprintf(file, "VRAM Size: %d\n", mVRAMSize);
    fprintf(file, "VRAM Speed: %f\n", static_cast<double>(mVRAMSpeed));
    fprintf(file, "RAM Size: %d\n", mRAMSize);
    fprintf(file, "CPU Freq: %d\n", mCPUFreq);
    fprintf(file, "Math Speed (%d): %f\n", gei_System.mF20, static_cast<double>(mMathSpeed));
    fprintf(file, "Pixel Fill Rate (%d): %f\n\n", gei_System.mRenderer, static_cast<double>(mFillRate));
    fflush(file);
    fclose(file);
}

int32_t SMachineParameters::Suggest() {
    mVRAMSize = gei_System.mRenderSystem->GetVRAMSize() >> 10;
    mVRAMSpeed = static_cast<float>(gei_TCache->TestUploadSpeed(0x100, 0x10));
    mRAMSize = static_cast<int32_t>(gei_System.mTotalPhys) >> 10;
    mCPUFreq = gei_System.mClocksPerMs / 1000;
    mMathSpeed = gei_MathUnit->TestMathSpeed();
    mFillRate = gei_DrawPrimitive->GetPixelFillRate();
    Output();
    mFlags = 0;
    // offset in gSMachineCfgData (the record's address minus r2 + 0x5c74), mode
    static const int32_t kRecords[16][2] = {
        {0x000, 2}, {0x040, 1}, {0x060, 1}, {0x080, 1}, {0x0a0, 1}, {0x0c0, 1}, {0x0e0, 1}, {0x100, 3},
        {0x160, 1}, {0x180, 3}, {0x1e0, 3}, {0x240, 3}, {0x2a0, 1}, {0x2c0, 3}, {0x340, 2}, {0x3a0, 2},
    };
    for (int i = 0; i < 16; ++i) {
        const uint32_t flags = Return(reinterpret_cast<SCfgRecord*>(gSMachineCfgData + kRecords[i][0]), kRecords[i][1]);
        mFlags = mFlags | flags;
    }
    return 1;
}
