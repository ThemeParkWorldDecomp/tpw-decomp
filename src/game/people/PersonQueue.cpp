// PersonQueue.cpp: name proven by __sinit_PersonQueue_cpp in the main application.

#include "analyser.h"
#include "CellBase.h"
#include "CMessageCentre.h"
#include "CMsg.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "helpers/PersonHelpers.h"
#include "HLSprite.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "Serialise.h"
#include "ThingParts2.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPSound.h"
#include "VehicleSteer.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// CPerson::ProcessStateIdle (0x1011baa8, 2,796 bytes; unit v5098): what an idle person does in a turn: look bored or sulk, drop litter, look for a
// bin, walk out of the park when unhappy, turn to fireworks or an entertainer, burst balloons or drop litter on purpose, then wander or decide.
// The person is reached by Mac offset (the host class has no members).

uint32_t gLitterKindCount = 6;   // 0x102a0b20
uint16_t gNoLitterBin = 0;       // 0x102a0ff0
uint16_t gNoFireworks = 0;       // 0x102a0ff2
uint16_t gNoBalloonThing = 0;    // 0x102a0ff4
uint16_t gNoDecision = 0;        // 0x102a0ff6

// The message is the original's CMsgPrankery (include/ThingReceivers.h), whose GetType (0x10120b90) answers 14, a prank:
// CStaffHQ::ReceiveMessage sends the nearest free guard after the prankster for that type. Found by samueleagle91
// (tpw-port issue #51).
int32_t CMsgPersonIdle::GetType() const { return 0xe; }

namespace {

using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10244aa8

template <class T>
T& At_4(void* base, std::ptrdiff_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }

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

    if (gWorld->mTurn - At_4<uint32_t>(p, 0x208) > 0x64u && Low8(At_4<float>(p, 0x19c)) > 0x50u) {
        SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(5));
        return;
    }

    const uint32_t bored = Low8(At_4<float>(p, 0x1b0));
    if (bored > 0x50u && bored == 100u && rand % 3 == 0) {
        SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(7));
        const uint16_t cell = CellOf(p);
        CellAt(reinterpret_cast<uint8_t*>(gWorldMap), cell)->PlaceLitter(static_cast<CMapCell::LitterType>(7));
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x12), ThingId{0});
        peep->PlaySound(gSoundBanks[1], 0xcc);
        At_4<float>(p, 0x1b0) = 0.0f;
        return;
    }

    if (Low8(At_4<float>(p, 0x1b4)) >= 0x5au) {
        const int32_t state = At_4<int32_t>(p, 0x220);
        if (state == 6 || state == 7 || state == 10) {
            const ThingId bin = FindNearestReachableLitterBin(3);
            if (bin.mIndex != gNoLitterBin) {
                At_4<uint16_t>(p, 0x1dc) = bin.mIndex;
                SetState(static_cast<PersonState>(9));
                return;
            }
            const uint32_t litterRand = gWorld->GetRand();
            const uint16_t cell = CellOf(p);
            const uint32_t divisor = gLitterKindCount - 1u;
            const uint32_t quotient = divisor != 0 ? litterRand / divisor : 0;   // divwu by 0 answers 0 on the PowerPC
            CellAt(reinterpret_cast<uint8_t*>(gWorldMap), cell)->PlaceLitter(static_cast<CMapCell::LitterType>(litterRand - quotient * divisor + 1u));
            At_4<float>(p, 0x1b4) = 0.0f;
        }
    }

    if (Low8(At_4<float>(p, 0x19c)) == 0 || At_4<int32_t>(p, 0x1bc) == 0 || gWorld->IsParkClosed() != 0) {
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
            At_4<int32_t>(p, 0x188) = 1;
        }
        At_4<int32_t>(p, 0x188) = 0;
    }

    const ThingId fireworks = FindNearestFireworks();
    if (fireworks.mIndex != gNoFireworks) {
        void* target = ThingAt(fireworks.mIndex);
        int32_t dx, dy;
        Delta(CellOf(target), CellOf(p), dx, dy);
        if (dx + dy != 0) {
            At_4<uint32_t>(p, 0x1c) = static_cast<uint32_t>(LbMaths_ArcTan(-dx, dy)) & 0xffffu;
            return;
        }
    }

    const MapId here{CellOf(p)};
    if (gWorld->GetParkAnalyser()->IsNearbyThingOfType(static_cast<CThing::ThingModel>(6), here, 1) != 0) {
        const ThingId entertainer = FindNearestEntertainer();
        void* target = ThingAt(entertainer.mIndex);
        if (At_4<int32_t>(target, 0x19c) == 0xe) {
            int32_t dx, dy;
            Delta(CellOf(target), CellOf(p), dx, dy);
            if (dx + dy != 0) {
                const ThingId theirs = reinterpret_cast<CThing*>(target)->GetThingId();   // r4 still holds the entertainer
                thoughts->RecordEvent(static_cast<CThoughts::EventType>(0xe), theirs);
                At_4<uint32_t>(p, 0x1c) = static_cast<uint32_t>(LbMaths_ArcTan(-dx, dy)) & 0xffffu;
                return;
            }
        }
    }

    bool acted = false;
    if (Low8(At_4<float>(p, 0x19c)) < 0xfu) {
        const uint32_t kind = At_4<uint8_t>(p, 0x1c0);
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
                        if (current != self.mIndex && At_4<uint8_t>(other, 2) == 1 && At_4<int32_t>(other, 0x210) != 0) {
                            reinterpret_cast<CPerson*>(other)->MakeBalloonPop();
                            acted = true;
                            break;
                        }
                        current = At_4<uint16_t>(other, 0xa);
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
                const float happy = 1.0f + At_4<float>(p, 0x19c);
                if (happy > 100.0f) {
                    At_4<float>(p, 0x19c) = 100.0f;
                } else if (happy < 0.0f) {
                    At_4<float>(p, 0x19c) = 0.0f;
                } else {
                    At_4<float>(p, 0x19c) = happy;
                }
            }
        }
    }

    const uint32_t pick = rand % 3u;
    if (pick == 1) {
        if (peep->SetRandomDestination() != 0) {
            SetState(static_cast<PersonState>(7));
        } else {
            At_4<uint32_t>(p, 0x1fc) = gWorld->mTurn;
        }
    } else if (pick == 0 && gWorld->mTurn > At_4<uint32_t>(p, 0x1fc) + 0x1eu) {
        MakeMajorDecision();
        if (At_4<uint16_t>(p, 0x1dc) != gNoDecision) {
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(2), ThingId{At_4<uint16_t>(p, 0x1dc)});
            SetState(static_cast<PersonState>(10));
        } else {
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(1), ThingId{0});
            SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(4));
            LowerHappiness(static_cast<Amount>(0));
            At_4<uint32_t>(p, 0x1fc) = gWorld->mTurn;
        }
    }
}

// Unit v5954: the person's walk through the ticket booth: CPerson::LeaveQueue, ProcessStatePayingAtBooth, ProcessStateWaitingForParkToOpen,
// ProcessStateWalkingToTicketBooths and FeelingAboutEntryFee. The person is reached by Mac offset.

// 0x102a0d58: the third halfword of the person sprite table (the header declares gPersonTable with two; the Mac reads the one after them
// when the bus is coming).
uint16_t gPersonTableBusSprite = 0x32;

