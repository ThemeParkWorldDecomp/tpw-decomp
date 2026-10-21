// Test library for nine functions of the main app (unit v51047; libtpw_unit_std_basic_istream_basic_istream.dylib,
// tests/test_std_basic_istream_basic_istream.py), main app:
//   src/game/save/FS_LoadSave.cpp, src/game/ui/InterfaceKnob.cpp, src/game/ui/UIGrapher.cpp, src/game/ui/ui_2dmap.cpp   std::basic_istream<char>::basic_istream, CUIGrapher::Clear and constructor,
//   CFSLoadSave::GetNextSaveFileDetails, CUI2DMapSprite destructor, SetVisibility, SetScale, entry, InterfaceKnob::operator-.
//
// The callees outside the unit are recording fakes (std::ios_base::init, basic_ios::widen, LbDraw_RectangleFilled, TPUniStrEq,
// CEZSprite::GetSpriteDetails and DeleteSpriteInstance, operator delete of a sprite, the runtime start-up, main, exit, the knob's
// WhatAmI). TbTList::Find and RemoveAt run for real on both sides. Globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_LoadSave.h"
#include "FinanceGraphPanelMsg.h"
#include "TPMain.h"
#include "TPString.h"
#include "TbTList.h"
#include "UI_Base.h"
#include "email.h"
#include "tb_ezsprite.h"
#include "tb_starfield.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
char gOstreamVt1[0x28];
char gIosBaseVt[0x20];
char gIfstreamVtblC[0x20];
CUI2DMap gUI2DMap;
CEZSprite gei_EZSprite;

namespace {

uint32_t g_base;
void* g_details;
void* g_objs[2];
void* g_elems[16];
int g_nelems;
void* g_prev;

int64_t I(int64_t v) { return v; }
uint32_t T32(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

int64_t ElemIndex(const void* p) {
    for (int i = 0; i < g_nelems; i++)
        if (g_elems[i] == p) return i;
    if (p == g_prev) return 100;
    return -1;
}

using SpriteList = TbTList<CUI2DMapSprite*, CUI2DMapSprite*>;

int32_t FakeNop(void*) { return 0; }
int32_t FakeWhatAmI(void* self) {
    int idx = self == g_objs[0] ? 0 : 1;
    ev("WhatAmI", {I(idx)});
    return static_cast<int32_t>(ret("WhatAmI", idx, 0));
}

}  // namespace

TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}

// ---- the fakes
void OstreamIosInit(MacOstreamIos* ios, uint32_t streambuf) {
    ev("Init", {I(static_cast<uint32_t>(T32(ios) - g_base)), I(streambuf)});
}
char OstreamWiden(MacOstreamIos* ios, char c) {
    ev("Widen", {I(static_cast<uint32_t>(T32(ios) - g_base)), I(static_cast<uint8_t>(c))});
    return static_cast<char>(ret("Widen"));
}
void LbDraw_RectangleFilled(const TbRect* rect, TbColour colour) {
    ev("Rect", {I(rect->left), I(rect->top), I(rect->right), I(rect->bottom),
                I((static_cast<uint32_t>(colour.mAlpha) << 24) | (static_cast<uint32_t>(colour.mRed) << 16) |
                  (static_cast<uint32_t>(colour.mGreen) << 8) | colour.mBlue),
                I(colour.mIndex)});
}
bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>& a, const TbDynamicStringTemplate<char16_t>& b) {
    ev("TPUniStrEq", {ElemIndex(&a), ElemIndex(&b)});
    return ret("TPUniStrEq") != 0;
}
CEZSprite::CEZSprite() {}
CEZSprite::~CEZSprite() {}
bool CEZSprite::DeleteSpriteInstance(void* instance) {
    ev("DeleteSpriteInstance", {I(T32(instance))});
    return true;
}
void* CEZSprite::GetSpriteDetails(void* instance) {
    ev("GetSpriteDetails", {I(T32(instance))});
    return g_details;
}
void UI2DMapDelete(void*) { ev("Delete", {I(1)}); }
void RuntimeInit() { ev("RuntimeInit", {}); }
void RuntimeExit(int32_t code) { ev("exit", {I(static_cast<uint32_t>(code))}); }
int ThemeParkWorldMain() {
    ev("main", {});
    return 0;
}

extern "C" {

uint64_t tpw_isg_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"OstreamVt1", gOstreamVt1}, {"IosBaseVt", gIosBaseVt}, {"IfstreamVt", gIfstreamVtblC},
        {"SpriteVt", gCUI2DMapSpriteVt}, {"UI2DMap", &gUI2DMap},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_isg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena's base: a pointer word inside a block is the address minus this (Ptr32).
uint64_t tpw_isg_arena_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_isg_layout(int64_t* out) {
    int32_t n = 0;
    CUIGrapher* g = reinterpret_cast<CUIGrapher*>(0x1000);
    CUI2DMapSprite* s = reinterpret_cast<CUI2DMapSprite*>(0x1000);
    InterfaceKnob* k = reinterpret_cast<InterfaceKnob*>(0x1000);
    out[n++] = static_cast<int64_t>(sizeof(CUIGrapher));
    out[n++] = reinterpret_cast<char*>(&g->mGrid) - reinterpret_cast<char*>(g);
    out[n++] = reinterpret_cast<char*>(&g->mBG) - reinterpret_cast<char*>(g);
    out[n++] = static_cast<int64_t>(sizeof(CUI2DMapSprite));
    out[n++] = reinterpret_cast<char*>(&s->mSprite) - reinterpret_cast<char*>(s);
    out[n++] = reinterpret_cast<char*>(&s->mVisible) - reinterpret_cast<char*>(s);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceKnob));
    out[n++] = reinterpret_cast<char*>(&k->mValue) - reinterpret_cast<char*>(k);
    // SetScale's detail block (the sprite instance) and its sprite: the sprite pointer, the scale, the image's width
    out[n++] = static_cast<int64_t>(offsetof(SSpriteGameData, mSprite));
    out[n++] = static_cast<int64_t>(offsetof(SSpriteGameData, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(SSpriteEngineData, mWidth));
    return n;
}

