// Unit w131 (the park's UI panel message functions): MainPanelButtonsMsg, HappyBarMsg, CashWindowCallback, BobbleCallback and GoldenStuffMsg
// (src/ui_mainpanel2_msg.cpp), CashPanelMsg and ControlPanelMsg (src/ui_entryPricePanel_msg.cpp), StaffPanelMsg, StaffLocMsg and
// ModifiedCheckButtonsCallback (src/ui_staffPanel_msg.cpp), UIInterface::CheckMouseContext and CIntrface::Process run for real; every callee
// outside the unit is a fake that records through calllog (see tests/test_UIPanels.py). swprintf is the platform's (platform/msl.cpp).
//
// The windows are fixed stores labelled 1 to 15; the other labelled objects are the park analyser (11), a thing (12, its kind byte at +2), the
// world (16), the master save (17), the CIntrface block (18) and the addresses of four global objects (19 to 22). GetWindow answers by the
// child's id (a queue per id, "W<hex id>"; the default is the window the script gives that id, else the generic child 9).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "TPCamera.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "intrface.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

namespace {
void* gAllocated[16];
int32_t gAllocatedCount;
void ObserveAllocation(std::size_t size, bool array, void* p);
}  // namespace
#define TPW_ALLOCATION_OBSERVER ObserveAllocation
#include "calllog.cpp"
using namespace calllog;

// ---- the globals the unit does not define (the staff panel's and the cash panel's are in files that are not linked)
InterfaceWindow* gMainPanel;   // (MainPanelMsg, in the same file as the unit's handlers, clears it)
InterfaceWindow* gStaffPanelA;
InterfaceWindow* gStaffPanelC;
InterfaceWindow* gCashPanel;
InterfaceWindow* gControlPanel;
InterfaceWindow* gAerialWindow;
int32_t gStaffPanelState;
uint32_t gFirstPersonFlags;
CFSGlobalMasterSave* gMasterSave;
CWorld* gGameWorld;
CWorld*& gGameWorldRef = gGameWorld;
UIInterface gUIInterface;
CIntrface gIntrface;
StaffStatusPanel gStaffStatusPanel;
VisitorStatusPanel gVisitorStatusPanel;
CUI2DMap gUI2DMap;

namespace {
constexpr int kWindows = 15;
constexpr int kLabAnalyser = 11, kLabThing = 12, kLabWorld = 16, kLabMaster = 17, kLabIntrface = 18, kLabUIInterface = 19, kLabMap = 20,
              kLabStaffStatus = 21, kLabVisitorStatus = 22;
constexpr int kLabGenericChild = 9, kLabParent = 10;
alignas(16) unsigned char gWindowStore[kWindows][sizeof(InterfaceWindow)];
alignas(16) unsigned char gAnalyser[64];
alignas(16) unsigned char gThing[256];
alignas(16) unsigned char gWorldStore[64];
alignas(16) unsigned char gMasterStore[256];
alignas(16) unsigned char gIntrfaceStore[sizeof(CIntrface)];

struct Child {
    int32_t id;
    int label;
};
const Child kChildren[] = {{0x36, 3}, {0x37, 4}, {0x34, 5}, {0x35, 6}, {0x149, 7}};

void* Obj(int32_t label) {
    switch (label) {
        case kLabAnalyser: return gAnalyser;
        case kLabThing: return gThing;
        case kLabWorld: return gWorldStore;
        case kLabMaster: return gMasterStore;
        case kLabIntrface: return gIntrfaceStore;
        case kLabUIInterface: return &gUIInterface;
        case kLabMap: return &gUI2DMap;
        case kLabStaffStatus: return &gStaffStatusPanel;
        case kLabVisitorStatus: return &gVisitorStatusPanel;
    }
    // labels 11 and 12 are the analyser and the thing; the windows are the other labels up to 15
    if (label >= 1 && label <= kWindows) return gWindowStore[label - 1];
    return nullptr;
}
InterfaceWindow* Win(int32_t label) { return static_cast<InterfaceWindow*>(Obj(label)); }

// the window pointer of a window member that may be NULL: the compiler must not learn that `this` is not
template <typename T>
const void* Self(T* self) {
    const void* p = self;
    asm volatile("" : "+r"(p));
    return p;
}

void ObserveAllocation(std::size_t size, bool, void* p) {
    if (gAllocatedCount < 16) gAllocated[gAllocatedCount++] = p;
    newlab(p);
    ev("operator_new", {size == sizeof(CFSGlobalMasterSave) ? 0x4c : static_cast<int64_t>(size), lab(p)});
}
}  // namespace

