// Test library for the park pointer (unit w142; libtpw_unit_ParkMouse.dylib, tests/test_ParkMouse.py), main app:
//   src/game/ui/ui_main.cpp   ContextMouse 0x10182548
//   src/game/Mouse.cpp    CMouse::ImmediateMouseHandler 0x1007e094, CMouse::ProcessIngameMouse 0x1007de84, CMouse::DoubleClick 0x1007e248
//                             and the two button flags gMouseLeftDown 0x102dc1ec, gMouseRightDown 0x102dc1e8
// Every callee outside the unit is a fake that records the event the PowerPC stub records: the mouse and its modes (a fake mode object whose
// virtuals record), the cell predicates, the object, staff and map members, the text table (GetConstString, FormatString), the help text, the
// cursor, the keyboard, GAME_ASSERTMSG and the nothrow operator new (sizes recorded as the Mac's: 1, 8 and 0xc). The globals of other files
// the unit reads are defined here, zero at start like the Mac's bss. Answers come from the case's queues.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include "Debug.h"
#include "InterfaceFont.h"
#include "InterfaceFontWrapper.h"
#include "TbTextResourceManager.h"
#include "TbFontRenderParams.h"
#include "LLCellSystem.h"
#include "MapCell.h"
#include "Mouse.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbAudioSystem.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TrackCell.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "intrface.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "ui_tooltip.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
CMouse* gMouse;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
int32_t gGameWinWordC34;
int32_t gGameWinWordC38;
int32_t gRideHighlightKeep;
char16_t gUIMainConvertBuffer[0x4000];
CThingArrayEntry gThingArray[kThingArraySize];
CText gText;
CIntrface gIntrface;
CWorld* gGameWorld;
namespace SamsUtilities {
UKeyboard* gKeyboard;
}
InterfaceFont::InterfaceFont() {}
InterfaceFont::~InterfaceFont() {}
TbTextResourceManager::TbTextResourceManager() {}
TbTextResourceManager::~TbTextResourceManager() {}
InterfaceFontWrapper::InterfaceFontWrapper(TbIFont* font) : mFont(font) {}
InterfaceFontWrapper::~InterfaceFontWrapper() {}
TbFontRenderParams::~TbFontRenderParams() {}
TbFontRenderParams::TbFontRenderParams() {}

namespace {
void* gObjs[64];         // label -> host address (the test objects)
void* gSelected;         // what UI_GetSelectedObject answers
alignas(16) unsigned char gRideStore[sizeof(RideInfo) + 64];
alignas(16) unsigned char gMouseStore[16];
alignas(16) unsigned char gTableStore[64];
char gNarrow[32];

// The mouse modes' virtuals record; GetMode answers the queue.
class FakeMode : public CMouseMode {
public:
    void OnLeftClick(int32_t x, int32_t y) override { ev("OnLeftClick", {lab(this), x, y}); }
    void OnLeftRelease(int32_t x, int32_t y) override { ev("OnLeftRelease", {lab(this), x, y}); }
    void OnRightClick(int32_t x, int32_t y) override { ev("OnRightClick", {lab(this), x, y}); }
    void OnRightRelease(int32_t x, int32_t y) override { ev("OnRightRelease", {lab(this), x, y}); }
    void OnDoubleClick(int32_t x, int32_t y) override { ev("OnDoubleClick", {lab(this), x, y}); }
    void OnMouseMove(int32_t a, int32_t b, int32_t c, int32_t d) override { ev("OnMouseMove", {lab(this), a, b, c, d}); }
    void WhileLeftDown(int32_t x, int32_t y) override { ev("WhileLeftDown", {lab(this), x, y}); }
    void WhileRightDown(int32_t x, int32_t y) override { ev("WhileRightDown", {lab(this), x, y}); }
    int32_t GetMode() override {
        ev("GetMode", {lab(this)});
        return static_cast<int32_t>(ret("GetMode", 0));
    }
    void OnAssign() override {}
};
FakeMode gFakeMode;

// The preset keyboard and the ones the nothrow new makes both answer GetModifiers from the queue "Modifiers".
class FakeKeyboard : public SamsUtilities::UKeyboard {
public:
    uint32_t GetModifiers() override {
        ev("Keyboard.GetModifiers", {lab(this)});
        return static_cast<uint32_t>(ret("Modifiers", 0));
    }
};
alignas(16) unsigned char gProtoStore[sizeof(FakeKeyboard)];
FakeKeyboard* gProto;
alignas(16) unsigned char gKeyboardStore[sizeof(FakeKeyboard)];

std::string Text16(const char16_t* text) { return text == nullptr ? std::string("(null)") : narrow(text); }
}  // namespace

