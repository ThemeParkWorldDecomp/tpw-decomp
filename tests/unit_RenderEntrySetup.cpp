// Fakes and entry points for the render list entry's primitive set-up (src/tb_renderentry_setup.cpp:
// SRenderListEntry::GetFaceSetBlendMode and SetupPrimitive; the test is tests/test_RenderEntrySetup.py).
//
// The one callee outside the unit that is a fake is CDrawPrimitive::GetNextPrimitive: the real one
// reaches into the render memory's tables, and it has its own test (tests/test_PrimitiveBuffer.py).
// The fake records every argument and answers the case's queue "Prim" (0: NULL, else the one
// primitive of the case, whose bytes were all 0xa5 before the call, so a stray write shows).
//
// Run as they are, as the original's out-of-line copies are on the PowerPC side:
// CTextureEntry::GetBlendingMode and StoreAsWrapping (the library holds src/tb_textureentry.cpp for
// them; everything else that file calls is an inert stand-in below, since nothing here reaches it)
// and CsTexture::GetTextureEntry and CTextureEntry::GetTexture (inline).
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Wavelet.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_loadmanager.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kPages = 8;       // texture pages, numbers 1..kPages
constexpr int32_t kTextures = 6;    // texture objects
constexpr int32_t kFrames = 4;      // entries of a texture object
constexpr int32_t kEntries = 16;    // texture entries
constexpr int32_t kLabelPrimitive = 5;

// The draw primitive: only GetNextPrimitive is called (below).
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

// The world of a case.
uint8_t* gImage = nullptr;                    // the node, its face sets and texture records, in the arena
CsTexture* gTextures[kTextures];              // raw storage in the arena (the records point at them)
CTextureEntry* gFrames[kTextures][kFrames];   // the entries of each texture object
alignas(CTextureEntry) unsigned char gEntryStore[sizeof(CTextureEntry) * kEntries];
char gPages[kPages + 1];
char gF38;                                    // what a non-null mF38 points at
SPrimitive gPrimitive;                        // the one primitive GetNextPrimitive hands out
SRenderListEntry gEntry;
TestDraw gDraw;

CTextureEntry* EntryAt(int32_t index) {
    return reinterpret_cast<CTextureEntry*>(gEntryStore) + index;
}

uint32_t Word(const void* p) {
    uint32_t w;
    std::memcpy(&w, p, 4);
    return w;
}

}  // namespace

// ---- the objects the unit reaches
CDisplay::CDisplay() {}
CDisplay gei_Display;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CSystem::CSystem() {}
CSystem gei_System;
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}

// ---- what src/tb_textureentry.cpp links against and the unit never reaches
TbMallocFn gMalloc = nullptr;
TbFreeFn gFree = nullptr;
uint8_t* gProcTextureBuffer = nullptr;
const int32_t gTextureBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};
CErrorHandler gei_ErrorHandler;
void CErrorHandler::Push(int32_t, int32_t) {}
alignas(8) char gCacheStore[16];
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStore);
int32_t CTextureCache::Remove(CTextureEntry*) { return 1; }
void msl_assertion_failed(const char* condition, const char* file, int32_t line) { ev("assert", {line}, {condition, file}); }
namespace SamsUtilities {
void _USwapBlock16(uint16_t*, int32_t) {}
void _USwapBlock32(uint32_t*, int32_t) {}
}  // namespace SamsUtilities

// ---- the fake
SPrimitive* CDrawPrimitive::GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                             void* texture) {
    ev("GetNextPrimitive", {lab(this), flags, primitiveType, vertexFormat, mode, lab(texture)});
    return ret("Prim", 1) != 0 ? &gPrimitive : nullptr;
}