// ---- the game type: silent (the guard cannot be reset between cases; the case sets the type)
CGameType::CGameType() { mType = kGameTypeNormal; }

// ---- the master save: the constructor labels the new object; the answers come from queues
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }
int32_t CFSGlobalMasterSave::HowManyKeys() const {
    ev("HowManyKeys", {lab(this)});
    return static_cast<int32_t>(ret("HowManyKeys", 0));
}
int32_t CFSGlobalMasterSave::SpendingPower() const {
    ev("SpendingPower", {lab(this)});
    return static_cast<int32_t>(ret("SpendingPower", 0));
}

// ---- the world and the analyser
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return static_cast<CParkAnalyser*>(Obj(static_cast<int32_t>(ret("Analyser", kLabAnalyser))));
}
float CParkAnalyser_GetHappinessLevel(CParkAnalyser* analyser, int32_t which) {
    ev("GetHappinessLevel", {lab(analyser), which});
    const uint32_t bits = static_cast<uint32_t>(ret("Happy", 0));
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

// ---- the untranslated callees the panels reach: each records and answers nothing
void GotoLastUsedPanel(int32_t type) { ev("GotoLastUsedPanel", {type}); }
void ShowControlPanel() { ev("ShowControlPanel"); }
void RemoveControlPanel() { ev("RemoveControlPanel"); }
void CUI2DMap::CreateMapPanel() { ev("CreateMapPanel", {lab(this)}); }
void CreateResearchPanel() { ev("CreateResearchPanel"); }
void CreateLoansPanel() { ev("CreateLoansPanel"); }
void CreateStaffCostsPanel() { ev("CreateStaffCostsPanel"); }
void CreateYearEndPanel() { ev("CreateYearEndPanel"); }
void RemoveSlideoutPanel(int16_t which) { ev("RemoveSlideoutPanel", {which}); }
void UI_DoPostcard() { ev("UI_DoPostcard"); }
void UIInterface::CamcorderMode() { ev("CamcorderMode", {lab(this)}); }
void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("UIMain_CancelAdvisorMessage", {static_cast<int32_t>(message)}); }
void UpdateStatus() { ev("UpdateStatus"); }
void StaffLocationSwitch(int32_t which) { ev("StaffLocationSwitch", {which}); }
void hackfollowmode() { ev("hackfollowmode"); }
void DoCashWin(InterfaceWindow* window) { ev("DoCashWin", {lab(window)}); }
void LLCC_SystemSetSessionCost(int32_t cost) { ev("LLCC_SystemSetSessionCost", {cost}); }
void ContextMouse(int32_t context) { ev("ContextMouse", {context}); }
void StaffStatusPanel::Show(CThing* thing) { ev("StaffStatusPanel.Show", {lab(this), lab(thing)}); }
void VisitorStatusPanel::Show(CThing* thing) { ev("VisitorStatusPanel.Show", {lab(this), lab(thing)}); }
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_PostMessage", {lab(window), message, a, b});
    return 0;
}

// ---- the check buttons and buttons: only the two members the panels call
int32_t InterfaceCheckButtons::SetCurrent(int32_t id) {
    ev("SetCurrent", {lab(Self(this)), id});
    return 0;
}
int32_t InterfaceButton::SetState(int8_t state, int8_t redraw) {
    ev("SetState", {lab(Self(this)), static_cast<uint8_t>(state), static_cast<uint8_t>(redraw)});
    return 0;
}

