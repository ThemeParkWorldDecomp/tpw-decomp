#include <cstdint>
#include <cmath>
#include "ThingReceivers.h"
#include "RideDB.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPSound.h"
#include "ppc_math.h"

// The balloon script's start block (0x1029fd48, initial word 0x10266a60; name invented) and the two limits of a balloon's life
// (0x102a0f0c = 25 and 0x102a0f10 = 255; names invented).
uint32_t gWinningBalloonBlock = 0x10266a60;
uint32_t gBalloonLifeMin = 25;
uint32_t gBalloonLifeMax = 255;

int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);   // 0x1007c9f8

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

constexpr char kLitter[] = "Litter gone up by %d, is now %d\n";                                               // 0x10244817
constexpr char kBought[] = "Customer bought a balloon.  Aaah.\n";                                            // 0x10244838
constexpr char kHasOne[] = "Trying to give a balloon to a person who already has one!\n";                    // 0x1024485b
constexpr char kCostume[] = "Customer returning a costume.\n";                                               // 0x10244896
constexpr char kBalance[] = "Balance file error!  Shop has unknown appearance effect (can safely be ignored for now)\n";   // 0x102448b5
constexpr char kSideshow[] = "Sideshow won - happiness up %d points to %d\n";                                // 0x1024490e

inline float& Fl(uint8_t* s, uint32_t off) { return *reinterpret_cast<float*>(s + off); }
inline uint32_t& Wd(uint8_t* s, uint32_t off) { return *reinterpret_cast<uint32_t*>(s + off); }

// The original's compare chain: above 100 -> 100, below 0 -> 0, else the sum (a NaN falls through to the store).
inline float Clamp(float sum, float stored) {
    if (sum > 100.0f) return 100.0f;
    if (sum < 0.0f) return 0.0f;
    return stored;
}

}  // namespace

