// Test library for the entry price panel (unit v5885; libtpw_unit_CreateEntryPricePanel_2.dylib, tests/test_CreateEntryPricePanel_2.py), main app:
//   src/game/ui/ui_entryPricePanel.cpp   CreateEntryPricePanel, EntryPricePanelTitleMsg, EntryPricePanelMsg
//
// Every callee outside the unit is a fake that records the arguments the real one reads and answers from the test's queue; the globals
// of other files the unit reads, defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
InterfaceWindow* gParkScreenWindow;
InterfaceWindow* gGameWindow;
CWorld* gGameWorld;
CThingArrayEntry gThingArray[kThingArraySize];
unsigned char gLoansGameTypeStorage[4];
int8_t gLoansGameTypeGuard;

namespace {
enum Blk { GAME, PANEL, T4AF, T4B0, T4AE, T4B1, T4AC, SELF, OBJ, NAME, STR, FONT, NEW1, NEW2, BANK, VT, NBLK };
uint64_t gBlk[NBLK];
int gNew;

void* B(int i) { return reinterpret_cast<void*>(static_cast<uintptr_t>(gBlk[i])); }
int64_t L(const void* p) { return lab(p); }
}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC recorder
bool UIMain_CheckPanel() {
    ev("CheckPanel", {});
    return true;
}
void SetLastPanel(int32_t type, int32_t panel) { ev("SetLastPanel", {type, panel}); }
void UIMain_SetPanel(InterfaceWindow* window) { ev("SetPanel", {L(window)}); }
InterfaceWindow* InterfaceWindow::Build(int16_t* description, InterfaceWindowCallback callback) {
    ev("Build", {L(this), L(description), L(reinterpret_cast<const void*>(callback))});
    return static_cast<InterfaceWindow*>(B(PANEL));
}
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {L(this), id});
    switch (id) {
    case 0x4f3af: return static_cast<InterfaceWindow*>(B(T4AF));
    case 0x4f3b0: return static_cast<InterfaceWindow*>(B(T4B0));
    case 0x4f3ae: return static_cast<InterfaceWindow*>(B(T4AE));
    case 0x4f3b1: return static_cast<InterfaceWindow*>(B(T4B1));
    case 0x4f3ac: return static_cast<InterfaceWindow*>(B(T4AC));
    }
    return nullptr;
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallbackFunction", {L(this), L(reinterpret_cast<const void*>(callback))});
}
void GAME_ASSERTMSG(int32_t condition, const char*, ...) { ev("Assert", {condition}); }
char16_t* UIMain_GetObjectName(CObject* object) {
    ev("GetObjectName", {L(object)});
    return static_cast<char16_t*>(B(NAME));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) { ev("SetTitleText", {L(window), id, L(text)}); }
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {L(this), L(object)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {L(this), horizontal, vertical}); }
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int32_t>(index)});
    return static_cast<InterfaceFont*>(B(FONT));
}
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {L(this), L(font)}); }
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {L(this), red, green, blue, alpha});
}
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return static_cast<const char16_t*>(B(STR));
}
void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetClientDrawText", {L(this), L(text)}); }
void InterfaceVaryBox::SetMinMaxValues(int32_t* minimum, int32_t* maximum) { ev("SetMinMaxValues", {L(this), *minimum, *maximum}); }
void InterfaceVaryBox::SetInc(int32_t* increment) { ev("SetInc", {L(this), *increment}); }
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {L(this)});
    return static_cast<CBankAccount*>(B(BANK));
}
int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {L(this)});
    return static_cast<int32_t>(ret("IsParkClosed"));
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {L(this), state, redraw});
    return 0;
}
CGameType::CGameType() {
    ev("CGameType", {L(this)});
    mType = static_cast<GameType>(ret("gametype"));
}
void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int32_t>(message)}); }
void RemoveEntryPricePanel() { ev("RemoveEntryPricePanel", {}); }
void CBankAccount::SetAdmissionFee(uint32_t fee) { ev("SetAdmissionFee", {L(this), static_cast<int64_t>(fee)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel", {}); }
void CreateLoansPanel() { ev("CreateLoansPanel", {}); }
void CreateFinancePanel() { ev("CreateFinancePanel", {}); }
void CWorld::SetParkOpenOrClosed(int32_t open, int32_t gateFlag) { ev("SetParkOpenOrClosed", {L(this), open, gateFlag}); }
int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {L(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultPanelMsg"));
}
void UI_TITLE_COLOUR(InterfaceWindow* window) { ev("UI_TITLE_COLOUR", {L(window)}); }
void ShowEditParkNamePanel() { ev("ShowEditParkNamePanel", {}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {L(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefaultWindowHandler"));
}

namespace lender_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {static_cast<int64_t>(size)});
    return B(gNew++ == 0 ? NEW1 : NEW2);
}
void SpriteCtor(void* mem) { ev("SpriteCtor", {L(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {L(mem)}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {L(window), value}); }
}  // namespace lender_callee

namespace entry_callee {
void WindowEnable(InterfaceWindow* window, int16_t on) { ev("WindowEnable", {L(window), on}); }
}  // namespace entry_callee

extern "C" {

uint64_t tpw_ep_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"parksc", &gParkScreenWindow},
        {"gamewin", &gGameWindow},
        {"world", &gGameWorld},
        {"script", gEntryPriceScript},
        {"fee", &gEntryPriceFee},
        {"closed", &gEntryPriceClosed},
        {"gtstore", gLoansGameTypeStorage},
        {"gtguard", &gLoansGameTypeGuard},
        {"msgfn", reinterpret_cast<const void*>(&EntryPricePanelMsg)},
        {"titlefn", reinterpret_cast<const void*>(&EntryPricePanelTitleMsg)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ep_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ep_blk(int32_t index, uint64_t address) {
    gBlk[index] = address;
    if (index == 0) gNew = 0;
}

void tpw_ep_setthing(int32_t index, uint64_t address) {
    gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

int32_t tpw_ep_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mGatesThing));
    out[n++] = static_cast<int64_t>(offsetof(CBankAccount, mAdmissionFee));
    return n;
}

void tpw_ep_create() { CreateEntryPricePanel(); }
int32_t tpw_ep_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return EntryPricePanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}
int32_t tpw_ep_title(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return EntryPricePanelTitleMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

}  // extern "C"
