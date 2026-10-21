// Test library for the buttons menu underlay, the online park panels' static initialisers and four UIInterface functions (unit v51037;
// libtpw_unit_ButtonsUnderlayMsg_2.dylib, tests/test_ButtonsUnderlayMsg_2.py), main app:
//   src/game/ui/UIInterface.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp   the nine functions and the new globals of the two initialisers
//
// The callees outside the unit are fakes that record what the real ones read (window pointers as plain numbers: the test hands tokens,
// nothing is dereferenced); the globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "UIInterface.h"
#include "ui_main.h"
#include "lobby.h"
#include "World.h"
#include "Debug.h"
#include "MainWindowCallback.h"
#include "UI_Base.h"
#include "ui_publishParkPanel.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

using namespace calllog;

void LocatorByeByeKid(CThing* thing);
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);

// ---- globals of other files the unit reads or writes, defined here
int32_t gOnlineWord88;
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineWindowE8c;
InterfaceWindow* gOnlineWindowE90;
CFSOnlineSystem* gOnlineSystem;
CLBScreenSystem* gLobbyScreens;
InterfaceWindow* gOnlineParkListPanel;
VisitorStatusPanel gVisitorStatusPanel;
CThing* gStaffRoomPanelThing;
CThingArrayEntry gThingArray[kThingArraySize];
alignas(16) uint8_t gOnlineParkInfoParkStorage[512];
// StaffStatusPanel is an empty class on the host but the Mac reads a word at +0x0c: give the symbol real storage.
alignas(64) uint8_t gStaffStatusStorage[64] __asm__("_gStaffStatusPanel");

namespace {

int64_t P(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)); }

}  // namespace

// ---- the fakes
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {P(this), on});
    return 0;
}
void InterfaceWindow::unlink() { ev("unlink", {P(this)}); }
void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {P(this), on}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CDWH", {P(this), message, a, b});
    return static_cast<int32_t>(ret("CDWH"));
}
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {P(window), message, a, b});
    return 0;
}
void ShowStaffPanel(CThing* thing) { ev("ShowStaffPanel", {P(thing)}); }
void ShowKidLocatorPanel() { ev("ShowKidLocatorPanel", {}); }
void AllKidsPanel_KidGone(CThing* thing) { ev("AllKidsPanel_KidGone", {lab(thing)}); }
void LocatorByeByeKid(CThing* thing) { ev("LocatorByeByeKid", {lab(thing)}); }
void VisitorStatusPanel::KidGone(CThing* thing) { ev("KidGone", {lab(this), lab(thing)}); }
void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year) { ev("UI_MainPanelDateChange", {day, month, year}); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
CFSOnlineSystem::CFSOnlineSystem() { ev("CFSOnlineSystem", {newlab(this)}); }
void CFSOnlineSystem::StopProcessing() { ev("StopProcessing", {lab(this)}); }
void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {P(this)}); }
void UIMain_DestroyWString(void*) {}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("__register_global_object", {lab(object), lab(destructor), lab(node)});
}

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("TbDynamicString", {lab(this)}, {narrow(text).c_str()});
}
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}   // the vtable of the instance names it; never called
// The members of the instances that CFSOnlinePark's constructor and the vtables name: empty, never called (the strings the unit builds are
// raw storage that only the constructor fake touches).
#define TPW_FAKE_STRING(Char)                                                                                          \
    template <> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}                                        \
    template <> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t) {}                                              \
    template <> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {}                                           \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>&) {  \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(                               \
        const TbDynamicStringTemplate<Char>&) {                                                                        \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char*) {                \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char&) {                \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>&) { \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char&) {               \
        return *this;                                                                                                  \
    }                                                                                                                  \
    template <> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char*) {               \
        return *this;                                                                                                  \
    }
TPW_FAKE_STRING(char16_t)
TPW_FAKE_STRING(char)
template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate() {}
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate() {}
CFSOnlinePark::~CFSOnlinePark() {}   // the key function: the vtable the constructor fake stores
CFSOnlinePark::CFSOnlinePark() { ev("CFSOnlinePark", {lab(this)}); }

extern "C" {

uint64_t tpw_bum_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"e90", &gOnlineWindowE90},
        {"e8c", &gOnlineWindowE8c},
        {"word88", &gOnlineWord88},
        {"panel", &gOnlinePanel},
        {"sys", &gOnlineSystem},
        {"lobby", &gLobbyScreens},
        {"listpanel", &gOnlineParkListPanel},
        {"spanel", gStaffStatusStorage},
        {"vpanel", &gVisitorStatusPanel},
        {"sthing", &gStaffRoomPanelThing},
        {"thingarr", &gThingArray[0]},
        {"infoStrA", gOnlineParkInfoStringA},
        {"infoNodeA", gOnlineParkInfoNodeA},
        {"infoStrB", gOnlineParkInfoStringB},
        {"infoNodeB", gOnlineParkInfoNodeB},
        {"infoNodePark", gOnlineParkInfoNodePark},
        {"infoPark", gOnlineParkInfoParkStorage},
        {"infoParkDtor", gOnlineParkInfoParkDtor},
        {"listStrA", gOnlineParkListStringA},
        {"listNodeA", gOnlineParkListNodeA},
        {"listStrB", gOnlineParkListStringB},
        {"listNodeB", gOnlineParkListNodeB},
        {"dtorglue", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bum_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bum_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    out[n++] = static_cast<int64_t>(offsetof(CLBScreenSystem, mAdvisor));
    out[n++] = static_cast<int64_t>(sizeof(CLBScreenSystem));
    return n;
}

int32_t tpw_bum_buttons(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ButtonsUnderlayMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
void tpw_bum_hide() { HideButtonsMenu(); }
void tpw_bum_sinit_info() { __sinit_ui_onlineParkInfoPanel_cpp(); }
void tpw_bum_remove() { RemoveOnlineParkListPanel(); }
void tpw_bum_sinit_list() { __sinit_ui_onlineParkListPanel_cpp(); }
void tpw_bum_date(const int32_t* w) {
    ParkDate d;
    std::memcpy(&d, w, sizeof(d));
    UIInterface ui;
    ui.DateChange(d);
}
void tpw_bum_staff() {
    UIInterface ui;
    ui.Screen_StaffLoc();
}
void tpw_bum_peep() {
    UIInterface ui;
    ui.Screen_PeepLoc();
}
void tpw_bum_death(uint32_t index) {
    UIInterface ui;
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> tag;
    tag.mIndex = static_cast<uint16_t>(index);
    ui.ThingDeath(tag);
}

}  // extern "C"
