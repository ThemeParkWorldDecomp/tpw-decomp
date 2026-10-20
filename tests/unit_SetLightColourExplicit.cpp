// Test library for the light-node helpers (unit v5272; libtpw_unit_SetLightColourExplicit.dylib), main app:
//   src/LevelSystems_SetLightColourExplicit.cpp   SetLightColourExplicit, EnableLight, SetLightColourRelative, DisableLight
//
// CSystem::UpdateLights is a recording fake; gei_System is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Light.h"
#include "tb_resource_system.h"

using namespace calllog;

CSystem gei_System;

void CSystem::UpdateLights() { ev("UpdateLights", {}); }
CSystem::CSystem() {}

extern "C" {

uint64_t tpw_light_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// HOST_LAYOUT: CLight mColor, mFlags, mColorWord, sizeof; sNodeLightParams mColour.
int32_t tpw_light_layout(int64_t* out) {
    int32_t n = 0;
    CLight* l = reinterpret_cast<CLight*>(0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&l->mColor[0]) - 0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&l->mFlags) - 0x1000);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&l->mColorWord) - 0x1000);
    out[n++] = static_cast<int64_t>(sizeof(CLight));
    out[n++] = static_cast<int64_t>(offsetof(sNodeLightParams, mColour));
    return n;
}

void tpw_light_explicit(uint64_t p, float r, float g, float b) {
    SetLightColourExplicit(reinterpret_cast<sNodeLightParams*>(p), r, g, b);
}
void tpw_light_relative(uint64_t p, float s) { SetLightColourRelative(reinterpret_cast<sNodeLightParams*>(p), s); }
void tpw_light_enable(uint64_t p) { EnableLight(reinterpret_cast<sNodeLightParams*>(p)); }
void tpw_light_disable(uint64_t p) { DisableLight(reinterpret_cast<sNodeLightParams*>(p)); }

}  // extern "C"
