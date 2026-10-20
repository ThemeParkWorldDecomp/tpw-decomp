// TPRide.cpp range of the main app (SimTheme Park 2.04b3), unit w133: TickLevelLights (0x100645b0, 532 bytes), the park's per-frame light
// update (the sun's direction and colour, and the engine's ambient light).
#include <cmath>

#include "Light.h"
#include "LevelInit.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "tb_resource_system.h"

float gGlobalLightScale = 1.0f;   // 0x1029cf78

// With `force` set: the ambient light (GlobalDiffuse: alpha 1, then red, green and blue from the ambient colour word, each byte over 255)
// and the sun. The sun's direction is normalised in place as in SetupLevelLights, but with the scale in the numerator (gGlobalLightScale
// over the length, then each component times that), and handed to the directional light's normal; then the light's colour word and its
// four colour floats (alpha in the low byte, red in the top byte, green, blue) are made from gGlobalLightColour. Every light write is
// preceded by gei_System.UpdateLights(). Without `force` nothing happens.
void TickLevelLights(int32_t force) {
    if (force == 0) return;
    GlobalDiffuse[0] = 1.0f;
    const uint32_t ambient = gGlobalAmbientColour;
    GlobalDiffuse[1] = static_cast<float>(ambient >> 24) / 255.0f;
    GlobalDiffuse[2] = static_cast<float>((ambient >> 16) & 0xff) / 255.0f;
    GlobalDiffuse[3] = static_cast<float>((ambient >> 8) & 0xff) / 255.0f;

    float* direction = &gGlobalLightDirection.x;
    const double x = direction[0];
    const float yy = direction[1] * direction[1];
    const float zz = direction[2] * direction[2];
    const double sum = static_cast<double>(zz) + (x * x + static_cast<double>(yy));
    const float length = static_cast<float>(std::sqrt(sum));
    const float scale = gGlobalLightScale / length;
    direction[0] = direction[0] * scale;
    direction[1] = direction[1] * scale;
    direction[2] = direction[2] * scale;
    const float nx = direction[0];
    const float ny = direction[1];
    const float nz = direction[2];
    CLight* light = gGlobalLight->mDirectional;
    gei_System.UpdateLights();
    light->mNormal.x = nx;
    light->mNormal.y = ny;
    light->mNormal.z = nz;

    const uint32_t colour = gGlobalLightColour;
    light = gGlobalLight->mDirectional;
    gei_System.UpdateLights();
    light->mColorWord = colour;
    light->mColor[0] = (static_cast<float>(colour & 0xff) / 255.0f) * 1.0f;
    light->mColor[1] = (static_cast<float>(colour >> 24) / 255.0f) * 1.0f;
    light->mColor[2] = (static_cast<float>((colour >> 16) & 0xff) / 255.0f) * 1.0f;
    light->mColor[3] = (static_cast<float>((colour >> 8) & 0xff) / 255.0f) * 1.0f;
}
