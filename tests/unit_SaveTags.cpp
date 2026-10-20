// Test library for the tag save, load and add functions (unit v5001; libtpw_unit_SaveTags.dylib, tests/test_SaveTags.py), main app:
//   src/ui_StatusPanel.cpp   AddATag 0x10191ef8, SaveTags 0x1019231c, LoadTags 0x10192704 and the globals gMaxTags, gTagCount,
//                            gTagFirstWindow
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs): the file
// functions, OverrideTag, ShowTagPanel, the window members, UI_PostMessage and SOUND_SendEvent. A window is a zeroed block whose
// word at +0x600 is its serial (the log names a window by it); the block at +0x400 is the text buffer getExtraParam(2) may answer.
// The globals of other files the unit reads (gGameWindow, gAerialWindow, gSoundBanks) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_main.h"

using namespace calllog;

InterfaceWindow* gGameWindow;     // 0x103949e4
InterfaceWindow* gAerialWindow;   // 0x103949d4
int32_t gSoundBanks[8];           // 0x102f7c94
sStaticTag gStaticTags[10];        // 0x1038cd84 (src/ui_StatusPanel.cpp)

static_assert(sizeof(InterfaceWindow) <= 0x400, "window block");

namespace {

std::vector<uint8_t> gFileData;
size_t gFilePos = 0;

int64_t I(int64_t v) { return v; }

int64_t Wid(const void* window) {
    if (window == nullptr) return 0;
    int32_t serial;
    std::memcpy(&serial, static_cast<const char*>(window) + 0x600, 4);
    return serial;
}

InterfaceWindow* WindowOf(int32_t word) {
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(static_cast<uint32_t>(word)));
}

int64_t Cookie(const void* file) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(file) & 0xffffffffu); }

}  // namespace

// ---- what the other functions of ui_StatusPanel.cpp (Init, DeInit, Create, Destroy) name and this unit never calls: link-only stand-ins
int32_t TagCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t TagDlgCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
int32_t UI_SendMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindow* gUIRootWindow;
int16_t gTagPanelScript[137];
// The members and constructors (a constructor here would need the vtable) as bare symbols that return at once.
#define LINK_ONLY(sym) asm(".globl _" sym "\n.p2align 2\n_" sym ":\n\tret\n");
LINK_ONLY("_ZN15InterfaceWindowC1EPS_iissss")
LINK_ONLY("_ZN15InterfaceWindowC2EPS_iissss")
LINK_ONLY("_ZN25InterfaceTextObjectSpriteC1EPv")
LINK_ONLY("_ZN25InterfaceTextObjectSpriteC2EPv")
LINK_ONLY("_ZN15InterfaceWindow5BuildEPsPFiPS_iiiE")
LINK_ONLY("_ZN15InterfaceWindow6unlinkEv")
LINK_ONLY("_ZN15InterfaceWindow4ShowEs")
LINK_ONLY("_ZN15InterfaceWindow9GetWindowEi")
LINK_ONLY("_ZN15InterfaceWindow19SetClientDrawObjectEP19InterfaceDrawObject")
LINK_ONLY("_ZN15InterfaceWindow13SetDrawObjectEP19InterfaceDrawObject")
LINK_ONLY("_ZN15InterfaceWindow7SetFontEP13InterfaceFont")
LINK_ONLY("_ZN15InterfaceWindow11SetFontWrapEii")
LINK_ONLY("_ZN15InterfaceWindow11SetFontJustEii")
LINK_ONLY("_ZN15InterfaceWindow19SetCallbackFunctionEPFiPS_iiiE")
LINK_ONLY("_Z17GetFontTableEntryj")
LINK_ONLY("_ZN17DrawObjectLibrary14FindDrawObjectEPc")

// ---- the fakes
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {Cookie(file), I(length)});
    uint8_t* out = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < length; ++i) out[i] = gFilePos + i < gFileData.size() ? gFileData[gFilePos + i] : 0;
    gFilePos += length;
    (void)transferred;
    return TbError(0);
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    (void)transferred;
    const uint8_t* b = static_cast<const uint8_t*>(buffer);
    if (length == 4) {
        uint32_t w;
        std::memcpy(&w, b, 4);
        ev("LbFile_Write", {Cookie(file), I(4), I(w)});
        return TbError(0);
    }
    auto u32 = [&](size_t o) { uint32_t w; std::memcpy(&w, b + o, 4); return I(w); };
    auto u16 = [&](size_t o) { uint16_t w; std::memcpy(&w, b + o, 2); return I(w); };
    uint32_t h = 7;
    int64_t n = 0;
    while (n < 256 && u16(4 + 2 * n) != 0) {
        h = h * 31u + static_cast<uint32_t>(u16(4 + 2 * n));
        ++n;
    }
    ev("LbFile_Write", {Cookie(file), I(length), u32(0), n, I(h), u32(0x204), u16(0x208), u32(0x20c), u32(0x210), u16(0x214), u32(0x218)});
    return TbError(0);
}

