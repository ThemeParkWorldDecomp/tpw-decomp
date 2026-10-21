// Test library for the ride status panel's upgrade list (unit v5074; libtpw_unit_FillUpgradeBox.dylib, tests/test_FillUpgradeBox.py), main app:
//   src/game/ui/ui_RideStatus.cpp   FillUpgradeBox, UpgradePanelMsg
//
// The callees outside the unit are recording fakes that record what the real one reads and answer from the test's tables (the pointers set
// by tpw_fub_setptr). The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "InterfaceFont.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CWorld* gGameWorld;
// gText is raw zeroed storage under gText's symbol: its constructor needs the font classes of files this library does not compile.
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");
CRideDatabase gRideDatabase;
char16_t gUIMainConvertBuffer[0x4000];
RideStatusPanel gRideStatusPanel;


namespace {

// The test's tables: slot 0 the object's ride record, 1 the ride count, 2..17 the rides, 18 the control count, 20..35 the control ids, 36..51 the
// control blocks, 90 the world, 91 the string table, 95..97 the window, the list's child and the extra parameter, 100 the ride position.
uint64_t gSlots[128];
char gTexts[3][256];

constexpr uint64_t kWorld = 0x70010000;

uint32_t HashText(const char16_t* text) {
    if (text == nullptr) return 0xFFFFFFFFu;
    uint32_t h = 2166136261u;
    for (int i = 0; i < 0x4000 && text[i] != 0; i++) h = (h ^ static_cast<uint32_t>(text[i])) * 16777619u;
    return h;
}

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {0});
    gSlots[100] = 0;
    return gSlots[1] > 0 ? reinterpret_cast<RideInfo*>(gSlots[2]) : nullptr;
}

RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {0});
    gSlots[100]++;
    return gSlots[100] < gSlots[1] ? reinterpret_cast<RideInfo*>(gSlots[2 + gSlots[100]]) : nullptr;
}

void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetTypeName", {id, size, buffer == gUIMainConvertBuffer ? 1 : 0});
    static const char kHex[] = "0123456789abcdef";
    buffer[0] = u'T';
    for (int i = 0; i < 4; i++) buffer[1 + i] = static_cast<char16_t>(kHex[(id >> (12 - 4 * i)) & 15]);
    buffer[5] = 0;
}

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(this) - kWorld), type.mIndex, flag});
    for (uint64_t i = 0; i < gSlots[18]; i++)
        if (gSlots[20 + i] == type.mIndex) return reinterpret_cast<ObjectControl*>(gSlots[36 + i]);
    return nullptr;
}

CGameType::CGameType() { mType = kGameTypeNormal; }

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {0});
    return reinterpret_cast<RideInfo*>(gSlots[0]);
}

int32_t CObject::RequiresUpgrade() const {
    ev("RequiresUpgrade", {0});
    return static_cast<int32_t>(ret("RequiresUpgrade"));
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), HashText(item[0].mText), item[1].mInt, data, position, update});
    return 0;
}

int32_t InterfaceListControl2::GetData(int32_t position, int32_t* out) {
    ev("GetData", {lab(this), position});
    *out = static_cast<int32_t>(ret("GetData"));
    return 0;
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    const char* text = gTexts[(id - 0x1a) & 3];
    int i = 0;
    for (; text[i] != 0; i++) gUIMainConvertBuffer[i] = static_cast<unsigned char>(text[i]);
    gUIMainConvertBuffer[i] = 0;
    return gUIMainConvertBuffer;
}

int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    va_list args;
    va_start(args, id);
    TbIntegerVTag* tag = static_cast<TbIntegerVTag*>(va_arg(args, TbVTag*));
    va_end(args);
    ev("FormatString", {lab(this), out == gUIMainConvertBuffer ? 1 : 0, size, id, tag->mMagic, tag->mId, tag->mValue});
    char text[64];
    std::snprintf(text, sizeof text, "F%u_%d", id, tag->mValue);
    int i = 0;
    for (; text[i] != 0; i++) out[i] = static_cast<unsigned char>(text[i]);
    out[i] = 0;
    return 0;
}

int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }

char16_t* wcstok(char16_t* string, const char16_t* delimiters, char16_t**) {
    static char16_t* saved;
    ev("wcstok", {HashText(string), HashText(delimiters)});
    char16_t* p = string != nullptr ? string : saved;
    auto isDelim = [&](char16_t c) {
        for (const char16_t* d = delimiters; *d != 0; d++)
            if (*d == c) return true;
        return false;
    };
    if (p == nullptr) return nullptr;
    while (*p != 0 && isDelim(*p)) p++;
    if (*p == 0) {
        saved = nullptr;
        return nullptr;
    }
    char16_t* start = p;
    while (*p != 0 && !isDelim(*p)) p++;
    if (*p != 0) {
        *p = 0;
        saved = p + 1;
    } else {
        saved = p;
    }
    return start;
}

int32_t RideStatusPanel::Upgrade(int32_t what, bool flag) {
    ev("Upgrade", {lab(this), what, flag});
    return static_cast<int32_t>(ret("Upgrade"));
}

