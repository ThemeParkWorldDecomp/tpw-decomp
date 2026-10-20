// Test library for the 2D map's sprite list and map picture functions (unit v5959; libtpw_unit_CUI2DMap_UnRegister.dylib,
// tests/test_CUI2DMap_UnRegister.py), main app:
//   src/CUI2DMap_CreateMapSprites_CUI2DMap_MoveCentre_CUI2DMap_UpdateSpritePosition_CUI2DMap_UnRegister.cpp
//   UnRegister, DeleteSpriteListEntries, UpdateAllSpritePositions, CloseSprites, Zoom, LoadMap, UpdateMap.
//
// The callees outside the unit (UpdateSpritePosition, UpdateMapZoom, UpdateAllDrawRelated, the EZSprite and map zoom engine calls, the
// string, loader and render area classes) are recording fakes. The list functions of TbTList run for real on both sides. The globals of
// other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"

using namespace calllog;

typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> SpriteList;

// ---- globals of other files
alignas(16) uint8_t gUI2DMapTgaName[0x40];
CEZSprite gei_EZSprite;
CMapZoom gei_MapZoom;
CTextureSettings gei_TextureSettings;

namespace {
char* gThemePath = nullptr;
char gStr[512];

void Append(const char* text) {
    size_t at = std::strlen(gStr);
    size_t n = std::strlen(text);
    if (at + n < sizeof gStr) std::memcpy(gStr + at, text, n + 1);
}
}  // namespace

// ---- the sprite's destructor (the Mac's virtual deleting one, called with flag 1)
// (the delete itself is logged by the call log as "delete" of a labelled pointer, as the PowerPC recorder's D does)
CUI2DMapSprite::~CUI2DMapSprite() {}

// ---- the map's untranslated members
void CUI2DMap::UpdateSpritePosition(CUI2DMapSprite* sprite) { ev("UpdateSpritePosition", {lab(this), lab(sprite)}); }
void CUI2DMap::UpdateMapZoom() { ev("UpdateMapZoom", {lab(this)}); }
void CUI2DMap::UpdateAllDrawRelated() { ev("UpdateAllDrawRelated", {lab(this)}); }

// ---- the engine's sprite object and map zoom
CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}
bool CEZSprite::DeleteSprite(void* sprite) {
    ev("DeleteSprite", {lab(this), static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(sprite)))});
    return false;
}
int32_t CEZSprite::Disable() {
    ev("Disable", {lab(this)});
    return 0;
}
int32_t CEZSprite::Close() {
    ev("Close", {lab(this)});
    return 0;
}
CMapZoom::CMapZoom() {}
CMapZoom::~CMapZoom() {}
void CMapZoom::Update(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { ev("MapZoomUpdate", {lab(this), x0, y0, x1, y1}); }
CTextureSettings::CTextureSettings() {}

char* get_theme_path() {
    ev("GetThemePath", {});
    return gThemePath;
}

// ---- the strings
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    ev("StrCtor", {newlab(this)}, {text});
    std::memset(gStr, 0, sizeof gStr);
    Append(text);
    mData = gStr;
}
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("StrDtor", {lab(this), -1});
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char& ch) {
    ev("StrAddChar", {lab(this)});
    char text[2] = {ch, 0};
    Append(text);
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>& other) {
    ev("StrAddStr", {lab(this), lab(&other)});
    Append(other.mData);
    return *this;
}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }

