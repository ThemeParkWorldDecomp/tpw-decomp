// Test library for CPerson::TooPricey and CPerson::ExtraExcitementEffects (unit v5238; libtpw_unit_CPerson_TooPricey.dylib, tests/test_CPerson_TooPricey.py), main app:
//   src/game/people/Person.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The ride record blocks
// (GetDBInfo: the first call answers block A, every later call block B) and the control block are set by the test; gMainBalance is raw bytes here
// (the real one is built by a constructor this library does not link); gGameWorld is defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "MainBalance.h"
#include "ObjectControl.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
alignas(16) char gMainBalanceRaw[sizeof(CMainBalance)] __asm__("_gMainBalance");
CWorld* gGameWorld;

namespace {
uint64_t gBlockA, gBlockB, gControl, gDbCalls;
}

// ---- the fakes
uint32_t CObject::GetPricePerUse() const {
    ev("GetPricePerUse", {lab(static_cast<const void*>(this))});
    return static_cast<uint32_t>(ret("GetPricePerUse"));
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(static_cast<const void*>(this))});
    return reinterpret_cast<RideInfo*>(gDbCalls++ == 0 ? gBlockA : gBlockB);
}
uint8_t CObject::GetChanceOfWinning() const {
    ev("GetChanceOfWinning", {lab(static_cast<const void*>(this))});
    return static_cast<uint8_t>(ret("GetChanceOfWinning"));
}
uint32_t CObject::GetPrizeValue() const {
    ev("GetPrizeValue", {lab(static_cast<const void*>(this))});
    return static_cast<uint32_t>(ret("GetPrizeValue"));
}
ObjectControl* CObject::GetControl() {
    ev("GetControl", {lab(static_cast<const void*>(this))});
    return reinterpret_cast<ObjectControl*>(gControl);
}
uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {lab(static_cast<const void*>(this)), flag ? 1 : 0});
    return static_cast<uint8_t>(ret("GetExcitementLevel"));
}
uint8_t CPerson::GetPreferredExcitement() const {
    ev("GetPreferredExcitement", {lab(static_cast<const void*>(this))});
    return static_cast<uint8_t>(ret("GetPreferredExcitement"));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(static_cast<const void*>(this))});
    return reinterpret_cast<CParkAnalyser*>(static_cast<uintptr_t>(0x20003000));
}
void CParkAnalyser::OfferOpinionOn(int32_t opinion, uint8_t value) {
    ev("OfferOpinionOn", {lab(static_cast<const void*>(this)), opinion, value});
}

extern "C" {

uint64_t tpw_t_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gMainBalance", gMainBalanceRaw},
        {"gGameWorld", &gGameWorld},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_t_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_t_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ObjectControl, mRipOffOK));
    return n;
}

void tpw_t_set(uint64_t blockA, uint64_t blockB, uint64_t control) {
    gBlockA = blockA;
    gBlockB = blockB;
    gControl = control;
    gDbCalls = 0;
}

uint32_t tpw_t_pricey(uint64_t person, uint64_t object) {
    return reinterpret_cast<CPerson*>(person)->TooPricey(reinterpret_cast<CObject*>(object)) ? 1 : 0;
}
uint32_t tpw_t_extra(uint64_t person, uint64_t object) {
    reinterpret_cast<CPerson*>(person)->ExtraExcitementEffects(reinterpret_cast<const CObject*>(object));
    return 0;
}

}  // extern "C"
