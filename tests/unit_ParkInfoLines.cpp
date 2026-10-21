// Test library for the park status panel (unit v5232; libtpw_unit_ParkInfoLines.dylib, tests/test_ParkInfoLines.py), main app:
//   src/game/ui/ui_parkStatus.cpp   ParkInfoLines, ParkStatusPanelMsg, DoDominantThoughts; gParkStatusLastTotal, gParkStatusLastInPark
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The fake windows are
// raw blocks whose vtable pointer leads to a table of one recording function (the SetValue the original calls through the glue).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gGameWorld;
uint8_t gParkStatusColours[5][5];
int32_t gParkStatusRange;
InterfaceWindow* gParkStatusPanel;
uint32_t gParkStatusFlags;

namespace {

struct FakeWindow {
    void* vptr;
    uint8_t pad[0x180];
};
FakeWindow gWin[4];
uint8_t gStatic[16][64];
void* gVtable[24];

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 1;
}

struct Init {
    Init() {
        for (void*& slot : gVtable) slot = reinterpret_cast<void*>(&FakeSetValue);
        for (FakeWindow& w : gWin) w.vptr = gVtable;
    }
} gInit;

// labels: 1..4 windows, 5 world, 6 analyser, 7 bank account, 8 grapher, 9 string, 10 the window argument
void* ObjAddr(int64_t label) {
    if (label >= 1 && label <= 4) return &gWin[label - 1];
    if (label >= 5 && label < 16) return gStatic[label];
    return nullptr;
}

const void* Launder(const void* p) {
    asm volatile("" : "+r"(p));
    return p;
}

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(Launder(this)), id});
    int64_t k = ret("GetWindow");
    return reinterpret_cast<InterfaceWindow*>(ObjAddr(k));
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(Launder(this)), on});
    return 1;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(Launder(this))});
    return reinterpret_cast<CParkAnalyser*>(ObjAddr(6));
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(Launder(this))});
    return reinterpret_cast<CBankAccount*>(ObjAddr(7));
}

int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("InPark"));
}

int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("NumberOfVisitorsSinceBeginning", {lab(this)});
    return static_cast<int32_t>(ret("Total"));
}

int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessUnder(int32_t level) const {
    ev("Under", {lab(this), level});
    return static_cast<int32_t>(ret("Under"));
}

int32_t CParkAnalyser::PercentageOfVisitorsWithHappinessOver(int32_t level) const {
    ev("Over", {lab(this), level});
    return static_cast<int32_t>(ret("Over"));
}

void CParkAnalyser::GetThreeMostDominantThoughts(uint8_t& first, uint8_t& second, uint8_t& third) const {
    ev("GetThree", {lab(this)});
    first = static_cast<uint8_t>(ret("t1"));
    second = static_cast<uint8_t>(ret("t2"));
    third = static_cast<uint8_t>(ret("t3"));
}

void CBankAccount::SetAdmissionFee(uint32_t fee) {
    ev("SetAdmissionFee", {lab(this), static_cast<int64_t>(fee)});
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(ObjAddr(9));
}

void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {lab(window), id, lab(text)});
}

void UIMain_CancelAdvisorMessage(AdvisorMessage message) { ev("CancelAdvisorMessage", {static_cast<int32_t>(message)}); }

int32_t UIMain_DefaultPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultPanelMsg", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

void CreateAllKidsPanel(CThing* thing) { ev("CreateAllKidsPanel", {lab(thing)}); }

void CreateAllItemsPanel(UIObjType type, CThing* thing) {
    ev("CreateAllItemsPanel", {static_cast<int32_t>(type), lab(thing)});
}

void CreateAllStaffPanel(StaffType type, CThing* thing) {
    ev("CreateAllStaffPanel", {static_cast<int32_t>(type), lab(thing)});
}

void RemoveParkStatusPanel() { ev("RemoveParkStatusPanel", {}); }

namespace {
int64_t Offset(const void* p, const void* base) {
    return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p)) - static_cast<int64_t>(reinterpret_cast<uintptr_t>(base));
}
}  // namespace

void GraphLine(CUIGrapher& grapher, CHistory<float, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    ev("GraphLineF", {lab(&grapher), static_cast<int32_t>(Offset(history, ObjAddr(6))),
                      static_cast<int32_t>(Offset(&colour, gParkStatusColours)), step, mode});
}

void GraphLine(CUIGrapher& grapher, CHistory<int32_t, 144>* history, TbColour& colour, int32_t step, int32_t mode) {
    ev("GraphLineI", {lab(&grapher), static_cast<int32_t>(Offset(history, ObjAddr(6))),
                      static_cast<int32_t>(Offset(&colour, gParkStatusColours)), step, mode});
}

extern "C" {

uint64_t tpw_pn_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gParkStatusFlags},
        {"world", &gGameWorld},
        {"lastTotal", &gParkStatusLastTotal},
        {"lastInPark", &gParkStatusLastInPark},
        {"colours", gParkStatusColours},
        {"range", &gParkStatusRange},
        {"panel", &gParkStatusPanel},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pn_obj(int32_t label) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ObjAddr(label))); }

int32_t tpw_pn_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TbColour));
    out[n++] = static_cast<int64_t>(sizeof(gParkStatusColours));
    return n;
}

void tpw_pn_info(uint64_t grapher) { ParkInfoLines(*reinterpret_cast<CUIGrapher*>(static_cast<uintptr_t>(grapher))); }

int32_t tpw_pn_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return ParkStatusPanelMsg(reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window)), message, a, b);
}

void tpw_pn_dom() { DoDominantThoughts(); }

}  // extern "C"
