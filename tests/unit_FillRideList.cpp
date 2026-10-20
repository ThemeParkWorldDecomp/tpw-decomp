// Test library for the ride list panel's FillRideList and UpdateViewWindow (unit v5363; libtpw_unit_FillRideList.dylib,
// tests/test_FillRideList.py), main app:
//   src/FillRideList.cpp   FillRideList 0x101b5c14, UpdateViewWindow 0x101b5f1c
//
// The callees outside the unit are recording fakes (the same event name and arguments as the PowerPC stubs of the test module). A pointer a
// fake answers is a handle from the case's queue: sPtr[handle] is the block the test built under that number (tpw_fr_setptr). The globals of
// other files the unit reads are defined here; the big ones (gText, gRideDatabase, gMainBalance) are raw storage under the program's symbol
// names so that no constructor is needed.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"

#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "MainBalance.h"
#include "MainWindowCallback.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
CWorld* gGameWorld;
int32_t gRideListType;
InterfaceWindow* gRideListPanel;
alignas(64) char gTextStorage[sizeof(CText)] __asm__("_gText");
alignas(64) char gRideDbStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");
alignas(64) char gBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

namespace {

void* sPtr[64];
char16_t sStrings[5][4];   // the answers of UIMain_GetString (0x86, 0x87, 0x89) and of UIMain_ConvertMulti (4)

int StringSlot(int32_t id) {
    switch (id) {
        case 0x86: return 0;
        case 0x87: return 1;
        case 0x89: return 2;
        default: return -1;
    }
}

template <class T>
T* P(int64_t handle) {
    return (handle >= 0 && handle < 64) ? static_cast<T*>(sPtr[handle]) : nullptr;
}

int64_t TextHash(const char16_t* text) {
    if (text == nullptr) return -1;
    uint32_t h = 0;
    int64_t n = 0;
    while (text[n] != 0 && n < 130) {
        h = h * 31u + text[n];
        ++n;
    }
    return static_cast<int64_t>(h) ^ (n << 32);
}

void ColourEvent(const char* name, const TbRect* rect, TbColour colour) {
    ev(name, {rect->left, rect->top, rect->right, rect->bottom, colour.mAlpha, colour.mRed, colour.mGreen, colour.mBlue, colour.mIndex});
}

int32_t FakeSetText(InterfaceWindow* self, char16_t* text) {
    ev("SetText", {lab(self), TextHash(text), 1});
    return 1;
}

void FakeBad() { ev("BadVirtual", {}); }

void* sVTable[32];

}  // namespace

// ---- CoolRegion and CoolRect: the virtuals are empty here (CoolRect() and ~CoolRect() are the fakes the test records)
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
CoolRegion::~CoolRegion() {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

// ---- the fakes
CoolRect::CoolRect() { ev("CoolRect", {lab(this)}); }
CoolRect::~CoolRect() { ev("~CoolRect", {lab(this)}); }
TbRenderArea::TbRenderArea() { ev("TbRenderArea", {lab(this)}); }

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return P<CParkAnalyser>(ret("GetParkAnalyser"));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return P<InterfaceWindow>(ret("GetWindow", id, 0));
}

void InterfaceListControl2::RemoveAllItems() { ev("RemoveAllItems", {lab(this)}); }

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    ev("AddItem", {lab(this), TextHash(item[0].mText), item[1].mInt, item[2].mInt, data, position, update});
    return 0;
}

void InterfaceListControl2::UpdateDisplay() { ev("UpdateDisplay", {lab(this)}); }

RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    return P<RideInfo>(ret("GetFirstRide"));
}

RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    return P<RideInfo>(ret("GetNextRide"));
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return P<RideInfo>(ret("GetRideInfo", id.mIndex, 0));
}

void CRideDatabase::GetTypeName(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetTypeName", {lab(this), id, size});
    buffer[0] = static_cast<char16_t>((id ^ 0x5a5a) & 0xffff);
    buffer[1] = 0x41;
    buffer[2] = 0;
}

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {lab(this), type.mIndex, flag});
    return P<ObjectControl>(ret("GetObjectControl", type.mIndex, 0));
}

bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("HasBeenBought", {lab(this), type.mIndex});
    return ret("HasBeenBought", type.mIndex, 0) != 0;
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObjectInPark", {lab(this), id.mIndex});
    return static_cast<int32_t>(ret("NumberOfObjectInPark", id.mIndex, 0));
}

tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CParkAnalyser::LastOfObjectTypeResearched(UIObjType type, int32_t back) const {
    ev("LastOfObjectTypeResearched", {lab(this), type, back});
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> answer;
    answer.mIndex = static_cast<uint16_t>(ret("Last", back, 0));
    return answer;
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {id});
    int slot = StringSlot(id);
    return slot < 0 ? nullptr : sStrings[slot];
}

