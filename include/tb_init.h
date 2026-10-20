// tb_init.cpp range (engine library): the engine's own start-up. PARTIAL.
#pragma once
#include <cstdint>

// 0x10013298: nonzero on success. EngineStartup_Final passes the texture format (4 or 5)
// and the version-like constant 0x92f2266.
int32_t EngineAdd_Init(int32_t textureFormat, int32_t version, char* name);
int32_t EngineAdd_Close();   // 0x10013ae0, always 1
bool EngineAdd_Reinit();  // 0x10013bbc

#include "m3d2.h"   // sAlignedBox
extern sAlignedBox gs_WorldBoundingArea;   // engine export (file not known)

#include "tb_particle.h"   // gei_Particle: the hooks EngineStartup_Final installs
