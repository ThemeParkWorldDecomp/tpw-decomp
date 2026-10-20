// Fakes for SRenderListEntry::CalcRenderEntry (src/tb_renderentry.cpp; the test is
// tests/test_RenderEntry.py). Every function it calls outside the unit records an event through
// tests/calllog.h, with pointers into the vertex and index memory as byte offsets:
//
//  - DotProduct records the six words of its two vectors and answers the case's next value. On
//    the call the case names it ends the case instead (AbortCase here; the PowerPC side stops
//    the emulator): the environment-mapped coordinates lead the original into a loop of
//    0xffffffff vertices, so a case that takes them is cut short inside them.
//  - CCompBase::AverageColor (a texture's compressed image) answers the queue "Average".
//  - ColorVertices records its arguments and changes each colour it is given (word * 3 + colour
//    + i), so where it runs shows in the vertices.
//  - CClipping::CPP_TestData records its arguments, writes the case's outside codes (ANDed with
//    the mask) and answers their AND (or the queue "TestData") and OR.
//  - CClipping::ClipPoly records the triangle before `indices`, rewrites it as 1 + "Clip.tri"
//    made-up triangles (none and three back for -1), adds "Clip.add" to the count and answers the
//    place after them.
//  - CDrawPrimitive::ApplyFog and the math unit's Project record their arguments and add
//    "Fog.add" and "Project.add" to the primitive's vertex count (where the count is read again
//    shows); Project sets the primitive's depth to "Project.poke" when queued and answers
//    "Project".
//
// The inline getters CsTexture::GetTextureEntry, CTextureEntry's five and CMatrix::Return run
// as they are (the original's out-of-line copies on the PowerPC side).
#include <cstdint>
#include <cstring>
#include <vector>

#include "MapWho.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kPages = 8;      // texture pages, numbers 1..kPages
constexpr int32_t kComps = 4;      // compressed images
constexpr int32_t kTextures = 6;   // texture objects
constexpr int32_t kFrames = 4;     // entries of a texture object

struct AbortCase {};               // DotProduct's way to end a case

// A compressed image: only its average colour is asked for.
class TestComp : public CCompBase {
public:
    int32_t Blit(SUnpackDestination*, int32_t) override { return 0; }
    int32_t Decompress(SUnpackDestination*) override { return 0; }
    uint32_t AverageColor() override {
        ev("AverageColor", {lab(this)});
        return static_cast<uint32_t>(ret("Average", 0));
    }
};

class TestDraw : public CDrawPrimitive {
public:
    int32_t Init() override { return 1; }
    int32_t SetBlendingMode(int32_t) override { return 1; }
    int32_t SetTexture(int32_t, void*) override { return 1; }
    int32_t Execute(SPrimitive*) override { return 1; }
    int32_t ExecuteShadow(SPrimitive*) override { return 1; }
    int32_t SetFogParameters(float, float, uint32_t) override { return 1; }
    int32_t BeginDraw() override { return 1; }
    int32_t EndDraw() override { return 1; }
    int32_t DrawTestPoly() override { return 1; }
    int32_t DrawFilledBox(float, float, float, float, int32_t, CTextureBase*) override { return 1; }
};

// The math unit: only Project is called.
class TestMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void*, void*, int32_t, int32_t, int32_t, int32_t) override {}
    float Project(void* vertices, int32_t stride, int32_t count, int32_t type) override;
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};

