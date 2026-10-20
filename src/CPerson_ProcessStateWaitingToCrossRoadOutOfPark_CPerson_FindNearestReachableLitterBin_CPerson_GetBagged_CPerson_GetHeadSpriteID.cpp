// Unit v51034: CPerson::GetHeadSpriteID, SetSpotAnim, ScoreOnCloserInspection, FinalDestroy, LOADVAR<ESP_Sprites>, Pilot::~Pilot,
// CParkTime::GetFunnyTimeSinceGameTurn. The person is reached by Mac offset.
#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ParkTime.h"
#include "Serialise.h"
#include "TPSound.h"
#include "HLSprite.h"
#include "ESP_sprites.h"
#include "TbTimeDiff.h"

namespace {
template<class T> T& Fld(void* object, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset); }
}  // namespace

// 0x1011836c: the costume words +0x20 / +0x24 when the sprite set 3 has sprites (bit 31 for kind 2), else a random head from set 1
// seeded by the thing id.
uint32_t CPerson::GetHeadSpriteID() {
    if (ESP_GetNumberOfSprites(static_cast<ESP_Sprites>(3)) != 0) {
        uint32_t word = Fld<uint32_t>(this, 0x20);
        if (Fld<int32_t>(this, 0x24) == 2) return word | 0x80000000u;
        return word;
    }
    auto id = reinterpret_cast<CThing*>(this)->GetThingId();
    gWorld->SeedRand(id.mIndex);
    return static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(1)));
}

// 0x10117e64
void CPerson::SetSpotAnim(CDelayedScript::ScriptIdx script) {
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(Fld<int32_t>(this, 0xc), &x, &y, &z, &mode);
    if (static_cast<int32_t>(script) == 4) {
        auto id = reinterpret_cast<CThing*>(this)->GetThingId();
        if ((id.mIndex & 0xf) == 0) reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x7e);
    }
    reinterpret_cast<CDelayedScript*>(reinterpret_cast<uint8_t*>(this) + 0xc)->SetSpriteScript(script);
    Fld<uint32_t>(this, 0x208) = gWorld->mTurn;
    Fld<uint32_t>(this, 0x224) = Fld<uint32_t>(this, 0x220);
    SetState(static_cast<PersonState>(8));
}

// 0x10119068
int32_t CPerson::ScoreOnCloserInspection(CObject* object) {
    if (object->IsDirty() != 0) return 100;
    int32_t preferred = GetPreferredExcitement();
    int32_t level = object->GetExcitementLevel(false);
    int32_t diff = (preferred & 0xff) - (level & 0xff);
    int32_t sign = diff < 0 ? 1 : -1;
    if (diff < 0) diff = -diff;
    if (diff < 0) diff = 0;
    else if (diff > 0x32) diff = 0x32;
    return sign * diff;
}

// 0x10116da4
void CPerson::FinalDestroy() {
    if (Fld<int32_t>(this, 0x210) != 0) {
        SCR_KillScript(Fld<int32_t>(this, 0x210));
        Fld<int32_t>(this, 0x210) = 0;
    }
    reinterpret_cast<CPeep*>(this)->FinalDestroy();
}

// 0x101161a8
template<> int32_t LOADVAR<ESP_Sprites>(void* file, ESP_Sprites* value, const char* name, int32_t noLog) {
    return serialise::LoadVar(file, value, name, noLog);
}

// 0x10113028: the Mac's deleting form; ~Vehicle runs after this empty body, and delete frees by itself (as ~Vehicle's note says).
Pilot::~Pilot() {}

// 0x10112c80
TbTimeDiff CParkTime::GetFunnyTimeSinceGameTurn(uint32_t turn) const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    const uint32_t turns = world.mTurn - turn;
    const uint64_t product = static_cast<uint64_t>(turns) * static_cast<uint64_t>(mFunnySecsPerRealSec);
    const int64_t quarter = static_cast<int64_t>(product) / 4;
    TbTimeDiff span;
    span.mTicks = static_cast<int64_t>(static_cast<uint64_t>(quarter) * 10000000u);
    return span;
}
