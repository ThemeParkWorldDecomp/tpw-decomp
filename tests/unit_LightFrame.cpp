// Fakes and drivers for the light manager's per-frame lighting (src/engine/LightManager.cpp, src/engine/MapWho.cpp)
// and the render memory's small members (src/engine/RenderMemory.cpp), run by
// tests/test_LightFrame.py.
//
// What the lighting code calls outside the unit is faked here and records through
// tests/calllog.h: CMatrix::Inverse (records the matrix and adds one to every word of it) and
// CMatrix::TransformVertex (records the matrix, the type, which light field it was given and
// the input vector; writes a permutation of the input so the next step has something to work
// on). The display, the light chain and the ambient colour are plain globals here.
//
// The Python side drives everything through flat arrays of 32-bit words, laid out as below, so
// that the PowerPC side (memory written big-endian) and this side see the same bytes:
//
//   light state (tpw_lf_load, tpw_lf_dump)
//     [0]                      chain head: 0 none, k the light with index k-1
//     [1 + 22*i + (w - 1)]     light i (0..5), word w (1..22) of the Mac object: 1..4 colour,
//                              5..7 position, 8..10 mF20, 11..13 normal, 14..16 mData,
//                              17 radius, 18 radius squared, 19 its inverse, 20 flags,
//                              21 colour word, 22 next (a light number as in [0])
//     [133..136]               GlobalDiffuse
//     [137..140]               gDiffuseResult
//     [141]                    gLightWorld: 0 none, k the k-th world matrix, 0xffff unknown
//     [142..157]               gLightInverseWorld
//     [158], [159]             the light manager's mF00 and mF04
//     [160..163]               display flags, fog colour, fog near and far (float bits)
//
//   render memory state (tpw_lf_rm_load, tpw_lf_rm_dump): the Mac object's words 0..24
//   (offsets 0x00..0x60), pointers as their low 32 bits; they are only ever subtracted.
#include <cstdint>
#include <cstring>

#include "Light.h"
#include "MapWho.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/d3d_types.h"
#include "tb_drawprimitive.h"
#include "tb_hfield.h"
#include "tb_light.h"
#include "tb_lightmanager.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"

using namespace calllog;

