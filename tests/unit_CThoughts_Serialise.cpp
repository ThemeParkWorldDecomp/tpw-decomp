// Test library for CThoughts (unit v5888; libtpw_unit_CThoughts_Serialise.dylib, tests/test_CThoughts_Serialise.py), main app:
//   src/MsgDeath_CThoughts_Serialise.cpp   the CThoughts functions and the script blocks
//
// The callees outside the unit are recording fakes; the globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "Serialise.h"
#include "World.h"
#include "FS_Online.h"
#include "ESP_sprites.h"
#include "analyser.h"
#include "Debug.h"
#include "TPCamera.h"
#include "LbSCBack.h"

using namespace calllog;

extern uint8_t gThoughtScripts[22][24];
extern uint8_t gEventHistCtorVec[16];

uint32_t gFirstPersonFlags;
CWorld* gGameWorld = nullptr;
CWorld*& gGameWorldRef = gGameWorld;

namespace {

uint8_t gFile[16];
const uint8_t* gBase = nullptr;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

int64_t NameIdx(const char* n) {
    static const char* const names[] = {"mActionHistIndex", "mEventHistory[i]", "mLastThought", "mThoughtScript", "mTimeBubbleShown"};
    static const int64_t offs[] = {0, 0x11, 0x22, 0x2f, 0x3e};
    for (int i = 0; i < 5; ++i)
        if (std::strcmp(n, names[i]) == 0) return offs[i];
    return -1;
}

}  // namespace

CGameType::CGameType() {}

#define FAKE_VAR(KIND, TYPE, EVNAME)                                                                                  \
    template <> int32_t KIND<TYPE>(void* file, TYPE* v, const char* name, int32_t noLog) {                            \
        ev(EVNAME, {lab(file), static_cast<int64_t>(reinterpret_cast<const uint8_t*>(v) - gBase), NameIdx(name), noLog}); \
        return static_cast<int32_t>(ret("ser"));                                                                      \
    }

FAKE_VAR(SAVEVAR, uint32_t, "SAVEVAR<Ui>")
FAKE_VAR(LOADVAR, uint32_t, "LOADVAR<Ui>")
FAKE_VAR(SAVEVAR, MacLong, "SAVEVAR<l>")
FAKE_VAR(LOADVAR, MacLong, "LOADVAR<l>")
FAKE_VAR(SAVEVAR, MacULong, "SAVEVAR<Ul>")
FAKE_VAR(LOADVAR, MacULong, "LOADVAR<Ul>")
FAKE_VAR(SAVEVAR, CThoughts::ThoughtType, "SAVEVAR<TT>")
FAKE_VAR(LOADVAR, CThoughts::ThoughtType, "LOADVAR<TT>")
FAKE_VAR(SAVEVARSTRUCT, CThoughts::EventHistType, "SAVEVARSTRUCT<EH>")
FAKE_VAR(LOADVARSTRUCT, CThoughts::EventHistType, "LOADVARSTRUCT<EH>")

void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {static_cast<int64_t>(condition & 0xff)}, {format});
}

void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {lab(block), static_cast<int64_t>(sprite), a, static_cast<int64_t>(acts), CanonBits(x), CanonBits(y), CanonBits(z)});
    return static_cast<int32_t>(ret("ESP"));
}

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    ev("__construct_array", {static_cast<int64_t>(reinterpret_cast<const uint8_t*>(array) - gBase), lab(ctor), lab(dtor), size, count});
}

extern "C" {

uint64_t tpw_ct_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fpflags", &gFirstPersonFlags},
        {"gworld", &gGameWorld},
        {"gt", &TheGameType().mType},
        {"file", gFile},
        {"scripts", gThoughtScripts},
        {"ctorvec", gEventHistCtorVec},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ct_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ct_layout(int64_t* out) {
    (void)TheGameType();  // construct the static now, so the fake constructor never logs in a case
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(gThoughtScripts[0]));
    return n;
}

void tpw_ct_ctor(uint64_t obj) {
    gBase = reinterpret_cast<const uint8_t*>(obj);
    new (reinterpret_cast<void*>(obj)) CThoughts();
}
void tpw_ct_process(uint64_t obj) { gBase = reinterpret_cast<const uint8_t*>(obj); reinterpret_cast<CThoughts*>(obj)->ProcessThoughtBubbles(); }
void tpw_ct_remove(uint64_t obj) { gBase = reinterpret_cast<const uint8_t*>(obj); reinterpret_cast<CThoughts*>(obj)->RemoveThoughtBubble(); }
void tpw_ct_show(uint64_t obj, int32_t type, uint32_t always) {
    gBase = reinterpret_cast<const uint8_t*>(obj);
    reinterpret_cast<CThoughts*>(obj)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(type), (always & 0xff) != 0);
}
void tpw_ct_record(uint64_t obj, int32_t type, uint32_t thing) {
    gBase = reinterpret_cast<const uint8_t*>(obj);
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(thing);
    reinterpret_cast<CThoughts*>(obj)->RecordEvent(static_cast<CThoughts::EventType>(type), id);
}
uint64_t tpw_ct_get(uint64_t obj, uint32_t index) {
    gBase = reinterpret_cast<const uint8_t*>(obj);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(reinterpret_cast<const CThoughts*>(obj)->GetRecentEvent(index)));
}
int32_t tpw_ct_serialise(uint64_t obj, uint64_t file, int32_t save, int32_t version, uint32_t unused) {
    gBase = reinterpret_cast<const uint8_t*>(obj);
    return reinterpret_cast<CThoughts*>(obj)->Serialise(reinterpret_cast<void*>(file), save, version, unused);
}

}  // extern "C"