// ---- the nothrow new: the answer is the queue "New" (0 a NULL). The size is recorded as the Mac's.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    int64_t mac = static_cast<int64_t>(size);
    if (size == sizeof(CMouse)) mac = 1;
    else if (size == sizeof(CMMDefault)) mac = 8;
    else if (size == sizeof(SamsUtilities::UKeyboard)) mac = 0xc;
    void* block = nullptr;
    if (ret("New", 1) != 0) {
        block = tpw::ArenaAllocZeroed(size);
        std::memset(block, 0xa5, size);
        newlab(block);
    }
    ev("operator_new", {mac, lab(block)});
    return block;
}

// ---- the keyboard
namespace SamsUtilities {
UKeyboard::UKeyboard() {
    // The object made by the unit's new is given the fake's vtable, so that GetModifiers records and answers like the Mac's fake.
    if (gProto != nullptr) std::memcpy(static_cast<void*>(this), static_cast<void*>(gProto), sizeof(void*));
    ev("UKeyboard.ctor", {lab(this)});
}
uint8_t UKeyboard::IsOptionDown() { return 0; }
uint8_t UKeyboard::IsCommandDown() { return 0; }
}  // namespace SamsUtilities

// ---- the mouse
void CMouse::AssignMouseMode(CMouseMode* mode) { ev("AssignMouseMode", {lab(this), lab(mode)}); }
CMMDefault::CMMDefault() { ev("CMMDefault.ctor", {lab(this)}); }
CMMDefault::~CMMDefault() {}
void CMMDefault::OnLeftClick(int32_t, int32_t) {}
void CMMDefault::OnRightClick(int32_t, int32_t) {}
int32_t CMMDefault::GetMode() { return 1; }
void CMMDefault::OnAssign() {}

// ---- the cells, the map, the objects
void HighlightRide() { ev("HighlightRide"); }
void LLCR_OverRide(uint16_t* out, int32_t x, int32_t y) {
    ev("LLCR_OverRide", {x, y});
    *out = static_cast<uint16_t>(ret("Override", 0));
}
int32_t LLCC_SystemGetMode() {
    ev("LLCC_SystemGetMode");
    return static_cast<int32_t>(ret("LLCC_SystemGetMode", 0));
}
int32_t CMapCell::IsEntrance() const { ev("IsEntrance", {lab(this)}); return static_cast<int32_t>(ret("IsEntrance", 0)); }
int32_t CMapCell::IsExit() const { ev("IsExit", {lab(this)}); return static_cast<int32_t>(ret("IsExit", 0)); }
int32_t CMapCell::IsRide() const { ev("IsRide", {lab(this)}); return static_cast<int32_t>(ret("IsRide", 0)); }
int32_t CMapCell::IsGrass() const { ev("IsGrass", {lab(this)}); return static_cast<int32_t>(ret("IsGrass", 0)); }
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CMapCell::GetObjectThingId() const {
    ev("GetObjectThingId", {lab(this)});
    return {static_cast<uint16_t>(ret("ThingId", 0))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetParkEntrancePos(int32_t which) const {
    ev("GetParkEntrancePos", {lab(this), which});
    return {static_cast<uint16_t>(ret("Entrance", 0))};
}
tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {lab(this), which});
    return {static_cast<uint16_t>(ret("Booth", 0))};
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return reinterpret_cast<RideInfo*>(gRideStore);
}
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    ev("UpgradesWhat", {lab(this)});
    return {static_cast<uint16_t>(ret("Upgrades", 0))};
}
void CObject::GetUIName(char16_t* out, uint32_t size) {
    ev("GetUIName", {lab(this), lab(out), size});
    const char16_t name[] = u"UIName";
    for (int i = 0; i < 7; ++i) out[i] = name[i];
}
void CStaff::BeingPointedAt() { ev("BeingPointedAt", {lab(this)}); }
const char16_t* CStaff::GetName() const {
    ev("StaffGetName", {lab(this)});
    static const char16_t name[] = u"StaffName";
    return name;
}

