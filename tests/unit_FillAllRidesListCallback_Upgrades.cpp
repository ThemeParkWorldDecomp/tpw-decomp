// Test library for the rides upgrades / shops / features list rows, AllKidsPanel_KidGone and the kids list's LCPostFormat (unit v5952;
// libtpw_unit_FillAllRidesListCallback_Upgrades.dylib, tests/test_FillAllRidesListCallback_Upgrades.py), main app:
//   src/game/ui/ui_allItemsPanel.cpp, src/game/ui/ui_allKidsPanel.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The objects
// the functions are given are 16 arena blocks the test sets (tpw_fav_setblock); a pointer is logged as its block's number.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "Debug.h"
#include "History.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files, defined here
InterfaceListControl2* gAllItemsList = nullptr;
int32_t gAllKidsCounter = 0;
InterfaceWindow* gAllKidsMsgWindow = nullptr;
CWorld* gGameWorld = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gAllKidsFontIndex = 7;

namespace {

constexpr int kBlocks = 16;
enum { kWin0 = 0, kDraw = 6, kFont = 7, kText = 8, kWorld = 9, kAnalyser = 10, kList = 11 };
uint8_t* gBlk[kBlocks];
int gCells = 0;

// The block a pointer is in and the offset in it, as one number; 0 for NULL, -1 for any other.
int64_t Where(const void* p) {
    if (p == nullptr) return 0;
    const uint8_t* q = static_cast<const uint8_t*>(p);
    for (int i = 0; i < kBlocks; ++i)
        if (gBlk[i] != nullptr && q >= gBlk[i] && q < gBlk[i] + 0x800) return (i + 1) * 0x1000 + (q - gBlk[i]);
    return -1;
}

// A data word (the arena offset of a block) as the block's number.
int64_t DataLab(int32_t data) {
    if (data == 0) return 0;
    for (int i = 0; i < kBlocks; ++i)
        if (gBlk[i] != nullptr && static_cast<int32_t>(tpw::ArenaOffset(gBlk[i])) == data) return i + 1;
    return -1;
}

template <class T>
T* Blk(int i) {
    return reinterpret_cast<T*>(gBlk[i]);
}

}  // namespace

// ---- the history's sum: the double comes from the queues "sumhi" and "sumlo"
template <>
int32_t CHistory<uint32_t, 30>::GetSumOfLast(double& sum, int32_t count) const {
    ev("GetSumOfLast", {Where(this), count});
    uint64_t bits = (static_cast<uint64_t>(static_cast<uint32_t>(ret("sumhi"))) << 32) | static_cast<uint32_t>(ret("sumlo"));
    std::memcpy(&sum, &bits, 8);
    return 0;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    ev("GetUpgradeParent", {lab(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetUpgradeParent"));
    return id;
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

void AddItemToAllRidesList(const CThing* thing, InterfaceListControl2* list, CObject* object) {
    ev("AddItemToAllRidesList", {lab(thing), lab(list), lab(object)});
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this)});
    return nullptr;
}

char16_t* UIMain_GetObjectName(CObject* object) {
    ev("UIMain_GetObjectName", {lab(object)});
    return Blk<char16_t>(kText);
}

int32_t CObject::GetProfitability() const {
    ev("GetProfitability", {lab(this)});
    return static_cast<int32_t>(ret("GetProfitability"));
}

int32_t CObject::GetTotalRevenue() const {
    ev("GetTotalRevenue", {lab(this)});
    return static_cast<int32_t>(ret("GetTotalRevenue"));
}

int32_t CObject::GetCustomerSatisfaction() const {
    ev("GetCustomerSatisfaction", {lab(this)});
    return static_cast<int32_t>(ret("GetCustomerSatisfaction"));
}

int32_t InterfaceListControl2::AddItem(UI_LCAdd* item, int32_t data, int32_t position, int32_t update) {
    if (gCells == 5)
        ev("AddItem", {lab(this), lab(item[0].mText), item[1].mInt, item[2].mInt, item[3].mInt, item[4].mInt, DataLab(data), position, update});
    else
        ev("AddItem", {lab(this), lab(item[0].mText), item[1].mInt, DataLab(data), position, update});
    return 0;
}

CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return Blk<CParkAnalyser>(kAnalyser);
}

int32_t CParkAnalyser::NumberOfObjectInPark(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) const {
    ev("NumberOfObjectInPark", {lab(this), id.mIndex});
    return static_cast<int32_t>(ret("NumberOfObjectInPark"));
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    return Blk<InterfaceWindow>(kList);
}