// 0x1011a570
void CPerson::WinningMods(CObject* object) {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    const CThing* objThing = reinterpret_cast<const CThing*>(object);
    const CThing* selfThing = reinterpret_cast<const CThing*>(this);

    thoughts->RecordEvent(static_cast<CThoughts::EventType>(8), objThing->GetThingId());
    // The ride record's words are read by name (the Mac's offsets: the UI type +0x7a8, the effect slots +0x144 to +0x15c): the host's
    // record is laid out wider (a vtable and 8-byte balance slots).
    const RideInfo* info = object->GetDBInfo();
    if (info->mUIType == 2) {
        object->RecordACost(object->GetPrizeValue());
        Wd(self, 0x1a0) = Wd(self, 0x1a0) + object->GetPrizeValue();
    } else {
        info = object->GetDBInfo();
        if (info->mUIType == 1) object->RecordACost(static_cast<uint32_t>(object->GetCostOfGoods()));
    }
    ExtraExcitementEffects(object);
    info = object->GetDBInfo();

    {
        float v = static_cast<float>(info->Value(kRideSlot_ThirstEffect).mInteger);
        float f = Fl(self, 0x1a4);
        float sum = -v + f;
        Fl(self, 0x1a4) = Clamp(sum, f - v);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_HungerEffect).mInteger);
        float f = Fl(self, 0x1a8);
        float sum = -v + f;
        Fl(self, 0x1a8) = Clamp(sum, f - v);
    }
    if (info->Value(kRideSlot_ThirstEffect).mInteger > info->Value(kRideSlot_HungerEffect).mInteger) {
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x83);
    } else if (info->Value(kRideSlot_ThirstEffect).mInteger < info->Value(kRideSlot_HungerEffect).mInteger) {
        reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x84);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_VomitEffect).mInteger);
        float f = Fl(self, 0x1b0);
        float sum = v + f;
        Fl(self, 0x1b0) = Clamp(sum, sum);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_HappinessEffect).mInteger);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, sum);
    }
    {
        float v = static_cast<float>(info->Value(kRideSlot_LitterEffect).mInteger);
        float f = Fl(self, 0x1b4);
        float sum = v + f;
        Fl(self, 0x1b4) = Clamp(sum, sum);
    }
    if (info->Value(kRideSlot_LitterEffect).mInteger > 0) {
        CTRACE(2, kLitter, info->Value(kRideSlot_LitterEffect).mInteger, static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1b4))) & 0xffu);
    }
    uint32_t ingredient = object->GetAmountOfSpecialIngredient() & 0xffu;
    if (info->Value(kRideSlot_HungerEffect).mInteger != 0) {
        uint32_t r = gWorld->GetRand();
        uint32_t total = static_cast<uint32_t>(info->Value(kRideSlot_HungerEffect).mInteger) + (ingredient + (r & 7u));
        if (total < 0x1eu) LowerHappiness(static_cast<Amount>(0));
    }
    if (info->Value(kRideSlot_ThirstEffect).mInteger != 0) {
        uint32_t r = gWorld->GetRand();
        uint32_t total = static_cast<uint32_t>(info->Value(kRideSlot_ThirstEffect).mInteger) + (ingredient + (r & 7u));
        if (total < 0x1eu) LowerHappiness(static_cast<Amount>(0));
    }
    {
        int32_t product = static_cast<int32_t>(static_cast<uint32_t>(info->Value(kRideSlot_HappinessEffect).mInteger) * ingredient);
        float v = static_cast<float>(product / 100);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, sum);
    }
    switch (info->Value(kRideSlot_SpecialIngredient).mInteger) {
    case 2: {
        float v = static_cast<float>(static_cast<int32_t>(ingredient));
        float f = Fl(self, 0x1a4);
        float sum = v + f;
        Fl(self, 0x1a4) = Clamp(sum, f + v);
        break;
    }
    case 1: {
        float v = static_cast<float>(static_cast<int32_t>(ingredient));
        float f = Fl(self, 0x1ac);
        float sum = v + f;
        Fl(self, 0x1ac) = Clamp(sum, f + v);
        break;
    }
    case 3: {
        int32_t product = static_cast<int32_t>(static_cast<uint32_t>(info->Value(kRideSlot_ThirstEffect).mInteger) * ingredient);
        float v = static_cast<float>(product / 100);
        float f = Fl(self, 0x1a4);
        float sum = v + f;
        Fl(self, 0x1a4) = Clamp(sum, sum);
        break;
    }
    case 4: {
        uint16_t* h = reinterpret_cast<uint16_t*>(self + 0xc4);
        *h = static_cast<uint16_t>(*h + static_cast<int16_t>(static_cast<int32_t>(ingredient * 6u) / 100));
        break;
    }
    default: break;
    }

    int32_t kind = info->Value(kRideSlot_AppearanceEffect).mInteger;
    if (kind == 1) {
        CTRACE(2, kBought);
        GAME_ASSERTMSG(Wd(self, 0x210) == 0 ? 1 : 0, kHasOne);
        ThingId id = selfThing->GetThingId();
        gWorld->SeedRand(id.mIndex);
        int32_t sprites = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(10));
        int32_t action = ESP_GetRandomActionFrom(static_cast<ESP_Sprites>(10), sprites);
        Wd(self, 0x210) = static_cast<uint32_t>(ESP_SpawnScript(&gWinningBalloonBlock, static_cast<ESP_Sprites>(10), sprites,
                                                                static_cast<ACTS>(action), 0.0f, 0.0f, 0.0f));
        uint32_t quality = object->GetProductQuality() & 0xffu;
        uint32_t life = (gBalloonLifeMax * quality) / 100u;
        Wd(self, 0x214) = life;
        uint32_t result = life;
        if (life < gBalloonLifeMin) result = gBalloonLifeMin;
        else if (life > gBalloonLifeMax) result = gBalloonLifeMax;
        Wd(self, 0x214) = result;
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xc), objThing->GetThingId());
    } else if (kind == 2) {
        if (Wd(self, 0x24) == 2) {
            CTRACE(2, kCostume);
            Wd(self, 0x24) = 0;
            ThingId id = selfThing->GetThingId();
            gWorld->SeedRand(id.mIndex);
            Wd(self, 0x20) = static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(Wd(self, 0x24))));
        } else {
            Wd(self, 0x24) = 2;
            Wd(self, 0x20) = static_cast<uint32_t>(ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(Wd(self, 0x24))));
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xb), objThing->GetThingId());
        }
    } else if (kind != 0) {
        GAME_ASSERTMSG(0, kBalance);
    }

    if ((*reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(object) + 0x2e) & 1u) != 0) {
        object->PersonUsedToilet(static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1ac))) & 0xffu);
        Fl(self, 0x1ac) = 0.0f;
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x11), objThing->GetThingId());
        if ((static_cast<uint32_t>(ppc_fctiwz(Fl(self, 0x1b0))) & 0xffu) > 0x5au) {
            ThingId none;
            none.mIndex = 0;
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x12), none);
            Fl(self, 0x1b0) = 0.0f;
        }
        *reinterpret_cast<uint16_t*>(self + 0xc2) = gPersonTable[1];
    }

    info = object->GetDBInfo();
    if (info->mUIType == 2) {
        Wd(self, 0x1d0) = Wd(self, 0x1d0) + 1;
        uint32_t prize = object->GetPrizeValue();
        uint32_t price = object->GetPricePerUse();
        double ln2 = std::log(2.0);
        double ratio = static_cast<double>(static_cast<int32_t>(prize)) / static_cast<double>(static_cast<int32_t>(price));
        double scale = std::log(ratio) / ln2;
        uint32_t happy = GetHappinessAmount(static_cast<Amount>(1)) & 0xffu;
        int32_t shown = ppc_fctiwz(Fl(self, 0x19c));
        int32_t gained = ppc_fctiwz(static_cast<double>(happy) * scale);
        CTRACE(2, kSideshow, gained, static_cast<uint32_t>(shown) & 0xffu);
        float v = static_cast<float>(gained);
        float f = Fl(self, 0x19c);
        float sum = v + f;
        Fl(self, 0x19c) = Clamp(sum, f + v);
        gWorld->GetRand();
    }
}