// The world of a case.
uint8_t* gImage = nullptr;                 // the node and what it points to, in the arena
CsTexture* gTextures[kTextures];           // raw storage in the arena (records point at them)
CTextureEntry* gFrames[kTextures][kFrames];
std::vector<CTextureEntry> gEntries;       // the entries, two blank ones after the case's
TestComp gComps[kComps];
char gPages[kPages + 1];
char gF38;                                 // what a non-null mF38 points at
std::vector<uint32_t> gSource;             // the entry's transformed vertices, four words each
std::vector<uint32_t> gColours;
std::vector<uint32_t> gVertexArea;
std::vector<uint16_t> gIndexArea;
SPrimitive gPrims[2];                      // the case's primitive and another one
SRenderListEntry gEntry;
TestDraw gDraw;
CClipping gClipping;
TestMath gMath;
CMatrix gView;
alignas(CLightManager) unsigned char gLightStorage[sizeof(CLightManager)];
std::vector<uint8_t> gCodes;               // CPP_TestData's outside codes
std::vector<uint32_t> gDots;               // DotProduct's answers
int32_t gDotAbort = 0, gDotCalls = 0;
uint32_t gSalt = 0;
int32_t gClipCalls = 0;

uint32_t Word(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    return w;
}
float Float(uint32_t w) {
    float f;
    std::memcpy(&f, &w, 4);
    return f;
}
void* PagePointer(int32_t label) {
    if (label == 0) return nullptr;
    if (label < 0) return reinterpret_cast<void*>(static_cast<uintptr_t>(0xa5a5a5a5a5a5a5a5ull));
    return &gPages[label];
}
uint8_t* VertexBase() { return reinterpret_cast<uint8_t*>(gVertexArea.data()); }
uint8_t* IndexBase() { return reinterpret_cast<uint8_t*>(gIndexArea.data()); }
int64_t VertexOffset(const void* p) { return static_cast<const uint8_t*>(p) - VertexBase(); }
int64_t IndexOffset(const void* p) { return static_cast<const uint8_t*>(p) - IndexBase(); }
// Whether `bytes` bytes at p lie in the vertex (index) memory: the fakes write only there.
bool InVertices(const uint8_t* p, size_t bytes) {
    return p >= VertexBase() && p + bytes <= VertexBase() + 4 * gVertexArea.size();
}
bool InIndices(const uint8_t* p, size_t bytes) {
    return p >= IndexBase() && p + bytes <= IndexBase() + 2 * gIndexArea.size();
}

}  // namespace

// ---- the objects CalcRenderEntry reaches
CDisplay::CDisplay() {}
CDisplay gei_Display;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CClipping* gei_Clipping = &gClipping;
CTransformBase* gei_MathUnit = &gMath;
CMatrix* gei_ViewMatrix = &gView;
CLightManager* gei_LightManager = reinterpret_cast<CLightManager*>(gLightStorage);
float gFaceX0, gFaceX1, gFaceX2, gFaceY0, gFaceY1, gFaceY2, gFaceW0, gFaceW1, gFaceW2;
CTextureEntry::CTextureEntry() { std::memset(static_cast<void*>(this), 0, sizeof *this); }
CTextureEntry::~CTextureEntry() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CTransformBase::~CTransformBase() {}
CCompBase::~CCompBase() {}
int32_t CCompBase::IsNaturallyFlipped() { return 0; }

// ---- the fakes
float DotProduct(D3DVECTOR a, D3DVECTOR b) {
    ev("DotProduct", {fbits(a.x), fbits(a.y), fbits(a.z), fbits(b.x), fbits(b.y), fbits(b.z)});
    ++gDotCalls;
    if (gDotCalls == gDotAbort) throw AbortCase{};
    if (gDots.empty()) return 0.0f;
    const size_t i = static_cast<size_t>(gDotCalls - 1);
    return Float(gDots[i < gDots.size() ? i : gDots.size() - 1]);
}

void ColorVertices(SFVF_TLCTEX* vertices, uint32_t colour, int32_t count, int32_t stride) {
    ev("ColorVertices", {VertexOffset(vertices), colour, count, stride});
    uint8_t* v = reinterpret_cast<uint8_t*>(vertices);
    for (int32_t i = 0; i < count; ++i, v += stride) {
        if (!InVertices(v + 0x10, 4)) continue;
        const uint32_t w = Word(v + 0x10) * 3u + colour + static_cast<uint32_t>(i);
        std::memcpy(v + 0x10, &w, 4);
    }
}

