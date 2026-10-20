// CPerson::SetState (0x10120354, 1944 bytes): stores the state word (+0x220) and switches on it (jump table at 0x102a1068, states 0 to 0x15
// in the order of the listing's blocks; any other state does nothing). Each state picks the sprite script and sets up its fields.
#include <cstddef>
#include <cstdint>

#include "ThingReceivers.h"
#include "World.h"
#include "Debug.h"
#include "analyser.h"
#include "ESP_sprites.h"
#include "HLSprite.h"

// 0x10244aa8 is the ThingArray row-zero text; the balloon text is at +0x8e9 of the same pool.
int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);   // 0x1007c9f8 (104 bytes, untranslated)

// 0x1029fd48 (name invented; initial word 0x10266a60): the block the balloon's script is spawned with.
uint32_t gPersonScriptBlock = 0;

namespace {

const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10244aa8
const char kBalloonText[] = "Trying to give a balloon to a person who already has one!\n";           // 0x10245391

uint8_t* Raw(void* p) { return reinterpret_cast<uint8_t*>(p); }
uint16_t& Half(uint8_t* s, size_t o) { return *reinterpret_cast<uint16_t*>(s + o); }
uint32_t& Word(uint8_t* s, size_t o) { return *reinterpret_cast<uint32_t*>(s + o); }

CDelayedScript* Script(uint8_t* s) { return reinterpret_cast<CDelayedScript*>(s + 0xc); }
CThoughts* Thoughts(uint8_t* s) { return reinterpret_cast<CThoughts*>(s + 0x30); }
void Sprite(uint8_t* s, int32_t idx) { Script(s)->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(idx)); }

uint8_t* RideOf(uint8_t* s) {
    uint16_t id = Half(s, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText);
    return Raw(gThingArray[id].mThing);
}

uint8_t* InfoOf(uint8_t* ride) { return Raw(reinterpret_cast<CObject*>(ride)->GetDBInfo()); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingTag(uint16_t v) {
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> t;
    t.mIndex = v;
    return t;
}

void CountForRide(uint8_t* s, int32_t type) {
    uint8_t* ride = RideOf(s);
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> obj;
    obj.mIndex = Half(ride, 0xe);
    gWorld->GetChallengeHQ()->AddToCounter(static_cast<ChallengeType>(type), 1, obj, ThingTag(Half(s, 0x1dc)));
}

}  // namespace

