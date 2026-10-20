// tb_decode_wavelet.cpp (engine library of SimTheme Park 2.04b3; the name is the one in
// its assertions, the inventory files the range under tb_depthoffield.cpp): the engine's
// reference to the wavelet decompressor. PARTIAL: CWaveLoad (0x10005de4..0x100067b8), the
// loader that uses it, is still to do.
#include "Wavelet.h"
#include "platform/msl.h"
#include "tb_proctexture.h"
#include "tb_rendermemory.h"

CWavelet* gWaveLoadWavelet = nullptr;

int32_t InitWaveLoad() {
    if (gProcTextureBuffer != nullptr) {
        msl_assertion_failed("0", "tb_decode_wavelet.cpp", 0x29);
        return 0;
    }
    gWaveLoadWavelet = CWavelet::Instance();
    if (gWaveLoadWavelet == nullptr) {
        msl_assertion_failed("0", "tb_decode_wavelet.cpp", 0x3a);
        return 0;
    }
    gProcTextureBuffer = gei_RenderMemory->LoadInfo(CRenderMemory::kDynamic)->mData + 0x400;
    return 1;
}

int32_t CloseWaveLoad() {
    if (gWaveLoadWavelet == nullptr) return 0;
    gWaveLoadWavelet->Release();
    gWaveLoadWavelet = nullptr;
    return 1;
}
