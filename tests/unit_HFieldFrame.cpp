// Fakes and entry points for the height field's per-frame members and the texture cache's
// frame calls (src/engine/Gamut.cpp, src/engine/tb_hfield.cpp, src/engine/TextureCache.cpp;
// tests/test_HFieldFrame.py). Every callee outside the unit records its arguments (objects by
// label, floats by bit pattern) and answers from the case's queues (calllog.h). The PowerPC
// side records the same events.
//
// A case arrives as a flat list of 32-bit numbers (the reader below; test_HFieldFrame.py
// writes them in the same order). The state goes back the same way: a short list of numbers,
// then a block of bulk words (the cells, the tiles and the overlays) that the test compares
// as bytes.
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

#include <cstddef>
#include <cstring>
#include <new>
#include <vector>

using namespace calllog;

namespace {

constexpr int32_t kMaxCells = 18000;   // the cells of a case, margins included
constexpr int32_t kMaxTextures = 16;   // the height field's texture table
constexpr int32_t kMaxCs = 8;          // texture objects
constexpr int32_t kMaxEntries = 24;    // texture entries
constexpr int32_t kMaxPlaces = 24;     // entry place records
constexpr int32_t kMaxPages = 12;      // pages of the cache

// Fixed labels (the same numbers as LABEL_* in test_HFieldFrame.py).
enum : int32_t {
    kLabField = 1, kLabGamut, kLabCache, kLabHandler, kLabProjection, kLabView, kLabErrors,
    kLabFirstEntry = 10, kLabFirstPage = 40,
};

// Reads a case's numbers in order.
struct Reader {
    const int32_t* p;
    int32_t i() { return *p++; }
    uint32_t u() { return static_cast<uint32_t>(*p++); }
    float f() {
        const uint32_t bits = u();
        float value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }
};

// ---- The height field part --------------------------------------------------------------

// What the fake TestConQuad does to the grid points' rows (see test_HFieldFrame.py).
struct Script {
    int32_t mode;   // 0 nothing, 1 widens the rows as the original does, 2 overwrites them
    int32_t first, end;
    int8_t low[128], high[128];
    int32_t setFar;
    float farValue;
} script;

CHField field;
sHField record;
sTexture textures[kMaxTextures];
CsTexture* csObjects[kMaxCs];
CTextureEntry* csFrames[kMaxCs][2];
CTextureEntry entries[kMaxEntries];
CGamut gamut;
CMatrix view;
CTextureCache cache;
CProcTextureHandler handler;
sHFieldQuad* cellBlock;   // the arena block of the cells
int32_t cellMargin, cellTotal;

// Copies of the objects before the call, for the check that nothing else changed.
struct Snapshot {
    unsigned char field[sizeof(CHField)];
    unsigned char record[sizeof(sHField)];
    unsigned char textures[sizeof textures];
    unsigned char cs[kMaxCs][sizeof(CsTexture)];
    unsigned char frames[sizeof csFrames];
    unsigned char entries[sizeof entries];
    unsigned char gamut[sizeof(CGamut)];
    unsigned char view[sizeof(CMatrix)];
    unsigned char display[sizeof(CDisplay)];
    unsigned char system[sizeof(CSystem)];
    unsigned char settings[sizeof(CTextureSettings)];
} before;

void Take() {
    std::memcpy(before.field, static_cast<void*>(&field), sizeof field);
    std::memcpy(before.record, &record, sizeof record);
    std::memcpy(before.textures, textures, sizeof textures);
    for (int32_t i = 0; i < kMaxCs; i++) std::memcpy(before.cs[i], static_cast<void*>(csObjects[i]), sizeof(CsTexture));
    std::memcpy(before.frames, csFrames, sizeof csFrames);
    std::memcpy(before.entries, static_cast<void*>(entries), sizeof entries);
    std::memcpy(before.gamut, static_cast<void*>(&gamut), sizeof gamut);
    std::memcpy(before.view, &view, sizeof view);
    std::memcpy(before.display, static_cast<void*>(&gei_Display), sizeof gei_Display);
    std::memcpy(before.system, static_cast<void*>(&gei_System), sizeof gei_System);
    std::memcpy(before.settings, &gei_TextureSettings, sizeof gei_TextureSettings);
}

// 1 when `after` is `saved` apart from the byte ranges [begin, end) the call may change.
struct Range {
    size_t begin, end;
};
int32_t Same(const unsigned char* saved, const void* after, size_t size, const std::vector<Range>& allowed) {
    static unsigned char copy[sizeof(CHField)];
    std::memcpy(copy, after, size);
    for (const Range& r : allowed) std::memcpy(copy + r.begin, saved + r.begin, r.end - r.begin);
    return std::memcmp(copy, saved, size) == 0 ? 1 : 0;
}

}  // namespace