namespace {

// The texts at the TOC base 0x10244aa8 (+0x43, +0x6c, +0xb4, +0xc4, +0xea, +0x10a, +0x12e, +0x14e).
const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kBigProblem[] = "Big problem...no way this should happen\n";
const char kLonghand[] = "Working out how exciting park is longhand - should do this in Analyser\n";
const char kGpAf[] = "GP: %d, AF: %d\n";
const char kBusComing[] = "The bus is coming!  RUUUUUUUUUUN!!!!\n";
const char kTooExpensive[] = "Person: park far too expensive\n";
const char kText10a[] = "Person: park on the expensive side\n";
const char kText12e[] = "Person: park on the cheap side\n";
const char kText14e[] = "Person: your admission fee is about right\n";

template <class T>
inline T& At_2(void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

inline uint16_t CellOf_2(void* person) {
    uint32_t x = At_2<uint8_t>(person, 4);
    uint32_t y = At_2<uint8_t>(person, 6);
    return static_cast<uint16_t>(x + (y << 7) + 1);
}

}  // namespace

// 0x1011c63c
int32_t CPerson::FeelingAboutEntryFee() {
    CTRACE(2, kLonghand);
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    uint32_t points = static_cast<uint32_t>(analyser->GetTotalParkAttractionPoints());
    uint32_t base = static_cast<uint32_t>(gMainBalance.mValues[13].mInteger);   // 0x3c
    uint32_t rnd = gWorld->GetRand();
    uint32_t modulus = (base >> 1) + 1;
    uint32_t remainder = rnd - (rnd / modulus) * modulus;
    uint32_t divisor = base + remainder;
    uint32_t quotient = divisor == 0 ? points : points / divisor;   // divwu by 0 is undefined on the hardware; the emulator answers the dividend
    CBankAccount* bank = gWorld->GetBankAccount();
    uint32_t balance = At_2<uint32_t>(bank, 0x118);
    uint32_t limit = static_cast<uint32_t>(gMainBalance.mValues[14].mInteger);   // 0x40
    uint32_t feeling = quotient + limit;
    CTRACE(2, kGpAf, static_cast<int32_t>(feeling), static_cast<int32_t>(balance));
    if (!(feeling > limit || balance > limit)) return 3;
    float fFeeling = static_cast<float>(feeling);
    float fBalance = static_cast<float>(balance);
    float c44 = gMainBalance.mValues[15].mFloat;
    float c48 = gMainBalance.mValues[16].mFloat;
    float c4c = gMainBalance.mValues[17].mFloat;
    float product = fFeeling * c48;
    if (fBalance >= product) {
        float product2 = fFeeling * c4c;
        if (fBalance >= product2) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[33].mInteger));   // 0x8c
            return 0;
        }
        gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[34].mInteger));   // 0x90
        return 1;
    }
    float product3 = fFeeling * c44;
    if (fBalance <= product3) {
        gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[35].mInteger));   // 0x94
        return 2;
    }
    gWorld->GetParkAnalyser()->OfferOpinionOn(0, static_cast<uint8_t>(gMainBalance.mValues[36].mInteger));   // 0x98
    return 3;
}

// 0x1011c88c
void CPerson::ProcessStateWalkingToTicketBooths() {
    bool bus = false;
    if (gWorld->ArrivalVehicle_IsSeagoing() == 0) {
        if (gWorld->ArrivalVehicle_GetStatus() == 3) {
            CTRACE(2, kBusComing);
            At_2<uint16_t>(this, 0xc2) = gPersonTableBusSprite;
            bus = true;
        }
    }
    if (!bus) {
        auto id = reinterpret_cast<CThing*>(this)->GetThingId();
        if ((id.mIndex & 3) == 0) At_2<uint16_t>(this, 0xc2) = gPersonTable[1];
        else At_2<uint16_t>(this, 0xc2) = gPersonTable[0];
    }
    int32_t walk = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walk == 0) {
        if (gWorld->IsParkClosed() != 0) SetState(static_cast<PersonState>(3));
        else SetState(static_cast<PersonState>(4));
    } else if (walk == 2) {
        GAME_ASSERTMSG(0, kBigProblem);
    }
}

// 0x1011c9f8
void CPerson::ProcessStateWaitingForParkToOpen() {
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    if (gWorld->IsParkClosed() == 0 && gWorld->ParkGates_GetStatus() == 1) {
        if (At_2<uint32_t>(this, 0x204) == 0) {
            CMap* map = gWorldMap;
            uint16_t booth = map->GetTicketBoothPos(0).mIndex;
            if (CellOf_2(this) == booth) {
                peep->SetDest(map->GetTicketBoothPos(0));
            } else {
                uint16_t booth1 = map->GetTicketBoothPos(1).mIndex;
                if (CellOf_2(this) == booth1) {
                    peep->SetDest(map->GetTicketBoothPos(1));
                } else {
                    uint32_t rnd = gWorld->GetRand();
                    peep->SetDest(map->GetTicketBoothPos(static_cast<int32_t>(rnd & 1)));
                }
            }
            At_2<uint32_t>(this, 0x200) = 0;
            SetState(static_cast<PersonState>(2));
        } else {
            int32_t cell = CellOf_2(this);
            uint16_t who = At_2<uint16_t>(gWorldMap, static_cast<uint32_t>((cell - 1) * 0x44 + 0x24));
            auto id = reinterpret_cast<CThing*>(this)->GetThingId();
            if (who == id.mIndex) {
                uint32_t rnd = gWorld->GetRand();
                peep->SetDest(gWorldMap->GetParkEntrancePos(static_cast<int32_t>(rnd & 1)));
                SetState(static_cast<PersonState>(5));
            }
        }
        return;
    }
    if (At_2<uint32_t>(this, 0x200) == 0) {
        At_2<int32_t>(this, 0x188) = 1;
        uint32_t rnd = gWorld->GetRand();
        peep->SetDest(gWorldMap->GetCrossingParkSidePos(static_cast<int32_t>(rnd & 1)));
        SetState(static_cast<PersonState>(0x12));
        At_2<uint32_t>(this, 0x1bc) = 0;
    } else {
        At_2<uint32_t>(this, 0x200) = At_2<uint32_t>(this, 0x200) - 1;
    }
}

