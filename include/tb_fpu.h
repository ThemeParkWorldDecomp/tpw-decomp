// tb_fpu.h: the engine's FPU precision switches (engine library; 0x10038b60..0x10038bc8, in
// the inventory's Wavelet.cpp range). The PC build put the x87 FPU in single precision for
// the frame (CMapWho::Render); the Mac build's are empty: SetSinglePrecision answers 0 for
// the state to restore, and the other two do nothing, so nothing on the host changes the
// rounding either (src/engine/EngineUtil.cpp).
#pragma once
#include <cstdint>

int16_t FPU_SetSinglePrecision();              // 0x10038b60
void FPU_RestorePrecision(int16_t previous);   // 0x10038b98
void FPU_SetFullPrecision(int16_t previous);   // 0x10038bc8