int32_t InterfaceListControl2::FindData(int32_t data) {
    ev("FindData", {lab(this), DataLab(data)});
    return static_cast<int32_t>(ret("FindData"));
}

int32_t InterfaceListControl2::RemoveItem(int32_t position) {
    ev("RemoveItem", {lab(this), position});
    return 0;
}

// ---- the kids list's attach function
void InterfaceWindow::SetClientDrawObject(InterfaceDrawObject* object) { ev("SetClientDrawObject", {lab(this), lab(object)}); }
void InterfaceWindow::SetFont(InterfaceFont* font) { ev("SetFont", {lab(this), lab(font)}); }
void InterfaceWindow::SetFontJust(int32_t horizontal, int32_t vertical) { ev("SetFontJust", {lab(this), horizontal, vertical}); }

InterfaceWindow* InterfaceWindow::GetChild(int32_t unused) {
    ev("GetChild", {lab(this), unused});
    return Blk<InterfaceWindow>(kWin0 + 1);
}

InterfaceWindow* InterfaceWindow::GetNext() {
    ev("GetNext", {lab(this)});
    int next = 0;
    for (int i = 0; i < 6; ++i)
        if (reinterpret_cast<uint8_t*>(this) == gBlk[i]) next = i + 1;
    return Blk<InterfaceWindow>(next);
}

InterfaceFont* GetFontTableEntry(uint32_t index) {
    ev("GetFontTableEntry", {index});
    return Blk<InterfaceFont>(kFont);
}

void UIMain_IndentClient(InterfaceWindow* window, int16_t a, int16_t b) { ev("UIMain_IndentClient", {lab(window), a, b}); }

InterfaceDrawObject* DrawObjectLibrary::FindDrawObject(char* name) {
    ev("FindDrawObject", std::initializer_list<int64_t>{}, {name});
    return Blk<InterfaceDrawObject>(kDraw);
}

namespace allitems_post_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    if (ret("OperatorNew") == 0) return nullptr;
    return tpw::ArenaAllocZeroed(size);
}
void NumberCtor(void* mem) { ev("NumberCtor", {newlab(mem)}); }
void CashCtor(void* mem) { ev("CashCtor", {newlab(mem)}); }
void GradientCtor(void* mem, const char* mesh) { ev("GradientCtor", {newlab(mem)}, {mesh}); }
void WindowSetValue(InterfaceWindow* window, int32_t value) { ev("WindowSetValue", {lab(window), value}); }
}  // namespace allitems_post_callee

namespace vsp_callee {
void TimeCtor(void* mem) { ev("TimeCtor", {newlab(mem)}); }
}  // namespace vsp_callee

// Only the src file's weak stand-in for VisitorNumCtor calls this; the fake below replaces that stand-in.
namespace lender_callee {
void SpriteCtor(void*) {}
}  // namespace lender_callee

namespace kidlist_callee {
void VisitorNumCtor(void* mem) { ev("VisitorNumCtor", {newlab(mem)}); }
void RectCtor(void*) { ev("CoolRectCtor", {0}); }
void RectDtor(void*) { ev("CoolRectDtor", {0}); }
}  // namespace kidlist_callee

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_fav_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"list", &gAllItemsList},       {"counter", &gAllKidsCounter},   {"msgwin", &gAllKidsMsgWindow},
        {"world", &gGameWorld},         {"font", &gAllKidsFontIndex},    {"noparent", &gAllRidesNoUpgradeParent},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_fav_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_fav_setblock(int32_t index, uint64_t address) { gBlk[index] = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(address)); }

void tpw_fav_setentry(int32_t index, uint64_t pointer) {
    gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(pointer));
}

int32_t tpw_fav_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    return n;
}

uint32_t tpw_fav_rides(uint64_t thing) { return FillAllRidesListCallback_Upgrades(reinterpret_cast<CThing*>(thing)); }

uint32_t tpw_fav_shops(uint64_t thing) {
    gCells = 5;
    return FillAllShopsListCallback(reinterpret_cast<CThing*>(thing));
}

uint32_t tpw_fav_features(uint64_t thing) {
    gCells = 2;
    return FillAllFeaturesListCallback(reinterpret_cast<CThing*>(thing));
}

uint32_t tpw_fav_kidgone(uint64_t thing) {
    AllKidsPanel_KidGone(reinterpret_cast<CThing*>(thing));
    return 0;
}

uint32_t tpw_fav_lcpost(uint64_t window) {
    AllKidsListLCPostFormat(reinterpret_cast<InterfaceWindow*>(window));
    return 0;
}

}  // extern "C"