// 0x1011cce4
void CPerson::ProcessStatePayingAtBooth() {
    uint32_t wait = At_2<uint32_t>(this, 0x200);
    if (wait != 0) {
        At_2<uint32_t>(this, 0x200) = wait - 1;
        return;
    }
    int32_t feeling = FeelingAboutEntryFee();
    bool pay = false;
    if (feeling == 2) {
        CTRACE(2, kText12e);
        RaiseHappiness(static_cast<Amount>(1));
        pay = true;
    } else if (feeling < 2) {
        if (feeling == 0) {
            CTRACE(2, kTooExpensive);
            At_2<int32_t>(this, 0x188) = 1;
            CMap* map = gWorldMap;
            uint32_t rnd = gWorld->GetRand();
            reinterpret_cast<CPeep*>(this)->SetDest(map->GetCrossingParkSidePos(static_cast<int32_t>(rnd & 1)));
            reinterpret_cast<CThoughts*>(reinterpret_cast<uint8_t*>(this) + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
            SetState(static_cast<PersonState>(0x12));
            At_2<uint32_t>(this, 0x1bc) = 0;
            return;
        }
        if (feeling < 0) return;
        CTRACE(2, kText10a);
        uint32_t rnd = gWorld->GetRand();
        At_2<uint32_t>(this, 0x200) = rnd % 0x32 + 0x32;
        reinterpret_cast<CThoughts*>(reinterpret_cast<uint8_t*>(this) + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
        LowerHappiness(static_cast<Amount>(1));
        int32_t happiness = ppc_fctiwz(static_cast<double>(At_2<float>(this, 0x19c)));
        if ((static_cast<uint32_t>(happiness) & 0xffu) != 0) return;
        SetState(static_cast<PersonState>(0x12));
        At_2<uint32_t>(this, 0x1bc) = 0;
        return;
    } else if (feeling >= 4) {
        return;
    }
    (void)pay;
    CTRACE(2, kText14e);
    gWorld->GetBankAccount()->PayAdmission();
    At_2<uint32_t>(this, 0x204) = 1;
    SetState(static_cast<PersonState>(3));
}

// Globals the original has in its initialised data (names invented, zero at start).
uint16_t gNoMajorRide = 0;   // 0x102a0ff8
uint16_t gNoMajorCell = 0;   // 0x102a0ffa

namespace {

const char kRowZeroMajor[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kQueueTooLongText[] = "Person %d: ride %d's queue is too long!\n";                       // 0x10244c21
const char kNoPlaceText[] = "Person %d: Couldn't get to my place in the queue, leaving!\n";         // 0x10244c4a
const char kNotExcitingText[] = "Person: ride is not exciting enough!\n";                          // 0x10244c86
const char kTooExcitingText[] = "Person: ride is too exciting!\n";                                 // 0x10244cac
const char kQueueMovedText[] = "The back of the queue has moved while I was walking here\n";       // 0x10244ccb
const char kStuckText[] = "The person has become stuck on their way to the ride they were interested in\n";   // 0x10244d05
const char kParkClosedText[] = "The park has closed underneath me!\n";                             // 0x10244d53

}  // namespace

// 0x1011cf18
void CPerson::ProcessStateWalksToMajor() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingTag;
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapTag;

    uint16_t ride = Fld_2<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroMajor);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[ride].mThing);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks != 0) {
        if (walks == 2) {
            CTRACE(2, kStuckText);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(3), ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
            LowerHappiness(static_cast<Amount>(2));
            Fld_2<uint16_t>(self, 0x1dc) = 0;
            SetState(static_cast<PersonState>(6));
            return;
        }
        if (gWorld->IsParkClosed() != 0) {
            CTRACE(2, kParkClosedText);
            LowerHappiness(static_cast<Amount>(2));
            Fld_2<uint16_t>(self, 0x1dc) = 0;
            SetState(static_cast<PersonState>(6));
            return;
        }
        Fld_2<uint8_t>(self, 0x2c) = static_cast<uint8_t>(Fld_2<uint8_t>(self, 0x2c) + 1);
        if (Fld_2<uint8_t>(self, 0x2c) <= 0xb) return;
        Fld_2<uint8_t>(self, 0x2c) = 0;
        LookForMinorDest();
        return;
    }
    if (Fld_2<uint16_t>(self, 0x1dc) != gNoMajorRide) {
        uint16_t place = static_cast<uint16_t>(Fld_2<uint8_t>(self, 4) + (static_cast<uint32_t>(Fld_2<uint8_t>(self, 6)) << 7) + 1);
        MapTag back = object->GetBackOfQueue();
        if (back.mIndex == place) {
            if (object->HasFreeQueueSpace() != 0) {
                int32_t score = ScoreOnCloserInspection(object);
                uint8_t excitement = object->GetExcitementLevel(true);
                int32_t magnitude = score < 0 ? -score : score;
                if (excitement != 0 && magnitude >= 0x2d) {
                    if (score < 0) {
                        CTRACE(2, kNotExcitingText);
                        thoughts->RecordEvent(static_cast<CThoughts::EventType>(5), ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
                        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xc), false);
                    } else {
                        CTRACE(2, kTooExcitingText);
                        thoughts->RecordEvent(static_cast<CThoughts::EventType>(4), ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
                        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xf), false);
                    }
                    AddRideToRecentTemporaryHistory(ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
                    Fld_2<uint16_t>(self, 0x1dc) = 0;
                    Fld_2<uint32_t>(self, 0x1fc) = 0;
                    SetState(static_cast<PersonState>(6));
                    return;
                }
                if (object->QueueTooLong()) {
                    uint16_t objectId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
                    uint16_t personId = me->GetThingId().mIndex;
                    CTRACE(2, kQueueTooLongText, static_cast<uint32_t>(personId), static_cast<uint32_t>(objectId));
                    thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x15), ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
                    AddRideToRecentTemporaryHistory(ThingTag{Fld_2<uint16_t>(self, 0x1dc)});
                    thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x10), false);
                    Fld_2<uint16_t>(self, 0x1dc) = 0;
                    SetState(static_cast<PersonState>(6));
                    return;
                }
                Fld_2<uint32_t>(self, 0x20c) = Fld_2<uint32_t>(self, 0x19c);
                object->AddPersonToQueue(me->GetThingId());
                if (FindQueueDestination() != 0) return;
                uint16_t id = me->GetThingId().mIndex;
                CTRACE(2, kNoPlaceText, static_cast<uint32_t>(id));
                object->RemovePersonFromQueue(me->GetThingId());
                DismissFromQueue();
                SetState(static_cast<PersonState>(6));
                return;
            }
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x15), ThingTag{0});
        } else {
            CTRACE(0x1a, kQueueMovedText);
            MapTag now = object->GetBackOfQueue();
            if (now.mIndex != gNoMajorCell && reinterpret_cast<CPeep*>(this)->SetDest(now) != 0) return;
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x16), ThingTag{0});
        }
    }
    SetState(static_cast<PersonState>(6));
}

// Unit v5529: three of the person's states, from the walk to a litter bin to the wait for the ride's script.
// The person is reached by Mac offset (the host layout of the person is the Mac's).

uint16_t gNoLitterRide = 0;   // 0x102a0ffc
uint16_t gNoAdmitState = 0;   // 0x102a0ffe

namespace {

uint8_t* Raw_2(const void* p) { return const_cast<uint8_t*>(static_cast<const uint8_t*>(p)); }
const char kPathRemovedText[] = "Person %d: the player has removed the path from under me and I can no longer get into object %d\n";
const char kBeingLoadedText[] = "Person %d being loaded to ride %d\n";
const char kNotFirstText[] = "Person been admitted to a ride that they're not first in queue for\n";
const char kNotReadyText[] = "Person been admitted to ride %d but ride %d not ready to admit!\n";
const char kQueueProblemText[] = "Problem with a queue - shouldn't be fatal, closing and reopening the ride with the problem!";
const char kNoPositionText[] = "Couldn't get to my intended queue position\n";
const char kBoredText[] = "Person %d: OK, I've been standing here too long, I'm bored and leaving the queue\n";
const char kNotRemovedText[] = "Person not correctly removed from queue\n";

// The two copies of the litter drop in ProcessStateWalksToLitterBin.
void DropLitter(uint8_t* self) {
    uint32_t rnd = gWorld->GetRand();
    uint32_t kinds = gLitterKindCount - 1;
    uint32_t quotient = kinds != 0 ? rnd / kinds : 0;   // divwu by 0 answers 0 on the emulator
    uint16_t cell = static_cast<uint16_t>(self[4] + (self[6] << 7) + 1);
    CMapCell* target = reinterpret_cast<CMapCell*>(Raw_2(gWorldMap) + static_cast<int32_t>(cell - 1) * 0x44);
    target->PlaceLitter(static_cast<CMapCell::LitterType>(rnd - quotient * kinds + 1));
}

}  // namespace

// 0x1011d400
void CPerson::ProcessStateWalksToLitterBin() {
    uint8_t* self = Raw_2(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        if (Fld_2<uint16_t>(self, 0x1dc) == gNoLitterRide) {
            DropLitter(self);
        } else {
            uint16_t ride = Fld_2<uint16_t>(self, 0x1dc);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
            uint8_t* object = Raw_2(gThingArray[ride].mThing);
            RSSE_SCRIPT* script = RSSE_GetScriptPointer(Fld_2<int32_t>(object, 0x20));
            RSSE_Setvar(script, 0, 1);
        }
        Fld_2<float>(self, 0x1b4) = 0.0f;
        SetState(static_cast<PersonState>(6));
    } else if (walks == 2) {
        DropLitter(self);
        Fld_2<float>(self, 0x1b4) = 0.0f;
        SetState(static_cast<PersonState>(6));
    }
}