// ---- the text
const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    std::snprintf(gNarrow, sizeof gNarrow, "cs%x", id);
    return reinterpret_cast<const unsigned char*>(gNarrow);
}
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    std::string shown;
    if (id == 0x226 || id == 0x22b || id == 0x244) {
        va_list args;
        va_start(args, id);
        const auto* tag = va_arg(args, const TbUniStringVTag*);
        va_end(args);
        shown = Text16(tag->mText);
        ev("FormatString", {lab(this), lab(out), size, id, tag->mMagic, tag->mId}, {shown.c_str()});
    } else if (id == 0x229) {
        va_list args;
        va_start(args, id);
        const auto* tag = va_arg(args, const TbIntegerVTag*);
        va_end(args);
        ev("FormatString", {lab(this), lab(out), size, id, tag->mMagic, tag->mId, tag->mValue});
    } else {
        ev("FormatString", {lab(this), lab(out), size, id});
    }
    char head[24];
    std::snprintf(head, sizeof head, "F%x", id);
    std::string all = std::string(head) + (shown.empty() ? "" : ":" + shown);
    uint32_t n = 0;
    while (n < all.size() && n + 1 < size) { out[n] = static_cast<char16_t>(all[n]); ++n; }
    out[n] = 0;
    return static_cast<int32_t>(n);
}
int32_t TbUniStringVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
int32_t TbIntegerVTag::Format(CStaticBuffer&, TbVarFormat&) { return 0; }
char16_t* UIMain_ConvertMulti(unsigned char* text) {
    const char* narrowText = reinterpret_cast<const char*>(text);
    ev("UIMain_ConvertMulti", {}, {narrowText});
    uint32_t n = 0;
    while (narrowText[n] != 0 && n < 0x1ff) { gUIMainConvertBuffer[n] = static_cast<char16_t>(static_cast<unsigned char>(narrowText[n])); ++n; }
    gUIMainConvertBuffer[n] = 0;
    return gUIMainConvertBuffer;
}
void SetHelpTextString(char16_t* text) {
    std::string shown = Text16(text);
    ev("SetHelpTextString", {lab(text)}, {shown.c_str()});
}
void* UI_GetSelectedObject() {
    ev("UI_GetSelectedObject");
    return gSelected;
}
void UI_SetGameWindowCursor(int16_t cursor) { ev("UI_SetGameWindowCursor", {cursor}); }
int32_t StaffPanelUp() {
    ev("StaffPanelUp");
    return static_cast<int32_t>(ret("StaffPanelUp", 0));
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    char shown[160];
    std::snprintf(shown, sizeof shown, "%.159s", format);
    ev("GAME_ASSERTMSG", {condition}, {shown});
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_pm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"buffer", gUIMainConvertBuffer},
        {"world", gGameWorld},
        {"map", gGameWorld == nullptr ? nullptr : static_cast<const void*>(gGameWorld->GetMap())},
        {"table", gTableStore},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// Once per process: the world (zero, never constructed) and the test objects; the fakes' prototype keyboard.
void tpw_pm_init() {
    if (gGameWorld == nullptr) gGameWorld = static_cast<CWorld*>(tpw::ArenaAllocZeroed(sizeof(CWorld)));
    if (gProto == nullptr) {
        gProto = new (gProtoStore) FakeKeyboard;
    }
    gText.mResourcePointers[7] = gTableStore;
}

