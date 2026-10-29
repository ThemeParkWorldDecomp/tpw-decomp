// Test library for the small online UI functions of unit v51067 (libtpw_unit_OnlineParkListPostGetAllParksInCityCallbackFn.dylib,
// tests/test_OnlineParkListPostGetAllParksInCityCallbackFn.py), main app:
//   src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp, src/game/ui/ui_onlineNewsPanel.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp   the 20 functions of the unit
//
// Every callee outside the unit is a fake that records what the real one reads (the same event name and arguments as the module's
// PowerPC stubs). The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "AdvisorScoring.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "email.h"
#include "lobby.h"
#include "online.h"
#include "ui_DrawObject.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_publishParkPanel.h"

using namespace calllog;

// ---- the unit's functions and globals (src/game/ui/ui_cityLobbyController.cpp, src/game/ui/ui_onlineCityInfoOverlay.cpp, src/game/ui/ui_onlineLogin.cpp, src/game/ui/ui_onlineNewsPanel.cpp, src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp, src/game/ui/ui_onlineParkFindPanel.cpp, src/game/ui/ui_onlineParkInfoPanel.cpp, src/game/ui/ui_onlineParkListPanel.cpp)
void OutboxCallback(EmailCompletionCode code, EmailErrorCode error);
void RemoveOnlinePanel();
void RemoveOnlineNewsPanel();
void __sinit_ui_onlineNewsPanel_cpp();
void __sinit_ui_onlineLogin_cpp();
void __sinit_ui_onlineCityInfoOverlay_cpp();
void HideOnlineCityInfoOverlay();
void ShowOnlineCityInfoOverlay();
void RemoveOnlineCityInfoOverlay();
void HideCityLobbyController();
void ShowCityLobbyController();
void RemoveCityLobbyController();
void ListParkOnlineParkInfoPanelCancelCallback();
extern uint8_t gOnlineNewsNodeA[];
extern uint8_t gOnlineLoginNodeA[];
extern uint8_t gOnlineCityInfoNodeA[];

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gOnlinePanel;
InterfaceWindow* gOnlineParkInfoPanel;
InterfaceWindow* gOnlineNewsPanel;
InterfaceWindow* gOnlineCityOverlay;
InterfaceWindow* gCityLobbyWindow;
CLBScreenSystem* gLobbyScreens;
COnlineControl* gOnlineControl;
uint32_t gContactsFontIndex;
int16_t gUIRealScreenHeight;
int32_t gMailStatusA;
int32_t gMailStatusB;
alignas(16) const char16_t kOnlineCityNoText[8] = {0};
alignas(16) char16_t gOnlineLoginEmptyText[8] = {0};
extern const char16_t gOnlineNewsText[];

namespace {
alignas(16) unsigned char gFontBlock[64];
InterfaceFont* gFontPtr = nullptr;
}  // namespace

// ---- the fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

void CLBScreenComponentAdvisor::ClearAll() { ev("ClearAll", {lab(this)}); }

void FillParkList(TbPtrList<CFSOnlinePark>* parks) { ev("FillParkList", {lab(parks)}); }
void RemoveOnlineParkListPanel() { ev("RemoveOnlineParkListPanel", {}); }
void FillOnlineParkFindList(TbPtrList<CFSOnlinePark>* parks) { ev("FillOnlineParkFindList", {lab(parks)}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return reinterpret_cast<InterfaceWindow*>(reinterpret_cast<uintptr_t>(this) + 0x100);
}
void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
int32_t MessagesLCEntryHandler(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {lab(this), callback == MessagesLCEntryHandler ? 1 : 0});
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}
void COnlineControl::UpdateUserList() const { ev("UpdateUserList", {lab(this)}); }
int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {lab(this)});
    return static_cast<int32_t>(ret("GetHeight"));
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return gFontPtr;
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {static_cast<int64_t>(size)});
    return tpw::ArenaAllocZeroed(size);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem), 0}); }
void CashCtor(void*) {}
void NumberCtor(void*) {}
void PercentCtor(void*) {}
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                int16_t bottom) {
    ev("WindowCtor", {newlab(mem), lab(parent), attributes, id, left, top, right, bottom});
}
void WindowSetValue(InterfaceWindow*, int32_t) {}
}  // namespace lender_callee

namespace contacts_callee {
int32_t FontCharWidth(InterfaceFont*, char16_t) { return 0; }
void WindowSlot1c(InterfaceWindow*, int32_t) {}
COnlineControl* NewOnlineControl() {
    ev("OperatorNew", {0xe8c});
    void* memory = tpw::ArenaAllocZeroed(0xe8c);
    ev("COnlineControlCtor", {newlab(memory)});
    return static_cast<COnlineControl*>(memory);
}
}  // namespace contacts_callee