// 0x1011d5ec
void CPerson::ProcessStateStandingInQueue() {
    uint8_t* self = Raw_2(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    uint16_t rideIndex = Fld_2<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[rideIndex].mThing);
    uint8_t* obj = Raw_2(object);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);

    if (Fld_2<uint8_t>(self, 0x1f1) == 0 && Fld_2<uint32_t>(self, 0x1f8) != 0) {
        if (object->ReadyToAdmit(me->GetThingId())) {
            Fld_2<uint32_t>(self, 0x1f8) = 0;
            MapPos first, second;
            object->GetStandPoint(0, &first, &second);
            if (reinterpret_cast<CPeep*>(this)->SetDest(first, second) != 0) {
                uint16_t ride = Fld_2<uint16_t>(self, 0x1dc);
                uint16_t id = me->GetThingId().mIndex;
                CTRACE(7, kBeingLoadedText, static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
                SetState(static_cast<PersonState>(0xd));
                return;
            }
            uint16_t objectId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
            uint16_t id = me->GetThingId().mIndex;
            CTRACE(2, kPathRemovedText, static_cast<uint32_t>(id), static_cast<uint32_t>(objectId));
            object->PersonLeftDuringAdmittance(me->GetThingId());
            LeaveQueue();
            return;
        }
    }
    GAME_ASSERTMSG(Fld_2<uint32_t>(self, 0x1f8) == 0, kNotFirstText);
    if (Fld_2<uint8_t>(self, 0x1f1) == 0 && Fld_2<uint32_t>(self, 0x1f8) != 0) {
        if (!object->ReadyToAdmit(me->GetThingId())) {
            uint16_t ride = Fld_2<uint16_t>(self, 0x1dc);
            GAME_ASSERTMSG(0, kNotReadyText, Fld_2<uint32_t>(self, 0x1f8), static_cast<uint32_t>(ride));
            return;
        }
    }
    if (object->IsDirty() != 0) {
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xe), false);
        LeaveQueue();
        return;
    }
    uint32_t place = static_cast<uint32_t>(object->GetPositionInQueue(me->GetThingId()));
    if (place + 0x10000u == 0xffffu) {   // the listing's addis / cmplwi pair, not a compare with -1
        CTRACE(2, kQueueProblemText);
        LeaveQueue();
        return;
    }
    uint32_t mine = Fld_2<uint8_t>(self, 0x1f1);
    if (mine == place) {
        uint32_t ideal = object->GetIdealQueueLength();
        if (Fld_2<uint8_t>(self, 0x1f1) > ideal) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x10), false);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x15), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{Fld_2<uint16_t>(self, 0x1dc)});
            LeaveQueue();
            return;
        }
        bool leave = false;
        if (object->IsCoaster() != 0 && IsCoasterUseable(object) == 0) {
            leave = true;
        } else {
            RideInfo* info = object->GetDBInfo();
            // The WhichTrackType slot (the Mac's +0x9c) by name: the host's record is laid out wider.
            if (info->Value(kRideSlot_WhichTrackType).mInteger == 1 && Fld_2<int32_t>(obj, 0x28) == 0) leave = true;
        }
        if (leave) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xd), false);
            LeaveQueue();
            return;
        }
    } else if (Fld_2<uint32_t>(self, 0x1f4) == 0 || mine - place > 2u) {
        if (!object->IsBrokenDown() && FindQueueDestination() == 0) {
            CTRACE(2, kNoPositionText);
            LeaveQueue();
            return;
        }
    } else {
        Fld_2<uint32_t>(self, 0x1f4) = Fld_2<uint32_t>(self, 0x1f4) - 1;
    }

    CWorld* world = gWorld;
    uint32_t turn = world->mTurn;
    if (turn - Fld_2<uint32_t>(self, 0x208) > 0x1eu) {
        uint32_t happy = static_cast<uint32_t>(ppc_fctiwz(Fld_2<float>(self, 0x19c))) & 0xffu;
        if (happy > 0x50u) {
            SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(5));
        } else if (happy < 0x14u) {
            if (happy < 10u) {
                thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xb), false);
                LeaveQueue();
            } else {
                SetSpotAnim(static_cast<CDelayedScript::ScriptIdx>(4));
            }
        } else {
            uint32_t other = static_cast<uint32_t>(ppc_fctiwz(Fld_2<float>(self, 0x1ac))) & 0xffu;
            if (other > 0x50u) {
                thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(4), false);
                if ((Fld_2<uint16_t>(obj, 0x2e) & 1u) == 0) LeaveQueue();
            }
        }
    } else if (turn > Fld_2<uint32_t>(self, 0x1fc) + 100u) {
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, kBoredText, static_cast<uint32_t>(id));
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(7), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{Fld_2<uint16_t>(self, 0x1dc)});
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xc), false);
        LeaveQueue();
    } else {
        uint32_t rnd = world->GetRand();
        if (rnd % 10u == 0) {
            int32_t angle = Fld_2<int32_t>(self, 0x1c);
            uint32_t rnd2 = gWorld->GetRand();
            int32_t sum = angle + static_cast<int32_t>(rnd2 % 800u - 400u);
            if (sum < 0) sum += 0x800;
            else if (sum >= 0x800) sum -= 0x800;
            Fld_2<int32_t>(self, 0x1c) = sum;
        }
    }
}

