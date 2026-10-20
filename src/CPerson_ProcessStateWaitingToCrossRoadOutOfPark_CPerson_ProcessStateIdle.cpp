// CPerson::ProcessStateIdle (0x1011baa8, 2,796 bytes; unit v5098): what an idle person does in a turn: look bored or sulk, drop litter, look for a
// bin, walk out of the park when unhappy, turn to fireworks or an entertainer, burst balloons or drop litter on purpose, then wander or decide.
// The person is reached by Mac offset (the host class has no members).
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "CMessageCentre.h"
#include "CMsg.h"
#include "Debug.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"

uint32_t gLitterKindCount = 6;   // 0x102a0b20
uint16_t gNoLitterBin = 0;       // 0x102a0ff0
uint16_t gNoFireworks = 0;       // 0x102a0ff2
uint16_t gNoBalloonThing = 0;    // 0x102a0ff4
uint16_t gNoDecision = 0;        // 0x102a0ff6

// Placeholder: the real type of the message (vtable 0x102a10dc) is not recovered.
int32_t CMsgPersonIdle::GetType() const { return 0; }

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10244aa8

template <class T>
T& At(void* base, std::ptrdiff_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }

// The map cell id of the thing at x = +4, y = +6.
uint16_t CellOf(void* thing) {
    const uint8_t* b = static_cast<const uint8_t*>(thing);
    return static_cast<uint16_t>(b[4] + (b[6] << 7) + 1);
}

CMapCell* CellAt(uint8_t* base, uint16_t cell) {
    return reinterpret_cast<CMapCell*>(base + static_cast<int32_t>(cell - 1) * 0x44);
}

uint32_t Low8(float value) { return static_cast<uint32_t>(ppc_fctiwz(value)) & 0xffu; }

void* ThingAt(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return gThingArray[id].mThing;
}

// The cell offsets from `mine` to `target` (the asm subtracts 1 in 32 bits, so cell 0 wraps).
void Delta(uint16_t target, uint16_t mine, int32_t& dx, int32_t& dy) {
    uint32_t t = static_cast<uint32_t>(target) - 1u;
    uint32_t m = static_cast<uint32_t>(mine) - 1u;
    dx = static_cast<int32_t>(t & 0x7f) - static_cast<int32_t>(m & 0x7f);
    dy = static_cast<int32_t>(t >> 7) - static_cast<int32_t>(m >> 7);
}

}  // namespace

