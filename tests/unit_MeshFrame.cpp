// Fakes for the mesh object's per-frame members (src/engine/DirectDraw.cpp, src/engine/Mesh.cpp, src/engine/RenderList.cpp; tests/test_MeshFrame.py).
//
// Real: CMesh::Prepare, Preprocess, PreprocessTextures, StaticCalculateLights, ColorVertices,
// DotProduct, and CsTexture::GetTextureEntry (inline). Everything else they call records its
// arguments and answers from the case's queues (calllog.h): the math unit, the render list,
// the texture cache, a texture entry's Lock, the light manager, a map list's PrepareLights,
// the height field's CheckGamutArea, the clipping object's CheckBoundingBox, the mesh's
// callback and the assertion.
//
// The mesh file's image (header, texture records, nodes, face sets, mesh words, list heads,
// matrices) is one arena block the test lays out; objects in it are named by their offset in
// the block (Where). The CMesh and the engine's objects are native and static.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Light.h"
#include "MapWho.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_meshdelay.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

// Fixed labels of the singletons (the same numbers as LABELS in test_MeshFrame.py).
enum : int32_t { kMath = 1, kRenderList, kCache, kLightManager, kHField, kClipping };

constexpr int32_t kMeshWhere = -2;    // Where() of the mesh object or its lights
constexpr int32_t kStackWhere = -1;   // anything else (a matrix on the stack)

uint8_t* gImage = nullptr;      // the arena block of the case
uint32_t gImageSize = 0;
CMesh* gMesh = nullptr;
struct AbortCase {};            // thrown by the assertion: the original would abort there

// Where a pointer is: 0 null, an offset in the image, or the mesh object (-2), or other (-1).
int64_t Where(const void* p) {
    if (p == nullptr) return 0;
    const auto* b = static_cast<const uint8_t*>(p);
    if (gImage != nullptr && b >= gImage && b < gImage + gImageSize) return b - gImage;
    const auto* m = reinterpret_cast<const uint8_t*>(gMesh);
    if (gMesh != nullptr && b >= m && b < m + sizeof(CMesh)) return kMeshWhere;
    return kStackWhere;
}

// A pointer the optimiser cannot see through: the list heads of PrepareLights may be NULL (the
// original tests `this`), which the compiler would otherwise take as impossible.
const void* Opaque(const void* p) {
    asm volatile("" : "+r"(p));
    return p;
}

// A matrix as two events of eight words.
void MatrixEvents(const void* matrix) {
    uint32_t w[16];
    std::memcpy(w, matrix, sizeof w);
    ev("Matrix", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("Matrix", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
}

// The math unit: MultMatrix records both matrices and writes a + 3 * b + index as words (so
// the product is exact on both sides).
class FakeMath : public CTransformBase {
public:
    ~FakeMath() override {}
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void*, void*, int32_t, int32_t, int32_t, int32_t) override {}
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float* a, const float* b, float* out) override {
        ev("MultMatrix", {kMath});
        MatrixEvents(a);
        MatrixEvents(b);
        uint32_t wa[16], wb[16], wo[16];
        std::memcpy(wa, a, sizeof wa);
        std::memcpy(wb, b, sizeof wb);
        for (int i = 0; i < 16; ++i) wo[i] = wa[i] + 3 * wb[i] + static_cast<uint32_t>(i);
        std::memcpy(out, wo, sizeof wo);
    }
};
FakeMath gMath;

alignas(16) unsigned char gMapStore[sizeof(CMapWho)];
alignas(16) unsigned char gHFieldStore[sizeof(CHField)];
alignas(16) unsigned char gListStore[sizeof(CRenderList)];
alignas(16) unsigned char gCacheStore[sizeof(CTextureCache)];
alignas(16) unsigned char gLightStore[sizeof(CLightManager)];
alignas(16) unsigned char gMeshStore[sizeof(CMesh)];
CClipping gClipping;
CMatrix gFinal;
CTextureEntry* gEntryArrays[4][4];

// The mesh's callback (Prepare calls it with the header).
void FakeCallback(tag_sM3D2Header* header) { ev("Callback", {Where(header)}); }

}  // namespace