// 0x1011dbf4
void CPerson::LeaveQueue() {
    uint16_t ride = At_2<uint16_t>(this, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = static_cast<CObject*>(gThingArray[ride].mThing);
    auto id = reinterpret_cast<CThing*>(this)->GetThingId();
    object->RemovePersonFromQueue(id);
    DismissFromQueue();
    SetState(static_cast<PersonState>(6));
}

// CPerson queue states and the nearest-thing searches (unit v5245):
//   FindNearestReachableLitterBin 0x1011e9ec, FindNearestEntertainer 0x1011ebc4,
//   ProcessStateBeingUnloaded 0x1011e0e4, ProcessStateBeingLoaded 0x1011dcc0.
// The person is reached by Mac offset (+4 / +6 cell bytes, +0x30 the CThoughts, +0x1dc ride halfword, +0x1de minor destination halfword).

// Halfword sentinels (NEW, no other file defines them); the Mac's bss is zero at start.
uint16_t gNoMinorDest;      // 0x102a1000
uint16_t gNoQueueBack;      // 0x102a1002
uint16_t gLitterListEnd;    // 0x102a1004
uint16_t gEntertainerEnd;   // 0x102a1006

namespace {

constexpr char kRowZeroText_2[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

uint8_t* Bytes(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
uint16_t& H(uint8_t* base, std::size_t off) { return *reinterpret_cast<uint16_t*>(base + off); }

uint8_t* ThingAt_2(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText_2);
    return Bytes(gThingArray[id].mThing);
}

uint32_t CellOf_3(const uint8_t* p) {
    return static_cast<uint16_t>(p[4] + (static_cast<uint32_t>(p[6]) << 7) + 1) - 1u;
}

uint32_t Distance(uint32_t a, uint32_t b) {
    int32_t dx = static_cast<int32_t>(std::labs(static_cast<int32_t>((a & 0x7fu) - (b & 0x7fu))));
    int32_t dy = static_cast<int32_t>(std::labs(static_cast<int32_t>((a >> 7) - (b >> 7))));
    return static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
}

}  // namespace

// 0x1011dcc0
void CPerson::ProcessStateBeingLoaded() {
    uint8_t* self = Bytes(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 2) {
        CTRACE(2, "Person %d: Got stuck in middle of very short queue step - this shouldn't happen\n");
        walks = 0;
    }
    if (walks != 0) return;
    uint8_t* raw = ThingAt_2(H(self, 0x1dc));
    CObject* object = reinterpret_cast<CObject*>(raw);
    if (TooPricey(object)) {
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d: Object %d is too expensive, I'm leaving the queue\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(10), reinterpret_cast<CThing*>(object)->GetThingId());
        LowerHappiness(static_cast<Amount>(1));
        object->WalkedAway();
        object->PersonLeftDuringAdmittance(me->GetThingId());
        object->RemovePersonFromQueue(me->GetThingId());
        DismissFromQueue();
        H(self, 0x1dc) = 0;
        SetState(static_cast<PersonState>(6));
        return;
    }
    if (object->AdmitPerson(me->GetThingId()) != 0) {
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d been AdmitPerson'd to ride %d, now waiting for script to admit me\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        SetState(static_cast<PersonState>(14));
        return;
    }
    if (FindQueueDestination() == 0) {
        CTRACE(2, "Couldn't rejoin FOQ even!\n");
        object->RemovePersonFromQueue(me->GetThingId());
        DismissFromQueue();
        SetState(static_cast<PersonState>(6));
    }
}

// Unit v51064: twenty-four small functions of person.cpp, personqueue.cpp, mechanichq.cpp, guard.cpp and vehicle.cpp (main app).

int32_t gPersonQueueSinitA;  // 0x1034c4a8
int32_t gPersonQueueSinitB;  // 0x1034c4ac
// 0x1011c5c4
CMsgPrankery* CMsgPrankery_Dtor(CMsgPrankery* self, int16_t deleteFlag) {
    if (self != nullptr && deleteFlag > 0) FixedSizeArrayOperatorDelete(self);
    return self;
}

// 0x1011df70
void CPerson::ForceRideAdmittance() { ProcessStateWaitingForScriptToAdmitMe(); }

// 0x1011dfc8
void CPerson::ProcessStateWaitingForScriptToAdmitMe() {
    uint8_t* self = Raw_2(this);
    uint16_t rideIndex = Fld_2<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[rideIndex].mThing);
    if (object->FinishedAdmitting() != 0) {
        object->RemovePersonFromQueue(reinterpret_cast<CThing*>(this)->GetThingId());
        GAME_ASSERTMSG(gNoAdmitState == Fld_2<uint16_t>(self, 0x228), kNotRemovedText);
        SetState(static_cast<PersonState>(0x10));
    }
}

// 0x1011e0e4
void CPerson::ProcessStateBeingUnloaded() {
    uint8_t* self = Bytes(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        if (H(self, 0x1de) != gNoMinorDest) {
            H(self, 0x1dc) = H(self, 0x1de);
            H(self, 0x1de) = 0;
            uint16_t ride = H(self, 0x1dc);
            uint8_t* object = ThingAt_2(ride);
            if (object[2] == 3) {
                uint8_t* again = ThingAt_2(H(self, 0x1dc));
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> back = reinterpret_cast<CObject*>(again)->GetBackOfQueue();
                if (back.mIndex != gNoQueueBack && reinterpret_cast<CPeep*>(this)->SetDest(back) != 0) {
                    CTRACE(2, "Left minor destination, found old major one again!");
                    SetState(static_cast<PersonState>(10));
                    return;
                }
            } else {
                CTRACE(2, "Deleted major dest while I was doing minor dest!\n");
                H(self, 0x1dc) = 0;
            }
        }
        uint16_t ride = H(self, 0x1dc);
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, "Person %d: successfully left ride %d, becoming idle\n", static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
        SetState(static_cast<PersonState>(6));
    } else if (walks == 2) {
        H(self, 0x1dc) = 0;
        SetState(static_cast<PersonState>(6));
    }
}

// CPerson: the three states of a guest leaving the park (unit v5006):
//   ProcessStateLeavingPark 0x1011e2e8, ProcessStateWaitingToCrossRoadOutOfPark 0x1011e42c, ProcessStateWaitingForBusOuttaHere 0x1011e654.

namespace {

template <class T>
T* At_3(void* base, uint32_t offset) {
    return reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

// fctiwz: truncates, saturating (the low word is what the listing keeps).
int32_t Fctiwz(float f) {
    if (f != f) return INT32_MIN;
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f < -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}

// The four cells around the bus stop: the stop, one to the east, two rows back, and that one to the east.
void BusStopCells(uint16_t* out) {
    out[0] = gWorldMap->GetBusStopPos(0).mIndex;
    out[1] = static_cast<uint16_t>(1 + (0 << 7) + gWorldMap->GetBusStopPos(0).mIndex);
    out[2] = static_cast<uint16_t>(0 - 256 + gWorldMap->GetBusStopPos(0).mIndex);
    out[3] = static_cast<uint16_t>(1 - 256 + gWorldMap->GetBusStopPos(0).mIndex);
}

uint16_t MyCell(CPerson* self) {
    uint32_t x = *At_3<uint8_t>(self, 4);
    uint32_t y = *At_3<uint8_t>(self, 6);
    return static_cast<uint16_t>(x + (y << 7) + 1);
}

void WalkTo(CPerson* self, uint16_t dest) {
    uint8_t a = static_cast<uint8_t>(gWorld->GetRand());
    uint8_t b = static_cast<uint8_t>(gWorld->GetRand());
    uint32_t m = static_cast<uint32_t>(dest) - 1;
    MapPos x;
    MapPos y;
    x.mCell = static_cast<uint8_t>(m & 0x7f);
    x.mOffset = a;
    y.mCell = static_cast<uint8_t>(m >> 7);
    y.mOffset = b;
    reinterpret_cast<CPeep*>(self)->SetDest(x, y);
    self->SetState(static_cast<CPerson::PersonState>(0x14));
}

}  // namespace

// 0x1011e2e8
void CPerson::ProcessStateLeavingPark() {
    if (*At_3<int32_t>(this, 0x1bc) > 0 && (Fctiwz(*At_3<float>(this, 0x19c)) & 0xff) != 0 && !gWorld->IsParkClosed() &&
        reinterpret_cast<CPeep*>(this)->IsInsidePark()) {
        CTRACE(2, "Make up your mind!\n");
        *At_3<int32_t>(this, 0x188) = 0;
        SetState(static_cast<PersonState>(6));
        return;
    }
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        SetState(static_cast<PersonState>(0x13));
    } else if (walks == 2) {
        CTRACE(2, "I'm stuck in the park, even though it's closed!!");
        *At_3<int32_t>(this, 0x188) = 0;
        SetState(static_cast<PersonState>(6));
    }
}

// 0x1011e42c
void CPerson::ProcessStateWaitingToCrossRoadOutOfPark() {
    uint16_t cells[4];
    BusStopCells(cells);
    if (gWorld->ArrivalVehicle_CrossingIsClear()) {
        uint16_t dest = cells[gWorld->GetRand() & 3];
        WalkTo(this, dest);
    }
}

// 0x1011e654
void CPerson::ProcessStateWaitingForBusOuttaHere() {
    int32_t status = gWorld->ArrivalVehicle_GetStatus();
    if (status == 4) {
        uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        uint16_t cell = MyCell(this);
        uint16_t who = reinterpret_cast<CMapCell*>(gWorldMap)[cell - 1].mWho;
        if (who == id) {
            reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
            reinterpret_cast<CThing*>(this)->KillMe();
        }
    } else if (static_cast<uint32_t>(status - 1) <= 2) {
        uint16_t cells[4];
        BusStopCells(cells);
        uint16_t dest;
        if (gWorld->ArrivalVehicle_IsSeagoing()) {
            if (MyCell(this) == cells[2]) return;
            if (MyCell(this) == cells[3]) return;
            dest = cells[2 + (gWorld->GetRand() & 1)];
        } else {
            if (MyCell(this) == cells[0]) return;
            if (MyCell(this) == cells[1]) return;
            dest = cells[gWorld->GetRand() & 1];
        }
        WalkTo(this, dest);
    }
}

// 0x1011e9ec: the hidden result pointer is r3, `this` r4 and the range r5 in the listing.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestReachableLitterBin(uint32_t range) {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H(Bytes(gWorld), 0x1da746);
    uint16_t best = 0;
    while (current != gLitterListEnd) {
        uint8_t* thing = ThingAt_2(current);
        if ((H(thing, 0x2e) & 0x40u) != 0) {
            uint32_t distance = Distance(CellOf_3(thing), CellOf_3(self));
            if (distance < range * range && distance < bestDistance) {
                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
                cell.mIndex = H(thing, 0x32);
                if (reinterpret_cast<CPeep*>(this)->SetDest(cell) != 0) {
                    best = current;
                    bestDistance = distance;
                }
            }
        }
        current = H(thing, 0xc);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}

// 0x1011ebc4: the hidden result pointer is r3, `this` r4.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestEntertainer() {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H(Bytes(gWorld), 0x1da740);
    uint16_t best = 0;
    while (current != gEntertainerEnd) {
        uint8_t* thing = ThingAt_2(current);
        uint32_t distance = Distance(CellOf_3(thing), CellOf_3(self));
        if (distance < bestDistance) {
            best = current;
            bestDistance = distance;
        }
        current = H(thing, 0x20c);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}

// CPerson::UpdateTurn (0x1011f7d0, 1464 bytes) and CPerson::FindNearestFireworks (0x1011ed54, 396 bytes).
// The person is reached by Mac offset (+0x4 and +0x6 the cell bytes, +0x30 the CThoughts, +0x19c / +0x1a4 / +0x1a8 / +0x1ac / +0x1b0 floats
// clamped to 0..100, +0x1bc, +0xc2 the sprite halfword, +0x1dc, +0x214, +0x220 state, +0x228 and +0x22a the queue links).

uint16_t gThingListEnd;   // 0x102a1008
uint16_t gQueueLinkA;     // 0x102a100a
uint16_t gQueueLinkB;     // 0x102a100c

namespace {

const char kQueueText[] = "Person %d is in queue for object %d (next %d, prev %d) but doesn't think he is\n";   // 0x10244aa8 + 0x899

uint16_t& H_2(uint8_t* base, int32_t off) { return *reinterpret_cast<uint16_t*>(base + off); }
float& F(uint8_t* base, int32_t off) { return *reinterpret_cast<float*>(base + off); }

// fctiwz's low word (saturating)
uint32_t Fctiwz_2(float value) {
    if (value != value) return 0x80000000u;
    if (value >= 2147483648.0f) return 0x7fffffffu;
    if (value < -2147483648.0f) return 0x80000000u;
    return static_cast<uint32_t>(static_cast<int32_t>(value));
}

// field = clamp(value, 0, 100) as the listing's compare chain does (a NaN is stored as it is)
void Store(float& field, float value) {
    if (value > 100.0f) field = 100.0f;
    else if (value < 0.0f) field = 0.0f;
    else field = value;
}

void AddCell(uint8_t* self, int32_t off, const int16_t* cell, int32_t index) {
    float cur = F(self, off);
    float value = static_cast<float>(cell[index]);
    Store(F(self, off), value + cur);
}

void CheckHundred(uint8_t* self, int32_t off) {
    if ((Fctiwz_2(F(self, off)) & 0xffu) == 100u) Store(F(self, 0x19c), -1.0f + F(self, 0x19c));
}

}  // namespace

// 0x1011ed54: the hidden result pointer is r3, `this` r4 in the listing.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CPerson::FindNearestFireworks() {
    uint8_t* self = Bytes(this);
    uint32_t bestDistance = 0xffffffffu;
    uint16_t current = H_2(Bytes(gWorld), 0x1da746);
    uint16_t best = 0;
    while (current != gThingListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZero);
        uint8_t* thing = Bytes(gThingArray[current].mThing);
        if ((H_2(thing, 0x2e) & 0x80u) != 0) {
            RSSE_SCRIPT* script = RSSE_GetScriptPointer(*reinterpret_cast<int32_t*>(thing + 0x20));
            if (RSSE_Getvar(script, 0) == 0) {
                uint32_t a = static_cast<uint16_t>(thing[4] + (static_cast<uint32_t>(thing[6]) << 7) + 1) - 1u;
                uint32_t b = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1) - 1u;
                int32_t dx = static_cast<int32_t>(std::labs(static_cast<int32_t>((a & 0x7fu) - (b & 0x7fu))));
                int32_t dy = static_cast<int32_t>(std::labs(static_cast<int32_t>((a >> 7) - (b >> 7))));
                uint32_t distance = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
                if (distance <= 4u && distance < bestDistance) {
                    best = current;
                    bestDistance = distance;
                }
            }
        }
        current = H_2(thing, 0xc);
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> result;
    result.mIndex = best;
    return result;
}

