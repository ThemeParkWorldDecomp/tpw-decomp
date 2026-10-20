// Fakes for the map zoom (src/tb_mapzoom_class.cpp: all ten CMapZoom methods, and the
// construction of the global gei_MapZoom, which is real in the unit).
//
// Everything the zoom calls outside the unit is a fake that records an event (tests/calllog.h):
//  - the texture entries: the constructor and destructor (the array new[] and delete[] run them;
//    the events carry the running element number, the destructor's also whether the entry's data
//    pointer is cleared), SetSize and GetBlendingMode (its answer comes from a queue);
//  - the texture cache: Remove and Prepare, with the entry named by its index in the zoom's array
//    (the pointer is never followed, so out-of-range indexes are harmless);
//  - the draw primitive's DrawSprite: its parameter words are an event, its sixteen corner floats
//    go to a dump of words, NaNs as one value;
//  - the assertion, as an event with the line and the two texts.
// The heap is counted by tests/calllog.cpp (tpw_cl_heap). The texture pages and the textures
// behind them are plain objects whose width and height the case sets.
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

#include "calllog.h"
#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_mapzoom.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

using namespace calllog;

namespace {

constexpr int32_t kObjects = 4;                  // textures and pages, labels 1..3 each
constexpr uint64_t kDataBase = 0x20000000u;      // the picture's address (never followed)
constexpr int32_t kMaxEntries = 1100;            // entries the state dump can hold

// A texture page's renderer side: only the width and height are read.
class TestTexture : public CTextureBase {
public:
    int32_t Create(int32_t, int32_t, int32_t, int32_t) override { return 1; }
    int32_t Destroy() override { return 1; }
    void GetTexture() override {}
    int32_t GetPixelCount() override { return 0; }
    int32_t LockSurface() override { return 1; }
    int32_t UnlockSurface() override { return 1; }
    int32_t MipmapsUsed() override { return 0; }
    int32_t Download(CTextureEntry*, int32_t, int32_t) override { return 1; }
    double TestUploadSpeed() override { return 0.0; }
};
TestTexture gTextures[kObjects];
CTextureLink gPages[kObjects];
alignas(CTextureCache) unsigned char gCacheStorage[sizeof(CTextureCache)];

// The draw primitive object: the zoom calls DrawSprite, a fake below.
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
TestDraw gDraw;

int32_t gBuilt = 0;              // entries constructed and not destroyed yet (the running element number)
std::vector<uint32_t> gDump;     // the corner floats of every DrawSprite call

uint32_t Canon(uint32_t word) {
    return (word & 0x7f800000u) == 0x7f800000u && (word & 0x007fffffu) != 0 ? 0x7fc00000u : word;
}
uint32_t Cfb(float value) { return Canon(fbits(value)); }
float Fl(uint32_t word) {
    float value;
    std::memcpy(&value, &word, 4);
    return value;
}

// The index of an entry in the zoom's array, whatever the pointer (it is only subtracted).
int64_t IndexOf(const CTextureEntry* entry) {
    const intptr_t bytes = reinterpret_cast<intptr_t>(entry) - reinterpret_cast<intptr_t>(gei_MapZoom.mEntries);
    return bytes / static_cast<intptr_t>(sizeof(CTextureEntry));
}

}  // namespace

// ---- the objects the zoom reaches
CDisplay::CDisplay() {}
CDisplay gei_Display;
CSystem::CSystem() {}
CSystem gei_System;
CTextureSettings::CTextureSettings() {}
CTextureSettings gei_TextureSettings;
CTextureCache* gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStorage);
CDrawPrimitive* gei_DrawPrimitive = &gDraw;
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CTextureBase::~CTextureBase() {}

// ---- the fakes
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}
CTextureEntry::CTextureEntry() {
    std::memset(static_cast<void*>(this), 0xa5, sizeof *this);
    ev("Entry.ctor", {gBuilt++});
}
CTextureEntry::~CTextureEntry() { ev("Entry.dtor", {--gBuilt, mData != nullptr ? 1 : 0}); }
void CTextureEntry::SetSize(int32_t width, int32_t height) { ev("Entry.SetSize", {IndexOf(this), 0, width, height}); }
uint32_t CTextureEntry::GetBlendingMode() {
    ev("Entry.GetBlendingMode", {IndexOf(this), 0});
    return static_cast<uint32_t>(ret("Entry.GetBlendingMode", 0));
}
int32_t CTextureCache::Remove(CTextureEntry* entry) {
    ev("TCache.Remove", {lab(this), IndexOf(entry), 0});
    return static_cast<int32_t>(ret("TCache.Remove", 1));
}
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCache.Prepare", {lab(this), IndexOf(entry), 0, fbits(distance)});
}
// The parameter record (the word at 0x14 is never set by the zoom and not recorded) and the four
// corners as floats.
int32_t CDrawPrimitive::DrawSprite(SSpriteDrawParameters* sprite, int32_t mode) {
    ev("DrawSprite", {sprite->mFlags, sprite->mF08, sprite->mBlendingMode, lab(sprite->mTexture), Cfb(sprite->mU0),
                      Cfb(sprite->mV0), Cfb(sprite->mU1), Cfb(sprite->mV1), sprite->mF28, mode});
    const uint32_t* corners = static_cast<const uint32_t*>(sprite->mVertices);
    for (int i = 0; i < 16; ++i) gDump.push_back(Canon(corners[i]));
    return static_cast<int32_t>(ret("DrawSprite", 1));
}

