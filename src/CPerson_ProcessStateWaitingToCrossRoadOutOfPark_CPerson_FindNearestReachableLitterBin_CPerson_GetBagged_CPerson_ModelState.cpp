// CPerson::ModelState 0x1011fdb4, OnAdmittance 0x1011f114, GetDebagged 0x1011b594, MakeMajorDecision 0x10118430 (unit v5543).
// The person is reached by Mac offset: +0x1c angle word, +0x20 and +0x24 script words, +0x188 word, +0x1d8 person index, +0x1dc ride halfword,
// +0x1f8 admit flag, +0x208 turn stamp, +0x220 state, +0x228 queue link.
#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "TPSound.h"

// The "none" ids these functions compare halfwords with (names invented; zero at start like the Mac's bss).
uint16_t gMajorListEnd;    // 0x102a0f18: ends the object list MakeMajorDecision walks
uint16_t gMajorNoQueue;    // 0x102a0f1a: the "nobody at the back of the queue" id
uint16_t gMajorNoRide;     // 0x102a0f1c: MakeMajorDecision's "no ride chosen"
uint16_t gModelListEnd;    // 0x102a100e: ends the guard list ModelState walks

namespace {

const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x102442be and 0x10244aa8
const char kBigProblemText[] = "Big problem...no way this should happen\n";                          // 0x10244aeb
const char kQueueText[] = "Person not correctly removed from queue\n";                              // 0x10245051
const char kStuckText[] = "Got stuck walking around outside park - this shouldn't happen\n";         // 0x10245159
const char kAdmitText[] = "Person %d: OnAdmittance (I think I'm on object %d)";                     // 0x102451dd
const char kHumphText[] = "<Humph>\n";                                                              // 0x10244981

inline uint8_t* Raw(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
template <class T>
inline T& Fld(uint8_t* base, uint32_t off) { return *reinterpret_cast<T*>(base + off); }

// A one word MapPos pair from a map cell id: x = low 7 bits of id - 1 and an offset, y = the high bits and an offset.
inline void CellPair(uint16_t id, uint8_t offX, uint8_t offY, MapPos& x, MapPos& y) {
    uint32_t v = static_cast<uint32_t>(id) - 1u;
    x.mCell = static_cast<uint8_t>(v & 0x7f);
    x.mOffset = offX;
    y.mCell = static_cast<uint8_t>((v >> 7) & 0xff);
    y.mOffset = offY;
}

}  // namespace

// 0x1011f114
void CPerson::OnAdmittance() {
    uint8_t* self = Raw(this);
    uint16_t ride = Fld<uint16_t>(self, 0x1dc);
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    CTRACE(2, kAdmitText, static_cast<uint32_t>(id), static_cast<uint32_t>(ride));
    Fld<int32_t>(self, 0x1f8) = 1;
}

// 0x1011b594
void CPerson::GetDebagged(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint8_t* self = Raw(this);
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    MapPos x, y;
    CellPair(cell.mIndex, 0x80, 0x80, x, y);
    peep->Teleport(x, y);
    reinterpret_cast<CDelayedScript*>(self + 0xc)->SpawnSpriteScript(static_cast<CDelayedScript::ScriptIdx>(1), Fld<int32_t>(self, 0x24),
                                                                    Fld<int32_t>(self, 0x20));
    peep->PlaySound(gSoundBanks[1], 0x80);
    Fld<int32_t>(self, 0x188) = 1;
    CMap* map = gWorldMap;
    uint32_t rand = gWorld->GetRand();
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> dest = map->GetCrossingParkSidePos(static_cast<int32_t>(rand & 1u));
    if (peep->SetDest(dest) != 0) {
        CTRACE(2, kHumphText);
        SetState(static_cast<PersonState>(0x12));
    } else {
        reinterpret_cast<CThing*>(this)->KillMe();
    }
}

// 0x10118430
void CPerson::MakeMajorDecision() {
    uint8_t* self = Raw(this);
    CPeep* peep = reinterpret_cast<CPeep*>(this);
    int32_t best = 0;
    Fld<uint16_t>(self, 0x1dc) = 0;
    uint16_t id = gWorld->mF1DA746;
    while (id != gMajorListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
        CObject* object = static_cast<CObject*>(gThingArray[id].mThing);
        uint8_t* o = Raw(object);
        if ((Fld<uint16_t>(o, 0x2e) & 4u) != 0) {
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> back = object->GetBackOfQueue();
            if (back.mIndex != gMajorNoQueue) {
                uint32_t scoreU = CalculateOptionScore(object);
                if (scoreU >= 10u) {
                    int32_t score = static_cast<int32_t>(scoreU);
                    if (score > best || (score == best && (gWorld->mTurn & 1u) != 0)) {
                        if (object->IsAvailableToChoose() != 0) {
                            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> where = object->GetBackOfQueue();
                            if (peep->SetDest(where) != 0) {
                                Fld<uint16_t>(self, 0x1dc) = id;
                                best = score;
                            }
                        }
                    }
                }
            }
        }
        id = Fld<uint16_t>(o, 0xc);
    }
    if (Fld<uint16_t>(self, 0x1dc) != gMajorNoRide) GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
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
            GAME_ASSERTMSG(gNoAdmitState == Fld<uint16_t>(self, 0x228), kQueueText);
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
            CTRACE(2, kStuckText);
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