// Unit v5150: a person in a ride's queue (CPerson::DismissFromQueue, EvaluateQueueEdit, FindQueueDestination, ExitRide).
// The person and the ride are reached by Mac offset.

namespace {

const char kNoSetDest[] = "QQQ - FindQueueDestination SetDest failed, so I'm standing in queue\n";                 // 0x10245198
const char kDismissed[] = "Person %d: dismissed from queue on ride %d\n";                                          // 0x10245210
const char kEdited[] = "Person %d: Queue edited underneath me while Standing In Queue (I'm in state %d)\n";        // 0x1024523c
const char kShortened[] = "Person %d: The queue was shortened and there's no room for me any more\n";              // 0x1024528d
const char kLeaving[] = "Person %d: ExitRide, leaving ride %d";                                                    // 0x102452d5
const char kBlocked[] = "Person %d: SetDest on ride exit failed, exit of object %d is blocked!\n";                 // 0x102452fa

uint32_t ThingIdOf(CPerson* self) {
    return reinterpret_cast<CThing*>(self)->GetThingId().mIndex;
}

CObject* RideRow(uint16_t index) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return static_cast<CObject*>(gThingArray[index].mThing);
}

CMapCell* CellOfId(uint16_t id) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + (static_cast<int32_t>(id) - 1) * 0x44);
}

// The common tail of DismissFromQueue and EvaluateQueueEdit (the same code after the trace).
void LeaveQueueTail(CPerson* self) {
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(At_3<uint8_t>(self, 0x30));
    thoughts->RecordEvent(static_cast<CThoughts::EventType>(6), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{*At_3<uint16_t>(self, 0x1dc)});
    uint16_t id = ThingIdOf(self);
    if ((id & 7) == 0) reinterpret_cast<CPeep*>(self)->PlaySound(gSoundBanks[1], 0x80);
    self->LowerHappiness(static_cast<CPerson::Amount>(1));
    *At_3<uint16_t>(self, 0x22a) = 0;
    *At_3<uint16_t>(self, 0x228) = 0;
    *At_3<int32_t>(self, 0x1f8) = 0;
    *At_3<uint16_t>(self, 0x1dc) = 0;
    *At_3<uint8_t>(self, 0x1f1) = 0;
    self->SetState(static_cast<CPerson::PersonState>(6));
}

}  // namespace

// 0x1011ef14
int32_t CPerson::FindQueueDestination() {
    uint16_t rideIndex = *At_3<uint16_t>(this, 0x1dc);
    CObject* ride = RideRow(rideIndex);
    int32_t position = ride->GetPositionInQueue(reinterpret_cast<CThing*>(this)->GetThingId());
    if (position == -1) return 0;
    *At_3<uint8_t>(this, 0x1f1) = static_cast<uint8_t>(position);
    uint16_t rideIndex2 = *At_3<uint16_t>(this, 0x1dc);
    CObject* ride2 = RideRow(rideIndex2);
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell{0};
    uint8_t x = 0, y = 0;
    ride2->ConvertQPositionToMapPos(static_cast<uint32_t>(position), &cell, &x, &y);
    uint32_t h = cell.mIndex;
    MapPos a{static_cast<uint8_t>((h - 1) >> 7), y};
    MapPos b{static_cast<uint8_t>((h - 1) & 0x7f), x};
    if (reinterpret_cast<CPeep*>(this)->SetDest(b, a)) {
        SetState(static_cast<PersonState>(0xc));
        return 1;
    }
    CTRACE(2, kNoSetDest);
    return 0;
}

// 0x1011f0b8: in state 0xb at the front of the queue (+0x1f1 is 0).
bool CPerson::ReadyForAdmitting() const {
    return Fld_2<int32_t>(Raw(this), 0x220) == 0xb && Fld_2<uint8_t>(Raw(this), 0x1f1) == 0;
}

// CPerson::ModelState 0x1011fdb4, OnAdmittance 0x1011f114, GetDebagged 0x1011b594, MakeMajorDecision 0x10118430 (unit v5543).
// The person is reached by Mac offset: +0x1c angle word, +0x20 and +0x24 script words, +0x188 word, +0x1d8 person index, +0x1dc ride halfword,
// +0x1f8 admit flag, +0x208 turn stamp, +0x220 state, +0x228 queue link.