namespace {

constexpr int32_t kLights = 6;
constexpr int32_t kLightWords = 22;     // words 1..22 of a light object
constexpr int32_t kWorlds = 8;
constexpr int32_t kStateWords = 164;
constexpr int32_t kUnknown = 0xffff;

// The lights are raw storage: only their fields are used, never their constructor or vtable.
alignas(CLight) unsigned char gLightStorage[kLights][sizeof(CLight)];
CLight& Light(int32_t i) { return *reinterpret_cast<CLight*>(gLightStorage[i]); }
alignas(CLightManager) unsigned char gManagerStorage[sizeof(CLightManager)];
CLightManager& Manager() { return *reinterpret_cast<CLightManager*>(gManagerStorage); }
CMatrix gWorlds[kWorlds];     // the matrices PrepareLighting is given

uint32_t Word(const float& value) {
    uint32_t w;
    std::memcpy(&w, &value, sizeof w);
    return w;
}
void Put(float& dst, uint32_t w) { std::memcpy(&dst, &w, sizeof w); }

// A light number (0 none) for a pointer into the light storage.
uint32_t LightNumber(const CLight* light) {
    if (light == nullptr) return 0;
    for (int32_t i = 0; i < kLights; ++i)
        if (light == &Light(i)) return static_cast<uint32_t>(i + 1);
    return kUnknown;
}
CLight* LightByNumber(uint32_t number) {
    return number >= 1 && number <= static_cast<uint32_t>(kLights) ? &Light(static_cast<int32_t>(number) - 1) : nullptr;
}

// Which light field a pointer is, for the events: light number * 16 plus 1 for mData, 2 for
// mNormal, 3 for mF20, 4 for mPosition; 15 for anything else.
int64_t FieldId(const void* p) {
    for (int32_t i = 0; i < kLights; ++i) {
        CLight& light = Light(i);
        const int64_t base = (i + 1) * 16;
        if (p == static_cast<const void*>(light.mData)) return base + 1;
        if (p == static_cast<const void*>(&light.mNormal)) return base + 2;
        if (p == static_cast<const void*>(&light.mF20)) return base + 3;
        if (p == static_cast<const void*>(&light.mPosition)) return base + 4;
    }
    return 15;
}

void RecordMatrix(const CMatrix* matrix) {
    uint32_t w[16];
    std::memcpy(w, matrix, sizeof w);
    ev("Matrix", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("Matrix", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

void SetLight(int32_t i, const uint32_t* w) {
    CLight& light = Light(i);
    Put(light.mColor[0], w[1]);
    Put(light.mColor[1], w[2]);
    Put(light.mColor[2], w[3]);
    Put(light.mColor[3], w[4]);
    Put(light.mPosition.x, w[5]);
    Put(light.mPosition.y, w[6]);
    Put(light.mPosition.z, w[7]);
    Put(light.mF20.x, w[8]);
    Put(light.mF20.y, w[9]);
    Put(light.mF20.z, w[10]);
    Put(light.mNormal.x, w[11]);
    Put(light.mNormal.y, w[12]);
    Put(light.mNormal.z, w[13]);
    light.mData[0] = w[14];
    light.mData[1] = w[15];
    light.mData[2] = w[16];
    Put(light.mRadius, w[17]);
    Put(light.mRadiusSquared, w[18]);
    Put(light.mInverseRadiusSquared, w[19]);
    light.mFlags = w[20];
    light.mColorWord = w[21];
    light.mNext = LightByNumber(w[22]);
}

void GetLight(int32_t i, uint32_t* w) {
    CLight& light = Light(i);
    w[1] = Word(light.mColor[0]);
    w[2] = Word(light.mColor[1]);
    w[3] = Word(light.mColor[2]);
    w[4] = Word(light.mColor[3]);
    w[5] = Word(light.mPosition.x);
    w[6] = Word(light.mPosition.y);
    w[7] = Word(light.mPosition.z);
    w[8] = Word(light.mF20.x);
    w[9] = Word(light.mF20.y);
    w[10] = Word(light.mF20.z);
    w[11] = Word(light.mNormal.x);
    w[12] = Word(light.mNormal.y);
    w[13] = Word(light.mNormal.z);
    w[14] = light.mData[0];
    w[15] = light.mData[1];
    w[16] = light.mData[2];
    w[17] = Word(light.mRadius);
    w[18] = Word(light.mRadiusSquared);
    w[19] = Word(light.mInverseRadiusSquared);
    w[20] = light.mFlags;
    w[21] = light.mColorWord;
    w[22] = LightNumber(light.mNext);
}

// A link base whose answers the test chooses.
class FakeLink : public CMapLinkBase {
public:
    int32_t Prepare() override {
        ev("Prepare", {lab(this)});
        return mPrepareResult;
    }
    int32_t GetLinkType() override {
        ev("GetLinkType", {lab(this)});
        return mType;
    }
    void* GetData() override { return nullptr; }

    int32_t mType = 0;
    int32_t mPrepareResult = 0;
};
FakeLink gLinks[8];
SMapLinkType gNodes[8];

alignas(CRenderMemory) unsigned char gMemoryStorage[sizeof(CRenderMemory)];
CRenderMemory& Memory() { return *reinterpret_cast<CRenderMemory*>(gMemoryStorage); }

uint8_t* BytePointer(uint32_t w) { return reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(w)); }
uint32_t Low32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- The unit's callees ------------------------------------------------------------------

CMapLinkBase::~CMapLinkBase() {}
CDisplay::CDisplay() {}
CDisplay gei_Display;
float GlobalDiffuse[4];
CLight* gev_LightChain = nullptr;
CMatrix gLightInverseWorld;

// Records the matrix as it comes in, then adds one to each word, so that what the next call
// sees shows that the inverse was taken first.
void CMatrix::Inverse() {
    ev("Inverse", {this == &gLightInverseWorld});
    RecordMatrix(this);
    uint32_t w[16];
    std::memcpy(w, this, sizeof w);
    for (uint32_t& word : w) ++word;
    std::memcpy(static_cast<void*>(this), w, sizeof w);
}

// Records the call and writes a permutation of the input: a direction (type 3) becomes
// (y, -x, z), a point (type 2) (z, x, y). Both keep their lengths.
void CMatrix::TransformVertex(void* out, void* in, int32_t type) {
    uint32_t v[3];
    std::memcpy(v, in, sizeof v);
    ev("TransformVertex", {this == &gLightInverseWorld, type, FieldId(out), FieldId(in)});
    RecordMatrix(this);
    ev("Vector", {v[0], v[1], v[2]});
    uint32_t r[3];
    if (type == 3) {
        r[0] = v[1];
        r[1] = v[0] ^ 0x80000000u;
        r[2] = v[2];
    } else {
        r[0] = v[2];
        r[1] = v[0];
        r[2] = v[1];
    }
    std::memcpy(out, r, sizeof r);
}

// ---- Drivers -----------------------------------------------------------------------------

extern "C" {

void tpw_lf_load(const uint32_t* s) {
    for (int32_t i = 0; i < kLights; ++i) SetLight(i, s + 1 + kLightWords * i - 1);
    gev_LightChain = LightByNumber(s[0]);
    for (int32_t i = 0; i < 4; ++i) {
        Put(GlobalDiffuse[i], s[133 + i]);
        Put(gDiffuseResult[i], s[137 + i]);
    }
    gLightWorld = nullptr;
    uint32_t w[16];
    std::memcpy(w, &s[142], sizeof w);
    std::memcpy(static_cast<void*>(&gLightInverseWorld), w, sizeof w);
    Manager().mF00 = static_cast<int32_t>(s[158]);
    Manager().mF04 = static_cast<int32_t>(s[159]);
    gei_Display.mFlags = s[160];
    gei_Display.mFogColor = s[161];
    Put(gei_Display.mFogNear, s[162]);
    Put(gei_Display.mFogFar, s[163]);
}

void tpw_lf_dump(uint32_t* out) {
    std::memset(out, 0, kStateWords * sizeof(uint32_t));
    out[0] = LightNumber(gev_LightChain);
    for (int32_t i = 0; i < kLights; ++i) {
        uint32_t w[kLightWords + 1];
        std::memset(w, 0, sizeof w);
        GetLight(i, w);
        std::memcpy(out + 1 + kLightWords * i, w + 1, kLightWords * sizeof(uint32_t));
    }
    for (int32_t i = 0; i < 4; ++i) {
        out[133 + i] = Word(GlobalDiffuse[i]);
        out[137 + i] = Word(gDiffuseResult[i]);
    }
    out[141] = gLightWorld == nullptr ? 0 : kUnknown;
    for (int32_t i = 0; i < kWorlds; ++i)
        if (gLightWorld == &gWorlds[i]) out[141] = static_cast<uint32_t>(i + 1);
    std::memcpy(&out[142], &gLightInverseWorld, 16 * sizeof(uint32_t));
    out[158] = static_cast<uint32_t>(Manager().mF00);
    out[159] = static_cast<uint32_t>(Manager().mF04);
    out[160] = gei_Display.mFlags;
    out[161] = gei_Display.mFogColor;
    out[162] = Word(gei_Display.mFogNear);
    out[163] = Word(gei_Display.mFogFar);
}

enum { kAdd = 1, kPrepare = 2, kDiffuse = 3, kIntensities = 4, kNode = 5, kFog = 6, kDisplay = 7, kMask = 8 };

// One operation on the light manager. `in` and `out` are the word arrays of the op:
//   kAdd:         in = count, light numbers          out = result
//   kPrepare:     in = matrix (16 words), world slot out = result
//   kDiffuse:     in = normal (3), x, y, z (float bits)
//   kIntensities: in = count, normals (3 each), vertices (4 each)   out = colours, 2 guards
//   kNode:        in = count, sets, normals (3 each), sets (12 each), indices   out = as above
//   kFog:         in = count, stride in bytes, vertex words          out = result, vertex words
//   kDisplay:     in = flags, fog colour, near, far     kMask: in = mF04, mF00
void tpw_lf_op(int32_t op, const uint32_t* in, uint32_t* out) {
    CLightManager& manager = Manager();
    switch (op) {
    case kAdd: {
        CLight* lights[kLights + 1];
        const int32_t count = static_cast<int32_t>(in[0]);
        for (int32_t i = 0; i < count; ++i) lights[i] = LightByNumber(in[1 + i]);
        lights[count] = nullptr;
        out[0] = static_cast<uint32_t>(manager.AddLights(lights));
        break;
    }
    case kPrepare: {
        CMatrix& world = gWorlds[in[16] - 1];
        std::memcpy(static_cast<void*>(&world), in, 16 * sizeof(uint32_t));
        out[0] = static_cast<uint32_t>(manager.PrepareLighting(&world, reinterpret_cast<sAlignedBox*>(0x1234)));
        break;
    }
    case kDiffuse: {
        _D3DVECTOR normal;
        Put(normal.x, in[0]);
        Put(normal.y, in[1]);
        Put(normal.z, in[2]);
        float x, y, z;
        Put(x, in[3]);
        Put(y, in[4]);
        Put(z, in[5]);
        manager.CalcDiffuseColor(&normal, x, y, z);
        break;
    }
    case kIntensities: {
        const int32_t count = static_cast<int32_t>(in[0]);
        _D3DVECTOR normals[16];
        SVertex vertices[16];
        std::memcpy(normals, in + 1, 12 * count);
        std::memcpy(vertices, in + 1 + 3 * count, 16 * count);
        uint32_t colours[16 + 2];
        for (uint32_t& c : colours) c = 0xa5a5a5a5u;
        manager.CalcIntensities(colours, normals, vertices, count);
        std::memcpy(out, colours, (count + 2) * sizeof(uint32_t));
        break;
    }
    case kNode: {
        const int32_t count = static_cast<int32_t>(in[0]);
        const int32_t sets = static_cast<int32_t>(in[1]);
        tag_sNode* node = static_cast<tag_sNode*>(tpw::ArenaAllocZeroed(sizeof(tag_sNode)));
        s3DPoint* normals = static_cast<s3DPoint*>(tpw::ArenaAlloc(12 * (count + 1)));
        sVertexPosnSet* positions = static_cast<sVertexPosnSet*>(tpw::ArenaAlloc(sizeof(sVertexPosnSet) * (sets + 1)));
        uint16_t* indices = static_cast<uint16_t*>(tpw::ArenaAlloc(2 * (count + 1)));
        std::memcpy(normals, in + 2, 12 * count);
        std::memcpy(positions, in + 2 + 3 * count, sizeof(sVertexPosnSet) * sets);
        for (int32_t i = 0; i < count; ++i) indices[i] = static_cast<uint16_t>(in[2 + 3 * count + 12 * sets + i]);
        node->mF5E = static_cast<uint16_t>(count);
        node->mPoints = normals;
        node->mVertices = positions;
        node->mF94 = indices;
        uint32_t colours[16 + 2];
        for (uint32_t& c : colours) c = 0xa5a5a5a5u;
        manager.CalcNodeVertexColors(colours, node);
        std::memcpy(out, colours, (count + 2) * sizeof(uint32_t));
        tpw::ArenaFree(indices);
        tpw::ArenaFree(positions);
        tpw::ArenaFree(normals);
        tpw::ArenaFree(node);
        break;
    }
    case kFog: {
        const int32_t count = static_cast<int32_t>(in[0]);
        const int32_t stride = static_cast<int32_t>(in[1]);
        const int32_t bytes = (count > 0 ? (count - 1) * stride : 0) + 0x20;
        const int32_t words = (bytes + 3) / 4 + 8;
        uint32_t* vertices = new uint32_t[words];
        std::memcpy(vertices, in + 2, words * sizeof(uint32_t));
        out[0] = manager.CalcVertexFogIntensities(reinterpret_cast<SFVF_TLCTEX*>(vertices), count, stride);
        std::memcpy(out + 1, vertices, words * sizeof(uint32_t));
        delete[] vertices;
        break;
    }
    case kDisplay:
        gei_Display.mFlags = in[0];
        gei_Display.mFogColor = in[1];
        Put(gei_Display.mFogNear, in[2]);
        Put(gei_Display.mFogFar, in[3]);
        break;
    case kMask:
        manager.mF04 = static_cast<int32_t>(in[0]);
        manager.mF00 = static_cast<int32_t>(in[1]);
        break;
    }
}

// SMapLinkType::PrepareLights on a list of `count` links (none: a NULL head). types[i] is what
// link i answers to GetLinkType, results[i] what its Prepare returns (ignored by the caller).
void tpw_lf_links(int32_t count, const int32_t* types, const int32_t* results) {
    for (int32_t i = 0; i < count; ++i) {
        gLinks[i].mType = types[i];
        gLinks[i].mPrepareResult = results[i];
        tpw_cl_label(11 + i, &gLinks[i]);
        gNodes[i].mLink = &gLinks[i];
        gNodes[i].mNext = i + 1 < count ? &gNodes[i + 1] : nullptr;
    }
    SMapLinkType* head = count > 0 ? &gNodes[0] : nullptr;
    asm volatile("" : "+r"(head));   // an empty list calls the member on NULL: keep the compiler from knowing
    head->PrepareLights();
}

// ---- The render memory ----

void tpw_lf_rm_load(const uint32_t* w) {
    CRenderMemory& m = Memory();
    std::memset(&m, 0xa5, sizeof m);
    m.mLoadFlags = static_cast<int32_t>(w[0]);
    m.mCurrentRenderType = static_cast<int32_t>(w[1]);
    for (int32_t p = 0; p < 2; ++p) {
        m.mPools[p].mData = BytePointer(w[2 + 6 * p]);
        m.mPools[p].mUsed = static_cast<int32_t>(w[3 + 6 * p]);
        m.mPools[p].mSize = static_cast<int32_t>(w[4 + 6 * p]);
        m.mPools[p].mIndexData = BytePointer(w[5 + 6 * p]);
        m.mPools[p].mIndexUsed = static_cast<int32_t>(w[6 + 6 * p]);
        m.mPools[p].mIndexSize = static_cast<int32_t>(w[7 + 6 * p]);
    }
    m.mCurrent = reinterpret_cast<SRenderLoadInfo*>(BytePointer(w[14]));
    m.mScratchEnd = BytePointer(w[15]);
    m.mF40 = static_cast<int32_t>(w[16]);
    m.mInfos = reinterpret_cast<SRenderLoadInfo*>(BytePointer(w[17]));
    m.mInfoCount = static_cast<int32_t>(w[18]);
    m.mRemembered = static_cast<int32_t>(w[19]);
    m.mNextDynamic = static_cast<int32_t>(w[20]);
    m.mNextStatic = static_cast<int32_t>(w[21]);
    m.mScratch = BytePointer(w[22]);
    m.mScratchNext = BytePointer(w[23]);
    m.mScratchBlock = BytePointer(w[24]);
}

void tpw_lf_rm_dump(uint32_t* w) {
    const CRenderMemory& m = Memory();
    w[0] = static_cast<uint32_t>(m.mLoadFlags);
    w[1] = static_cast<uint32_t>(m.mCurrentRenderType);
    for (int32_t p = 0; p < 2; ++p) {
        w[2 + 6 * p] = Low32(m.mPools[p].mData);
        w[3 + 6 * p] = static_cast<uint32_t>(m.mPools[p].mUsed);
        w[4 + 6 * p] = static_cast<uint32_t>(m.mPools[p].mSize);
        w[5 + 6 * p] = Low32(m.mPools[p].mIndexData);
        w[6 + 6 * p] = static_cast<uint32_t>(m.mPools[p].mIndexUsed);
        w[7 + 6 * p] = static_cast<uint32_t>(m.mPools[p].mIndexSize);
    }
    w[14] = Low32(m.mCurrent);
    w[15] = Low32(m.mScratchEnd);
    w[16] = static_cast<uint32_t>(m.mF40);
    w[17] = Low32(m.mInfos);
    w[18] = static_cast<uint32_t>(m.mInfoCount);
    w[19] = static_cast<uint32_t>(m.mRemembered);
    w[20] = static_cast<uint32_t>(m.mNextDynamic);
    w[21] = static_cast<uint32_t>(m.mNextStatic);
    w[22] = Low32(m.mScratch);
    w[23] = Low32(m.mScratchNext);
    w[24] = Low32(m.mScratchBlock);
}

// op 1: SaveInfo(vertexEnd, indexEnd), op 2: GetVertexEndPosition, op 3: GetIndexEndPosition.
// The result (the low 32 bits of the pointer for 2 and 3) goes to out[0].
void tpw_lf_rm_op(int32_t op, uint32_t a, uint32_t b, uint32_t* out) {
    CRenderMemory& m = Memory();
    out[0] = 0;
    if (op == 1) {
        m.SaveInfo(reinterpret_cast<char*>(BytePointer(a)), reinterpret_cast<uint16_t*>(BytePointer(b)));
    } else if (op == 2) {
        out[0] = Low32(m.GetVertexEndPosition());
    } else if (op == 3) {
        out[0] = Low32(m.GetIndexEndPosition());
    }
}

}  // extern "C"