// ---- the loaders and render areas
TbTGALoader::TbTGALoader() { ev("TgaCtor", {newlab(this)}); }
TbTGALoader::~TbTGALoader() { ev("TgaDtor", {lab(this), -1}); }
TbRenderArea::TbRenderArea() { ev("AreaCtor", {newlab(this)}); }
TbResult TbRenderArea::AllocMemory() {
    ev("AllocArea", {lab(this)});
    return TbResult{0};
}
void TbRenderArea::FreeMemory() { ev("FreeArea", {lab(this)}); }
TbResult TbGraphicFileLoader::ReadHeader(const char* path, const char* second, TbRenderArea* area) {
    ev("ReadHeader", {lab(this), lab(second), lab(area)}, {path});
    area->mWidth = static_cast<int32_t>(ret("ReadW"));
    area->mPictureHeight = static_cast<int32_t>(ret("ReadH"));
    return TbResult{0};
}
TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other) {
    ev("Load", {lab(this), lab(&area), lab(second), lab(other)}, {path});
    return TbResult{0};
}
TbResult TbRAConv::Move(TbRenderArea& destination, TbRenderArea& source, TbPalette* palette) {
    ev("Move", {lab(this), lab(&destination), lab(&source), lab(palette)});
    return TbResult{0};
}
TbRAConv::~TbRAConv() { ev("ConvDtor", {lab(this), -1}); }

extern "C" {

uint64_t tpw_c2m_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"ez", &gei_EZSprite},
        {"mz", &gei_MapZoom},
        {"ts", &gei_TextureSettings},
        {"tga", gUI2DMapTgaName},
        {"sep", &gUI2DMapSeparator},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_c2m_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_c2m_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CTextureSettings, mPixelFormat));
    return n;
}

// A sprite the list functions may delete: plain operator new memory, zeroed.
uint64_t tpw_c2m_newsprite() {
    void* memory = ::operator new(sizeof(CUI2DMapSprite));
    std::memset(memory, 0, sizeof(CUI2DMapSprite));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memory));
}

uint64_t tpw_c2m_newlist() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(new SpriteList())); }

void tpw_c2m_addtail(uint64_t list, uint64_t data) {
    reinterpret_cast<SpriteList*>(list)->AddTail(reinterpret_cast<CUI2DMapSprite*>(data));
}

// out: the count, the head's data, the tail's data, then per node its data and its previous node's data (0xdead: none).
int32_t tpw_c2m_dump(uint64_t listAddress, uint64_t* out, int32_t capacity) {
    SpriteList* list = reinterpret_cast<SpriteList*>(listAddress);
    int32_t n = 0;
    out[n++] = static_cast<uint64_t>(list->mCount);
    out[n++] = list->mHead ? reinterpret_cast<uint64_t>(list->mHead->mData) : 0xdead;
    out[n++] = list->mTail ? reinterpret_cast<uint64_t>(list->mTail->mData) : 0xdead;
    for (SpriteList::CNode* node = list->mHead; node != nullptr && n + 2 <= capacity; node = node->mNext) {
        out[n++] = reinterpret_cast<uint64_t>(node->mData);
        out[n++] = node->mPrev ? reinterpret_cast<uint64_t>(node->mPrev->mData) : 0xdead;
    }
    return n;
}

void tpw_c2m_setptrs(uint64_t theme, uint64_t tga) {
    gThemePath = reinterpret_cast<char*>(theme);
    reinterpret_cast<TbStringBase<char>*>(gUI2DMapTgaName)->mData = reinterpret_cast<char*>(tga);
}

void tpw_c2m_unregister(uint64_t map, uint64_t sprite) {
    reinterpret_cast<CUI2DMap*>(map)->UnRegister(reinterpret_cast<CUI2DMapSprite*>(sprite));
}
void tpw_c2m_delete_entries(uint64_t map) { reinterpret_cast<CUI2DMap*>(map)->DeleteSpriteListEntries(); }
void tpw_c2m_update_all(uint64_t map) { reinterpret_cast<CUI2DMap*>(map)->UpdateAllSpritePositions(); }
void tpw_c2m_close(uint64_t map) { reinterpret_cast<CUI2DMap*>(map)->CloseSprites(); }
void tpw_c2m_zoom(uint64_t map, int32_t zoom, float amount) {
    reinterpret_cast<CUI2DMap*>(map)->Zoom(static_cast<EUI2DMapZoom>(zoom), amount);
}
void tpw_c2m_load(uint64_t map, int32_t reload) { reinterpret_cast<CUI2DMap*>(map)->LoadMap(reload); }
void tpw_c2m_update_map(uint64_t map) { reinterpret_cast<CUI2DMap*>(map)->UpdateMap(); }

}  // extern "C"