uint16_t gModelListEnd;    // 0x102a100e: ends the guard list ModelState walks

namespace {

const char kBigProblemText[] = "Big problem...no way this should happen\n";                          // 0x10244aeb
const char kQueueText_2[] = "Person not correctly removed from queue\n";                              // 0x10245051
const char kStuckText_2[] = "Got stuck walking around outside park - this shouldn't happen\n";         // 0x10245159
const char kAdmitText[] = "Person %d: OnAdmittance (I think I'm on object %d)";                     // 0x102451dd
}  // namespace

// 0x1011f114
void CPerson::OnAdmittance() {
    uint8_t* self = Raw(this);
    uint16_t ride = Fld<uint16_t>(self, 0x1dc);
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    CTRACE(2, kAdmitText, static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
    Fld<int32_t>(self, 0x1f8) = 1;
}

// 0x1011f1b8
void CPerson::DismissFromQueue() {
    uint16_t ride = *At_3<uint16_t>(this, 0x1dc);
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kDismissed, static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
    LeaveQueueTail(this);
}

// 0x1011f2f8
void CPerson::EvaluateQueueEdit() {
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kEdited, static_cast<uint32_t>(id), *At_3<int32_t>(this, 0x220));
    uint16_t rideIndex = *At_3<uint16_t>(this, 0x1dc);
    CObject* ride = RideRow(rideIndex);
    int32_t length = *At_3<int32_t>(ride, 0x3c);
    uint32_t position = static_cast<uint32_t>(ride->GetPositionInQueue(reinterpret_cast<CThing*>(this)->GetThingId()));
    if (position < static_cast<uint32_t>(length << 2)) return;
    if (*At_3<int32_t>(this, 0x220) == 0xe) return;
    uint16_t id2 = ThingIdOf(this);
    CTRACE(2, kShortened, static_cast<uint32_t>(id2));
    float x, y, z;
    int32_t mode;
    SCR_GetScriptPosition(*At_3<int32_t>(this, 0xc), &x, &y, &z, &mode);
    uint16_t id3 = ThingIdOf(this);
    if (id3 % 3 == 0) reinterpret_cast<CThoughts*>(At_3<uint8_t>(this, 0x30))->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xd), false);
    ride->RemovePersonFromQueue(reinterpret_cast<CThing*>(this)->GetThingId());
    uint16_t rideId = *At_3<uint16_t>(this, 0x1dc);
    uint16_t id4 = ThingIdOf(this);
    CTRACE(2, kDismissed, static_cast<uint32_t>(id4), static_cast<uint32_t>(rideId));
    LeaveQueueTail(this);
    *At_3<uint16_t>(this, 0x1dc) = 0;
    SetState(static_cast<PersonState>(6));
}