// A zeroed block of the pool for this label (allocated on first use, zeroed again at every call).
uint64_t tpw_pm_obj(int32_t label, uint32_t size) {
    if (label == 8) { std::memset(gMouseStore, 0xa5, sizeof gMouseStore); return reinterpret_cast<uintptr_t>(gMouseStore); }
    if (label == 9) return reinterpret_cast<uintptr_t>(&gFakeMode);
    if (label == 12) { std::memcpy(gKeyboardStore, gProtoStore, sizeof gKeyboardStore); return reinterpret_cast<uintptr_t>(gKeyboardStore); }
    if (gObjs[label] == nullptr) gObjs[label] = tpw::ArenaAllocZeroed(size);
    std::memset(gObjs[label], 0, size);
    return reinterpret_cast<uintptr_t>(gObjs[label]);
}

// The fixed state of a case: key numbers are the Python module's.
void tpw_pm_set(int32_t key, int64_t value, int64_t other) {
    switch (key) {
        case 0: SamsUtilities::gKeyboard = reinterpret_cast<SamsUtilities::UKeyboard*>(value); break;
        case 1: gMouse = reinterpret_cast<CMouse*>(value); break;
        case 2: gMouseMode = reinterpret_cast<CMouseMode*>(value); break;
        case 3: gMouseWord1E0 = static_cast<int32_t>(value); break;
        case 4: gMouseWord1E4 = static_cast<int32_t>(value); break;
        case 5: gMouseLeftDown = static_cast<int32_t>(value); break;
        case 6: gMouseRightDown = static_cast<int32_t>(value); break;
        case 7: gRideHighlightKeep = static_cast<int32_t>(value); break;
        case 8: gGameWinWordC34 = static_cast<int32_t>(value); break;
        case 9: gGameWinWordC38 = static_cast<int32_t>(value); break;
        case 10: gIntrface.mPointedStaffOrGuest = reinterpret_cast<CThing*>(value); gIntrface.mF24 = static_cast<uint16_t>(other); break;
        case 11: gThingArray[value].mThing = reinterpret_cast<void*>(other); break;
        case 12: gSelected = reinterpret_cast<void*>(value); break;
        case 13: {
            RideInfo* ride = reinterpret_cast<RideInfo*>(gRideStore);
            ride->mUIType = static_cast<int32_t>(value);
            ride->Value(kRideSlot_UpgradeType).mInteger = static_cast<int32_t>(other);
            break;
        }
        case 14: std::memset(gUIMainConvertBuffer, 0x77, 0x100); break;
        case 15: std::memset(gThingArray, 0, sizeof(CThingArrayEntry) * 16); break;
        default: break;
    }
}

// The state read back: the words as labels or numbers (the order the Python module reads them).
void tpw_pm_dump(int64_t* out) {
    int n = 0;
    out[n++] = lab(SamsUtilities::gKeyboard);
    out[n++] = lab(gMouse);
    out[n++] = lab(gMouseMode);
    out[n++] = gMouseWord1E0;
    out[n++] = gMouseWord1E4;
    out[n++] = gMouseLeftDown;
    out[n++] = gMouseRightDown;
    out[n++] = gRideHighlightKeep;
    out[n++] = static_cast<uint32_t>(gGameWinWordC34);
    out[n++] = gGameWinWordC38;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_pm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMapCell));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mMapCells));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mType));
    out[n++] = static_cast<int64_t>(offsetof(CCellBase, mParentID));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mUIType));
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRideStore);
    out[n++] = static_cast<int64_t>(reinterpret_cast<unsigned char*>(&ride->Value(kRideSlot_UpgradeType).mInteger) - gRideStore);
    return n;
}

// ---- the functions
void tpw_pm_context(int32_t context) { ContextMouse(context); }
void tpw_pm_immediate(int32_t x, int32_t y) { reinterpret_cast<CMouse*>(gMouseStore)->ImmediateMouseHandler(x, y); }
void tpw_pm_ingame(int32_t key, int32_t down, int32_t x, int32_t y) { CMouse::ProcessIngameMouse(key, down, x, y, nullptr); }
void tpw_pm_double(int32_t x, int32_t y) { reinterpret_cast<CMouse*>(gMouseStore)->DoubleClick(x, y); }

}  // extern "C"