void CClipping::CPP_TestData(uint8_t* result, void* clipping, uint8_t* codes, const void* vertices, int32_t count,
                             int32_t stride, uint32_t mask) {
    ev("CPP_TestData", {lab(clipping), VertexOffset(vertices), count, stride, mask});
    uint8_t all = 0xff, any = 0;
    for (int32_t i = 0; i < count; ++i) {
        const uint8_t code =
            static_cast<uint8_t>((i < static_cast<int32_t>(gCodes.size()) ? gCodes[i] : 0) & mask);
        codes[i] = code;
        all &= code;
        any |= code;
    }
    result[0] = static_cast<uint8_t>(ret("TestData", count > 0 ? all : 0));
    result[1] = any;
}

uint16_t* CClipping::ClipPoly(int32_t planes, uint16_t* indices, SFVF_TLCTEX* vertices, int32_t& count,
                              int32_t stride) {
    const bool inside = InIndices(reinterpret_cast<uint8_t*>(indices - 3), 6);
    ev("ClipPoly", {lab(this), planes, IndexOffset(indices), inside ? indices[-3] : -1, inside ? indices[-2] : -1,
                    inside ? indices[-1] : -1, VertexOffset(vertices), count, stride});
    const int32_t extra = static_cast<int32_t>(ret("Clip.tri", 0));
    for (int32_t k = 0; k < 3 + 3 * extra; ++k) {
        uint16_t* at = indices - 3 + k;
        if (!InIndices(reinterpret_cast<uint8_t*>(at), 2)) continue;
        *at = static_cast<uint16_t>(gSalt + 37u * static_cast<uint32_t>(gClipCalls) + 11u * static_cast<uint32_t>(k));
    }
    ++gClipCalls;
    count += static_cast<int32_t>(ret("Clip.add", 0));
    return indices + 3 * extra;
}

void CDrawPrimitive::ApplyFog(SFVF_TLCTEX* vertices, int32_t count) {
    ev("ApplyFog", {lab(this), VertexOffset(vertices), count});
    gPrims[0].mVertexCount += static_cast<int32_t>(ret("Fog.add", 0));
}

float TestMath::Project(void* vertices, int32_t stride, int32_t count, int32_t type) {
    ev("Project", {lab(this), VertexOffset(vertices), stride, count, type});
    gPrims[0].mVertexCount += static_cast<int32_t>(ret("Project.add", 0));
    const int64_t poke = ret("Project.poke", -1);
    if (poke >= 0) gPrims[0].mDepth = Float(static_cast<uint32_t>(poke));
    return Float(static_cast<uint32_t>(ret("Project", 0)));
}