// ---- engine data the code reads
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CMeshDelay gMeshDelay;
uint32_t gRenderEntryFlags = 0;
CLight* gev_LightChain = nullptr;
CMatrix* gei_FinalMatrix = &gFinal;
CMatrix gei_LightMatrix;
CTransformBase::~CTransformBase() {}
CTransformBase* gei_MathUnit = &gMath;
CMapWho* gei_MapWho = reinterpret_cast<CMapWho*>(gMapStore);
CHField* gei_HField = reinterpret_cast<CHField*>(gHFieldStore);
CClipping* gei_Clipping = &gClipping;
CRenderList* gei_RenderList = reinterpret_cast<CRenderList*>(gListStore);
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
CLightManager* gei_LightManager = reinterpret_cast<CLightManager*>(gLightStore);

// ---- the mesh object (only what the vtable needs)
CMapLinkBase::~CMapLinkBase() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }

// ---- the fakes
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("Assert", {line}, {condition, file});
    throw AbortCase{};
}

void SMapLinkType::PrepareLights() {
    const void* self = Opaque(this);
    ev("PrepareLights", {Where(self), Where(gev_LightChain)});
    // The lights it finds: the chain now starts at this list.
    if (self != nullptr) gev_LightChain = reinterpret_cast<CLight*>(const_cast<void*>(self));
}
int32_t CLightManager::AddLights(CLight** lights) {
    ev("AddLights", {kLightManager, Where(lights), Where(gev_LightChain)});
    return 1;
}
int32_t CLightManager::PrepareLighting(CMatrix* world, sAlignedBox* box) {
    ev("PrepareLighting", {kLightManager, Where(box), Where(gev_LightChain)});
    MatrixEvents(world);
    return 1;
}
void CLightManager::CalcNodeVertexColors(uint32_t* colours, tag_sNode* node) {
    ev("CalcNodeVertexColors", {kLightManager, Where(colours), Where(node), mF04});
}
int32_t CHField::CheckGamutArea(int8_t* area) {
    ev("CheckGamutArea", {kHField, area[0], area[1], area[2], area[3]});
    return static_cast<int32_t>(ret("Gamut", 1));
}
uint8_t CClipping::CheckBoundingBox(const sAlignedBox& box, CMatrix& matrix) {
    uint32_t w[6];
    std::memcpy(w, &box, sizeof w);
    ev("CheckBoundingBox", {kClipping, w[0], w[1], w[2], w[3], w[4], w[5]});
    MatrixEvents(&matrix);
    // The depth it leaves behind for PreprocessTextures.
    const uint32_t depthBits = static_cast<uint32_t>(ret("Depth", 0));
    std::memcpy(&mMinZ, &depthBits, sizeof mMinZ);
    return static_cast<uint8_t>(ret("Clip", 0x40));
}
int32_t CRenderList::AddEntry(CMatrix& view, CMatrix& unused, CLight** lights, tag_sNode* node, int32_t flags) {
    ev("AddEntry", {kRenderList, Where(lights), Where(node), static_cast<uint32_t>(flags)});
    MatrixEvents(&view);
    MatrixEvents(&unused);
    return static_cast<int32_t>(ret("AddEntry", 1));
}
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("CachePrepare", {kCache, Where(entry), fbits(distance)});
}
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("CacheRemove", {kCache, Where(entry)});
    return 1;
}
void CTextureEntry::Lock() { ev("EntryLock", {Where(this)}); }

