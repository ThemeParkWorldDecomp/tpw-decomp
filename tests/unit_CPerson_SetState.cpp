// Test library for CPerson::SetState (unit v5370; libtpw_unit_CPerson_SetState.dylib, tests/test_CPerson_SetState.py), main app:
//   src/game/people/PersonQueue.cpp
//
// The callees outside the unit are recording fakes (objects as offsets from the case's base block) that answer from the test's queues; the
// globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "ThingReceivers.h"
#include "World.h"
#include "Debug.h"
#include "analyser.h"
#include "ESP_sprites.h"
#include "HLSprite.h"


using namespace calllog;

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);
extern uint32_t gPersonScriptBlock;

// ---- globals of other files
CWorld* gWorld = nullptr;
CMap* gWorldMap = nullptr;
CThingArrayEntry gThingArray[kThingArraySize];
uint16_t gPersonTable[2] = {0, 25};

namespace {

uintptr_t gBase = 0;

int64_t Rel(const void* p) { return static_cast<int64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gBase)); }
int64_t IsWorld(const void* p) { return p == static_cast<const void*>(gWorld) ? 1 : 0; }
int64_t U(uint32_t v) { return static_cast<int64_t>(v); }
int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    ev("GAME_ASSERTMSG", {U(static_cast<uint32_t>(condition)), static_cast<int64_t>(std::strlen(format))});
}

void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetSpriteScript", {Rel(this), U(static_cast<uint32_t>(script))}); }
void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("SpawnSpriteScript", {Rel(this), U(static_cast<uint32_t>(script)), U(static_cast<uint32_t>(set)), U(static_cast<uint32_t>(index))});
}
void CDelayedScript::KillSpriteScript() { ev("KillSpriteScript", {Rel(this)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetTicketBoothPos(int32_t which) const {
    ev("GetTicketBoothPos", {Rel(this), U(static_cast<uint32_t>(which))});
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret(which == 0 ? "booth0" : "booth1"));
    return t;
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {IsWorld(this)});
    return static_cast<uint32_t>(ret("GetRand"));
}
void CWorld::SeedRand(uint32_t seed) { ev("SeedRand", {IsWorld(this), U(seed)}); }
CChallengeHQ* CWorld::GetChallengeHQ() {
    ev("GetChallengeHQ", {IsWorld(this)});
    return reinterpret_cast<CChallengeHQ*>(gBase + 0x300);
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {IsWorld(this)});
    return reinterpret_cast<CParkAnalyser*>(gBase + 0x340);
}

void CChallengeHQ::AddToCounter(ChallengeType type, int32_t n, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> object,
                                tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("AddToCounter", {Rel(this), U(static_cast<uint32_t>(type)), U(static_cast<uint32_t>(n)), U(object.mIndex), U(thing.mIndex)});
}
void CParkAnalyser::PersonLeftPark(int32_t turns) { ev("PersonLeftPark", {Rel(this), U(static_cast<uint32_t>(turns))}); }

bool CObject::WillIWin(CPerson* person) const {
    ev("WillIWin", {Rel(this), Rel(person)});
    return ret("WillIWin") != 0;
}
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {Rel(this)});
    return reinterpret_cast<RideInfo*>(reinterpret_cast<uintptr_t>(this) + 0x200);
}

void CPeep::UpdatePositionInEngine(float step) { ev("UpdatePositionInEngine", {Rel(this), CanonBits(step)}); }
void CPeep::SpawnParticleEffect(int32_t effect) { ev("SpawnParticleEffect", {Rel(this), U(static_cast<uint32_t>(effect))}); }
int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDest", {Rel(this), static_cast<int64_t>((x.mCell << 8) | x.mOffset), static_cast<int64_t>((y.mCell << 8) | y.mOffset)});
    return static_cast<int32_t>(ret("setdest"));
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Rel(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = static_cast<uint16_t>(ret("thingid"));
    return t;
}

void CThoughts::RemoveThoughtBubble() { ev("RemoveThoughtBubble", {Rel(this)}); }
void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {Rel(this), U(static_cast<uint32_t>(type)), U(thing.mIndex)});
}

void CPerson::MakeBalloonPop() { ev("MakeBalloonPop", {Rel(this)}); }
uint32_t CPerson::GetTimeInPark() const {
    ev("GetTimeInPark", {Rel(this)});
    return static_cast<uint32_t>(ret("timeinpark"));
}

int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {U(static_cast<uint32_t>(set))});
    return static_cast<int32_t>(ret("sprites"));
}
int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites) {
    ev("ESP_GetRandomActionFrom", {U(static_cast<uint32_t>(set)), U(static_cast<uint32_t>(sprites))});
    return static_cast<int32_t>(ret("action"));
}
int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {block == static_cast<void*>(&gPersonScriptBlock) ? 1 : 0, U(static_cast<uint32_t>(sprite)),
                           U(static_cast<uint32_t>(a)), U(static_cast<uint32_t>(acts)), CanonBits(x), CanonBits(y), CanonBits(z)});
    return static_cast<int32_t>(ret("script"));
}
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {U(static_cast<uint32_t>(handle))}); }

extern "C" {

uint64_t tpw_ss_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gPersonTable", &gPersonTable[0]},
        {"gBlock", &gPersonScriptBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ss_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ss_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = kThingArraySize >= 16 ? 1 : 0;
    return n;
}

void tpw_ss_setup(uint64_t world, uint64_t base, uint64_t map) {
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gWorldMap = reinterpret_cast<CMap*>(static_cast<uintptr_t>(map));
    gBase = static_cast<uintptr_t>(base);
}

void tpw_ss_setthing(int32_t index, uint64_t p) { gThingArray[index].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(p)); }

void tpw_ss_setstate(uint64_t person, uint32_t state) {
    reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->SetState(static_cast<CPerson::PersonState>(state));
}

int32_t tpw_ss_walkfix() {
    return 0;
}

}  // extern "C"
