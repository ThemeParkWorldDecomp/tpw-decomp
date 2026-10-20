// Test library for seven CPerson / Pilot / CParkTime functions (unit v51034; libtpw_unit_CPerson_GetHeadSpriteID.dylib,
// tests/test_CPerson_GetHeadSpriteID.py), main app:
//   src/CPerson_..._CPerson_GetHeadSpriteID.cpp   GetHeadSpriteID, SetSpotAnim, ScoreOnCloserInspection, FinalDestroy, LOADVAR<ESP_Sprites>,
//                                                 Pilot::~Pilot, CParkTime::GetFunnyTimeSinceGameTurn
//
// The callees outside the unit are fakes recording what the real ones read (pointers as offsets from the case's object block); the globals of
// other files (gWorld, gSoundBanks, gSaveLoad) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TPSound.h"
#include "HLSprite.h"
#include "ESP_sprites.h"
#include "SaveLoad.h"
#include "TbTimeDiff.h"
#include "Debug.h"

using namespace calllog;

CWorld* gWorld = nullptr;
int32_t gSoundBanks[8];
CSaveLoad gSaveLoad;

namespace {
uint8_t* gBase = nullptr;
int64_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(p) - gBase); }
}  // namespace

// ---- the fakes
int32_t ESP_GetNumberOfSprites(ESP_Sprites set) {
    ev("ESP_GetNumberOfSprites", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("ESP_GetNumberOfSprites"));
}
int32_t ESP_GetRandomSpritesFrom(ESP_Sprites set) {
    ev("ESP_GetRandomSpritesFrom", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("ESP_GetRandomSpritesFrom"));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {Off(this)});
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(ret("GetThingId"));
    return id;
}
void CWorld::SeedRand(uint32_t seed) { ev("SeedRand", {this == gWorld ? 1 : 0, seed}); }
void SCR_GetScriptPosition(int32_t handle, float*, float*, float*, int32_t*) { ev("SCR_GetScriptPosition", {handle}); }
void CPeep::PlaySound(int32_t category, int32_t sound) { ev("PlaySound", {Off(this), category, sound}); }
void CDelayedScript::SetSpriteScript(ScriptIdx script) { ev("SetSpriteScript", {Off(this), static_cast<int32_t>(script)}); }
void CPerson::SetState(PersonState state) { ev("SetState", {Off(this), static_cast<int32_t>(state)}); }
int32_t CObject::IsDirty() const {
    ev("IsDirty", {Off(this)});
    return static_cast<int32_t>(ret("IsDirty"));
}
uint8_t CObject::GetExcitementLevel(bool flag) const {
    ev("GetExcitementLevel", {Off(this), flag ? 1 : 0});
    return static_cast<uint8_t>(ret("GetExcitementLevel"));
}
uint8_t CPerson::GetPreferredExcitement() const {
    ev("GetPreferredExcitement", {Off(this)});
    return static_cast<uint8_t>(ret("GetPreferredExcitement"));
}
void SCR_KillScript(int32_t handle) { ev("SCR_KillScript", {handle}); }
void CPeep::FinalDestroy() { ev("CPeep_FinalDestroy", {Off(this)}); }
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("WriteSerialiseLog", {size, static_cast<int32_t>(state)}, {name});
}
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {Off(file), Off(buffer), length});
    uint32_t data = static_cast<uint32_t>(ret("LbFile_Read.data"));
    std::memcpy(buffer, &data, 4);
    uint32_t n = static_cast<uint32_t>(ret("LbFile_Read.n"));
    if (n != 0xffffffffu) *transferred = n;
    return TbError(static_cast<int32_t>(ret("LbFile_Read.err")));
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
Vehicle::~Vehicle() { ev("Vehicle_dtor", {Off(this), 0}); }
CWorld& CWorldSavvy::GetWorld() const {
    ev("GetWorld", {Off(this)});
    return *gWorld;
}

extern "C" {

uint64_t tpw_gh_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gSoundBanks", gSoundBanks},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_gh_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_gh_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParkTime, mFunnySecsPerRealSec));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    return n;
}

uint32_t tpw_gh_head(uint64_t p) {
    gBase = reinterpret_cast<uint8_t*>(p);
    return reinterpret_cast<CPerson*>(p)->GetHeadSpriteID();
}
void tpw_gh_spot(uint64_t p, int32_t script) {
    gBase = reinterpret_cast<uint8_t*>(p);
    reinterpret_cast<CPerson*>(p)->SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(script));
}
int32_t tpw_gh_score(uint64_t p, uint64_t object) {
    gBase = reinterpret_cast<uint8_t*>(p);
    return reinterpret_cast<CPerson*>(p)->ScoreOnCloserInspection(reinterpret_cast<CObject*>(object));
}
void tpw_gh_final(uint64_t p) {
    gBase = reinterpret_cast<uint8_t*>(p);
    reinterpret_cast<CPerson*>(p)->FinalDestroy();
}
int32_t tpw_gh_loadvar(uint64_t base, uint64_t file, uint64_t value, uint64_t name, int32_t noLog) {
    gBase = reinterpret_cast<uint8_t*>(base);
    return LOADVAR<ESP_Sprites>(reinterpret_cast<void*>(file), reinterpret_cast<ESP_Sprites*>(value), reinterpret_cast<const char*>(name), noLog);
}
// The Mac's deleting form: the destructor, then the delete the Mac does when the flag (a halfword) is above 0. The host frees nothing (arena).
// Answers 1 when the Mac would delete.
int32_t tpw_gh_pilot(uint64_t p, int32_t flag) {
    gBase = reinterpret_cast<uint8_t*>(p);
    if (p == 0) return 0;
    reinterpret_cast<Pilot*>(p)->~Pilot();
    return static_cast<int16_t>(flag) > 0 ? 1 : 0;
}
uint64_t tpw_gh_funny(uint64_t p, uint32_t turn) {
    gBase = reinterpret_cast<uint8_t*>(p);
    TbTimeDiff d = reinterpret_cast<CParkTime*>(p)->GetFunnyTimeSinceGameTurn(turn);
    return static_cast<uint64_t>(d.mTicks);
}

}  // extern "C"