extern "C" {
void* tpw_mf_arena_base() { return tpw::ArenaBase(); }

// The settings of a case (test_MeshFrame.py has the same field order).
struct MfSetup {
    uint32_t imageSize;
    int32_t headerOffset;
    int32_t textureSlots[4];           // image offsets of the texture objects (0: none)
    int32_t textureEntries[4][4];      // image offsets of each one's entries (0: NULL)
    uint32_t meshFlags;
    int32_t meshF90, meshF94;
    uint32_t meshF14C;
    int32_t hasCallback;
    uint32_t meshMatrix[16], meshMatrix2[16];
    uint32_t finalMatrix[16], lightMatrix[16];
    int32_t mapF30, mapList6;          // image offsets of the list heads (0: NULL)
    int32_t chain;                     // image offset of the light chain's head (0: NULL)
    uint32_t displayFlags;
    int32_t frame, f84, f928, f92C, f930, f934;
    uint32_t systemFlags;
    int32_t lightFrame, renderer;
    uint32_t settingsFlags, entryFlags;
    int32_t lightMask;
    uint32_t clipWord0, clipDepth;
    int32_t delayCount;
};

// A fresh arena block for the case's image, zeroed (the previous one goes).
void* tpw_mf_alloc(uint32_t size) {
    if (gImage != nullptr) tpw::ArenaFree(gImage);
    gImage = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(size));
    gImageSize = size;
    return gImage;
}

// Builds the case's native objects around the image, which the test has filled in.
void tpw_mf_setup(const MfSetup* s) {
    gMesh = new (gMeshStore) CMesh;
    gMesh->mFlags = s->meshFlags;
    gMesh->mHeader = reinterpret_cast<tag_sM3D2Header*>(gImage + s->headerOffset);
    std::memcpy(&gMesh->mMatrix, s->meshMatrix, 64);
    std::memcpy(&gMesh->mMatrix2, s->meshMatrix2, 64);
    gMesh->mF90 = s->meshF90;
    gMesh->mF94 = s->meshF94;
    std::memset(gMesh->mF98, 0, sizeof gMesh->mF98);
    gMesh->mF14C = s->meshF14C;
    gMesh->mCallback = s->hasCallback ? FakeCallback : nullptr;
    // Texture objects in their slots, with the entries they hold.
    for (int t = 0; t < 4; ++t) {
        if (s->textureSlots[t] == 0) continue;
        auto* object = reinterpret_cast<CsTexture*>(gImage + s->textureSlots[t]);
        for (int f = 0; f < 4; ++f) {
            gEntryArrays[t][f] = s->textureEntries[t][f] != 0
                                     ? reinterpret_cast<CTextureEntry*>(gImage + s->textureEntries[t][f])
                                     : nullptr;
        }
        object->mEntries = gEntryArrays[t];
        object->mEntryCount = 4;
    }
    std::memcpy(gei_FinalMatrix, s->finalMatrix, 64);
    std::memcpy(&gei_LightMatrix, s->lightMatrix, 64);
    gei_MapWho->mF30 = s->mapF30 != 0 ? reinterpret_cast<SMapLinkType*>(gImage + s->mapF30) : nullptr;
    gei_MapWho->mF14[6] = s->mapList6 != 0 ? reinterpret_cast<SMapLinkType*>(gImage + s->mapList6) : nullptr;
    gev_LightChain = s->chain != 0 ? reinterpret_cast<CLight*>(gImage + s->chain) : nullptr;
    gei_Display.mFlags = s->displayFlags;
    gei_Display.mFrameCount = s->frame;
    gei_Display.mF84 = s->f84;
    gei_Display.mF928 = s->f928;
    gei_Display.mF92C = s->f92C;
    gei_Display.mF930 = s->f930;
    gei_Display.mF934 = s->f934;
    gei_System.mFlags = s->systemFlags;
    gei_System.mLightFrame = s->lightFrame;
    gei_System.mRenderer = s->renderer;
    gei_TextureSettings.mFlags = s->settingsFlags;
    gRenderEntryFlags = s->entryFlags;
    gei_LightManager->mF04 = s->lightMask;
    gClipping.mF00 = s->clipWord0;
    std::memcpy(&gClipping.mMinZ, &s->clipDepth, sizeof gClipping.mMinZ);
    gMeshDelay.mCount = s->delayCount;
    std::memset(gMeshDelay.mMeshes, 0, sizeof gMeshDelay.mMeshes);
}

// Changes the display's frame stamp, the system's flags and the mesh's flags between the steps
// of a script.
void tpw_mf_set(int32_t frame, int32_t f84, uint32_t systemFlags, int32_t lightFrame, uint32_t meshFlags) {
    gei_Display.mFrameCount = frame;
    gei_Display.mF84 = f84;
    gei_System.mFlags = systemFlags;
    gei_System.mLightFrame = lightFrame;
    gMesh->mFlags = meshFlags;
}

