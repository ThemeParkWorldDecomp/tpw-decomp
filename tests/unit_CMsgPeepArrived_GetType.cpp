// Test library for twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (unit v51064;
// libtpw_unit_CMsgPeepArrived_GetType.dylib, tests/test_CMsgPeepArrived_GetType.py), main app:
//   src/CPerson_..._LOADVAR_CMsgPeepArrived_GetType.cpp
//
// The callees outside the unit are recording fakes: CDelayedScript::SetSpriteScript, CPerson::SetState, CPerson::ProcessStateWaitingForScriptToAdmitMe,
// CThing::FinalDestroy and the delete the destructors call. gMainBalance is defined here (its table functions are fakes; the values start at zero).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "VehicleSteer.h"
#include "MainBalance.h"

using namespace calllog;

// ---- globals of other files, defined here
CMainBalance gMainBalance;
void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs)
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetSpriteScript", {lab(this), static_cast<int32_t>(script)}); }
void CPerson::SetState(PersonState state) { ev("SetState", {lab(this), static_cast<int32_t>(state)}); }
void CPerson::ProcessStateWaitingForScriptToAdmitMe() { ev("Admit", {lab(this)}); }
void CThing::FinalDestroy() { ev("FinalDestroy", {lab(this)}); }
void FixedSizeArrayOperatorDelete(void* object) { ev("delete", {lab(object)}); }

extern "C" {

uint64_t tpw_v51064_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"bal", &gMainBalance.mValues[0]},
        {"s3A", &gPersonSinitA},
        {"s3B", &gPersonSinitB},
        {"s13A", &gPersonQueueSinitA},
        {"s13B", &gPersonQueueSinitB},
        {"s16A", &gMechanicHQSinitA},
        {"s16B", &gMechanicHQSinitB},
        {"s22A", &gVehicleCallbacksSinitA},
        {"s22B", &gVehicleCallbacksSinitB},
        {"s23A", &gGuardSinitA},
        {"s23B", &gGuardSinitB},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51064_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// message flags offset, message id offset, BehaviourEntry size, BehaviourEntry pointer offset
int32_t tpw_v51064_layout(int64_t* out) {
    int32_t n = 0;
    CMsgPeepArrived m(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0});
    out[n++] = reinterpret_cast<const char*>(&m.mFlags) - reinterpret_cast<const char*>(&m);
    out[n++] = reinterpret_cast<const char*>(&m.mThingId) - reinterpret_cast<const char*>(&m);
    out[n++] = static_cast<int64_t>(sizeof(BehaviourEntry));
    out[n++] = static_cast<int64_t>(offsetof(BehaviourEntry, mBehaviour));
    return n;
}

// One entry for all 24 functions: op is the index in the module's OPS table; a is the object (or 0), b the second argument.
uint64_t tpw_v51064_call(int32_t op, uint64_t a, int64_t b) {
    using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(a));
    CPerson* person = static_cast<CPerson*>(p);
    switch (op) {
    case 0: { CMsgPeepArrived m(ThingId{0}); return static_cast<uint32_t>(m.GetType()); }
    case 1: return reinterpret_cast<uintptr_t>(CMsgPeepArrived_Dtor(static_cast<CMsgPeepArrived*>(p), static_cast<int16_t>(b)));
    case 2: new (p) CMsgPeepArrived(ThingId{static_cast<uint16_t>(b)}); return 0;
    case 3: __sinit_Person_cpp(); return 0;
    case 4: return person->GetPreferredExcitement();
    case 5: return reinterpret_cast<uintptr_t>(CMsgPrankery_Dtor(static_cast<CMsgPrankery*>(p), static_cast<int16_t>(b)));
    case 6: person->AddRideToRecentTemporaryHistory(ThingId{static_cast<uint16_t>(b)}); return 0;
    case 7: person->AddRideToRecentHistory(ThingId{static_cast<uint16_t>(b)}); return 0;
    case 8: person->ForceRideAdmittance(); return 0;
    case 9: person->EndSpotAnim(); return 0;
    case 10: return person->ReadyForAdmitting() ? 1 : 0;
    case 11: return static_cast<uint32_t>(person->InQueueingState());
    case 12: { CMsgPrankery m; return static_cast<uint32_t>(m.GetType()); }
    case 13: __sinit_PersonQueue_cpp(); return 0;
    case 14: static_cast<CPeep*>(p)->SetStandScript(); return 0;
    case 15: return static_cast<CPeep*>(p)->LastCellVisited().mIndex;
    case 16: __sinit_MechanicHQ_cpp(); return 0;
    case 17: static_cast<CMechanicHQ*>(p)->FinalDestroy(); return 0;
    case 18: return reinterpret_cast<uintptr_t>(Arrival_Dtor(static_cast<Arrival*>(p), static_cast<int16_t>(b)));
    case 19: return reinterpret_cast<uintptr_t>(Seek_Dtor(static_cast<Seek*>(p), static_cast<int16_t>(b)));
    case 20: return reinterpret_cast<uintptr_t>(Behaviour_Dtor(static_cast<Behaviour*>(p), static_cast<int16_t>(b)));
    case 21: BehaviourEntry_Ctor(static_cast<BehaviourEntry*>(p)); return 0;
    case 22: __sinit_VehicleCallbacks_cpp(); return 0;
    case 23: __sinit_guard_cpp(); return 0;
    }
    return 0;
}

}  // extern "C"