// 0x1011f5a4
int32_t CPerson::ExitRide() {
    uint16_t rideIndex = *At_3<uint16_t>(this, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CObject* ride = static_cast<CObject*>(gThingArray[rideIndex].mThing);
    uint16_t rideId = *At_3<uint16_t>(this, 0x1dc);
    uint16_t id = ThingIdOf(this);
    CTRACE(2, kLeaving, static_cast<uint32_t>(id), static_cast<uint32_t>(rideId));
    MapPos first{}, second{};
    ride->GetStandPoint(1, &first, &second);
    reinterpret_cast<CPeep*>(this)->Teleport(first, second);
    uint16_t entry = *At_3<uint16_t>(ride, 0x34);
    CMapCell* cell = CellOfId(entry);
    uint8_t direction;
    if (*At_3<uint16_t>(ride, 0x34) == *At_3<uint16_t>(ride, 0x32)) {
        direction = OPPOSITE(cell->GetDirection());
    } else {
        direction = cell->GetDirection();
    }
    CMapCell* neighbour = cell->GetNeighbouringCell(direction);
    uint16_t next = *reinterpret_cast<uint16_t*>(neighbour);
    if (!CellOfId(next)->IsQueue() &&
        reinterpret_cast<CPeep*>(this)->SetDest(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{next})) {
        UseObject(ride);
        SetState(static_cast<PersonState>(0xf));
        return 1;
    }
    uint16_t rideId2 = *At_3<uint16_t>(this, 0x1dc);
    uint16_t id2 = ThingIdOf(this);
    CTRACE(2, kBlocked, static_cast<uint32_t>(id2), static_cast<uint32_t>(rideId2));
    ride->NotifyExitBlocked();
    return 0;
}

// 0x1011f7d0
void CPerson::UpdateTurn() {
    uint8_t* self = Bytes(this);
    reinterpret_cast<CPeep*>(this)->UpdateTurn();
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    if ((Bytes(gWorld) != nullptr) && ((gWorld->mTurn & 3u) == (static_cast<uint32_t>(id) & 3u))) {
        *reinterpret_cast<int32_t*>(self + 0x1bc) -= 1;
        uint16_t cellId = static_cast<uint16_t>(self[4] + (static_cast<uint32_t>(self[6]) << 7) + 1);
        tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cellTag;
        cellTag.mIndex = cellId;
        const int16_t* cell = reinterpret_cast<const int16_t*>(gWorldMap->GetRegionalEffectCell(cellTag));
        AddCell(self, 0x19c, cell, 0);
        AddCell(self, 0x1b0, cell, 1);
        AddCell(self, 0x1a8, cell, 2);
        if ((gWorld->mTurn & 0xfu) == 0) {
            Store(F(self, 0x1ac), 1.0f + F(self, 0x1ac));
            Store(F(self, 0x1a8), 2.0f + F(self, 0x1a8));
            Store(F(self, 0x1a4), 2.0f + F(self, 0x1a4));
        }
        CheckHundred(self, 0x1b0);
        CheckHundred(self, 0x1a8);
        CheckHundred(self, 0x1a4);
        CheckHundred(self, 0x1ac);
        if ((Fctiwz_2(F(self, 0x1ac)) & 0xffu) > 0x50u) H_2(self, 0xc2) = gPersonTable[1];
        else H_2(self, 0xc2) = gPersonTable[0];
        uint32_t state = *reinterpret_cast<uint32_t*>(self + 0x220);
        if (state != 0x10 && state != 0x11) {
            if (reinterpret_cast<CPeep*>(this)->IsInsidePark() != 0) {
                uint32_t rand = gWorld->GetRand();
                if (rand % 10u == 0) DoSomeThinking();
                uint32_t balloon = *reinterpret_cast<uint32_t*>(self + 0x214);
                if (balloon != 0) {
                    balloon -= 1;
                    *reinterpret_cast<uint32_t*>(self + 0x214) = balloon;
                    if (balloon == 0) MakeBalloonPop();
                }
            }
        }
        reinterpret_cast<CThoughts*>(self + 0x30)->ProcessThoughtBubbles();
    }
    if (InQueueingState() == 0) {
        uint16_t prev = H_2(self, 0x22a);
        uint16_t next = H_2(self, 0x228);
        uint16_t object = H_2(self, 0x1dc);
        uint16_t thingId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
        int32_t ok = 0;
        if (gQueueLinkA == H_2(self, 0x228) && gQueueLinkB == H_2(self, 0x22a)) ok = 1;
        GAME_ASSERTMSG(ok, kQueueText, static_cast<uint32_t>(thingId), static_cast<uint32_t>(object), static_cast<uint32_t>(next),
                       static_cast<uint32_t>(prev));
        H_2(self, 0x228) = 0;
        H_2(self, 0x22a) = 0;
    }
    UpdateTemporaryHistory();
}

// 0x1011fdb4
void CPerson::ModelState() {
    uint8_t* self = Raw(this);
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    uint32_t state = Fld<uint32_t>(self, 0x220);
    if (state > 0x15u) return;
    switch (state) {
    case 0: {
        int32_t walks = peep->PeepWalks();
        if (walks == 0) {
            Fld<int32_t>(self, 0x1c) = 0x400;
            SetState(static_cast<PersonState>(1));
        } else if (walks == 2) {
            GAME_ASSERTMSG(0, kBigProblemText);
        }
        break;
    }
    case 1: {
        if (gWorld->ArrivalVehicle_CrossingIsClear() != 0) {
            CMap* map = gWorldMap;
            uint32_t rand = gWorld->GetRand();
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> booth = map->GetTicketBoothPos(static_cast<int32_t>(rand & 1u));
            uint8_t offX = static_cast<uint8_t>(gWorld->GetRand() & 0xffu);
            uint8_t offY = static_cast<uint8_t>(gWorld->GetRand() & 0xffu);
            MapPos x, y;
            CellPair(booth.mIndex, offX, offY, x, y);
            peep->SetDest(x, y);
            SetState(static_cast<PersonState>(2));
        }
        break;
    }
    case 2:
        ProcessStateWalkingToTicketBooths();
        break;
    case 4:
        ProcessStatePayingAtBooth();
        break;
    case 5: {
        if (gWorld->IsParkClosed() != 0 || gWorld->ParkGates_GetStatus() != 1) {
            CMap* map = gWorldMap;
            uint32_t rand = gWorld->GetRand();
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> booth = map->GetTicketBoothPos(static_cast<int32_t>(rand & 1u));
            peep->SetDest(booth);
            SetState(static_cast<PersonState>(2));
        } else {
            int32_t walks = peep->PeepWalks();
            if (walks == 0) {
                Fld<int32_t>(self, 0x1d8) = gWorld->GetNewPersonIndex();
                SetState(static_cast<PersonState>(6));
            } else if (walks == 2) {
                GAME_ASSERTMSG(0, kBigProblemText);
            }
        }
        break;
    }
    case 3:
        ProcessStateWaitingForParkToOpen();
        break;
    case 9:
        ProcessStateWalksToLitterBin();
        break;
    case 6:
        ProcessStateIdle();
        break;
    case 7: {
        int32_t walks = peep->PeepWalks();
        if (walks == 0 || walks == 2) {
            if ((gWorld->GetRand() & 3u) == 0 && peep->SetRandomDestination() != 0) {
                SetState(static_cast<PersonState>(7));
            } else {
                SetState(static_cast<PersonState>(6));
            }
        }
        break;
    }
    case 10:
        ProcessStateWalksToMajor();
        break;
    case 11:
        ProcessStateStandingInQueue();
        break;
    case 12: {
        int32_t walks = peep->PeepWalks();
        if (walks == 0) SetState(static_cast<PersonState>(0xb));
        else if (walks == 2) SetState(static_cast<PersonState>(0xb));
        break;
    }
    case 13:
        ProcessStateBeingLoaded();
        break;
    case 14: {
        uint16_t ride = Fld<uint16_t>(self, 0x1dc);
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
        CObject* object = static_cast<CObject*>(gThingArray[ride].mThing);
        if (object->FinishedAdmitting() != 0) {
            tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> me = reinterpret_cast<CThing*>(this)->GetThingId();
            object->RemovePersonFromQueue(me);
            GAME_ASSERTMSG(gNoAdmitState == Fld<uint16_t>(self, 0x228), kQueueText_2);
            SetState(static_cast<PersonState>(0x10));
        }
        break;
    }
    case 15:
        ProcessStateBeingUnloaded();
        break;
    case 18:
        ProcessStateLeavingPark();
        break;
    case 19:
        ProcessStateWaitingToCrossRoadOutOfPark();
        break;
    case 20: {
        int32_t walks = peep->PeepWalks();
        if (walks == 0) {
            if (gWorld->ArrivalVehicle_IsSeagoing() != 0) Fld<int32_t>(self, 0x1c) = 0x7ff;
            else Fld<int32_t>(self, 0x1c) = 0x400;
            SetState(static_cast<PersonState>(0x15));
        } else if (walks == 2) {
            CTRACE(2, kStuckText_2);
        }
        break;
    }
    case 21:
        ProcessStateWaitingForBusOuttaHere();
        break;
    case 16: {
        uint16_t ride = Fld<uint16_t>(self, 0x1dc);
        (void)ride;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
        break;
    }
    case 17: {
        bool found = false;
        uint16_t id = gWorld->mF1DA744;
        while (id != gModelListEnd) {
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
            CObject* object = static_cast<CObject*>(gThingArray[id].mThing);
            uint8_t* o = Raw(object);
            uint16_t me = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
            if (Fld<uint16_t>(o, 0x20e) == me && reinterpret_cast<CGuard*>(object)->IsCarrying()) {
                found = true;
                break;
            }
            id = Fld<uint16_t>(o, 0x20c);
        }
        if (!found) reinterpret_cast<CThing*>(this)->KillMe();
        break;
    }
    case 8: {
        if (gWorld->mTurn > Fld<uint32_t>(self, 0x208) + 10u) EndSpotAnim();
        break;
    }
    }
}

// CPerson::SetState (0x10120354, 1944 bytes): stores the state word (+0x220) and switches on it (jump table at 0x102a1068, states 0 to 0x15
// in the order of the listing's blocks; any other state does nothing). Each state picks the sprite script and sets up its fields.

// 0x10244aa8 is the ThingArray row-zero text; the balloon text is at +0x8e9 of the same pool.
int32_t ESP_GetRandomActionFrom(ESP_Sprites set, int32_t sprites);   // 0x1007c9f8 (104 bytes, untranslated)

// 0x1029fd48 (name invented; initial word 0x10266a60): the block the balloon's script is spawned with.
uint32_t gPersonScriptBlock = 0;

namespace {

const char kBalloonText[] = "Trying to give a balloon to a person who already has one!\n";           // 0x10245391

uint8_t* Raw_3(void* p) { return reinterpret_cast<uint8_t*>(p); }
uint16_t& Half(uint8_t* s, size_t o) { return *reinterpret_cast<uint16_t*>(s + o); }
uint32_t& Word(uint8_t* s, size_t o) { return *reinterpret_cast<uint32_t*>(s + o); }

CDelayedScript* Script(uint8_t* s) { return reinterpret_cast<CDelayedScript*>(s + 0xc); }
CThoughts* Thoughts(uint8_t* s) { return reinterpret_cast<CThoughts*>(s + 0x30); }
void Sprite(uint8_t* s, int32_t idx) { Script(s)->SetSpriteScript(static_cast<CDelayedScript::ScriptIdx>(idx)); }

uint8_t* RideOf(uint8_t* s) {
    uint16_t id = Half(s, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kRowZeroText);
    return Raw_3(gThingArray[id].mThing);
}

// The ride record (its words are read by name, the Mac's +0x100 and +0x15c: the host's record is laid out wider).
const RideInfo* InfoOf(uint8_t* ride) { return reinterpret_cast<CObject*>(ride)->GetDBInfo(); }

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
    uint8_t* s = Raw_3(this);
    Word(s, 0x220) = static_cast<uint32_t>(state);
    uint8_t* world = Raw_3(gWorld);
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
        if (InfoOf(ride)->Value(kRideSlot_RequiresTeleport).mInteger != 0) {
            reinterpret_cast<CPeep*>(this)->UpdatePositionInEngine(0.0f);
            reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
        }
        if (Word(s, 0x214) != 0) {
            ride = RideOf(s);
            if (InfoOf(ride)->Value(kRideSlot_AppearanceEffect).mInteger != 1) {
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
        if (InfoOf(ride)->Value(kRideSlot_RequiresTeleport).mInteger != 0) reinterpret_cast<CPeep*>(this)->SpawnParticleEffect(0x13);
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

// 0x10120b2c: state 8 looks at the saved state (+0x224); states 0xb to 0xe count.
int32_t CPerson::InQueueingState() const {
    int32_t state = Fld_2<int32_t>(Raw(this), 0x220);
    if (state == 8) state = Fld_2<int32_t>(Raw(this), 0x224);
    return (state >= 0xb && state < 0xf) ? 1 : 0;
}
void __sinit_PersonQueue_cpp() {
    gPersonQueueSinitA = 0x10000;
    gPersonQueueSinitB = 0;
}