// ---- Globals the unit reads (defined by untranslated or other files in the game) ----------
CSystem gei_System;
CDisplay gei_Display;
CTextureSettings gei_TextureSettings;
CGamut* gei_Gamut = &gamut;
CTextureCache* gei_TCache = &cache;
CMatrix* gei_ViewMatrix = &view;
CMatrix gei_ProjectionMatrix;
CErrorHandler gei_ErrorHandler;

// ---- Constructors and destructors of the objects above (not part of the unit) -------------
CSystem::CSystem() {}
CDisplay::CDisplay() {}
CTextureSettings::CTextureSettings() {}
CGamut::CGamut() {}
CHField::CHField() {}
CHField::~CHField() {}
CTextureCache::CTextureCache() {}
CTextureBase::~CTextureBase() {}
CTextureEntry::CTextureEntry() {}
CTextureEntry::~CTextureEntry() {}
CProcTextureHandler::CProcTextureHandler() {}
CProcTextureHandler::~CProcTextureHandler() {}

// ---- The fakes ----------------------------------------------------------------------------
// The gamut's cells in view (its grid points are CGamut::GetVertices, which runs for real).
SGamutQuads* CGamut::GetQuads() {
    ev("GetQuads", {lab(this)});
    return reinterpret_cast<SGamutQuads*>(&mFC4);
}

// TestConQuad records its box and does what the case's script says to the grid points' rows.
void CHField::TestConQuad(int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    ev("TestConQuad", {lab(this), x0, z0, x1, z1});
    SGamutQuads& rows = *VertexRows();
    if (script.mode == 1) {
        if (z0 < rows.mFirstRow) rows.mFirstRow = z0;
        if (rows.mEndRow < z1) rows.mEndRow = z1;
        for (int32_t z = z0; z <= z1; z++) {
            if (rows.mLow[z] > x0) rows.mLow[z] = static_cast<int8_t>(x0);
            if (rows.mHigh[z] < x1) rows.mHigh[z] = static_cast<int8_t>(x1);
        }
        if (mF1C < script.farValue) mF1C = script.farValue;
    } else if (script.mode == 2) {
        rows.mFirstRow = script.first;
        rows.mEndRow = script.end;
        std::memcpy(rows.mLow, script.low, 128);
        std::memcpy(rows.mHigh, script.high, 128);
        if (script.setFar != 0) mF1C = script.farValue;
    }
}

void CTextureEntry::Lock() { ev("EntryLock", {lab(this)}); }
void CTextureEntry::Unlock() { ev("EntryUnlock", {lab(this)}); }
void CTextureCache::Prepare(CTextureEntry* entry, float distance) {
    ev("TCPrepare", {lab(this), lab(entry), fbits(distance)});
}

// The procedural texture handler: every tile and overlay it is given.
int32_t CProcTextureHandler::PrepareList(PrepareEntry* tiles, sTileOverlay* overlays, int32_t count) {
    ev("PrepareList", {lab(this), count});
    for (int32_t i = 0; i < count; i++)
        ev("PLEntry", {i, tiles[i].mTile, lab(overlays[i].mData), overlays[i].mWidth, overlays[i].mHeight,
                       overlays[i].mFormat, overlays[i].mFlags});
    return 1;
}

void CMatrix::AdjustProjection(float farPlane) { ev("AdjustProjection", {lab(this), fbits(farPlane)}); }
int32_t PreprocessMatrices() {
    ev("PreprocessMatrices");
    return 1;
}