extern "C" {

// The address of an object the test labels: 0 the draw primitive, 1 page i, 2 the primitive.
void* tpw_rs_pointer(int32_t kind, int32_t i) {
    switch (kind) {
    case 0: return &gDraw;
    case 1: return &gPages[i];
    case 2: return &gPrimitive;
    }
    return nullptr;
}

// The node's image (little-endian, pointers as offsets in the image at the `relocs` places, 0 for
// null): copied into the arena and its pointers made Ptr32s there.
void tpw_rs_image(const uint8_t* bytes, int32_t size, const uint32_t* relocs, int32_t count) {
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

// The texture entries, four words each: flags, page number (0: none), whether mF38 is set, mF40.
// Everything else in an entry is zero.
void tpw_rs_entries(const uint32_t* words, int32_t count) {
    std::memset(gEntryStore, 0, sizeof gEntryStore);
    for (int32_t k = 0; k < count; ++k) {
        const uint32_t* w = words + 4 * k;
        CTextureEntry& e = *EntryAt(k);
        e.mFlags = w[0];
        e.mTexture = w[1] != 0 ? reinterpret_cast<CTextureBase*>(&gPages[w[1]]) : nullptr;
        e.mF38 = w[2] != 0 ? &gF38 : nullptr;
        e.mF40 = static_cast<int32_t>(w[3]);
    }
}

// The texture objects: kFrames entry numbers each (-1: NULL).
void tpw_rs_textures(const int32_t* frames, int32_t count) {
    for (int32_t t = 0; t < kTextures; ++t) {
        if (gTextures[t] == nullptr) gTextures[t] = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
        for (int32_t f = 0; f < kFrames; ++f) {
            const int32_t k = t < count ? frames[kFrames * t + f] : -1;
            gFrames[t][f] = k >= 0 ? EntryAt(k) : nullptr;
        }
        gTextures[t]->mEntries = gFrames[t];
    }
}

// The texture record at `recordOffset` of the image points at texture object `texture` (-1: none).
void tpw_rs_record_texture(int32_t recordOffset, int32_t texture) {
    reinterpret_cast<sTexture*>(gImage + recordOffset)->mTexture = texture >= 0 ? gTextures[texture] : nullptr;
}

// The display's flags, the texture settings' flags and the renderer.
void tpw_rs_globals(uint32_t display, uint32_t settings, int32_t renderer) {
    gei_Display.mFlags = display;
    gei_TextureSettings.mFlags = settings;
    gei_System.mRenderer = renderer;
}

// The render list entry: its node (offset in the image) and flags. The other fields are poison.
void tpw_rs_entry(int32_t nodeOffset, uint32_t flags) {
    std::memset(static_cast<void*>(&gEntry), 0xa5, sizeof gEntry);
    gEntry.mNode = reinterpret_cast<tag_sNode*>(gImage + nodeOffset);
    gEntry.mFlags = flags;
}

// GetFaceSetBlendMode on the face set at `faceSetOffset` of the image and texture entry
// `entryIndex` (-1: NULL).
int32_t tpw_rs_blend(int32_t faceSetOffset, int32_t entryIndex) {
    return gEntry.GetFaceSetBlendMode(reinterpret_cast<sFaceSet*>(gImage + faceSetOffset),
                                      entryIndex >= 0 ? EntryAt(entryIndex) : nullptr);
}

// SetupPrimitive on face set `faceSet`, with *extra starting as `extraBefore`: the primitive's
// label (0: NULL), *extra afterwards in out[0].
int32_t tpw_rs_setup(int32_t faceSet, int32_t extraBefore, int32_t* out) {
    std::memset(static_cast<void*>(&gPrimitive), 0xa5, sizeof gPrimitive);
    int32_t extra = extraBefore;
    SPrimitive* primitive = gEntry.SetupPrimitive(faceSet, &extra);
    out[0] = extra;
    return primitive == nullptr ? 0 : primitive == &gPrimitive ? kLabelPrimitive : -1;
}

// After SetupPrimitive: the primitive's flags and mF34, and how many other bytes of it changed.
void tpw_rs_primitive(int32_t* out) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&gPrimitive);
    const size_t f34 = offsetof(SPrimitive, mF34);
    int32_t strays = 0;
    for (size_t i = 0; i < sizeof gPrimitive; ++i) {
        const bool known = i < 4 || (i >= f34 && i < f34 + 4);
        if (!known && bytes[i] != 0xa5) ++strays;
    }
    out[0] = static_cast<int32_t>(gPrimitive.mFlags);
    out[1] = gPrimitive.mF34;
    out[2] = strays;
}

}  // extern "C"
