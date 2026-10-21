// LightManager.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_light.h"
#include "tb_lightmanager.h"
#include "tb_resource_display.h"
#include "Wavelet.h"
#include <cmath>
#include <cstring>
#include <new>

// tb_mapzoom.cpp range of the engine library (SimTheme Park 2.04b3; CMapWho is from
// tb_map.cpp by its own assertion): the light manager's and the map lists' start-up and
// shutdown. PARTIAL: the rest of the range (lighting, linking, rendering) is still to do.

// ---- The light manager ------------------------------------------------------------------
CLightManager* gei_LightManager = nullptr;
float GlobalDiffuse[4];
class CLight;
CLight* gev_LightChain;

int32_t LightManager_Init() {
    if (gei_LightManager != nullptr) return 0;
    gei_LightManager = new (std::nothrow) CLightManager;
    if (gei_LightManager == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t LightManager_Close() {
    if (gei_LightManager == nullptr) return 0;
    delete gei_LightManager;
    gei_LightManager = nullptr;
    return 1;
}

CLightManager::CLightManager() {
    mF00 = 0;
    mF04 = -1;
    GlobalDiffuse[0] = 1.0f;
    GlobalDiffuse[1] = 0.15f;
    GlobalDiffuse[2] = 0.15f;
    GlobalDiffuse[3] = 0.15f;
}

// tb_lightmanager_frame.cpp (engine library of SimTheme Park 2.04b3): the light manager's
// per-frame lighting. The inventory files the range under tb_mapzoom.cpp (the construction and
// Init/Close are in src/tb_mapzoom.cpp); the function after the light manager in the binary,
// SMapLinkType::PrepareLights, is the map lists' and sits here because the lights are what it
// prepares.
//
//   0x10014418 CLightManager::CalcDiffuseColor      (628 bytes)
//   0x100146d4 CLightManager::PrepareLighting       (420)
//   0x100148c4 CLightManager::AddLights             (60)
//   0x10014938 CLightManager::CalcIntensities       (416)
//   0x10014b28 CLightManager::CalcNodeVertexColors  (456)
//   0x10014d38 CLightManager::CalcVertexFogIntensities (224)
//   0x10017454 SMapLinkType::PrepareLights          (112)

CMatrix* gLightWorld = nullptr;
float gDiffuseResult[4];

namespace {

// CLight::mFlags. The low nibble says what the light is (CalcDiffuseColor compares it whole);
// PrepareLighting tests the two bits on their own, so a light with both is a spot light.
constexpr uint32_t kDirectional = 2;
constexpr uint32_t kPoint = 4;
constexpr uint32_t kKindMask = 0xf;
constexpr uint32_t kNotChained = 0x10000000;   // the light does not join the prepared chain

// CLight keeps its object-space direction as three raw words (the words GetData hands out).
float AsFloat(uint32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

void SetFloat(uint32_t& word, float value) { std::memcpy(&word, &value, sizeof word); }

// One channel of a lit colour: 255 times the value, truncated by fctiwz (NaN and anything too
// small give the lowest integer, anything too large the highest) and clamped to a byte.
uint32_t Channel(float value) {
    int32_t byte = ppc_fctiwz(static_cast<double>(255.0f * value));
    if (byte > 0xff) {
        byte = 0xff;
    } else if (byte < 0) {
        byte = 0;
    }
    return static_cast<uint32_t>(byte);
}

// gDiffuseResult as the word the vertices keep: red in the top byte, then green, blue and
// an opaque low byte, all ANDed with the light manager's mask (mF04, -1 normally). The
// result's own first channel is not used.
uint32_t LitColour(uint32_t mask) {
    const uint32_t red = Channel(gDiffuseResult[1]);
    const uint32_t blue = Channel(gDiffuseResult[3]);
    const uint32_t green = Channel(gDiffuseResult[2]);
    return mask & ((red << 24) | (green << 16) | (blue << 8) | 0xffu);
}

// With the display's lighting off every vertex simply gets the mask. The count is used as the
// original uses it: a negative one would run for four billion words.
void FillColours(uint32_t* colours, uint32_t mask, int32_t count) {
    while (count != 0) {
        --count;
        *colours++ = mask;
    }
}

// Each light adds its colour times the share the vertex gets from it.
void AddShare(const CLight* light, float share) {
    gDiffuseResult[0] = ppc_fmadds(light->mColor[0], share, gDiffuseResult[0]);
    gDiffuseResult[1] = ppc_fmadds(light->mColor[1], share, gDiffuseResult[1]);
    gDiffuseResult[2] = ppc_fmadds(light->mColor[2], share, gDiffuseResult[2]);
    gDiffuseResult[3] = ppc_fmadds(light->mColor[3], share, gDiffuseResult[3]);
}

}  // namespace

// The colour of a point with the given normal: the ambient colour plus a share from every
// prepared light. A directional light gives -(direction . normal) when that is not negative.
// A point light gives (radius^2 - distance^2) / radius^2 inside its radius. A spot light (both
// bits) multiplies the normal's dot product with the direction to the light by the point
// light's share, which it takes from the squared distance the LAST POINT LIGHT ABOVE IT in
// the chain left in f30: the original works out the spot light's own distance, calls sqrt
// and throws the answer away. Without a point light before it the register holds whatever the
// caller had, which no caller sets; the translation starts it at zero.
void CLightManager::CalcDiffuseColor(_D3DVECTOR* normal, float x, float y, float z) {
    std::memcpy(gDiffuseResult, GlobalDiffuse, sizeof gDiffuseResult);
    float lastDistance = 0.0f;   // f30
    for (CLight* light = gev_LightChain; light != nullptr; light = light->mNext) {
        const uint32_t kind = light->mFlags & kKindMask;
        float share;
        if (kind == 2) {
            const float partial = ppc_fmsubs(ppc_fneg(AsFloat(light->mData[0])), normal->x,
                                             AsFloat(light->mData[1]) * normal->y);
            share = ppc_fnmsubs(AsFloat(light->mData[2]), normal->z, partial);
            if (!(share >= 0.0f)) continue;
        } else if (kind == 4) {
            const float dx = light->mF20.x - x;
            const float dy = light->mF20.y - y;
            const float dz = light->mF20.z - z;
            float distance = dx * dx;
            distance = ppc_fmadds(dy, dy, distance);
            distance = ppc_fmadds(dz, dz, distance);
            lastDistance = distance;
            if (!(distance < light->mRadiusSquared)) continue;
            share = light->mInverseRadiusSquared * (light->mRadiusSquared - distance);
        } else if (kind == 6) {
            const float dy = light->mF20.y - y;
            const float dx = light->mF20.x - x;
            const float dz = light->mF20.z - z;
            float along = dy * normal->y;
            along = ppc_fmadds(dx, normal->x, along);
            along = ppc_fmadds(dz, normal->z, along);
            if (!(along >= 0.0f)) continue;
            share = along * (light->mInverseRadiusSquared * (light->mRadiusSquared - lastDistance));
        } else {
            continue;
        }
        AddShare(light, share);
    }
}

// Brings the lights into the space of `world`: the matrix is copied to gLightInverseWorld and
// inverted, then every light on the chain with the directional bit has its direction (mNormal)
// carried through it without the translation into mData and made a unit vector (left alone when
// its squared length is exactly 0 or 1), and every light with the point bit has its position
// (mPosition) carried through it into mF20. The bounding box is not looked at.
int32_t CLightManager::PrepareLighting(CMatrix* world, sAlignedBox* box) {
    CLight* light = gev_LightChain;
    gLightWorld = world;
    std::memcpy(static_cast<sMatrix*>(&gLightInverseWorld), static_cast<const sMatrix*>(world), sizeof(sMatrix));
    gLightInverseWorld.Inverse();
    for (; light != nullptr; light = light->mNext) {
        if ((light->mFlags & kDirectional) != 0) {
            gLightInverseWorld.TransformVertex(light->mData, &light->mNormal, 3);
            const float x = AsFloat(light->mData[0]);
            const float y = AsFloat(light->mData[1]);
            const float z = AsFloat(light->mData[2]);
            const float xx = x * x;
            const float yy = y * y;
            const float zz = z * z;
            const float partial = xx + yy;
            const float lengthSquared = zz + partial;
            if (lengthSquared != 0.0f && lengthSquared != 1.0f) {
                const float length = static_cast<float>(std::sqrt(static_cast<double>(lengthSquared)));
                const float scale = 1.0f / length;
                SetFloat(light->mData[0], AsFloat(light->mData[0]) * scale);
                SetFloat(light->mData[1], AsFloat(light->mData[1]) * scale);
                SetFloat(light->mData[2], AsFloat(light->mData[2]) * scale);
            }
        }
        if ((light->mFlags & kPoint) != 0) {
            gLightInverseWorld.TransformVertex(&light->mF20, &light->mPosition, 2);
        }
    }
    return 1;
}

int32_t CLightManager::AddLights(CLight** lights) {
    while (*lights != nullptr) {
        CLight* light = *lights;
        if ((light->mFlags & kNotChained) == 0) {
            light->mNext = gev_LightChain;
            gev_LightChain = *lights;
        }
        ++lights;
    }
    return 1;
}

// The vertices are 16 bytes (position, then a word the math unit uses) and the normals 12.
void CLightManager::CalcIntensities(uint32_t* colours, _D3DVECTOR* normals, SVertex* vertices, int32_t count) {
    if ((gei_Display.mFlags & CDisplay::kFlag800) != 0) {
        FillColours(colours, static_cast<uint32_t>(mF04), count);
        return;
    }
    while (count != 0) {
        --count;
        CalcDiffuseColor(normals, vertices->x, vertices->y, vertices->z);
        ++vertices;
        *colours++ = LitColour(static_cast<uint32_t>(mF04));
        ++normals;
    }
}

// A node lists its vertices by index: word i of mF94 picks lane (index & 3) of position set
// (index >> 2), and the normal for entry i is the i-th of mPoints (which is not indexed by the
// word).
void CLightManager::CalcNodeVertexColors(uint32_t* colours, tag_sNode* node) {
    uint32_t count = node->mF5E;
    _D3DVECTOR* normal = reinterpret_cast<_D3DVECTOR*>(static_cast<s3DPoint*>(node->mPoints));
    sVertexPosnSet* sets = node->mVertices;
    if ((gei_Display.mFlags & CDisplay::kFlag800) != 0) {
        FillColours(colours, static_cast<uint32_t>(mF04), static_cast<int32_t>(count));
        return;
    }
    uint32_t entry = 0;
    while (count != 0) {
        --count;
        const uint32_t index = node->mF94[entry];
        ++entry;
        const sVertexPosnSet& set = sets[index >> 2];
        const uint32_t lane = index & 3;
        CalcDiffuseColor(normal, set.x[lane], set.y[lane], set.z[lane]);
        ++normal;
        *colours++ = LitColour(static_cast<uint32_t>(mF04));
    }
}

// Vertex fog: a vertex nearer than the display's fog start gets a zero specular word (no fog),
// one beyond the fog end the fog colour with full strength, and one between them the fog
// colour with the strength (depth - near) / (far - near) times 255 in the low byte (the
// original's fused 1 - (far - depth) / (far - near) in double precision, then the runtime's
// double to unsigned conversion). The depth is the vertex's third float. Returns 1 when any
// vertex was past the fog start.
uint8_t CLightManager::CalcVertexFogIntensities(SFVF_TLCTEX* vertices, int32_t count, int32_t stride) {
    uint32_t fogged = 0;
    const double nearDistance = gei_Display.mFogNear;
    const double farDistance = gei_Display.mFogFar;
    const uint32_t colour = gei_Display.mFogColor & 0xffffff00u;
    const double span = farDistance - nearDistance;
    const double reciprocal = 1.0 / span;
    unsigned char* vertex = reinterpret_cast<unsigned char*>(vertices);
    while (count != 0) {
        --count;
        SFVF_TLCTEX* current = reinterpret_cast<SFVF_TLCTEX*>(vertex);
        const double depth = current->z;
        if (depth > nearDistance) {
            fogged = 1;
            if (depth > farDistance) {
                current->specular = colour | 0xffu;
            } else {
                const double strength = ppc_fnmsub(reciprocal, farDistance - depth, 1.0);
                current->specular = colour | cw_cvt_fp2unsigned(255.0 * strength);
            }
        } else {
            current->specular = 0;
        }
        vertex += stride;
    }
    return static_cast<uint8_t>(fogged);
}

// tb_light.cpp (decompiled from the engine library of SimTheme Park 2.04b3): one global,
// constructed by __sinit_tb_light_cpp (0x10014200) with the inline CMatrix constructor.

CMatrix gLightInverseWorld;