extern "C" {

// The address of an object the test labels: 0 the draw primitive, 1 the clipper, 2 the math
// unit, 3 page i, 4 compressed image i.
void* tpw_re_pointer(int32_t kind, int32_t i) {
    switch (kind) {
    case 0: return &gDraw;
    case 1: return &gClipping;
    case 2: return &gMath;
    case 3: return &gPages[i];
    case 4: return &gComps[i];
    }
    return nullptr;
}

// The node's image (little-endian, pointers as offsets in the image at the `relocs` places,
// 0 for null): copied into the arena and its pointers made Ptr32s there.
void tpw_re_image(const uint8_t* bytes, int32_t size, const uint32_t* relocs, int32_t count) {
    tpw::ArenaFree(gImage);
    gImage = static_cast<uint8_t*>(tpw::ArenaAlloc(static_cast<size_t>(size)));
    std::memcpy(gImage, bytes, static_cast<size_t>(size));
    const uint32_t base = tpw::ArenaOffset(gImage);
    for (int32_t i = 0; i < count; ++i) {
        uint32_t w = Word(gImage + relocs[i]);
        if (w != 0) w += base;
        std::memcpy(gImage + relocs[i], &w, 4);
    }
}

// The texture entries, eight words each: flags, page number, U size, V size, U offset, V offset
// (bits), whether mF38 is set, the compressed image of mData (-1: none). Two blank entries follow.
void tpw_re_entries(const uint32_t* words, int32_t count) {
    gEntries.clear();
    gEntries.resize(static_cast<size_t>(count) + 2);
    for (int32_t k = 0; k < count; ++k) {
        const uint32_t* w = words + 8 * k;
        CTextureEntry& e = gEntries[k];
        e.mFlags = w[0];
        e.mTexture = static_cast<CTextureBase*>(PagePointer(static_cast<int32_t>(w[1])));
        std::memcpy(&e.mUSize, &w[2], 4);
        std::memcpy(&e.mVSize, &w[3], 4);
        std::memcpy(&e.mUOffset, &w[4], 4);
        std::memcpy(&e.mVOffset, &w[5], 4);
        e.mF38 = w[6] != 0 ? &gF38 : nullptr;
        const int32_t comp = static_cast<int32_t>(w[7]);
        e.mData = comp >= 0 ? static_cast<void*>(&gComps[comp]) : nullptr;
    }
}

// The texture objects: kFrames entry numbers each (-1: NULL), and for each a texture record
// of the image (its offset) to point at it.
void tpw_re_textures(const int32_t* frames, int32_t count) {
    for (int32_t t = 0; t < kTextures; ++t) {
        if (gTextures[t] == nullptr) gTextures[t] = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
        for (int32_t f = 0; f < kFrames; ++f) {
            const int32_t k = t < count ? frames[kFrames * t + f] : -1;
            gFrames[t][f] = k >= 0 ? &gEntries[k] : nullptr;
        }
        gTextures[t]->mEntries = gFrames[t];
    }
}
void tpw_re_record_texture(int32_t recordOffset, int32_t texture) {
    reinterpret_cast<sTexture*>(gImage + recordOffset)->mTexture = texture >= 0 ? gTextures[texture] : nullptr;
}

// The render list entry: its node (offset in the image), flags, transformed vertices (four
// words each) and colours (none when `haveColours` is 0).
void tpw_re_entry(int32_t nodeOffset, uint32_t flags, const uint32_t* source, int32_t sourceWords,
                  const uint32_t* colours, int32_t colourWords, int32_t haveColours) {
    gSource.assign(source, source + sourceWords);
    gColours.assign(colours, colours + colourWords);
    std::memset(static_cast<void*>(&gEntry), 0xa5, sizeof gEntry);
    gEntry.mNode = reinterpret_cast<tag_sNode*>(gImage + nodeOffset);
    gEntry.mFlags = flags;
    gEntry.mVertices = reinterpret_cast<uint8_t*>(gSource.data());
    gEntry.mData = haveColours != 0 ? gColours.data() : nullptr;
}

// The texture settings' flags, the light manager's mask, the display's mZ54 (bits) and the
// view matrix (16 words).
void tpw_re_globals(uint32_t settings, uint32_t mask, uint32_t z54, const uint32_t* view) {
    gei_TextureSettings.mFlags = settings;
    std::memset(gLightStorage, 0xa5, sizeof gLightStorage);
    gei_LightManager->mF04 = static_cast<int32_t>(mask);
    std::memcpy(&gei_Display.mZ54, &z54, 4);
    std::memcpy(&gView.m[0][0], view, 64);
}

// The vertex memory (words) and the index memory (halfwords).
void tpw_re_areas(const uint32_t* vertices, int32_t vertexWords, const uint16_t* indices, int32_t indexCount) {
    gVertexArea.assign(vertices, vertices + vertexWords);
    gIndexArea.assign(indices, indices + indexCount);
}

// The draw primitive: vertex size, which primitive is current (0 or 1), the ends of the vertex
// and index memory (byte offsets in the areas).
void tpw_re_draw(int32_t vertexSize, int32_t current, int32_t vertexEnd, int32_t indexEnd) {
    std::memset(gDraw.mFields, 0xa5, sizeof gDraw.mFields);
    gDraw.SetVertexSize(vertexSize);
    gDraw.mCurrent = &gPrims[current];
    gDraw.mVertexEnd = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(VertexBase()) + static_cast<intptr_t>(vertexEnd));
    gDraw.mIndexEnd = reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(IndexBase()) + static_cast<intptr_t>(indexEnd));
}

