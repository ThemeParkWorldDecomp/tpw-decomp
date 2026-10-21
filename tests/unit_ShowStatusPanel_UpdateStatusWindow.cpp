// Test library for the sideshow status panel (unit v5547; libtpw_unit_ShowStatusPanel_UpdateStatusWindow.dylib,
// tests/test_ShowStatusPanel_UpdateStatusWindow.py), main app:
//   src/game/ui/ui_showStatus.cpp   UpdateStatusWindow, ApplyChanges, applytoallcallback, Show; gShowStatusThing, gShowStatusScript
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The windows are fake
// objects whose virtual SetValue is a hand-made vtable slot. The globals of other files the unit reads are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_main.h"
#include "MainWindowCallback.h"

using namespace calllog;

// ---- globals of other files
CThing* gStaffRoomPanelThing;
CWorld* gGameWorld;
// gText is only a block of zero bytes here (CText's constructor needs the font classes, which this library does not compile); the unit
// reads only the table pointer at mResourcePointers[6].
alignas(16) char gTextBytes[sizeof(CText)] __asm__("_gText");
char16_t gUIMainConvertBuffer[0x4000];

namespace {

const int kWindowIds[8] = {0xa09d, 0xa096, 0xa093, 0xa092, 0xa099, 0xa094, 0xa0a0, 0xa09e};

struct FakeWin {
    void** mVptr;
    char mPad[56];
};
FakeWin gWin[8];
void* gVtable[256];
alignas(16) char gRideBuf[sizeof(RideInfo)];
char gAnalyserDummy[16];

int WinIndex(int id) {
    for (int i = 0; i < 8; ++i)
        if (kWindowIds[i] == id) return i;
    return -1;
}

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value});
    return 0;
}

void FakeNotify(void* self) { ev("Notify", {lab(self)}); }

void InitWindows() {
    static bool done = false;
    if (done) return;
    done = true;
    int32_t (InterfaceWindow::*pmf)(int32_t) = &InterfaceWindow::SetValue;
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    size_t slot = (words[0] & ~static_cast<uintptr_t>(1)) / sizeof(void*);
    gVtable[slot] = reinterpret_cast<void*>(&FakeSetValue);
    for (FakeWin& w : gWin) w.mVptr = gVtable;
}

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    char key[32];
    std::snprintf(key, sizeof key, "Win%d", id);
    InitWindows();
    int index = WinIndex(id);
    if (ret(key) != 0 && index >= 0) return reinterpret_cast<InterfaceWindow*>(&gWin[index]);
    return nullptr;
}

void InterfaceWindow::SetClientDrawText(char16_t* text) { ev("SetText", {lab(this)}, {narrow(text).c_str()}); }

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return reinterpret_cast<RideInfo*>(gRideBuf);
}

uint32_t CObject::GetPercentageOfWinners() const {
    ev("GetPercentageOfWinners", {lab(this)});
    return static_cast<uint32_t>(ret("GetPercentageOfWinners"));
}

int32_t CObject::GetProfitability() const {
    ev("GetProfitability", {lab(this)});
    return static_cast<int32_t>(ret("GetProfitability"));
}

int32_t CObject::GetProspectiveExcitementLevel(int32_t speed, int32_t duration, int32_t capacity) const {
    ev("GetProspectiveExcitementLevel", {lab(this), speed, duration, capacity});
    return static_cast<int32_t>(ret("GetProspectiveExcitementLevel"));
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {lab(this)});
    return static_cast<int32_t>(ret("GetCustomerSatisfaction"));
}

uint32_t CObject::GetCurrentResidualValue() const {
    ev("GetCurrentResidualValue", {lab(this)});
    return static_cast<uint32_t>(ret("GetCurrentResidualValue"));
}

void CObject::SetPricePerUse(int32_t price) { ev("SetPricePerUse", {lab(this), price}); }

void CObject::SetChanceOfWinning(uint8_t chance) { ev("SetChanceOfWinning", {lab(this), chance}); }

void CObject::SetPrizeValue(int32_t value) { ev("SetPrizeValue", {lab(this), value}); }