// The texture cache's steps in Execute and the frequency table.
void CTextureCache::DumpTCStreamToLog_Execute() { ev("DumpStream", {lab(this)}); }
void CTextureCache::BalanceCache_FreeLockedTextureSpace() { ev("FreeLocked", {lab(this)}); }
void CTextureCache::SortTextures() { ev("SortTextures", {lab(this)}); }
void CTextureCache::SortEntries() { ev("SortEntries", {lab(this)}); }
void CTextureCache::BalanceCache_LevelSurfaceUsage() { ev("LevelSurface", {lab(this)}); }
int32_t CTextureCache::GenerateTexturePages() {
    ev("GeneratePages", {lab(this)});
    return static_cast<int32_t>(ret("GeneratePages", 1));
}
void CycleFrequencyTable() { ev("CycleFrequencyTable"); }
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Push", {lab(this), level, code}); }

// ---- The test's entry points ----------------------------------------------------------------
extern "C" {

// Builds the height field case: see test_HFieldFrame.py (hf_flat) for the order.
void tpw_hf_begin(const int32_t* numbers) {
    Reader r{numbers};
    // The arena blocks live across cases.
    static bool made = false;
    if (!made) {
        made = true;
        for (int32_t i = 0; i < kMaxCs; i++) csObjects[i] = static_cast<CsTexture*>(tpw::ArenaAllocZeroed(sizeof(CsTexture)));
        cellBlock = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(kMaxCells * sizeof(sHFieldQuad)));
    }
    gei_System.mFlags = r.u();
    gei_TextureSettings.mFlags = r.u();
    gei_TextureSettings.mFormat = r.i();
    gei_Display.mFlags = r.u();
    gei_Display.mFirstPersonViewDistance = r.f();
    gei_Display.mF928 = gei_Display.mF92C = gei_Display.mF930 = gei_Display.mF934 = static_cast<int32_t>(0xa5a5a5a5);
    for (int32_t i = 0; i < 16; i++) (&view.m[0][0])[i] = r.f();
    std::memset(&record, 0xa5, sizeof record);
    for (int32_t i = 0; i < 4; i++) record.mF08[i] = r.f();
    record.mF18[0] = r.u();
    record.mF18[1] = r.u();
    std::memset(static_cast<void*>(&field), 0xa5, sizeof field);
    field.mF00 = &record;
    field.mF04 = textures;
    field.mInvCellX = r.f();
    field.mInvCellZ = r.f();
    field.mF234 = r.i();
    field.mF238 = r.i();
    field.mF23C = r.i();
    field.mF240 = r.i();
    field.mProcTextures = r.i() != 0 ? &handler : nullptr;
    field.mF1C = r.f();
    SGamutQuads* blocks[4] = {field.QuadRows(), field.VertexRows(),
                              reinterpret_cast<SGamutQuads*>(&gamut.mFC4), gamut.GetVertices()};
    std::memset(static_cast<void*>(&gamut), 0xa5, sizeof gamut);
    for (SGamutQuads* block : blocks) {
        block->mFirstRow = r.i();
        block->mEndRow = r.i();
        for (int32_t i = 0; i < 128; i++) block->mLow[i] = static_cast<int8_t>(r.i());
        for (int32_t i = 0; i < 128; i++) block->mHigh[i] = static_cast<int8_t>(r.i());
    }
    cellMargin = r.i();
    cellTotal = r.i();
    for (int32_t i = 0; i < cellTotal; i++) {
        cellBlock[i].mF00 = static_cast<uint16_t>(r.i());
        cellBlock[i].mF02 = static_cast<uint16_t>(r.i());
    }
    record.mQuads = cellBlock + cellMargin;
    std::memset(textures, 0xa5, sizeof textures);
    const int32_t textureCount = r.i();
    for (int32_t i = 0; i < textureCount; i++) {
        const int32_t which = r.i();
        textures[i].mF00 = static_cast<uint16_t>(r.i());
        textures[i].mF02 = static_cast<uint16_t>(r.i());
        textures[i].mTexture = which >= 0 ? csObjects[which] : nullptr;
    }
    std::memset(entries, 0xa5, sizeof entries);
    const int32_t csCount = r.i();
    for (int32_t i = 0; i < csCount; i++) {
        const int32_t which = r.i();
        std::memset(static_cast<void*>(csObjects[i]), 0xa5, sizeof(CsTexture));
        csFrames[i][0] = which >= 0 ? &entries[which] : nullptr;
        csFrames[i][1] = &entries[0];
        csObjects[i]->mEntries = csFrames[i];
    }
    const int32_t entryCount = r.i();
    for (int32_t i = 0; i < entryCount; i++) {
        entries[i].mWidth = static_cast<int16_t>(r.i());
        entries[i].mHeight = static_cast<int16_t>(r.i());
    }
    script.mode = r.i();
    script.first = r.i();
    script.end = r.i();
    for (int32_t i = 0; i < 128; i++) script.low[i] = static_cast<int8_t>(r.i());
    for (int32_t i = 0; i < 128; i++) script.high[i] = static_cast<int8_t>(r.i());
    script.setFar = r.i();
    script.farValue = r.f();
    std::memset(static_cast<void*>(gTileOverlays), 0xa5, sizeof gTileOverlays);
    Take();
    tpw_cl_label(kLabField, &field);
    tpw_cl_label(kLabGamut, &gamut);
    tpw_cl_label(kLabCache, &cache);
    tpw_cl_label(kLabHandler, &handler);
    tpw_cl_label(kLabProjection, &gei_ProjectionMatrix);
    tpw_cl_label(kLabView, &view);
    for (int32_t i = 0; i < kMaxEntries; i++) tpw_cl_label(kLabFirstEntry + i, &entries[i]);
}