void RideStatusPanel::UpdateStatusWindow() { ev("UpdateStatusWindow", {lab(this)}); }
void RideStatusPanel::Remove() { ev("Remove", {lab(this)}); }
void RideStatusPanel::UpgradePanelByeBye() { ev("UpgradePanelByeBye", {lab(this)}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return ret("GetWindow") != 0 ? reinterpret_cast<InterfaceWindow*>(gSlots[96]) : nullptr;
}

void* InterfaceWindow::getExtraParam(int16_t index) {
    ev("getExtraParam", {lab(this), index});
    return reinterpret_cast<void*>(gSlots[97]);
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefaultWindowHandler", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("Default"));
}

int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    return 0;
}

// ---- the online park list's LCPostFormat: the draw objects' constructors are fakes under the mangled names of the real ones (the new
// expression calls the complete-object constructor; the real classes are not in this library), the window methods record.
extern "C" void FakeTextObjectCtor(void* self, void* data) __asm__("__ZN25InterfaceTextObjectSpriteC1EPv");
extern "C" void FakeTextObjectCtor(void* self, void* data) {
    ev("TextObjectCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}

extern "C" void FakeCashObjectCtor(void* self, void* data) __asm__("__ZN27InterfaceNullCashTextObjectC1EPv");
extern "C" void FakeCashObjectCtor(void* self, void* data) {
    ev("CashObjectCtor", {newlab(self), static_cast<int64_t>(reinterpret_cast<uintptr_t>(data))});
}

namespace {

constexpr uint64_t kFontBase = 0x70000600;

// The row's first child: a block whose first word is a vtable of fakes (the Mac's has the vtable pointer at 0x118; the host's is at 0).
struct FakeWindow {
    void** vptr;
    char pad[0x400];
};
FakeWindow gFakeChild;
void* gFakeVtable[64];

int32_t FakeSetValue(InterfaceWindow* self, int32_t value) {
    ev("SetValue", {lab(self), value, 1});
    return 0;
}

int32_t FakeWrongSlot(InterfaceWindow* self, int32_t value) {
    ev("WrongVirtual", {lab(self), value});
    return 0;
}

}  // namespace

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return reinterpret_cast<InterfaceFont*>(kFontBase + 0x10 * static_cast<uint64_t>(index));
}

void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }

void InterfaceWindow::SetFont(InterfaceFont* font) {
    ev("SetFont", {lab(this), static_cast<int64_t>(reinterpret_cast<uintptr_t>(font)) - static_cast<int64_t>(kFontBase)});
}

void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}

void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return reinterpret_cast<InterfaceWindow*>(&gFakeChild);
}

extern "C" {

// The first child the fakes answer, with its vtable set up so that SetValue (whatever its slot) reaches the recording fake.
uint64_t tpw_fub_child() {
    int32_t (InterfaceWindow::*member)(int32_t) = &InterfaceWindow::SetValue;
    uintptr_t raw[2];
    std::memcpy(raw, &member, sizeof raw);
    size_t index = raw[0] / sizeof(void*);   // arm64: the offset itself, the virtual flag is in the adjustment word
    for (void*& slot : gFakeVtable) slot = reinterpret_cast<void*>(&FakeWrongSlot);
    gFakeVtable[index] = reinterpret_cast<void*>(&FakeSetValue);
    gFakeChild.vptr = gFakeVtable;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gFakeChild));
}

void tpw_fub_lc(uint64_t window) { OnlineParkListPanelLCPostFormat(reinterpret_cast<InterfaceWindow*>(window)); }

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_fub_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gametype", &TheGameType().mType},
        {"buffer", gUIMainConvertBuffer},
        {"panel", &gRideStatusPanel},
        {"fontindex", &gParkListFontIndex},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_fub_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// Sets a table slot (see gSlots); the world and the string table go to their globals.
void tpw_fub_setptr(int32_t slot, uint64_t value) {
    if (slot == 90) gGameWorld = reinterpret_cast<CWorld*>(value);
    else if (slot == 91) reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[6] = reinterpret_cast<void*>(value);
    else gSlots[slot] = value;
}

void tpw_fub_text(int32_t index, const char* text) { std::snprintf(gTexts[index], sizeof gTexts[index], "%s", text); }

// The offsets and sizes the module relies on (HOST_LAYOUT), then the host-only offsets (the ride record has pointers after +0x7f0).
int32_t tpw_fub_layout(int64_t* out) {
    int32_t n = 0;
    RideInfo* ride = nullptr;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF4C));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradeCost));
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mAvailable));
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradesAvailable));
    out[n++] = static_cast<int64_t>(sizeof(ObjectControl));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mId));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&ride->Value(static_cast<RideBalanceSlot>(158))));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&ride->Value(static_cast<RideBalanceSlot>(107))));
    out[n++] = static_cast<int64_t>(reinterpret_cast<uintptr_t>(&ride->Value(static_cast<RideBalanceSlot>(139))));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mLoadFailed));
    out[n++] = static_cast<int64_t>(sizeof(RideInfo));
    out[n++] = static_cast<int64_t>(offsetof(CText, mResourcePointers));
    return n;
}

// ---- the functions, one wrapper each with C types only
void tpw_fub_fill(int32_t kind, uint64_t list, int32_t clear, uint64_t object) {
    FillUpgradeBox(kind, reinterpret_cast<InterfaceListControl2*>(list), clear, reinterpret_cast<CObject*>(object));
}

int32_t tpw_fub_msg(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return UpgradePanelMsg(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}

}  // extern "C"
