// Test library for CPerson::CPerson and CPerson::ReceiveMessage (unit v5052; libtpw_unit_CPerson_CPerson.dylib, tests/test_CPerson_CPerson.py), main app:
//   src/game/people/Person.cpp   the two functions and the three halfword globals
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues; the globals of
// other files the unit reads are defined here (zero at start like the Mac's bss). The person is a raw block the test hands in: the
// translation reaches its fields by Mac offset.
#include <cstdio>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"

#include "CMessageCentre.h"
#include "CMsg.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "TPSound.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
CMap* gWorldMap;
CMessageCentre* gMessageCentre;
CThingArrayEntry gThingArray[kThingArraySize];
int32_t gSoundBanks[8];
// gMainBalance without its constructor and vtable: raw storage under the same symbol.
alignas(16) unsigned char gMainBalanceStorage[sizeof(CMainBalance)] __asm__("_gMainBalance");

namespace {

uint8_t* gSelf = nullptr;   // the person under test: fakes log object pointers as offsets from it

int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<const uint8_t*>(p) - gSelf); }

struct TestMsg : CMsg {
    uint16_t mThingId;   // host +0x0c (the Mac's +0x08)
    int32_t mType;
    int32_t GetType() const override { return mType; }
};

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stubs
CPeep::CPeep(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, CThing::ThingModel model) {
    ev("CPeep", {cell.mIndex, static_cast<int32_t>(model)});
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(ret("GetThingId"))};
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {});
    return static_cast<uint32_t>(ret("GetRand"));
}

void CWorld::SeedRand(uint32_t seed) { ev("SeedRand", {seed}); }

int32_t CWorld::GetNewPersonIndex() {
    ev("GetNewPersonIndex", {});
    return static_cast<int32_t>(ret("GetNewPersonIndex"));
}

int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("ESP_GetRandomSpritesFrom"));
}

void CDelayedScript::SpawnSpriteScript(ScriptIdx script, int32_t set, int32_t index) {
    ev("SpawnSpriteScript", {Off(this), static_cast<int32_t>(script), set, index});
}

void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }

int32_t CPeep::IsInsidePark() const {
    ev("IsInsidePark", {});
    return static_cast<int32_t>(ret("IsInsidePark"));
}

int32_t CPeep::SetDest(MapPos x, MapPos y) {
    ev("SetDest", {x.mCell, x.mOffset, y.mCell, y.mOffset});
    return static_cast<int32_t>(ret("SetDest"));
}

void CPeep::UpdatePositionInEngine(float step) { ev("UpdatePositionInEngine", {fbits(step)}); }

void CPeep::SpawnParticleEffect(int32_t effect) { ev("SpawnParticleEffect", {effect}); }

void CPeep::PlaySound(int32_t category, int32_t sound) { ev("PlaySound", {category, sound}); }

void CPerson::SetState(PersonState state) { ev("SetState", {static_cast<int32_t>(state)}); }

int32_t CPerson::InQueueingState() const {
    ev("InQueueingState", {});
    return static_cast<int32_t>(ret("InQueueingState"));
}

void CPerson::DismissFromQueue() { ev("DismissFromQueue", {}); }

void CPerson::LowerHappiness(Amount amount) { ev("LowerHappiness", {static_cast<int32_t>(amount)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetCrossingBSSidePos(int32_t which) const {
    ev("GetCrossingBSSidePos", {lab(this), which});
    return tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(ret("GetCrossingBSSidePos"))};
}

void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {Off(this), static_cast<int32_t>(type), thing.mIndex});
}

CMsgPeepArrived::CMsgPeepArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> peep) { ev("CMsgPeepArrived", {peep.mIndex}); }

int32_t CMessageCentre::PostMessage(const CMsg&) {
    ev("PostMessage", {lab(this)});
    return 0;
}

CSubscriptionHeader::CSubscriptionHeader() { ev("CSubscriptionHeader", {}); }
CSubscriptionHeader::~CSubscriptionHeader() {}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    uint32_t hash = 0;
    bool formatted = false;
    for (const char* c = format; *c; c++) {
        hash = hash * 31u + static_cast<uint8_t>(*c);
        if (*c == '%') formatted = true;
    }
    int64_t args[4] = {0, 0, 0, 0};
    if (formatted) {
        va_list list;
        va_start(list, format);
        for (int64_t& a : args) a = va_arg(list, int32_t);
        va_end(list);
    }
    ev("GAME_ASSERTMSG", {condition & 0xff, hash, args[0], args[1], args[2], args[3]});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys). "bal" is the Mac's gMainBalance address 0x102b4568 mapped onto the
// host table: Mac offset 8 + 4 * slot is host mValues[slot].
uint64_t tpw_pers_addr(const char* name) {
    struct Entry {
        const char* name;
        uint64_t address;
    };
    const Entry entries[] = {
        {"world", reinterpret_cast<uintptr_t>(&gWorld)},
        {"wmap", reinterpret_cast<uintptr_t>(&gWorldMap)},
        {"mc", reinterpret_cast<uintptr_t>(&gMessageCentre)},
        {"bal", reinterpret_cast<uintptr_t>(&gMainBalance.mValues[0])},
        {"tbl", reinterpret_cast<uintptr_t>(&gPersonTable[0])},
        {"cella", reinterpret_cast<uintptr_t>(&gPersonCellA)},
        {"cellb", reinterpret_cast<uintptr_t>(&gPersonCellB)},
        {"tarr", reinterpret_cast<uintptr_t>(&gThingArray[0])},
        {"snd", reinterpret_cast<uintptr_t>(&gSoundBanks[0])},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return 0;
}

uint64_t tpw_pers_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pers_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CMessageCentre));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    out[n++] = static_cast<int64_t>(sizeof(CDelayedScript));
    out[n++] = static_cast<int64_t>(offsetof(TestMsg, mThingId));
    return n;
}

void tpw_pers_ctor(uint64_t self, uint32_t cell) {
    gSelf = reinterpret_cast<uint8_t*>(self);
    new (reinterpret_cast<void*>(self)) CPerson(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)});
}

void tpw_pers_recv(uint64_t self, uint32_t id, int32_t type) {
    gSelf = reinterpret_cast<uint8_t*>(self);
    TestMsg message;
    message.mFlags = 0;
    message.mThingId = static_cast<uint16_t>(id);
    message.mType = type;
    reinterpret_cast<CPerson*>(self)->ReceiveMessage(message);
}

}  // extern "C"