// ---- the windows: every member records; SetValue and SetText are the virtual ones the Mac reaches through the vtable at +0x118
InterfaceWindow::InterfaceWindow(InterfaceWindow* parent, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) { mParent = parent; }
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t value) {
    ev("SetValue", {lab(this), value});
    return 0;
}
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t* text) {
    ev("SetText", {lab(this)}, {narrow(text).c_str()});
    return 0;
}
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(Self(this)), id});
    int label = kLabGenericChild;
    for (const Child& child : kChildren) {
        if (child.id == id) label = child.label;
    }
    char key[16];
    std::snprintf(key, sizeof key, "W%x", static_cast<uint32_t>(id));
    return Win(static_cast<int32_t>(ret(key, label)));
}
InterfaceWindow* InterfaceWindow::GetParent() {
    ev("GetParent", {lab(Self(this))});
    return Win(static_cast<int32_t>(ret("Parent", kLabParent)));
}
int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(Self(this)), on});
    return 1;
}
void InterfaceWindow::unlink() { ev("unlink", {lab(Self(this))}); }
int16_t InterfaceWindow::setAnimTimer(int32_t id, int32_t ms) {
    ev("setAnimTimer", {lab(Self(this)), id, ms});
    return static_cast<int16_t>(ret("setAnim", 1));
}
int32_t InterfaceWindow::startAnimTimer(int32_t id) {
    ev("startAnimTimer", {lab(Self(this)), id});
    return 0;
}
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(Self(this)), message, a, b});
    return static_cast<int32_t>(ret("Default", 0));
}

// ---- the C interface the Python module drives
namespace {
// the state words, in the order of the Python module
enum { kState, kThing, kA, kC, kCash, kControl, kAerial, kFirstPerson, kCashGuard, kCashFrames, kCashWord, kSpendGuard, kLastSpend,
       kKeysGuard, kLastKeys, kMaster, kGameType, kThingKind, kIntrfaceWord, kIntrfaceCount, kStateWords };

CThing* ThingPtr() { return reinterpret_cast<CThing*>(gThing); }
}  // namespace

