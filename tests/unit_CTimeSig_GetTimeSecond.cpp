// Test library for the small stand-ins of unit v51118 (libtpw_unit_CTimeSig_GetTimeSecond.dylib, tests/test_CTimeSig_GetTimeSecond.py), main app:
//   src/game/park/SkyObjects.cpp, src/game/save/SaveLoad.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/MessageCentre.cpp   CSkyObjects, CSaveLoad logs, InterfaceFontWrapper::GetHeight, destructors, constructors.
//
// The callees outside the unit are recording fakes (the same event names and arguments as the PowerPC stubs); the globals of other files
// (gWorld, gClock) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "World.h"
#include "TimeSig.h"
#include "CBalanceTable.h"
#include "Clock.h"
#include "FS_MasterSave.h"
#include "InterfaceFontWrapper.h"
#include "MsgCheatMode.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "SkyObjects.h"
#include "TPEngine.h"
#include "TbResource.h"
#include "ThingReceivers.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld = nullptr;
CVariableTime::CVariableTime() {}
CFixedTime::CFixedTime() {}   // the clock's members are built by other files' constructors; only the address of gClock matters here
CClock gClock;

// ---- the fakes
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {lab(this)});
    return *gWorld;
}
void CClock::PreSave() { ev("PreSave", {lab(this)}); }
void CBalanceTable::UnInit() { ev("UnInit", {lab(this)}); }
void CRideDatabase::ClearDatabase() { ev("ClearDatabase", {lab(this)}); }
void WStringListClear(MacWStringList* list) { ev("ListClear", {lab(list)}); }
namespace v51118 {
void OperatorDelete(void* block) { ev("delete", {lab(block)}); }
void KeyStringDtor(void* key, int16_t flag) { ev("StrDtor", {lab(key), flag}); }
void SetTreeDestroy(MacU16Set* set, uint32_t node) { ev("SetTreeDestroy", {lab(set), node}); }
}  // namespace v51118
uint8_t fontwrapper_callee::GetVerticalSpacing(TbIFont* font) {
    ev("GetVerticalSpacing", {lab(font)});
    return static_cast<uint8_t>(ret("GetVerticalSpacing"));
}
// The messages' vtable needs these two (defined by other files in the program).
CMsgCheatMode::~CMsgCheatMode() {}
int32_t CMsgCheatMode::GetType() const { return 0; }

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_v51118_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gClock", &gClock},
        {"gWStringListCtorWord", &gWStringListCtorWord},
        {"gParkTreeCtorWord", &gParkTreeCtorWord},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51118_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// 0 msg flags, 1 cheat payload, 2 research payload, 3 font pointer, 4..7 tree count/root/slot8/begin, 8 sizeof(CMsgCheatMode),
// 9 sizeof(CMsgResearchCategoryUpdated), 10 sizeof(ParkThemeTree), 11 offsetof(CWorld, mTurn)
int32_t tpw_v51118_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CMsgCheatMode, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CMsgCheatMode, mCheating));
    out[n++] = static_cast<int64_t>(offsetof(CMsgResearchCategoryUpdated, mCategory));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceFontWrapper, mFont));
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mCount));
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mSlot8));
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mBegin));
    out[n++] = static_cast<int64_t>(sizeof(CMsgCheatMode));
    out[n++] = static_cast<int64_t>(sizeof(CMsgResearchCategoryUpdated));
    out[n++] = static_cast<int64_t>(sizeof(ParkThemeTree));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    return n;
}

// One entry for every function, by its index in the test's KINDS; the arguments are words (flags as the low 16 bits).
uint64_t tpw_v51118_call(int32_t index, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    auto p = [](uint64_t v) { return reinterpret_cast<void*>(static_cast<uintptr_t>(v)); };
    auto flag = [](uint64_t v) { return static_cast<int16_t>(v); };
    uintptr_t self = static_cast<uintptr_t>(a);
    switch (index) {
        case 0: return static_cast<CTimeSig*>(p(a))->GetTimeSecond();
        case 1: return static_cast<uint32_t>(static_cast<CTimeSig*>(p(a))->TurnsSinceLastUpdate());
        case 2: static_cast<CTimeSig*>(p(a))->SetTimeSig(); return 0;
        case 3: return reinterpret_cast<uintptr_t>(v51118::SkyObjectsDtor(static_cast<CSkyObjects*>(p(a)), flag(b)));
        case 4: static_cast<CSaveLoad*>(p(a))->PreSaveOperations(); return 0;
        case 5: static_cast<CSaveLoad*>(p(a))->WriteSerialiseLog(static_cast<int32_t>(b), static_cast<const char*>(p(c)),
                                                                static_cast<CSaveLoad::LogfileState>(static_cast<int32_t>(d))); return 0;
        case 6: static_cast<CSaveLoad*>(p(a))->CloseLogfile(); return 1;
        case 7: static_cast<CSaveLoad*>(p(a))->OpenLogfile(static_cast<int32_t>(b)); return 0;
        case 8: return static_cast<uint32_t>(static_cast<InterfaceFontWrapper*>(p(a))->GetHeight());
        case 9: return reinterpret_cast<uintptr_t>(v51118::U16SetDtor(static_cast<MacU16Set*>(p(a)), flag(b)));
        case 10: return reinterpret_cast<uintptr_t>(v51118::WStringListDtor(static_cast<MacWStringList*>(p(a)), flag(b)));
        case 11: return reinterpret_cast<uintptr_t>(v51118::RideDatabaseDtor(static_cast<CRideDatabase*>(p(a)), flag(b)));
        case 12: return reinterpret_cast<uintptr_t>(v51118::RideBalanceDtor(static_cast<CRideBalance*>(p(a)), flag(b)));
        case 13: ParkThemeTreeCallee::PairDtor(static_cast<ParkThemeTree::Pair*>(p(a)), flag(b)); return self;
        case 14: new (p(a)) CMsgCheatMode(static_cast<bool>(b)); return self;
        case 15: new (p(a)) CMsgResearchCategoryUpdated(static_cast<ResearchCat>(static_cast<int32_t>(b))); return self;
        case 16: v51118::WStringListCtor(static_cast<MacWStringList*>(p(a))); return self;
        case 17: ParkThemeTreeCallee::NodeDeallocate(static_cast<ParkThemeTree*>(p(a)), static_cast<ParkThemeTree::node*>(p(b)),
                                                      static_cast<unsigned long>(c)); return self;
        case 18: ParkThemeTreeCallee::PairDestroy(static_cast<ParkThemeTree*>(p(a)), static_cast<ParkThemeTree::Pair*>(p(b))); return self;
        case 19: v51118::ParkThemeTreeCtor(static_cast<ParkThemeTree*>(p(a)), static_cast<const uint8_t*>(p(b))); return self;
    }
    return 0;
}

}  // extern "C"
