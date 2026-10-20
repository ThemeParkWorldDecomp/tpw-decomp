// fpu.h: the x87 FPU status word that the PC build of the bullfrog library reads with
// inline assembler (TbFPUFlag::DoFlagGet, TbFPUPrecision.cpp). The Mac build has no
// assembler for it: it reports "Missing assembler" through DebugStr and goes on with
// whatever r30 already holds (the caller's value). The port asks the platform layer for
// the value instead, so the behaviour is defined; tests feed the original's r30.
#pragma once
#include <cstdint>

uint16_t PlatformFPUFlags();