// Runs one operation: 1 Prepare, 2 DivideAndConquer, 3 GetVertices (answers its offset in the gamut).
int32_t tpw_hf_run(int32_t op) {
    switch (op) {
    case 1:
        return field.Prepare();
    case 2:
        return field.DivideAndConquer();
    case 3:
        return static_cast<int32_t>(reinterpret_cast<char*>(gamut.GetVertices()) - reinterpret_cast<char*>(&gamut));
    }
    return -99;
}

// The state after the call: `small` numbers, then the bulk words. Returns the total.
int32_t tpw_hf_state(int32_t* out, int32_t* smallCount) {
    int32_t n = 0;
    out[n++] = static_cast<int32_t>(fbits(field.mF1C));
    SGamutQuads* blocks[2] = {field.VertexRows(), field.QuadRows()};
    for (SGamutQuads* block : blocks) {
        out[n++] = block->mFirstRow;
        out[n++] = block->mEndRow;
        for (int32_t i = 0; i < 128; i++) out[n++] = block->mLow[i];
        for (int32_t i = 0; i < 128; i++) out[n++] = block->mHigh[i];
    }
    out[n++] = field.mF1296C;
    out[n++] = gei_Display.mF928;
    out[n++] = gei_Display.mF92C;
    out[n++] = gei_Display.mF930;
    out[n++] = gei_Display.mF934;
    out[n++] = static_cast<int32_t>(gei_TextureSettings.mFlags);
    // Nothing else changed: the field outside the members the functions write, and the rest.
    const size_t quads = offsetof(CHField, mF128) + sizeof(SGamutQuads);
    out[n++] = Same(before.field, static_cast<void*>(&field), sizeof field,
                    {{offsetof(CHField, mF1C), offsetof(CHField, mF1C) + 4},
                     {offsetof(CHField, mF20), quads},
                     {offsetof(CHField, mPrepareEntries), offsetof(CHField, mPrepareEntries) + sizeof field.mPrepareEntries},
                     {offsetof(CHField, mF1296C), offsetof(CHField, mF1296C) + 4}});
    out[n++] = Same(before.record, &record, sizeof record, {});
    out[n++] = Same(before.textures, textures, sizeof textures, {});
    int32_t csSame = 1;
    for (int32_t i = 0; i < kMaxCs; i++) csSame &= Same(before.cs[i], static_cast<void*>(csObjects[i]), sizeof(CsTexture), {});
    out[n++] = csSame;
    out[n++] = Same(before.frames, csFrames, sizeof csFrames, {});
    out[n++] = Same(before.entries, static_cast<void*>(entries), sizeof entries, {});
    out[n++] = Same(before.gamut, static_cast<void*>(&gamut), sizeof gamut, {});
    out[n++] = Same(before.view, &view, sizeof view, {});
    out[n++] = Same(before.system, static_cast<void*>(&gei_System), sizeof gei_System, {});
    out[n++] = Same(before.display, static_cast<void*>(&gei_Display), sizeof gei_Display,
                    {{offsetof(CDisplay, mF928), offsetof(CDisplay, mF934) + 4}});
    out[n++] = Same(before.settings, &gei_TextureSettings, sizeof gei_TextureSettings, {{0, 4}});
    *smallCount = n;
    for (int32_t i = 0; i < CHField::kMaxOverlays; i++) out[n++] = static_cast<int32_t>(field.mPrepareEntries[i].mTile);
    for (int32_t i = 0; i < CHField::kMaxOverlays; i++) {
        const sTileOverlay& o = gTileOverlays[i];
        out[n++] = lab(o.mData);
        out[n++] = o.mWidth;
        out[n++] = o.mHeight;
        out[n++] = o.mFormat;
        out[n++] = static_cast<int32_t>(o.mFlags);
    }
    for (int32_t i = 0; i < cellTotal; i++) {
        out[n++] = cellBlock[i].mF00;
        out[n++] = cellBlock[i].mF02;
    }
    return n;
}

}  // extern "C"

