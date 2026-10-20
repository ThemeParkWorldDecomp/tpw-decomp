// Light-node helpers (unit v5272).
#include "Light.h"
#include "ppc_math.h"
#include "tb_resource_system.h"

namespace {

// The shared tail: alpha 1.0, the three colours, then the colour word from the stored floats.
void StoreColour(CLight* light, float red, float green, float blue) {
    gei_System.UpdateLights();
    light->mColor[0] = 1.0f;
    light->mColor[1] = red;
    light->mColor[2] = green;
    light->mColor[3] = blue;
    const uint32_t r = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[1])));
    const uint32_t g = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[2])));
    const uint32_t b = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[3])));
    const uint32_t a = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(255.0f * light->mColor[0])));
    light->mColorWord = (r << 24) | ((g & 0xffu) << 16) | ((b & 0xffu) << 8) | (a & 0xffu);
}

}  // namespace

// 0x1006498c (232 bytes)
void SetLightColourExplicit(sNodeLightParams* params, float red, float green, float blue) {
    StoreColour(*params->mLight, red, green, blue);
}

// 0x10064ab8 (68 bytes)
void EnableLight(sNodeLightParams* params) {
    CLight* light = *params->mLight;
    gei_System.UpdateLights();
    light->mFlags &= 0xefffffffu;
}

// 0x10064854 (248 bytes): the colour is read before UpdateLights.
void SetLightColourRelative(sNodeLightParams* params, float scale) {
    const float* colour = params->mColour;
    CLight* light = *params->mLight;
    const float b = colour[2] * scale;
    const float g = colour[1] * scale;
    const float r = colour[0] * scale;
    StoreColour(light, r, g, b);
}

// 0x10064b30 (68 bytes)
void DisableLight(sNodeLightParams* params) {
    CLight* light = *params->mLight;
    gei_System.UpdateLights();
    light->mFlags |= 0x10000000u;
}
