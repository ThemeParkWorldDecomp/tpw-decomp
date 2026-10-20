// CPerson::CPerson (0x10116634, 1488 bytes) and CPerson::ReceiveMessage (0x10116e24, 604 bytes). The person's fields are reached by Mac
// offset (see the comment in ThingReceivers.h); every field is a 32-bit scalar, so the host layout of that memory is the Mac's.
#include <cstddef>
#include <cstdint>
#include <new>

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

// The tables the person reads (initialised data; names invented in ThingReceivers.h).
uint16_t gPersonTable[2] = {0, 25};   // 0x102a0d54: halfword 1 is the sprite id stored at +0xc2
uint16_t gPersonCellA = 0;            // 0x102a0f14
uint16_t gPersonCellB = 0;            // 0x102a0f16

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

template <class T>
T& At(CPerson* p, size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(p) + offset);
}

// The Mac slot of gMainBalance at a Mac offset: (offset - 8) / 4.
int32_t Bal(int32_t offset) { return gMainBalance.mValues[(offset - 8) / 4].mInteger; }

}  // namespace

// 0x10116634
CPerson::CPerson(MapId cell) {
    new (this) CPeep(cell, static_cast<CThing::ThingModel>(1));
    const float zero = 0.0f;
    At<float>(this, 0x19c) = zero;
    At<int32_t>(this, 0x1a0) = 0;
    At<float>(this, 0x1a4) = zero;
    At<float>(this, 0x1a8) = zero;
    At<float>(this, 0x1ac) = zero;
    At<float>(this, 0x1b0) = zero;
    At<float>(this, 0x1b4) = zero;
    At<float>(this, 0x1b8) = zero;
    At<int32_t>(this, 0x1bc) = 0;
    At<uint8_t>(this, 0x1c0) = 0;
    At<int32_t>(this, 0x1c4) = 0;
    At<int32_t>(this, 0x1c8) = 0;
    At<int32_t>(this, 0x1cc) = 0;
    At<int32_t>(this, 0x1d0) = 0;
    At<int32_t>(this, 0x1d4) = 0;
    At<uint16_t>(this, 0x1dc) = 0;
    At<uint16_t>(this, 0x1de) = 0;
    // __construct_array of the two arrays of four thing ids (+0x1e0 and +0x1e8): the element constructor writes nothing the loop below
    // does not write again.
    At<uint8_t>(this, 0x1f0) = 0;
    At<uint8_t>(this, 0x1f1) = 0;
    At<int32_t>(this, 0x1f4) = 0;
    At<int32_t>(this, 0x1f8) = 0;
    At<int32_t>(this, 0x1fc) = 0;
    At<int32_t>(this, 0x200) = 0;
    At<int32_t>(this, 0x204) = 0;
    At<int32_t>(this, 0x208) = 0;
    At<float>(this, 0x20c) = zero;
    At<int32_t>(this, 0x210) = 0;
    At<int32_t>(this, 0x214) = 0;
    At<float>(this, 0x218) = zero;
    At<float>(this, 0x21c) = zero;
    At<int32_t>(this, 0x220) = 6;
    At<int32_t>(this, 0x224) = 6;
    At<uint16_t>(this, 0x228) = 0;
    At<uint16_t>(this, 0x22a) = 0;

    At<uint32_t>(this, 0x1d4) = gWorld->mTurn;
    At<int32_t>(this, 0x1d8) = 0;
    At<int32_t>(this, 0x1bc) = Bal(0xc);
    int32_t spread = Bal(0x10);
    int32_t a = static_cast<int32_t>(gWorld->GetRand());
    int32_t d = spread * 2;
    int32_t rem = a - (a / d) * d;
    At<int32_t>(this, 0x1bc) = At<int32_t>(this, 0x1bc) + (rem - spread);

    int32_t count = Bal(0x194);
    int32_t b = static_cast<int32_t>(gWorld->GetRand());
    int32_t which = b - (b / count) * count;
    At<uint8_t>(this, 0x1f0) = static_cast<uint8_t>(which);
    int32_t base = Bal(0xa8 + which * 12);
    int32_t range = Bal(0x14);
    int32_t d2 = range * 2 + 1;
    int32_t c = static_cast<int32_t>(gWorld->GetRand());
    int32_t rem2 = c - (c / d2) * d2;
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(base) * static_cast<uint32_t>(rem2 - range + 100));
    int32_t hi = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * static_cast<int64_t>(scaled)) >> 32);
    int32_t shifted = hi >> 5;
    int32_t result = shifted + static_cast<int32_t>(static_cast<uint32_t>(shifted) >> 31);
    if (result < 0) result = 0;
    At<int32_t>(this, 0x1a0) = result;
    At<float>(this, 0x19c) = 50.0f;

    At<float>(this, 0x1a4) = static_cast<float>(gWorld->GetRand() % 50u);
    At<float>(this, 0x1a8) = static_cast<float>(gWorld->GetRand() % 50u);
    At<float>(this, 0x1ac) = static_cast<float>(gWorld->GetRand() % 30u);
    At<float>(this, 0x1b4) = zero;
    At<float>(this, 0x1b0) = zero;
    At<float>(this, 0x1b8) = zero;
    gWorld->GetRand();
    uint32_t pick = gWorld->GetRand() % 100u;
    At<uint8_t>(this, 0x1c0) = pick < static_cast<uint32_t>(Bal(0x9c)) ? 0x64 : 0;
    if (At<uint8_t>(this, 0x1c0) == 0x64) {
        uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        At<uint8_t>(this, 0x1c0) = static_cast<uint8_t>(At<uint8_t>(this, 0x1c0) + id % 3u);
    }

    At<uint16_t>(this, 0x1dc) = 0;
    At<uint16_t>(this, 0x1de) = 0;
    At<uint16_t>(this, 0x228) = 0;
    At<uint16_t>(this, 0x22a) = 0;
    At<uint8_t>(this, 0x1f1) = 0;
    At<int32_t>(this, 0x1f8) = 0;
    At<int32_t>(this, 0x208) = 0;
    gWorld->SeedRand(reinterpret_cast<CThing*>(this)->GetThingId().mIndex);
    At<int32_t>(this, 0x24) = 0;
    At<int32_t>(this, 0x20) = ESP_GetRandomSpritesFrom(static_cast<ESP_Sprites>(At<int32_t>(this, 0x24)));
    At<CDelayedScript>(this, 0xc).SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1), At<int32_t>(this, 0x24), At<int32_t>(this, 0x20));
    At<uint16_t>(this, 0xc2) = gPersonTable[1];

    if (reinterpret_cast<CPeep*>(this)->IsInsidePark() == 0) {
        MapId side = gWorldMap->GetCrossingBSSidePos(static_cast<int32_t>(gWorld->GetRand() & 1u));
        uint8_t offset = static_cast<uint8_t>(gWorld->GetRand());
        uint32_t cellMinus1 = static_cast<uint32_t>(side.mIndex) - 1u;
        MapPos x;
        MapPos y;
        x.mCell = static_cast<uint8_t>(cellMinus1 & 0x7f);
        x.mOffset = offset;
        y.mCell = static_cast<uint8_t>(cellMinus1 >> 7);
        y.mOffset = 200;
        if (reinterpret_cast<CPeep*>(this)->SetDest(x, y) != 0) {
            SetState(static_cast<PersonState>(0));
        } else {
            SetState(static_cast<PersonState>(6));
        }
        reinterpret_cast<CPeep*>(this)->UpdatePositionInEngine(zero);
        reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
    } else {
        At<int32_t>(this, 0x1d8) = gWorld->GetNewPersonIndex();
        SetState(static_cast<PersonState>(6));
    }

    At<int32_t>(this, 0x204) = 0;
    for (int32_t i = 0; i < 4; i++) {
        At<uint16_t>(this, 0x1e0 + i * 2) = 0;
        At<uint16_t>(this, 0x1e8 + i * 2) = 0;
    }

    CMsgPeepArrived arrived(reinterpret_cast<CThing*>(this)->GetThingId());
    if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;   // 0x1d0 bytes: 29 subscription headers
    gMessageCentre->PostMessage(arrived);
}