extern "C" {

// Object addresses for the labels: kind 0 the zoom, 1 the draw primitive, 2 the texture cache,
// 3 a texture, 4 a page.
void* tpw_mz_pointer(int32_t kind, int32_t index) {
    switch (kind) {
    case 0: return &gei_MapZoom;
    case 1: return static_cast<CDrawPrimitive*>(&gDraw);
    case 2: return gCacheStorage;
    case 3: return &gTextures[index];
    case 4: return &gPages[index];
    }
    return nullptr;
}

// The case's world: the globals the zoom reads, and the textures and pages empty. The zoom itself
// is set by tpw_mz_start. (An entry array left by the previous case is leaked, as the original
// leaks one that Init is called over.)
void tpw_mz_world(int32_t renderer, uint32_t displayFlags, int32_t format) {
    gBuilt = 0;
    gDump.clear();
    gei_System.mRenderer = renderer;
    gei_Display.mFlags = displayFlags;
    gei_TextureSettings.mFormat = format;
    gei_TCache = reinterpret_cast<CTextureCache*>(gCacheStorage);
    gei_DrawPrimitive = &gDraw;
    for (int32_t i = 0; i < kObjects; ++i) {
        gTextures[i].mWidth = 0;
        gTextures[i].mHeight = 0;
        gPages[i].mTexture = nullptr;
    }
}

// The size of texture `label` and the texture (label, 0 for none) that page `index` holds.
void tpw_mz_texture(int32_t label, int32_t width, int32_t height) {
    gTextures[label].mWidth = width;
    gTextures[label].mHeight = height;
}
void tpw_mz_page(int32_t index, int32_t textureLabel) {
    gPages[index].mTexture = textureLabel == 0 ? nullptr : &gTextures[textureLabel];
}

// One word of the zoom by its Mac offset (the entry pointer at 0x1c reads as 0 or 1 and cannot be set).
void tpw_mz_set(int32_t offset, uint32_t word) {
    CMapZoom& z = gei_MapZoom;
    switch (offset) {
    case 0x00: z.mFlags = word; break;
    case 0x04: z.mWidth = static_cast<int32_t>(word); break;
    case 0x08: z.mHeight = static_cast<int32_t>(word); break;
    case 0x0c: z.mPitch = static_cast<int32_t>(word); break;
    case 0x10: z.mColumns = static_cast<int32_t>(word); break;
    case 0x14: z.mRows = static_cast<int32_t>(word); break;
    case 0x18: z.mF18 = static_cast<int32_t>(word); break;
    case 0x20: z.mUseX0 = Fl(word); break;
    case 0x24: z.mUseY0 = Fl(word); break;
    case 0x28: z.mUseX1 = Fl(word); break;
    case 0x2c: z.mUseY1 = Fl(word); break;
    case 0x30: z.mWindowX0 = Fl(word); break;
    case 0x34: z.mWindowY0 = Fl(word); break;
    case 0x38: z.mWindowX1 = Fl(word); break;
    case 0x3c: z.mWindowY1 = Fl(word); break;
    case 0x40: z.mColour = word; break;
    case 0x44: z.mZ = Fl(word); break;
    case 0x48: z.mFirstEntry = static_cast<int32_t>(word); break;
    case 0x4c: z.mF4C = static_cast<int32_t>(word); break;
    case 0x50: z.mF50 = static_cast<int32_t>(word); break;
    case 0x54: z.mF54 = static_cast<int32_t>(word); break;
    case 0x58: z.mEntryCount = static_cast<int32_t>(word); break;
    case 0x5c: z.mFiltering = static_cast<int32_t>(word); break;
    default: break;
    }
}

// The zoom's start: all zero (engine data) and then the construction of __sinit_tb_mapzoom_cpp
// (start 0), or every word set from `words` (24 of them) with no entries (start 1).
void tpw_mz_start(int32_t kind, const uint32_t* words) {
    std::memset(static_cast<void*>(&gei_MapZoom), 0, sizeof gei_MapZoom);
    if (kind == 0) {
        new (&gei_MapZoom) CMapZoom();
        return;
    }
    for (int32_t i = 0; i < 24; ++i) tpw_mz_set(i * 4, words[i]);
    gei_MapZoom.mEntries = nullptr;
}

// The zoom's 24 words by Mac offset (the entry pointer as 0 or 1).
void tpw_mz_words(uint32_t* out) {
    const CMapZoom& z = gei_MapZoom;
    const uint32_t w[24] = {z.mFlags,
                            static_cast<uint32_t>(z.mWidth),
                            static_cast<uint32_t>(z.mHeight),
                            static_cast<uint32_t>(z.mPitch),
                            static_cast<uint32_t>(z.mColumns),
                            static_cast<uint32_t>(z.mRows),
                            static_cast<uint32_t>(z.mF18),
                            z.mEntries != nullptr ? 1u : 0u,
                            fbits(z.mUseX0),
                            fbits(z.mUseY0),
                            fbits(z.mUseX1),
                            fbits(z.mUseY1),
                            fbits(z.mWindowX0),
                            fbits(z.mWindowY0),
                            fbits(z.mWindowX1),
                            fbits(z.mWindowY1),
                            z.mColour,
                            fbits(z.mZ),
                            static_cast<uint32_t>(z.mFirstEntry),
                            static_cast<uint32_t>(z.mF4C),
                            static_cast<uint32_t>(z.mF50),
                            static_cast<uint32_t>(z.mF54),
                            static_cast<uint32_t>(z.mEntryCount),
                            static_cast<uint32_t>(z.mFiltering)};
    std::memcpy(out, w, sizeof w);
}

// Entry `index`: flags, the page (label, 0 none) and texture (label) it names, and the four floats.
void tpw_mz_entry_set(int32_t index, uint32_t flags, int32_t page, int32_t texture, const uint32_t* floats) {
    CTextureEntry& e = gei_MapZoom.mEntries[index];
    e.mFlags = flags;
    e.mPage = page == 0 ? nullptr : &gPages[page];
    e.mTexture = texture == 0 ? nullptr : &gTextures[texture];
    e.mUSize = Fl(floats[0]);
    e.mVSize = Fl(floats[1]);
    e.mUOffset = Fl(floats[2]);
    e.mVOffset = Fl(floats[3]);
}

// All entries (up to kMaxEntries), nine words each: flags, pitch, the data pointer less the picture's
// address (0x80000001 for NULL), the page and texture labels, and the four floats. Returns how many.
int32_t tpw_mz_entries(uint32_t* out) {
    if (gei_MapZoom.mEntries == nullptr) return 0;
    int32_t n = gei_MapZoom.mEntryCount;
    if (n > kMaxEntries) n = kMaxEntries;
    for (int32_t i = 0; i < n; ++i) {
        const CTextureEntry& e = gei_MapZoom.mEntries[i];
        uint32_t* o = out + 9 * i;
        o[0] = e.mFlags;
        o[1] = static_cast<uint16_t>(e.mPitch);
        o[2] = e.mData == nullptr ? 0x80000001u
                                  : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(e.mData) - kDataBase);
        o[3] = static_cast<uint32_t>(lab(e.mPage));
        o[4] = static_cast<uint32_t>(lab(e.mTexture));
        o[5] = Cfb(e.mUSize);
        o[6] = Cfb(e.mVSize);
        o[7] = Cfb(e.mUOffset);
        o[8] = Cfb(e.mVOffset);
    }
    return n;
}

