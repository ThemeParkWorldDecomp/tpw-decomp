// tb_fpu.cpp: the engine's FPU precision switches (include/tb_fpu.h): empty on the Mac.
#include "tb_fpu.h"

int16_t FPU_SetSinglePrecision() { return 0; }

void FPU_RestorePrecision(int16_t) {}

void FPU_SetFullPrecision(int16_t) {}