extern "C" {
void* tpw_up_obj(int32_t label) { return Obj(label); }

// Frees what the last case left behind and puts the world in its starting state: windows constructed over 0xa5 bytes, the other stores 0xa5.
void tpw_up_reset() {
    tpw_cl_on = 0;
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    for (auto& store : gWindowStore) {
        std::memset(store, 0xa5, sizeof store);
        new (store) InterfaceWindow(nullptr, 0, 0, 0, 0, 0, 0);
    }
    std::memset(gAnalyser, 0xa5, sizeof gAnalyser);
    std::memset(gThing, 0xa5, sizeof gThing);
    std::memset(gWorldStore, 0xa5, sizeof gWorldStore);
    std::memset(gMasterStore, 0xa5, sizeof gMasterStore);
    std::memset(gIntrfaceStore, 0xa5, sizeof gIntrfaceStore);
}

void tpw_up_state(const int32_t* in) {
    gStaffPanelState = in[kState];
    gStaffPanelThing = in[kThing] != 0 ? ThingPtr() : nullptr;
    gStaffPanelA = Win(in[kA]);
    gStaffPanelC = Win(in[kC]);
    gCashPanel = Win(in[kCash]);
    gControlPanel = Win(in[kControl]);
    gAerialWindow = Win(in[kAerial]);
    gFirstPersonFlags = static_cast<uint32_t>(in[kFirstPerson]);
    gCashWindowGuard = static_cast<int8_t>(in[kCashGuard]);
    gCashWindowFrames = static_cast<uint32_t>(in[kCashFrames]);
    gCashWindowWord = in[kCashWord];
    gGoldenSpendGuard = static_cast<int8_t>(in[kSpendGuard]);
    gGoldenLastSpending = in[kLastSpend];
    gGoldenKeysGuard = static_cast<int8_t>(in[kKeysGuard]);
    gGoldenLastKeys = in[kLastKeys];
    gMasterSave = static_cast<CFSGlobalMasterSave*>(Obj(in[kMaster]));
    TheGameType().mType = static_cast<GameType>(in[kGameType]);
    gThing[2] = static_cast<uint8_t>(in[kThingKind]);
    gGameWorld = static_cast<CWorld*>(Obj(kLabWorld));
    auto* block = reinterpret_cast<CIntrface*>(gIntrfaceStore);
    block->mPointedThing = reinterpret_cast<CThing*>(static_cast<uintptr_t>(in[kIntrfaceWord]));   // the Mac's 32-bit word
    block->mProcessCount = in[kIntrfaceCount];
    for (int32_t& count : tpw_cl_heap) count = 0;
}

// The dump: the state words, the heap counts and the CIntrface block (its two words by value, then every other byte).
int32_t tpw_up_dump(int32_t* out) {
    int k = 0;
    out[k++] = gStaffPanelState;
    out[k++] = lab(gStaffPanelThing);
    out[k++] = lab(gStaffPanelA);
    out[k++] = lab(gStaffPanelC);
    out[k++] = lab(gCashPanel);
    out[k++] = lab(gControlPanel);
    out[k++] = lab(gAerialWindow);
    out[k++] = static_cast<int32_t>(gFirstPersonFlags);
    out[k++] = static_cast<uint8_t>(gCashWindowGuard);
    out[k++] = static_cast<int32_t>(gCashWindowFrames);
    out[k++] = gCashWindowWord;
    out[k++] = static_cast<uint8_t>(gGoldenSpendGuard);
    out[k++] = gGoldenLastSpending;
    out[k++] = static_cast<uint8_t>(gGoldenKeysGuard);
    out[k++] = gGoldenLastKeys;
    out[k++] = lab(gMasterSave);
    out[k++] = TheGameType().mType;
    out[k++] = gThing[2];
    for (int32_t count : tpw_cl_heap) out[k++] = count;
    auto* block = reinterpret_cast<CIntrface*>(gIntrfaceStore);
    out[k++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(block->mPointedThing));
    out[k++] = block->mProcessCount;
    // The Mac's bytes 4 to 11 are the two pointers after the first (nothing writes them: 4 entries each, 0xff when set); then the
    // bytes after the count (the host's pointers are 8 bytes wide, so they sit 12 bytes further on).
    // (the store starts filled with 0xa5, which Process leaves alone: such a pointer answers 0xa5, as the Mac's bytes do)
    for (const CThing* pointer : {block->mPointedStaffOrGuest, block->mThingUnderPointer})
        for (int i = 0; i < 4; ++i)
            out[k++] = reinterpret_cast<uintptr_t>(pointer) == static_cast<uintptr_t>(0xa5a5a5a5a5a5a5a5ull) ? 0xa5 : pointer != nullptr ? 0xff : 0;
    for (std::size_t i = offsetof(CIntrface, mF10); i < sizeof(CIntrface); ++i) out[k++] = gIntrfaceStore[i];
    return k;
}

int32_t tpw_up_call(int32_t op, int32_t window, int32_t message, int32_t a, int32_t b) {
    InterfaceWindow* w = Win(window);
    switch (op) {
        case 0: return MainPanelButtonsMsg(w, message, a, b);
        case 1: return CashPanelMsg(w, message, a, b);
        case 2: return StaffPanelMsg(w, message, a, b);
        case 3: return HappyBarMsg(w, message, a, b);
        case 4: return ControlPanelMsg(w, message, a, b);
        case 5: return StaffLocMsg(w, message, a, b);
        case 6: return CashWindowCallback(w, message, a, b);
        case 7: return BobbleCallback(w, message, a, b);
        case 8: return ModifiedCheckButtonsCallback(w, message, a, b);
        case 9: gUIInterface.CheckMouseContext(message); return 0;
        case 10: reinterpret_cast<CIntrface*>(gIntrfaceStore)->Process(); return 0;
        case 11: return GoldenStuffMsg(w, message, a, b);
    }
    return 0;
}
}
