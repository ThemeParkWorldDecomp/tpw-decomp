// Light.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of Light code, named after it.

#include "LevelInit.h"
#include "LevelInit2.h"
#include "Light.h"
#include "MapWho.h"
#include "ppc_math.h"
#include "tb_error.h"
#include "tb_resource_system.h"
#include "TPEngine.h"
#include "Wavelet.h"
#include <new>

// The park's lights (main application, TPRide.cpp and Advisor.cpp ranges; unit w110): CreateGlobalLight with the three CLight
// setters it calls. The CMapLinkBase constructor it also calls (0x10007b7c) is inline in include/MapWho.h.

// 0x1000784c (236 bytes). The four floats are stored, then the colour word is built from 255.0f times each (single precision),
// truncated toward zero: alpha and green and blue masked to a byte, red shifted into the top byte (the shift drops its
// upper bits, so it is masked in effect).
void CLight::SetColor(float alpha, float red, float green, float blue) {
    gei_System.UpdateLights();
    mColor[0] = alpha;
    mColor[1] = red;
    mColor[2] = green;
    mColor[3] = blue;
    const float scale = 255.0f;
    const int32_t redByte = ppc_fctiwz(scale * mColor[1]);
    const int32_t greenByte = ppc_fctiwz(scale * mColor[2]);
    const int32_t blueByte = ppc_fctiwz(scale * mColor[3]);
    const int32_t alphaByte = ppc_fctiwz(scale * mColor[0]);
    mColorWord = (static_cast<uint32_t>(alphaByte) & 0xffu) | ((static_cast<uint32_t>(blueByte) & 0xffu) << 8) |
                 (static_cast<uint32_t>(redByte) << 24) | ((static_cast<uint32_t>(greenByte) & 0xffu) << 16);
}

// 0x10007964 (100 bytes).
void CLight::SetNormal(float x, float y, float z) {
    gei_System.UpdateLights();
    mNormal.x = x;
    mNormal.y = y;
    mNormal.z = z;
}

CLight::CLight() : mFlags(0) {}
CLight::~CLight() = default;
void* CLight::GetData() { return (mFlags & 0xf) == 2 ? mData : nullptr; }
int32_t CLight::GetLinkType() { return 1; }
int32_t CLight::Prepare() {
    if ((mFlags & 0x10000000) == 0) {
        mNext = gev_LightChain;
        gev_LightChain = this;
    }
    return 1;
}

// tb_mapzoom.cpp range of the engine library (SimTheme Park 2.04b3; CMapWho is from
// tb_map.cpp by its own assertion): the light manager's and the map lists' start-up and
// shutdown. PARTIAL: the rest of the range (lighting, linking, rendering) is still to do.

CMapLinkBase::~CMapLinkBase() {}