// 0x10116e24
void CPerson::ReceiveMessage(const CMsg& message) {
    if (message.GetType() != 10) return;
    const uint16_t gone = static_cast<const CMsgRideDeleted&>(message).mThingId;
    if (At<uint16_t>(this, 0x1dc) == gone) {
        if (At<int32_t>(this, 0x220) == 0x10) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            if (At<int32_t>(this, 0xc) != 0) {
                At<CDelayedScript>(this, 0xc).SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1));
            } else {
                At<CDelayedScript>(this, 0xc).SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(3), At<int32_t>(this, 0x24), At<int32_t>(this, 0x20));
            }
            At<int32_t>(this, 0x28) = 0;
            At<CThoughts>(this, 0x30).RecordEvent(static_cast<CThoughts::EventType>(0xd), ThingId{At<uint16_t>(this, 0x1dc)});
            reinterpret_cast<CPeep*>(this)->PlaySound(gSoundBanks[1], 0x80);
        } else {
            if (InQueueingState() != 0) DismissFromQueue();
        }
        LowerHappiness(static_cast<Amount>(0));
        At<uint16_t>(this, 0x1dc) = 0;
        uint16_t c22a = At<uint16_t>(this, 0x22a);
        uint16_t c228 = At<uint16_t>(this, 0x228);
        uint16_t c1dc = At<uint16_t>(this, 0x1dc);
        uint16_t myId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        int32_t ok = 0;
        if (gPersonCellA == At<uint16_t>(this, 0x228) && gPersonCellB == At<uint16_t>(this, 0x22a)) ok = 1;
        GAME_ASSERTMSG(ok, "Person %d is in queue for object %d (next %d, prev %d) but doesn't think he is\n", myId, c1dc, c228, c22a);
        SetState(static_cast<PersonState>(6));
    }
    if (At<uint16_t>(this, 0x1de) == gone) At<uint16_t>(this, 0x1de) = 0;
    for (int32_t i = 0; i < 4; i++) {
        if (At<uint16_t>(this, 0x1e0 + i * 2) == gone) {
            At<uint16_t>(this, 0x1e0 + i * 2) = 0;
            At<uint16_t>(this, 0x1e8 + i * 2) = 0;
        }
    }
}