namespace parkinfo_callee {
void OnlineIslandsSlot30(void* screen) { ev("Slot30", {lab(screen)}); }
void DownloadGlobeSlot4c(void*) {}
}  // namespace parkinfo_callee

extern "C" {
void* CityLobbyStringConstruct(void* self, const char16_t* text) {
    ev("StringCtor", {newlab(self), lab(text)});
    return self;
}
void CityLobbyStringDestructor(void*, int16_t) {}
void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), destructor == reinterpret_cast<void*>(&CityLobbyStringDestructor) ? 1 : 0, lab(node)});
}

// The host address of a global by the module's name for it.
uint64_t tpw_ov_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gLobbyScreens", &gLobbyScreens},
        {"gOnlineParkInfoPanel", &gOnlineParkInfoPanel},
        {"gOnlinePanel", &gOnlinePanel},
        {"gOnlineNewsPanel", &gOnlineNewsPanel},
        {"gOnlineCityOverlay", &gOnlineCityOverlay},
        {"gCityLobbyWindow", &gCityLobbyWindow},
        {"gOnlineControl", &gOnlineControl},
        {"gContactsFontIndex", &gContactsFontIndex},
        {"gUIRealScreenHeight", &gUIRealScreenHeight},
        {"gMailStatusA", &gMailStatusA},
        {"gMailStatusB", &gMailStatusB},
        {"loginText1", gOnlineLoginEmptyText + 1},
        {"cityText1", kOnlineCityNoText + 1},
        {"newsText", gOnlineNewsText},
        {"newsNode", gOnlineNewsNodeA},
        {"loginNode", gOnlineLoginNodeA},
        {"cityNode", gOnlineCityInfoNodeA},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ov_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ov_layout(int64_t*) { return 0; }

// What the host needs that the Mac keeps in plain words: what 0 = the screens (a = advisor, b = online islands), 1 = the font block.
void tpw_ov_set(int32_t what, uint64_t a, uint64_t b) {
    alignas(16) static unsigned char screens[sizeof(CLBScreenSystem)];
    if (what == 0) {
        CLBScreenSystem* system = reinterpret_cast<CLBScreenSystem*>(screens);
        std::memset(screens, 0, sizeof(screens));
        system->mAdvisor = reinterpret_cast<CLBScreenComponentAdvisor*>(static_cast<uintptr_t>(a));
        system->mOnlineIslands = reinterpret_cast<CLBScreenComponentOnlineIslands*>(static_cast<uintptr_t>(b));
        gLobbyScreens = system;
    } else {
        gFontPtr = reinterpret_cast<InterfaceFont*>(static_cast<uintptr_t>(a));
    }
}

int64_t tpw_ov_lab(uint64_t p) { return lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

// One entry for the 20 functions: op as in the module's OPS list.
int64_t tpw_ov_call(int32_t op, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    auto P = [](uint64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); };
    switch (op) {
        case 0:
            OnlineParkListPostGetAllParksInCityCallbackFn(static_cast<int32_t>(a), P(b), static_cast<TbPtrList<CFSOnlinePark>*>(P(c)));
            return 0;
        case 1: RemoveOnlineParkInfoPanel(); return 0;
        case 2:
            FindParksPostGetPlayerParksCallbackFn(static_cast<int32_t>(a), P(b), static_cast<TbPtrList<CFSOnlinePark>*>(P(c)));
            return 0;
        case 3: RemoveOnlinePanel(); return 0;
        case 4: FillContactList(); return 0;
        case 5: MessagesLCPostFormat(static_cast<InterfaceWindow*>(P(a))); return 0;
        case 6: return lab(MessagesLCFormat());
        case 7:
            OutboxCallback(static_cast<EmailCompletionCode>(static_cast<int32_t>(a)), static_cast<EmailErrorCode>(static_cast<int32_t>(b)));
            return 0;
        case 8: __sinit_ui_onlineNewsPanel_cpp(); return 0;
        case 9: RemoveOnlineNewsPanel(); return 0;
        case 10: __sinit_ui_onlineLogin_cpp(); return 0;
        case 11: __sinit_ui_onlineCityInfoOverlay_cpp(); return 0;
        case 12: HideOnlineCityInfoOverlay(); return 0;
        case 13: ShowOnlineCityInfoOverlay(); return 0;
        case 14: RemoveOnlineCityInfoOverlay(); return 0;
        case 15:
            return CityInfoOverlayMsg(static_cast<InterfaceWindow*>(P(a)), static_cast<int32_t>(b), static_cast<int32_t>(c),
                                      static_cast<int32_t>(d));
        case 16: HideCityLobbyController(); return 0;
        case 17: ShowCityLobbyController(); return 0;
        case 18: RemoveCityLobbyController(); return 0;
        case 19: ListParkOnlineParkInfoPanelCancelCallback(); return 0;
    }
    return 0;
}

}  // extern "C"