// 0x10120354
void CPerson::SetState(PersonState state) {
    uint8_t* s = Raw(this);
    Word(s, 0x220) = static_cast<uint32_t>(state);
    uint8_t* world = Raw(gWorld);
    // The jump table at 0x102a1068 (state -> the block's number in address order, 0x1012039c = 0 ... 0x10120aa8 = 21); the cases below
    // are numbered by block.
    static const uint8_t kBlock[22] = {0, 1, 2, 5, 3, 4, 20, 21, 19, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
    uint32_t st = static_cast<uint32_t>(state);
    switch (st < 22u ? static_cast<uint32_t>(kBlock[st]) : 99u) {
    case 0: Sprite(s, 1); break;
    case 1: Sprite(s, 3); break;
    case 2: Sprite(s, 1); break;
    case 3: {
        uint16_t cell = static_cast<uint16_t>(s[4] + (static_cast<uint32_t>(s[6]) << 7) + 1);
        uint16_t booth = gWorldMap->GetTicketBoothPos(0).mIndex;
        if (cell == booth) {
            Word(s, 0x1c) = 0x200;
        } else {
            booth = gWorldMap->GetTicketBoothPos(1).mIndex;
            if (cell == booth) Word(s, 0x1c) = 0x600;
        }
        Sprite(s, 3);
        break;
    }
    case 4: Sprite(s, 1); break;
    case 5: {
        Sprite(s, 3);
        uint32_t r = gWorld->GetRand();
        uint32_t q = static_cast<uint32_t>((static_cast<uint64_t>(0x1b4e81b5u) * r) >> 32) >> 4;
        Word(s, 0x200) = r - q * 150u + 200u;
        break;
    }
    case 6: Sprite(s, 1); break;
    case 7: Sprite(s, 1); break;
    case 8: {
        Sprite(s, 3);
        Word(s, 0x1fc) = Word(world, 0x1da70c);
        float scaled = 1.2f * static_cast<float>(static_cast<uint32_t>(s[0x1f1]));
        Word(s, 0x1f4) = static_cast<uint32_t>(scaled);   // __cvt_fp2unsigned (0x10237cd8), the value is small and positive
        break;
    }
    case 9:
        Half(s, 0xc2) = gPersonTable[0];
        Sprite(s, 1);
        break;
    case 10: Sprite(s, 1); break;
    case 11: {
        Sprite(s, 3);
        uint8_t* ride = RideOf(s);
        s[0x1f1] = static_cast<uint8_t>(reinterpret_cast<CObject*>(ride)->WillIWin(this));
        break;
    }
    case 12: {
        if (Word(s, 0xc) != 0) Sprite(s, 1);
        else Script(s)->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1), static_cast<int32_t>(Word(s, 0x24)),
                                          static_cast<int32_t>(Word(s, 0x20)));
        Word(s, 0x28) = 0;
        uint8_t* ride = RideOf(s);
        if (Word(InfoOf(ride), 0x100) != 0) {
            reinterpret_cast<CPeep*>(this)->UpdatePositionInEngine(0.0f);
            reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
        }
        if (Word(s, 0x214) != 0) {
            ride = RideOf(s);
            if (static_cast<int32_t>(Word(InfoOf(ride), 0x15c)) != 1) {
                GAME_ASSERTMSG(Word(s, 0x210) == 0 ? 1 : 0, kBalloonText);
                uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
                gWorld->SeedRand(id);
                int32_t sprites = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(10));
                int32_t action = ESP_GetRandomActionFrom(static_cast<ESP_Sprites>(10), sprites);
                Word(s, 0x210) = static_cast<uint32_t>(ESP_SpawnScript(&gPersonScriptBlock, static_cast<ESP_Sprites>(10), sprites,
                                                                        static_cast<ACTS>(action), 0.0f, 0.0f, 0.0f));
            }
        }
        break;
    }
    case 13: {
        uint8_t* ride = RideOf(s);
        if (Word(InfoOf(ride), 0x100) != 0) reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
        ride = RideOf(s);
        if (Half(ride, 0x2e) & 0x20) {
            Word(s, 0x28) = 1;
            Sprite(s, 3);
        } else {
            Script(s)->KillSpriteScript();
        }
        Thoughts(s)->RemoveThoughtBubble();
        if (Word(s, 0x210) != 0) {
            SCR_KillScript(static_cast<int32_t>(Word(s, 0x210)));
            Word(s, 0x210) = 0;
        }
        Thoughts(s)->RecordEvent(static_cast<CThoughts::EventType>(0x14), ThingTag(0));
        CountForRide(s, 0x11);
        CountForRide(s, 0x14);
        CountForRide(s, 0x16);
        break;
    }
    case 14:
        Thoughts(s)->RecordEvent(static_cast<CThoughts::EventType>(0x10), ThingTag(0));
        Thoughts(s)->RemoveThoughtBubble();
        Half(s, 0x1dc) = 0;
        if (Word(s, 0x210) != 0) MakeBalloonPop();
        Script(s)->KillSpriteScript();
        break;
    case 15:
        Thoughts(s)->RecordEvent(static_cast<CThoughts::EventType>(0x13), ThingTag(0));
        Half(s, 0x1dc) = 0;
        Sprite(s, 1);
        break;
    case 16: Sprite(s, 3); break;
    case 17: Sprite(s, 1); break;
    case 18:
        if (Word(s, 0x204) != 0) {
            CWorld* w = gWorld;
            uint32_t turns = GetTimeInPark();
            w->GetParkAnalyser()->PersonLeftPark(static_cast<int32_t>(turns << 2));
        }
        Sprite(s, 3);
        break;
    case 19: Word(s, 0x208) = Word(world, 0x1da70c); break;
    case 20: Sprite(s, 3); break;
    case 21:
        if (Word(s, 0xd0) != 0) {
            MapPos a, b;
            a.mCell = s[0x18];
            a.mOffset = s[0x19];
            b.mCell = s[0x1a];
            b.mOffset = s[0x1b];
            reinterpret_cast<CPeep*>(this)->SetDest(a, b);
        }
        Sprite(s, 1);
        break;
    default: break;
    }
}