// ---- The texture cache part ---------------------------------------------------------------

namespace {

// A page's texture: the unit only clears a flag of it.
struct FakeTexture : CTextureBase {
    int32_t Create(int32_t, int32_t, int32_t, int32_t) override { return 0; }
    int32_t Destroy() override { return 0; }
    void GetTexture() override {}
    int32_t GetPixelCount() override { return 0; }
    int32_t LockSurface() override { return 0; }
    int32_t UnlockSurface() override { return 0; }
    int32_t MipmapsUsed() override { return 0; }
    int32_t Download(CTextureEntry*, int32_t, int32_t) override { return 0; }
    double TestUploadSpeed() override { return 0.0; }
};

FakeTexture pageTextures[kMaxPages];
CTextureLink pageLinks[kMaxPages];
STexturePlace places[kMaxPlaces];
int32_t pageCount, entryCount, placeCount;

struct CacheSnapshot {
    unsigned char cache[sizeof(CTextureCache)];
    unsigned char links[sizeof pageLinks];
    unsigned char entries[sizeof entries];
    unsigned char system[sizeof(CSystem)];
    unsigned char settings[sizeof(CTextureSettings)];
    unsigned char display[sizeof(CDisplay)];
} cacheBefore;

}  // namespace

extern "C" {

// Builds the cache case: system flags, texture settings flags, the frequency index, the cache's
// flags, the head of the waiting list (an entry index or -1), the page count of each size,
// then for each page its texture's flags and the head of its entries, the entries (flags, next,
// place, priority bits) and the place records (16 bytes, then two numbers).
void tpw_tc_begin(const int32_t* numbers) {
    Reader r{numbers};
    gei_System.mFlags = r.u();
    gei_TextureSettings.mFlags = r.u();
    gFrequencyIndex = r.u();
    std::memset(static_cast<void*>(&cache), 0xa5, sizeof cache);
    cache.mFlags = r.u();
    const int32_t waiting = r.i();
    pageCount = 0;
    int32_t heads[kMaxPages];
    for (int32_t size = 0; size < CTextureCache::kSizes; size++) {
        cache.mPageCount[size] = r.i();
        for (int32_t page = 0; page < cache.mPageCount[size]; page++) {
            pageTextures[pageCount].mFlags = r.u();
            heads[pageCount] = r.i();
            pageLinks[pageCount] = CTextureLink();
            pageLinks[pageCount].mTexture = &pageTextures[pageCount];
            cache.mPages[size][page] = &pageLinks[pageCount];
            pageCount++;
        }
    }
    std::memset(static_cast<void*>(entries), 0xa5, sizeof entries);
    entryCount = r.i();
    int32_t nexts[kMaxEntries];
    for (int32_t i = 0; i < entryCount; i++) {
        entries[i].mFlags = r.u();
        nexts[i] = r.i();
        const int32_t place = r.i();
        entries[i].mPlace = &places[place];
        entries[i].mPriority = r.f();
    }
    for (int32_t i = 0; i < entryCount; i++) entries[i].mNext = nexts[i] >= 0 ? &entries[nexts[i]] : nullptr;
    for (int32_t i = 0; i < pageCount; i++) pageLinks[i].mEntries = heads[i] >= 0 ? &entries[heads[i]] : nullptr;
    cache.mF18AC = waiting >= 0 ? &entries[waiting] : nullptr;
    placeCount = r.i();
    for (int32_t i = 0; i < placeCount; i++) {
        for (int32_t b = 0; b < 16; b++) places[i].mF00[b] = static_cast<uint8_t>(r.i());
        places[i].mF10 = r.i();
        places[i].mF14 = r.i();
    }
    std::memcpy(cacheBefore.cache, static_cast<void*>(&cache), sizeof cache);
    std::memcpy(cacheBefore.links, static_cast<void*>(pageLinks), sizeof pageLinks);
    std::memcpy(cacheBefore.entries, static_cast<void*>(entries), sizeof entries);
    std::memcpy(cacheBefore.system, static_cast<void*>(&gei_System), sizeof gei_System);
    std::memcpy(cacheBefore.settings, &gei_TextureSettings, sizeof gei_TextureSettings);
    std::memcpy(cacheBefore.display, static_cast<void*>(&gei_Display), sizeof gei_Display);
    tpw_cl_label(kLabCache, &cache);
    tpw_cl_label(kLabErrors, &gei_ErrorHandler);
    for (int32_t i = 0; i < kMaxEntries; i++) tpw_cl_label(kLabFirstEntry + i, &entries[i]);
}

// Runs one operation: 1 Refresh, 2 Execute.
int32_t tpw_tc_run(int32_t op) {
    gei_TCache = &cache;
    return op == 1 ? cache.Refresh() : cache.Execute();
}

// The state after the call, as the order the test reads the original's.
int32_t tpw_tc_state(int32_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int32_t>(cache.mFlags);
    out[n++] = cache.mF18AC == nullptr ? 0 : 1 + static_cast<int32_t>(cache.mF18AC - entries);
    out[n++] = static_cast<int32_t>(gFrequencyIndex);
    for (int32_t i = 0; i < pageCount; i++) out[n++] = static_cast<int32_t>(pageTextures[i].mFlags);
    for (int32_t i = 0; i < entryCount; i++) {
        out[n++] = static_cast<int32_t>(entries[i].mFlags);
        out[n++] = static_cast<int32_t>(fbits(entries[i].mPriority));
        out[n++] = entries[i].mNext == nullptr ? 0 : 1 + static_cast<int32_t>(entries[i].mNext - entries);
    }
    for (int32_t i = 0; i < placeCount; i++) {
        for (int32_t b = 0; b < 16; b++) out[n++] = places[i].mF00[b];
        out[n++] = places[i].mF10;
        out[n++] = places[i].mF14;
    }
    std::vector<Range> entryRanges;
    for (int32_t i = 0; i < kMaxEntries; i++) {
        const size_t base = i * sizeof(CTextureEntry);
        entryRanges.push_back({base + offsetof(CTextureEntry, mFlags), base + offsetof(CTextureEntry, mFlags) + 4});
        entryRanges.push_back({base + offsetof(CTextureEntry, mPriority), base + offsetof(CTextureEntry, mPriority) + 4});
    }
    out[n++] = Same(cacheBefore.cache, static_cast<void*>(&cache), sizeof cache,
                    {{offsetof(CTextureCache, mF18AC), offsetof(CTextureCache, mF18AC) + sizeof cache.mF18AC},
                     {offsetof(CTextureCache, mFlags), offsetof(CTextureCache, mFlags) + 4}});
    out[n++] = Same(cacheBefore.links, static_cast<void*>(pageLinks), sizeof pageLinks, {});
    out[n++] = Same(cacheBefore.entries, static_cast<void*>(entries), sizeof entries, entryRanges);
    out[n++] = Same(cacheBefore.system, static_cast<void*>(&gei_System), sizeof gei_System, {});
    out[n++] = Same(cacheBefore.settings, &gei_TextureSettings, sizeof gei_TextureSettings, {});
    out[n++] = Same(cacheBefore.display, static_cast<void*>(&gei_Display), sizeof gei_Display, {});
    return n;
}

}  // extern "C"