bool OverrideTag(CTagParams& p) {
    ev("OverrideTag", {I(p.mId), I(p.mF4), I(p.mText != nullptr), I(p.mTime), I(p.mFrame), I(p.mF14), I(p.mF18), I(p.mF1C), I(p.mF20),
                       I(p.mShow)});
    return ret("override") != 0;
}

void ShowTagPanel(InterfaceWindow* window) { ev("ShowTagPanel", {Wid(window)}); }

int32_t InterfaceWindow::GetDims(int16_t* width, int16_t* height, int16_t adjust) {
    ev("GetDims", {Wid(this), I(width != nullptr), I(height != nullptr), I(adjust)});
    return 0;
}

void InterfaceWindow::SetFrame(int32_t frame) { ev("SetFrame", {Wid(this), I(frame)}); }

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {Wid(this), I(index)});
    static const char* const keys[] = {"gep0", "gep1", "gep2", "gep3", "gep4", "gep5", "gep6", "gep7", "gep8"};
    int64_t v = ret(keys[index]);
    if (index == 2) return v == 1 ? static_cast<char*>(static_cast<void*>(this)) + 0x400 : nullptr;
    return reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(v)));
}

int32_t InterfaceWindow::setExtraParam(int16_t index, void* value) {
    ev("setExtraParam", {Wid(this), I(index), I(static_cast<int32_t>(reinterpret_cast<intptr_t>(value)))});
    return 0;
}

int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {Wid(this), I(id), I(ms)});
    return 0;
}

int32_t InterfaceWindow::startAnimTimer(int32_t which) {
    ev("startAnimTimer", {Wid(this), I(which)});
    return 1;
}

int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    const void* target = nullptr;   // the 32-bit word names one of the static tags' windows
    for (int i = 0; i < 10 && a != 0; ++i)
        if (static_cast<int32_t>(reinterpret_cast<uintptr_t>(gStaticTags[i].mWindow)) == a) target = gStaticTags[i].mWindow;
    ev("UI_PostMessage", {Wid(window), I(message), Wid(target), I(b)});
    return 0;
}

int32_t SOUND_SendEvent(int32_t entity, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("SOUND_SendEvent", {I(entity), I(bank), I(event), I(a), I(b), I(c)});
    return 0;
}

extern "C" {

uint64_t tpw_st_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"game", &gGameWindow}, {"aerial", &gAerialWindow}, {"first", &gTagFirstWindow},
        {"count", &gTagCount},  {"max", &gMaxTags},         {"banks", gSoundBanks},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_st_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_st_tag(int32_t index) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gStaticTags[index])); }

int32_t tpw_st_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(sStaticTag, mActive));
    out[n++] = static_cast<int64_t>(offsetof(sStaticTag, mPayload));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mF134));
    return n;
}

void tpw_st_setfile(const uint8_t* data, uint32_t length) {
    gFileData.assign(data, data + length);
    gFilePos = 0;
}

uint64_t tpw_st_add(uint32_t id, int32_t f4, uint64_t text, int32_t time, int32_t frame, int32_t f14, int32_t f18, uint32_t f1c, int32_t f20,
                    uint32_t show) {
    CTagParams p;
    p.mId = id;
    p.mF4 = f4;
    p.mText = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text));
    p.mTime = time;
    p.mFrame = static_cast<int16_t>(frame);
    p.mF14 = f14;
    p.mF18 = f18;
    p.mF1C = static_cast<uint16_t>(f1c);
    p.mF20 = f20;
    p.mShow = static_cast<uint8_t>(show);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(AddATag(p)));
}

void tpw_st_save(uint32_t cookie) { SaveTags(reinterpret_cast<void*>(static_cast<uintptr_t>(cookie))); }

void tpw_st_load(uint32_t cookie) { LoadTags(reinterpret_cast<void*>(static_cast<uintptr_t>(cookie))); }

}  // extern "C"