char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    ev("UIMain_ConvertMulti", {lab(text)});
    return sStrings[3];
}

int32_t InterfaceWindow::GetClientRect(CoolRect*) {
    ev("GetClientRect", {lab(this)});
    return 0;
}

uint8_t* InterfaceWindow::GetClientObjectSurface(int32_t* width, int32_t* height) {
    ev("GetClientObjectSurface", {lab(this)});
    *width = static_cast<int32_t>(ret("SurfW"));
    *height = static_cast<int32_t>(ret("SurfH"));
    return P<uint8_t>(ret("Surface"));
}

void InterfaceWindow::FinishWithClientObjectSurface() { ev("FinishWithClientObjectSurface", {lab(this)}); }

void MakeRenderArea(uint8_t* pixels, TbRenderArea* area, int32_t width, int32_t height) {
    ev("MakeRenderArea", {lab(pixels), lab(area), width, height});
}

void LbDraw_SetCurrent(TbRenderArea* area) { ev("LbDraw_SetCurrent", {lab(area)}); }

void LbDraw_RectangleFilled(const TbRect* rect, TbColour colour) { ColourEvent("LbDraw_RectangleFilled", rect, colour); }

void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) { ev("UIMain_ShowRidePreview", {lab(&rect), id, z}); }

const unsigned char* TbITable::GetConstString(uint32_t id) const {
    ev("GetConstString", {lab(this), id});
    return P<unsigned char>(ret("GetConstString", id, 0));
}

int32_t* LLCC_GetRideProfileInfo(void* ride) {
    ev("LLCC_GetRideProfileInfo", {lab(ride)});
    return P<int32_t>(ret("Profile"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_fr_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gameworld", &gGameWorld},
        {"rdb", gRideDbStorage},
        {"textptr", &reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[21]},
        {"ltype", &gRideListType},
        {"panel", &gRideListPanel},
        {"bal", &reinterpret_cast<CMainBalance*>(gBalanceStorage)->mValues[(0x5cc - 8) / 4]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable).
uint64_t tpw_fr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host offset of a field the module writes by name (the Mac's offsets are the module's own table).
int64_t tpw_fr_off(const char* name) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(0x1000);
    auto off = [&](const void* p) { return static_cast<int64_t>(reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(ride)); };
    if (std::strcmp(name, "id") == 0) return off(&ride->mId);
    if (std::strcmp(name, "cost") == 0) return off(&ride->Value(kRideSlot_GoldenTicketCost));
    if (std::strcmp(name, "typestr") == 0) return off(&ride->Value(kRideSlot_RideTypeStringIndex));
    if (std::strcmp(name, "flag") == 0) return off(&ride->Value(static_cast<RideBalanceSlot>(158)));
    if (std::strcmp(name, "uitype") == 0) return off(&ride->mUIType);
    if (std::strcmp(name, "failed") == 0) return off(&ride->mLoadFailed);
    if (std::strcmp(name, "typeno") == 0) return off(&ride->mRideTypeNumber);
    if (std::strcmp(name, "z") == 0) return static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    return -1;
}

// The layout the module relies on (HOST_LAYOUT).
int32_t tpw_fr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradeCost));
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mAvailable));
    return n;
}

// The handle table and the strings the fakes answer.
void tpw_fr_setptr(uint32_t handle, uint64_t address) {
    if (handle < 64) sPtr[handle] = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
}

void tpw_fr_init() {
    const int keys[5] = {0x86, 0x87, 0x89, 0x1000, 0};
    for (int i = 0; i < 4; ++i) {
        sStrings[i][0] = static_cast<char16_t>(0x6000 + keys[i]);
        sStrings[i][1] = static_cast<char16_t>(0x7000 + (keys[i] & 0xff));
        sStrings[i][2] = 0;
    }
    for (int i = 0; i < 32; ++i) sVTable[i] = reinterpret_cast<void*>(&FakeBad);
    sVTable[10] = reinterpret_cast<void*>(&FakeSetText);   // SetText: the 11th slot (the destructor takes two)
}

// A window block whose virtual calls go to the fakes.
void tpw_fr_setup_vt(uint64_t address) { *reinterpret_cast<void***>(static_cast<uintptr_t>(address)) = sVTable; }

// ---- the functions
void tpw_fr_fill(int32_t type, int32_t refresh) { FillRideList(static_cast<UIObjType>(type), refresh); }

void tpw_fr_view(uint64_t idAddress) {
    UpdateViewWindow(*reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>*>(static_cast<uintptr_t>(idAddress)));
}

}  // extern "C"