template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    int64_t offset = reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gStaffRoomPanelThing);
    ev("GetSumOfLast", {offset, count});
    char key[32];
    std::snprintf(key, sizeof key, "Sum%d", static_cast<int>(offset));
    sum = static_cast<double>(static_cast<int32_t>(ret(key))) / 4.0;
    return 0;
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    ShowStatusTag* a = va_arg(args, ShowStatusTag*);
    ShowStatusTag* b = va_arg(args, ShowStatusTag*);
    va_end(args);
    ev("FormatString", {lab(this), out == gUIMainConvertBuffer ? 1 : 0, size, id, a->mMagic, a->mId, a->mValue, a->mFlag, b->mMagic,
                        b->mId, b->mValue, b->mFlag});
    int64_t v = ret("FormatString");
    out[0] = static_cast<char16_t>(0x41 + (v & 15));
    out[1] = 0;
    return 0;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserDummy);
}

int32_t CParkAnalyser::GetAverageHappinessAroundPoint(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, uint32_t radius) const {
    ev("Happiness", {lab(this), cell.mIndex, radius});
    return static_cast<int32_t>(ret("Happiness"));
}

int32_t UIMain_GetObjectState(CObject* object, int32_t flag) {
    ev("GetObjectState", {lab(object), flag});
    return static_cast<int32_t>(ret("GetObjectState"));
}

void UIMain_AdvisorMessage(AdvisorMessage message) { ev("AdvisorMessage", {static_cast<int64_t>(message)}); }

void UIStatusPanel::OpenWindow(int16_t* description, InterfaceWindowCallback callback, int32_t windowA, int32_t windowB, int32_t windowC) {
    ev("OpenWindow", {lab(this), lab(description), lab(reinterpret_cast<void*>(callback)), windowA, windowB, windowC});
}

int32_t ShowStatusMsg(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

// The key function of the tag's vtable (the real one is in a file this library does not compile).
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

extern "C" {

uint64_t tpw_uvs_addr(const char* name) {
    InitWindows();
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gStaffRoomPanelThing", &gStaffRoomPanelThing},
        {"gGameWorld", &gGameWorld},
        {"table_slot", &gText.mResourcePointers[6]},
        {"buffer", gUIMainConvertBuffer},
        {"gShowStatusThing", &gShowStatusThing},
        {"script", gShowStatusScript},
        {"callback", reinterpret_cast<const void*>(&ShowStatusMsg)},
        {"notify_fn", reinterpret_cast<const void*>(&FakeNotify)},
        {"analyser", gAnalyserDummy},
        {"win0", &gWin[0]}, {"win1", &gWin[1]}, {"win2", &gWin[2]}, {"win3", &gWin[3]},
        {"win4", &gWin[4]}, {"win5", &gWin[5]}, {"win6", &gWin[6]}, {"win7", &gWin[7]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_uvs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The base the Ptr32 offsets are counted from.
uint64_t tpw_uvs_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The ride record's word that ApplyChanges tests (Mac +0x10c).
void tpw_uvs_set_flag(int32_t value) {
    reinterpret_cast<RideInfo*>(gRideBuf)->Value(kRideSlot_NonWinnableSideshow).mInteger = value;
}

int32_t tpw_uvs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory6C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory22C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase) + offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(sizeof(UIStatusPanelFields));
    return n;
}

void tpw_uvs_update(uint64_t panel) { reinterpret_cast<ShowStatusPanel*>(panel)->UpdateStatusWindow(); }
void tpw_uvs_apply(uint64_t panel) { reinterpret_cast<ShowStatusPanel*>(panel)->ApplyChanges(); }
void tpw_uvs_applyall(uint64_t panel, uint64_t thing) {
    reinterpret_cast<ShowStatusPanel*>(panel)->applytoallcallback(reinterpret_cast<CThing*>(thing));
}
void tpw_uvs_show(uint64_t panel, uint64_t thing) { reinterpret_cast<ShowStatusPanel*>(panel)->Show(reinterpret_cast<CThing*>(thing)); }

}  // extern "C"