void tpw_isg_istream(uint64_t self, int32_t flag, uint32_t streambuf) {
    g_base = T32(reinterpret_cast<void*>(self));
    ifstream_char_callee::IstreamCtor(reinterpret_cast<MacIfstream*>(self), flag, streambuf);
}

void tpw_isg_clear(uint64_t self) { reinterpret_cast<CUIGrapher*>(self)->Clear(); }
void tpw_isg_ctor(uint64_t self) { CUIGrapher_Construct(reinterpret_cast<CUIGrapher*>(self)); }

uint64_t tpw_isg_gnsfd(const uint64_t* elems, int32_t n, uint64_t prev) {
    g_nelems = n;
    for (int i = 0; i < n; i++) g_elems[i] = reinterpret_cast<void*>(elems[i]);
    g_prev = reinterpret_cast<void*>(prev);
    CFSLoadSave* list = static_cast<CFSLoadSave*>(tpw::ArenaAllocZeroed(256));
    auto* nodes = static_cast<CFSLoadSave::CNode*>(tpw::ArenaAllocZeroed(sizeof(CFSLoadSave::CNode) * (n + 1)));
    for (int i = 0; i < n; i++) {
        nodes[i].mData = reinterpret_cast<TFSLoadSaveSaveFileDetails*>(elems[i]);
        nodes[i].mNext = i + 1 < n ? &nodes[i + 1] : nullptr;
        nodes[i].mPrev = i > 0 ? &nodes[i - 1] : nullptr;
    }
    list->mHead = n > 0 ? &nodes[0] : nullptr;
    list->mTail = n > 0 ? &nodes[n - 1] : nullptr;
    list->mCount = n;
    TFSLoadSaveSaveFileDetails* answer = list->GetNextSaveFileDetails(reinterpret_cast<const TFSLoadSaveSaveFileDetails*>(prev));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(answer));
}

// The sprite list at gUI2DMap + 0x20080: real nodes (RemoveAt deletes them).
void tpw_isg_setlist(const uint64_t* sprites, int32_t n) {
    SpriteList* list = static_cast<SpriteList*>(tpw::ArenaAllocZeroed(sizeof(SpriteList) + 16));
    SpriteList::CNode* prev = nullptr;
    for (int i = 0; i < n; i++) {
        SpriteList::CNode* node = new SpriteList::CNode(reinterpret_cast<CUI2DMapSprite*>(sprites[i]));
        node->mPrev = prev;
        if (prev != nullptr)
            prev->mNext = node;
        else
            list->mHead = node;
        prev = node;
    }
    list->mTail = prev;
    list->mCount = n;
    *reinterpret_cast<SpriteList**>(gUI2DMap.mBytes + 0x20080) = list;
}

// out: count, head is NULL, tail is the last node, n, then the data pointers in order.
int32_t tpw_isg_getlist(uint64_t* out, int32_t cap) {
    SpriteList* list = *reinterpret_cast<SpriteList**>(gUI2DMap.mBytes + 0x20080);
    SpriteList::CNode* node = list->mHead;
    SpriteList::CNode* last = nullptr;
    int32_t n = 0;
    while (node != nullptr && n < cap) {
        out[4 + n++] = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(node->mData));
        last = node;
        node = node->mNext;
    }
    out[0] = static_cast<uint64_t>(list->mCount);
    out[1] = list->mHead == nullptr ? 1 : 0;
    out[2] = last == list->mTail ? 1 : 0;
    out[3] = static_cast<uint64_t>(n);
    return n;
}

void tpw_isg_set_details(uint64_t details) { g_details = reinterpret_cast<void*>(details); }

uint64_t tpw_isg_sdtor(uint64_t sprite, int32_t flags) {
    return static_cast<uint64_t>(
        reinterpret_cast<uintptr_t>(CUI2DMapSpriteDtor(reinterpret_cast<CUI2DMapSprite*>(sprite), static_cast<int16_t>(flags))));
}
void tpw_isg_setvis(uint64_t sprite, int32_t visible) { reinterpret_cast<CUI2DMapSprite*>(sprite)->SetVisibility(visible); }
void tpw_isg_setscale(uint64_t sprite, float scale) { reinterpret_cast<CUI2DMapSprite*>(sprite)->SetScale(scale); }

void tpw_isg_entry() { entry(); }

int32_t tpw_isg_knob_sub(uint64_t a, uint64_t b) {
    static void* vt[64];
    auto member = &InterfaceWindow::WhatAmI;
    uintptr_t pair[2];
    std::memcpy(pair, &member, sizeof(pair));
    size_t slot = pair[0] / sizeof(void*);
    for (size_t i = 0; i < 64; i++) vt[i] = reinterpret_cast<void*>(&FakeNop);
    vt[slot] = reinterpret_cast<void*>(&FakeWhatAmI);
    *reinterpret_cast<void***>(a) = vt;
    *reinterpret_cast<void***>(b) = vt;
    g_objs[0] = reinterpret_cast<void*>(a);
    g_objs[1] = reinterpret_cast<void*>(b);
    InterfaceKnob* knob = reinterpret_cast<InterfaceKnob*>(a);
    return knob->InterfaceKnob::operator-(*reinterpret_cast<InterfaceWindow*>(b));
}

}  // extern "C"
