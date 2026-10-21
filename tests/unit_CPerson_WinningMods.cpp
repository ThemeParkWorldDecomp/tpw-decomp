// Test library for CPerson::WinningMods (unit v5671; libtpw_unit_CPerson_WinningMods.dylib, tests/test_CPerson_WinningMods.py), main app:
//   src/game/people/Person.cpp
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queue. The globals of other
// files the unit reads (gWorld, gSoundBanks, gPersonTable) are defined here.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ride_layout.h"
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "World.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPSound.h"

using namespace calllog;

// ---- globals of other files, defined here
CWorld* gWorld;
int32_t gSoundBanks[8];
uint16_t gPersonTable[2] = {0, 25};

namespace {
void* gInfo = nullptr;   // what GetDBInfo answers (the test sets it)
}

// ---- the fakes
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {lab(const_cast<CThing*>(this))});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret(lab(const_cast<CThing*>(this)) == 1 ? "PersonId" : "ObjId"));
    return id;
}

void CThoughts::RecordEvent(EventType type, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    ev("RecordEvent", {lab(this), static_cast<int64_t>(type), thing.mIndex});
}

RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(const_cast<CObject*>(this))});
    return static_cast<RideInfo*>(gInfo);
}

uint32_t CObject::GetPrizeValue() const {
    ev("GetPrizeValue", {lab(const_cast<CObject*>(this))});
    return static_cast<uint32_t>(ret("Prize"));
}

uint32_t CObject::GetPricePerUse() const {
    ev("GetPricePerUse", {lab(const_cast<CObject*>(this))});
    return static_cast<uint32_t>(ret("Price"));
}

int32_t CObject::GetCostOfGoods() const {
    ev("GetCostOfGoods", {lab(const_cast<CObject*>(this))});
    return static_cast<int32_t>(ret("Cost"));
}

uint8_t CObject::GetAmountOfSpecialIngredient() const {
    ev("GetAmountOfSpecialIngredient", {lab(const_cast<CObject*>(this))});
    return static_cast<uint8_t>(ret("Ingredient"));
}

uint8_t CObject::GetProductQuality() const {
    ev("GetProductQuality", {lab(const_cast<CObject*>(this))});
    return static_cast<uint8_t>(ret("Quality"));
}

void CObject::RecordACost(uint32_t amount) { ev("RecordACost", {lab(this), amount}); }

void CObject::PersonUsedToilet(uint32_t amount) { ev("PersonUsedToilet", {lab(this), amount}); }

void CPeep::PlaySound(int32_t category, int32_t sound) { ev("PlaySound", {lab(this), category, sound}); }

void CPerson::ExtraExcitementEffects(const CObject* object) {
    ev("ExtraExcitementEffects", {lab(this), lab(const_cast<CObject*>(object))});
}

void CPerson::LowerHappiness(Amount amount) { ev("LowerHappiness", {lab(this), static_cast<int64_t>(amount)}); }

uint32_t CPerson::GetHappinessAmount(Amount amount) const {
    ev("GetHappinessAmount", {lab(const_cast<CPerson*>(this)), static_cast<int64_t>(amount)});
    return static_cast<uint32_t>(ret("Happiness"));
}

uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("Rand"));
}

void CWorld::SeedRand(uint32_t seed) { ev("SeedRand", {lab(this), seed}); }

int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {static_cast<int64_t>(set)});
    return static_cast<int32_t>(ret("Sprites"));
}

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites) {
    ev("ESP_GetRandomActionFrom", {static_cast<int64_t>(set), sprites});
    return static_cast<int32_t>(ret("Action"));
}

int32_t ESP_SpawnScript(void* block, ESP_Sprites sprite, int32_t a, ACTS acts, float x, float y, float z) {
    ev("ESP_SpawnScript", {block == static_cast<void*>(&gWinningBalloonBlock) ? 1 : 0, static_cast<int64_t>(sprite), a,
                           static_cast<int64_t>(acts), fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("Script"));
}

void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%d") != nullptr) {
        va_list ap;
        va_start(ap, format);
        int64_t a = va_arg(ap, int);
        int64_t b = va_arg(ap, int);
        va_end(ap);
        ev("CTRACE", {level, a, b}, {format});
    } else {
        ev("CTRACE", {level}, {format});
    }
}

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

extern "C" {

uint64_t tpw_wm_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"sound", &gSoundBanks[0]},
        {"ptable", &gPersonTable[0]},
        {"gworld", &gWorld},
        {"info", &gInfo},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_wm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_wm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gSoundBanks[0]));
    out[n++] = static_cast<int64_t>(sizeof(gPersonTable));
    return n;
}

void tpw_wm_winning(uint64_t person, uint64_t object) {
    reinterpret_cast<CPerson*>(static_cast<uintptr_t>(person))->WinningMods(reinterpret_cast<CObject*>(static_cast<uintptr_t>(object)));
}

}  // extern "C"
