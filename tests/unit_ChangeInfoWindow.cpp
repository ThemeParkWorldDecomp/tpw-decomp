// Test library for the ride list panel's info window and help line (unit v5102; libtpw_unit_ChangeInfoWindow.dylib, tests/test_ChangeInfoWindow.py), main app:
//   src/game/ui/ui_ridepanel.cpp   ChangeInfoWindow, UpdateHelpText; gHelpTextState
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. CoolRect (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp) runs
// for real on both sides. The globals of other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "analyser.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_main.h"
#include "ui_tooltip.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gRideListPanel = nullptr;   // 0x10395998
int16_t gRideListF88 = 0;                    // 0x10395988
CWorld* gGameWorld = nullptr;                // 0x1037ebfc
// gText and gRideDatabase are zeroed storage under the symbols' names (their constructors need files this library does not compile).
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");                      // 0x1037ff18
alignas(16) char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");   // 0x1037f6c8
static CText& TextObject() { return *reinterpret_cast<CText*>(gTextStorage); }
static CRideDatabase& RideDatabaseObject() { return *reinterpret_cast<CRideDatabase*>(gRideDatabaseStorage); }

namespace {

alignas(16) char gCtrlBuf[64];
alignas(16) char gRideBuf[0x900];

constexpr uintptr_t kWindowBase = 0x30001000;
constexpr uintptr_t kBank = 0x30200000;

ObjectControl* Ctrl() { return reinterpret_cast<ObjectControl*>(gCtrlBuf); }
RideInfo* Ride() { return reinterpret_cast<RideInfo*>(gRideBuf); }

// The first six code units of a text as events, zero after the end.
void TextInts(const char16_t* text, int64_t* out) {
    bool ended = false;
    for (int i = 0; i < 6; ++i) {
        int64_t c = ended ? 0 : text[i];
        if (c == 0) ended = true;
        out[i] = c;
    }
}

}  // namespace

// ---- the fakes
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    if (ret("GetWindow", id, 0) == 0) return nullptr;
    return reinterpret_cast<InterfaceWindow*>(kWindowBase + static_cast<uintptr_t>(id) * 16);
}

int32_t InterfaceWindow::Show(int16_t on) {
    ev("Show", {lab(this), on});
    return 0;
}

int32_t InterfaceWindow::GetClientRect(CoolRect* rect) {
    ev("GetClientRect", {lab(this)});
    rect->mLeft = static_cast<int16_t>(ret("rectL"));
    rect->mTop = static_cast<int16_t>(ret("rectT"));
    rect->mRight = static_cast<int16_t>(ret("rectR"));
    rect->mBottom = static_cast<int16_t>(ret("rectB"));
    return 0;
}

void UI_GetMouseXY(int16_t& x, int16_t& y) {
    ev("UI_GetMouseXY", {});
    x = static_cast<int16_t>(ret("mouseX"));
    y = static_cast<int16_t>(ret("mouseY"));
}

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {lab(this), type.mIndex, flag});
    return Ctrl();
}

CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(kBank);
}

int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return Ride();
}

bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("HasBeenBought", {lab(this), type.mIndex});
    return ret("HasBeenBought") != 0;
}

bool CGoldenTicketControl::CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("CanIAfford", {lab(this), type.mIndex});
    return ret("CanIAfford") != 0;
}

// Writes "T" and the low three hex digits of the id; ids 0x96 and 0x94 carry a TbUniStringVTag whose fields are recorded.
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    static const char kHex[] = "0123456789abcdef";
    uint32_t low = id & 0xfff;
    int n = 0;
    out[n++] = u'T';
    bool started = false;
    for (int shift = 8; shift >= 0; shift -= 4) {
        uint32_t digit = (low >> shift) & 15;
        if (digit != 0 || started || shift == 0) {
            out[n++] = static_cast<char16_t>(kHex[digit]);
            started = true;
        }
    }
    out[n] = 0;
    if (reinterpret_cast<uintptr_t>(this) == 0x30000010 && (id == 0x96 || id == 0x94)) {
        va_list args;
        va_start(args, id);
        TbUniStringVTag* tag = va_arg(args, TbUniStringVTag*);
        va_end(args);
        int64_t t[6];
        TextInts(tag->mText, t);
        ev("FormatString", {lab(this), size, id, tag->mMagic, tag->mId, t[0], t[1], t[2], t[3], t[4], t[5]});
    } else {
        ev("FormatString", {lab(this), size, id});
    }
    return 0;
}

int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }   // never called; emits the vtable

void SetHelpTextString(char16_t* text) {
    int64_t t[6];
    TextInts(text, t);
    ev("SetHelpTextString", {t[0], t[1], t[2], t[3], t[4], t[5]});
}

extern "C" {

uint64_t tpw_ciw_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"panel", &gRideListPanel},
        {"state", &gHelpTextState},
        {"f88", &gRideListF88},
        {"world", &gGameWorld},
        {"text7", &TextObject().mResourcePointers[7]},
        {"text21", &TextObject().mResourcePointers[21]},
        {"ridedb", &RideDatabaseObject()},
        {"ride46", &Ride()->Value(kRideSlot_GoldenTicketCost).mInteger},
        {"ride33", &Ride()->Value(kRideSlot_RideTypeStringIndex).mInteger},
        {"ctrlcost", &Ctrl()->mUpgradeCost},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ciw_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ciw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradeCost));
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_ciw_change(int32_t type) { ChangeInfoWindow(static_cast<UIObjType>(type)); }

void tpw_ciw_update(uint32_t id) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> tag{static_cast<uint16_t>(id)};
    UpdateHelpText(tag);
}

}  // extern "C"