// 0x1011baa8
void CPerson::ProcessStateIdle() {
    uint8_t* const p = reinterpret_cast<uint8_t*>(this);
    CPeep* const peep = reinterpret_cast<CPeep*>(this);
    CThing* const thing = reinterpret_cast<CThing*>(this);
    CThoughts* const thoughts = reinterpret_cast<CThoughts*>(p + 0x30);

    const uint32_t rand = gWorld->GetRand();

    if (gWorld->mTurn - At<uint32_t>(p, 0x208) > 0x64u && Low8(At<float>(p, 0x19c)) > 0x50u) {
        SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(5));
        return;
    }

    const uint32_t bored = Low8(At<float>(p, 0x1b0));
    if (bored > 0x50u && bored == 100u && rand % 3 == 0) {
        SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(7));
        const uint16_t cell = CellOf(p);
        CellAt(reinterpret_cast<uint8_t*>(gWorldMap), cell)->PlaceLitter(static_cast<CMapCell::LitterType>(7));
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x12), ThingId{0});
        peep->PlaySound(gSoundBanks[1], 0xcc);
        At<float>(p, 0x1b0) = 0.0f;
        return;
    }

    if (Low8(At<float>(p, 0x1b4)) >= 0x5au) {
        const int32_t state = At<int32_t>(p, 0x220);
        if (state == 6 || state == 7 || state == 10) {
            const ThingId bin = FindNearestReachableLitterBin(3);
            if (bin.mIndex != gNoLitterBin) {
                At<uint16_t>(p, 0x1dc) = bin.mIndex;
                SetState(static_cast<PersonState>(9));
                return;
            }
            const uint32_t litterRand = gWorld->GetRand();
            const uint16_t cell = CellOf(p);
            const uint32_t divisor = gLitterKindCount - 1u;
            const uint32_t quotient = divisor != 0 ? litterRand / divisor : 0;   // divwu by 0 answers 0 on the PowerPC
            CellAt(reinterpret_cast<uint8_t*>(gWorldMap), cell)->PlaceLitter(static_cast<CMapCell::LitterType>(litterRand - quotient * divisor + 1u));
            At<float>(p, 0x1b4) = 0.0f;
        }
    }

    if (Low8(At<float>(p, 0x19c)) == 0 || At<int32_t>(p, 0x1bc) == 0 || gWorld->IsParkClosed() != 0) {
        LowerHappiness(static_cast<Amount>(2));
        gWorld->ParkGates_GetStatus();
        MapId sides[2];
        sides[0] = gWorldMap->GetCrossingParkSidePos(0);
        sides[1] = gWorldMap->GetCrossingParkSidePos(1);
        if ((rand & 1) != 0) {
            const MapId first = sides[0];
            sides[0] = sides[1];
            sides[1] = first;
        }
        for (int32_t round = 0; round < 2; ++round) {
            for (int32_t i = 0; i < 2; ++i) {
                if (peep->SetDest(sides[i]) != 0) {
                    SetState(static_cast<PersonState>(0x12));
                    return;
                }
            }
            At<int32_t>(p, 0x188) = 1;
        }
        At<int32_t>(p, 0x188) = 0;
    }

    const ThingId fireworks = FindNearestFireworks();
    if (fireworks.mIndex != gNoFireworks) {
        void* target = ThingAt(fireworks.mIndex);
        int32_t dx, dy;
        Delta(CellOf(target), CellOf(p), dx, dy);
        if (dx + dy != 0) {
            At<uint32_t>(p, 0x1c) = static_cast<uint32_t>(LbMaths_ArcTan(-dx, dy)) & 0xffffu;
            return;
        }
    }

    const MapId here{CellOf(p)};
    if (gWorld->GetParkAnalyser()->IsNearbyThingOfType(static_cast<CThing::ThingModel>(6), here, 1) != 0) {
        const ThingId entertainer = FindNearestEntertainer();
        void* target = ThingAt(entertainer.mIndex);
        if (At<int32_t>(target, 0x19c) == 0xe) {
            int32_t dx, dy;
            Delta(CellOf(target), CellOf(p), dx, dy);
            if (dx + dy != 0) {
                const ThingId theirs = reinterpret_cast<CThing*>(target)->GetThingId();   // r4 still holds the entertainer
                thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xe), theirs);
                At<uint32_t>(p, 0x1c) = static_cast<uint32_t>(LbMaths_ArcTan(-dx, dy)) & 0xffffu;
                return;
            }
        }
    }

    bool acted = false;
    if (Low8(At<float>(p, 0x19c)) < 0xfu) {
        const uint32_t kind = At<uint8_t>(p, 0x1c0);
        if (rand % 101u < kind) {
            if (kind >= 100u) {
                const int32_t which = static_cast<int32_t>(kind) - 100;
                uint8_t* const world = reinterpret_cast<uint8_t*>(gWorld) + 0x2d8;
                if (which == 1) {
                    CellAt(world, CellOf(p))->PlaceLitter(static_cast<CMapCell::LitterType>(rand % 5u + 1u));
                    acted = true;
                } else if (which < 1) {
                    if (which >= 0 && rand % 100u < static_cast<uint32_t>(gMainBalance.mValues[38].mInteger)) {
                        CellAt(world, CellOf(p))->PlaceLitter(static_cast<CMapCell::LitterType>(8));
                        peep->PlaySound(gSoundBanks[1], 0xcf);
                        acted = true;
                    }
                } else if (which < 3) {
                    uint16_t current = CellAt(world, CellOf(p))->mWho;
                    while (current != gNoBalloonThing) {
                        void* other = ThingAt(current);
                        const ThingId self = thing->GetThingId();
                        if (current != self.mIndex && At<uint8_t>(other, 2) == 1 && At<int32_t>(other, 0x210) != 0) {
                            reinterpret_cast<CPerson*>(other)->MakeBalloonPop();
                            acted = true;
                            break;
                        }
                        current = At<uint16_t>(other, 0xa);
                    }
                }
            }
            if (acted) {
                thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xf), ThingId{0});
                const ThingId first = thing->GetThingId();
                const uint32_t third = static_cast<uint32_t>(first.mIndex) % 3u;
                const ThingId second = thing->GetThingId();
                CMsgPersonIdle message;
                message.mFlags = 0;
                message.mThingId = second.mIndex;
                message.mValue = third;
                if (gMessageCentre == nullptr) gMessageCentre = new CMessageCentre;   // 0x1d0 bytes: 29 subscription headers
                gMessageCentre->PostMessage(message);
                const float happy = 1.0f + At<float>(p, 0x19c);
                if (happy > 100.0f) {
                    At<float>(p, 0x19c) = 100.0f;
                } else if (happy < 0.0f) {
                    At<float>(p, 0x19c) = 0.0f;
                } else {
                    At<float>(p, 0x19c) = happy;
                }
            }
        }
    }

    const uint32_t pick = rand % 3u;
    if (pick == 1) {
        if (peep->SetRandomDestination() != 0) {
            SetState(static_cast<PersonState>(7));
        } else {
            At<uint32_t>(p, 0x1fc) = gWorld->mTurn;
        }
    } else if (pick == 0 && gWorld->mTurn > At<uint32_t>(p, 0x1fc) + 0x1eu) {
        MakeMajorDecision();
        if (At<uint16_t>(p, 0x1dc) != gNoDecision) {
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(2), ThingId{At<uint16_t>(p, 0x1dc)});
            SetState(static_cast<PersonState>(10));
        } else {
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(1), ThingId{0});
            SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(4));
            LowerHappiness(static_cast<Amount>(0));
            At<uint32_t>(p, 0x1fc) = gWorld->mTurn;
        }
    }
}