// Ops: 1 Prepare, 2 Preprocess(node, matrix, matrix2), 3 PreprocessTextures(node),
// 4 StaticCalculateLights(node, matrix, lights, flags). Offsets are image offsets (lights:
// 0 null, -2 the mesh's own). Returns the result, or -777 when the assertion ended the call.
int64_t tpw_mf_run(int32_t op, const int64_t* a) {
    auto* image = gImage;
    auto node = [&](int64_t offset) { return reinterpret_cast<tag_sNode*>(image + offset); };
    auto matrix = [&](int64_t offset) { return reinterpret_cast<CMatrix*>(image + offset); };
    try {
        switch (op) {
        case 1: return gMesh->Prepare();
        case 2: return gMesh->Preprocess(node(a[0]), matrix(a[1]), matrix(a[2]));
        case 3:
            gMesh->PreprocessTextures(node(a[0]));
            return 0;
        case 4: {
            CLight** lights = a[2] == 0 ? nullptr
                              : a[2] == kMeshWhere ? reinterpret_cast<CLight**>(gMesh->mF98)
                                                   : reinterpret_cast<CLight**>(image + a[2]);
            return gMesh->StaticCalculateLights(node(a[0]), *matrix(a[1]), lights, static_cast<int32_t>(a[3]));
        }
        }
    } catch (const AbortCase&) {
        return -777;
    }
    return -99;
}

// The native state, in the order of Ppc.state in the Python module: the mesh, the engine data
// the code writes, the delay list.
int32_t tpw_mf_state(int64_t* out) {
    int32_t n = 0;
    out[n++] = gMesh->mFlags;
    out[n++] = gMesh->mF90;
    out[n++] = gMesh->mF94;
    out[n++] = gMesh->mF14C;
    out[n++] = gMesh->mCallback != nullptr;
    uint32_t w[16];
    std::memcpy(w, &gMesh->mMatrix, 64);
    for (uint32_t v : w) out[n++] = v;
    std::memcpy(w, &gMesh->mMatrix2, 64);
    for (uint32_t v : w) out[n++] = v;
    for (uint32_t v : gMesh->mF98) out[n++] = v;
    out[n++] = gRenderEntryFlags;
    out[n++] = Where(gev_LightChain);
    out[n++] = gei_LightManager->mF04;
    out[n++] = gei_Display.mFlags;
    out[n++] = gei_System.mFlags;
    out[n++] = gei_System.mLightFrame;
    out[n++] = gei_TextureSettings.mFlags;
    out[n++] = gClipping.mF00;
    uint32_t minZBits;
    std::memcpy(&minZBits, &gClipping.mMinZ, sizeof minZBits);
    out[n++] = minZBits;
    out[n++] = gMeshDelay.mCount;
    for (int i = 0; i < 4; ++i) {
        const CMesh* m = gMeshDelay.mMeshes[i];
        out[n++] = m == nullptr ? 0 : m == gMesh ? kMeshWhere : kStackWhere;
    }
    std::memcpy(w, gei_FinalMatrix, 64);
    for (uint32_t v : w) out[n++] = v;
    std::memcpy(w, &gei_LightMatrix, 64);
    for (uint32_t v : w) out[n++] = v;
    return n;
}

// ColorVertices over a buffer; DotProduct (the vectors as float bits); both direct.
void tpw_mf_color(void* vertices, uint32_t colour, int32_t count, int32_t stride) {
    ColorVertices(static_cast<SFVF_TLCTEX*>(vertices), colour, count, stride);
}
uint32_t tpw_mf_dot(const uint32_t* bits) {
    float v[6];
    std::memcpy(v, bits, sizeof v);
    const float result = DotProduct(D3DVECTOR{v[0], v[1], v[2]}, D3DVECTOR{v[3], v[4], v[5]});
    uint32_t out;
    std::memcpy(&out, &result, sizeof out);
    return out;
}
}