// fn 0 destructor, 1 EnableFiltering, 2 DisableFiltering, 3 Init (a: width, height, pitch), 4 Close,
// 5 Use and 6 Window (a: float bits), 7 Update (a: ints), 8 Prepare, 9 Execute. Answers what the
// method answers (0 for the void ones).
int32_t tpw_mz_call(int32_t fn, const uint32_t* a) {
    CMapZoom& z = gei_MapZoom;
    switch (fn) {
    case 0: z.~CMapZoom(); return 0;
    case 1: z.EnableFiltering(); return 0;
    case 2: z.DisableFiltering(); return 0;
    case 3:
        return z.Init(reinterpret_cast<void*>(static_cast<uintptr_t>(kDataBase)), static_cast<int32_t>(a[0]),
                      static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]));
    case 4: return z.Close();
    case 5: z.Use(Fl(a[0]), Fl(a[1]), Fl(a[2]), Fl(a[3])); return 0;
    case 6: z.Window(Fl(a[0]), Fl(a[1]), Fl(a[2]), Fl(a[3])); return 0;
    case 7:
        z.Update(static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]),
                 static_cast<int32_t>(a[3]));
        return 0;
    case 8: return z.Prepare();
    case 9: return z.Execute();
    }
    return -1;
}

uint32_t tpw_mz_dump_n() { return static_cast<uint32_t>(gDump.size()); }
void tpw_mz_dump_copy(uint32_t* out) { std::memcpy(out, gDump.data(), gDump.size() * 4); }

}  // extern "C"
