// The park's lights (main application, TPRide.cpp and Advisor.cpp ranges; unit w110): CreateGlobalLight with the three CLight
// setters it calls. The CMapLinkBase constructor it also calls (0x10007b7c) is inline in include/MapWho.h.
#include "Light.h"
#include "LevelInit.h"
#include "LevelInit2.h"
#include "MapWho.h"
#include "TPEngine.h"
#include "ppc_math.h"
#include "tb_resource_system.h"

namespace {

// The text DataErrorHandler is given for a light of type 0 (0x1023d3de).
char gInvalidLightText[] = "Invalid lightsource creation attempted";

// What both object branches of CreateGlobalLight do once the light exists. The colour word is stored first; the four colour
// floats follow, each a byte over 255 times 1.0f (the original's multiply by one stays in the listing), in the order alpha
// (low byte), red (top byte), green (bits 16..23), blue (bits 8..15); the light is told it changed before the word and again
// before the caller's vector goes in.
void StoreLightColour(CLight* light, uint32_t colour) {
    gei_System.UpdateLights();
    light->mColorWord = colour;
    const float one = 1.0f;
    const float alpha = static_cast<float>(static_cast<double>(colour & 0xff)) / 255.0f;
    const float red = static_cast<float>(static_cast<double>(colour >> 24)) / 255.0f;
    const float green = static_cast<float>(static_cast<double>((colour >> 16) & 0xff)) / 255.0f;
    const float blue = static_cast<float>(static_cast<double>((colour >> 8) & 0xff)) / 255.0f;
    light->mColor[0] = alpha * one;
    light->mColor[1] = red * one;
    light->mColor[2] = green * one;
    light->mColor[3] = blue * one;
}

}  // namespace

// 0x10063d24 (1,156 bytes). The record is 0x14 bytes of zeroed memory on the Mac (MReleaseAllocF with flag 1; the host's record
// is larger, with native pointers). Type 2 makes a CLightParallel (flags 2, normal (0, 0, 1) and colour white until the colour
// word overwrites the floats), type 3 a CLightPoint (flags 4, radius 100); both copy the caller's vector (direction for 2,
// position for 3, read after the colour and without a NULL test: the original faults on NULL), link themselves into the map's
// list 0x70 and go in the record. Type 1 writes the engine's GlobalDiffuse and makes nothing. Type 0 reports and answers NULL
// (the record is leaked); a negative type or 4 and above answers the bare record. The original tests the new object for NULL
// before its set-up but dereferences it afterwards anyway; the host's new never answers NULL.
sGlobalLight* CreateGlobalLight(int32_t type, uint32_t colour, s3DPoint* position, s3DPoint* direction) {
    sGlobalLight* record = static_cast<sGlobalLight*>(MReleaseAllocF(sizeof(sGlobalLight), 1));
    if (type == 2) {
        CLightParallel* object = new CLightParallel;
        CLight* light = object;
        light->mFlags = 2;
        light->SetNormal(0.0f, 0.0f, 1.0f);
        light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        StoreLightColour(light, colour);
        const float z = direction->z;
        const float y = direction->y;
        const float x = direction->x;
        gei_System.UpdateLights();
        light->mNormal.x = x;
        light->mNormal.y = y;
        light->mNormal.z = z;
        gei_MapWho->Link(light, 0x70);
        record->mDirectional = object;
    } else if (type < 2) {
        if (type == 0) {
            DataErrorHandler(gInvalidLightText);
            return nullptr;
        }
        if (type >= 0) {
            GlobalDiffuse[0] = 1.0f;
            const float red = static_cast<float>(static_cast<double>(colour >> 24));
            const float green = static_cast<float>(static_cast<double>((colour >> 16) & 0xff));
            const float blue = static_cast<float>(static_cast<double>((colour >> 8) & 0xff));
            GlobalDiffuse[1] = red / 255.0f;
            GlobalDiffuse[2] = green / 255.0f;
            GlobalDiffuse[3] = blue / 255.0f;
        }
    } else if (type < 4) {
        CLightPoint* object = new CLightPoint;
        CLight* light = object;
        light->mFlags = 4;
        light->SetRadius(100.0f);
        light->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
        StoreLightColour(light, colour);
        const float z = position->z;
        const float y = position->y;
        const float x = position->x;
        gei_System.UpdateLights();
        light->mPosition.x = x;
        light->mPosition.y = y;
        light->mPosition.z = z;
        gei_MapWho->Link(light, 0x70);
        record->mPoint = object;
    }
    record->mColour = colour;
    record->mType = type;
    return record;
}

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

// 0x100641f8 (92 bytes): the square is stored in single precision and the inverse is made from the stored square.
void CLight::SetRadius(float radius) {
    gei_System.UpdateLights();
    mRadius = radius;
    mRadiusSquared = radius * radius;
    mInverseRadiusSquared = 1.0f / mRadiusSquared;
}
