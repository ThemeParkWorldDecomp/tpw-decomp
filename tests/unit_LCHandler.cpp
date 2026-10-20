// Test library for the ride list panel's row functions (unit v5231; libtpw_unit_LCHandler.dylib, tests/test_LCHandler.py), main app:
//   src/CreateRideListPanel_LCHandler.cpp   RideListLCHandler, RideListLCEntryHandler, RideListLCFormat, RideListLCPostFormat and the two
//                                           globals gRideListFontIndex and gRideListRowHeight it defines
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The objects the
// functions are given (windows, fonts, the world, the bank account) are bare addresses nobody dereferences; the ride record and the object
// control are real blocks (the translation reads them). The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gRideListPanel;   // 0x10395998
CWorld* gGameWorld;                // 0x1037ebfc
int16_t gUIRealScreenHeight;       // 0x10395bba
// The ride database (0x1037f6c8): only its address matters; the class has a constructor the library does not compile.
alignas(16) char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

namespace {

// The addresses of the objects the functions are given (the same numbers as the module's); the host never dereferences them.
constexpr uintptr_t kFont = 0x30003000, kChild = 0x30004000, kNext = 0x30005000, kDraw = 0x30006000, kCreated = 0x30007000, kBank = 0x30009000;
constexpr uintptr_t kWorld = 0x20000000;

alignas(16) char gControlBlock[0x20];
alignas(16) char gRideBlock[sizeof(RideInfo) + 16];

// The offset of an object from the world, 32 bits (the Mac's addresses are words).
int64_t Off(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gGameWorld))); }

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void ChangeInfoWindow(UIObjType type) { ev("ChangeInfoWindow", {static_cast<int64_t>(type)}); }
void FillRideList(UIObjType type, int32_t refresh) { ev("FillRideList", {static_cast<int64_t>(type), refresh}); }
const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {id});
    return reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(static_cast<uint32_t>(ret("GetString"))));
}
void UIMain_SetTitleText(InterfaceWindow* window, int32_t id, char16_t* text) {
    ev("SetTitleText", {lab(window), id, static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(text)))});
}
InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {static_cast<int64_t>(index)});
    return reinterpret_cast<InterfaceFont*>(kFont);
}
int32_t InterfaceFont::GetHeight() {
    ev("GetHeight", {lab(this)});
    return static_cast<int32_t>(ret("GetHeight"));
}

int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("CallDefault", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("CallDefault"));
}
void InterfaceWindow::SetFontColour(int32_t red, int32_t green, int32_t blue, int32_t alpha) {
    ev("SetFontColour", {lab(this), red, green, blue, alpha});
}
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback callback) {
    ev("SetCallback", {lab(this), callback == RideListLCEntryHandler ? 1 : 0});
}
InterfaceWindow* InterfaceWindow::CreateObject(int32_t kind, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right,
                                               int16_t bottom) {
    ev("CreateObject", {lab(this), kind, attributes, id, left, top, right, bottom});
    return reinterpret_cast<InterfaceWindow*>(kCreated);
}
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetDrawObject(InterfaceDrawObject* object) { ev("SetDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }
InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return reinterpret_cast<InterfaceWindow*>(kChild);
}
InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    return reinterpret_cast<InterfaceWindow*>(kNext);
}
InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", {}, {name});
    return reinterpret_cast<InterfaceDrawObject*>(kDraw);
}

ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    ev("GetObjectControl", {Off(this), type.mIndex, flag});
    return reinterpret_cast<ObjectControl*>(gControlBlock);
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {Off(this)});
    return reinterpret_cast<CBankAccount*>(kBank);
}
int32_t CBankAccount::GetBalance() const {
    ev("GetBalance", {lab(this)});
    return static_cast<int32_t>(ret("GetBalance"));
}
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {this == reinterpret_cast<CRideDatabase*>(gRideDatabaseStorage) ? 1 : 0, id.mIndex});
    return reinterpret_cast<RideInfo*>(gRideBlock);
}
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("HasBeenBought", {Off(this), type.mIndex});
    return ret("HasBeenBought") != 0;
}
bool CGoldenTicketControl::CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("CanIAfford", {Off(this), type.mIndex});
    return ret("CanIAfford") != 0;
}

namespace lender_callee {
void* OperatorNew(uint32_t size) { return tpw::ArenaAllocZeroed(size); }
void SpriteCtor(void* mem) { ev("SpriteCtor", {newlab(mem)}); }
void WindowCtor(void* mem, InterfaceWindow* parent, int32_t attributes, int32_t id, int16_t left, int16_t top, int16_t right, int16_t bottom) {
    ev("WindowCtor", {newlab(mem), lab(parent), attributes, id, left, top, right, bottom});
}
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("SetValue", {lab(window), value}); }
}  // namespace lender_callee

// The constructor of InterfaceCashAndGTickTextObject (0x1018e190), faked under its mangled names (the class's vtable is not in this library).
extern "C" {
void FakeCashCtorC1(void* self, void* data) __asm__("__ZN31InterfaceCashAndGTickTextObjectC1EPv");
void FakeCashCtorC2(void* self, void* data) __asm__("__ZN31InterfaceCashAndGTickTextObjectC2EPv");
void FakeCashCtorC1(void* self, void*) { ev("CashCtor", {newlab(self)}); }
void FakeCashCtorC2(void* self, void*) { ev("CashCtor", {newlab(self)}); }
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_lc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gRideListPanel", &gRideListPanel},
        {"gGameWorld", &gGameWorld},
        {"gUIRealScreenHeight", &gUIRealScreenHeight},
        {"gRideListFontIndex", &gRideListFontIndex},
        {"gRideListRowHeight", &gRideListRowHeight},
        {"gRideDatabase", gRideDatabaseStorage},
        {"ctrl", gControlBlock},
        {"ride", gRideBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// The offsets the module relies on (HOST_LAYOUT): the golden ticket slot in the ride record (the Mac's 0xc4), the upgrade cost in a control (4).
int32_t tpw_lc_layout(int64_t* out) {
    int32_t n = 0;
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRideBlock);
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&ride->Value(kRideSlot_GoldenTicketCost).mInteger) - reinterpret_cast<char*>(ride));
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mUpgradeCost));
    return n;
}

int32_t tpw_lc_handler(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return RideListLCHandler(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
int32_t tpw_lc_entry(uint64_t window, int32_t message, int32_t a, int32_t b) {
    return RideListLCEntryHandler(reinterpret_cast<InterfaceWindow*>(window), message, a, b);
}
int64_t tpw_lc_format() { return static_cast<int64_t>(lab(RideListLCFormat())); }
void tpw_lc_post(uint64_t window) { RideListLCPostFormat(reinterpret_cast<InterfaceWindow*>(window)); }

}  // extern "C"