// Primitive i: flags, blending mode, page, second and third pages, type, format, vertex count,
// index count, vertex data and index data (byte offsets in the areas), depth (bits), mF34.
void tpw_re_prim(int32_t i, const uint32_t* w) {
    SPrimitive& p = gPrims[i];
    std::memset(static_cast<void*>(&p), 0xa5, sizeof p);
    p.mFlags = w[0];
    p.mBlendingMode = w[1];
    p.mTexture = PagePointer(static_cast<int32_t>(w[2]));
    p.mTexture1 = PagePointer(static_cast<int32_t>(w[3]));
    p.mTexture2 = PagePointer(static_cast<int32_t>(w[4]));
    p.mPrimitiveType = static_cast<int32_t>(w[5]);
    p.mVertexFormat = w[6];
    p.mVertexCount = static_cast<int32_t>(w[7]);
    p.mIndexCount = static_cast<int32_t>(w[8]);
    p.mVertexData = VertexBase() + static_cast<int32_t>(w[9]);
    p.mIndexData = reinterpret_cast<uint16_t*>(IndexBase() + static_cast<int32_t>(w[10]));
    std::memcpy(&p.mDepth, &w[11], 4);
    p.mF34 = static_cast<int32_t>(w[12]);
}

// The fakes' data: the outside codes, DotProduct's answers and the call it ends the case on
// (0: none), the salt of ClipPoly's made-up indices, the nine facing values (bits).
void tpw_re_data(const uint8_t* codes, int32_t codeCount, const uint32_t* dots, int32_t dotCount, int32_t abort,
                 uint32_t salt, const uint32_t* face) {
    gCodes.assign(codes, codes + codeCount);
    gDots.assign(dots, dots + dotCount);
    gDotAbort = abort;
    gSalt = salt;
    float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(globals[i], &face[i], 4);
}

// The call; -999 when DotProduct ended it.
int32_t tpw_re_run(int32_t faceSet, int32_t stride) {
    gDotCalls = 0;
    gClipCalls = 0;
    try {
        return gEntry.CalcRenderEntry(&gPrims[0], faceSet, stride);
    } catch (const AbortCase&) {
        return -999;
    }
}

// After the call: both primitives (13 words each, the shape tpw_re_prim takes, pages as labels,
// data as offsets), the nine facing values, the areas.
void tpw_re_state(int32_t* prims, uint32_t* face, uint32_t* vertices, uint16_t* indices) {
    for (int32_t i = 0; i < 2; ++i) {
        const SPrimitive& p = gPrims[i];
        int32_t* w = prims + 13 * i;
        w[0] = static_cast<int32_t>(p.mFlags);
        w[1] = static_cast<int32_t>(p.mBlendingMode);
        w[2] = lab(p.mTexture);
        w[3] = lab(p.mTexture1);
        w[4] = lab(p.mTexture2);
        w[5] = p.mPrimitiveType;
        w[6] = static_cast<int32_t>(p.mVertexFormat);
        w[7] = p.mVertexCount;
        w[8] = p.mIndexCount;
        w[9] = static_cast<int32_t>(VertexOffset(p.mVertexData));
        w[10] = static_cast<int32_t>(IndexOffset(p.mIndexData));
        w[11] = static_cast<int32_t>(fbits(p.mDepth));
        w[12] = p.mF34;
    }
    const float* globals[9] = {&gFaceX0, &gFaceX1, &gFaceX2, &gFaceY0, &gFaceY1, &gFaceY2, &gFaceW0, &gFaceW1, &gFaceW2};
    for (int32_t i = 0; i < 9; ++i) std::memcpy(&face[i], globals[i], 4);
    std::memcpy(vertices, gVertexArea.data(), 4 * gVertexArea.size());
    std::memcpy(indices, gIndexArea.data(), 2 * gIndexArea.size());
}

}  // extern "C"
