#include <cstdint>
#include "ThingReceivers.h"
#include "RideDB.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "MapCell.h"
#include "rsse.h"
#include "TPCoaster.h"
#include "ppc_math.h"

// Unit v5529: three of the person's states, from the walk to a litter bin to the wait for the ride's script.
// The person is reached by Mac offset (the host layout of the person is the Mac's).

uint16_t gNoLitterRide = 0;   // 0x102a0ffc
uint16_t gNoAdmitState = 0;   // 0x102a0ffe

namespace {

uint8_t* Raw(const void* p) { return const_cast<uint8_t*>(static_cast<const uint8_t*>(p)); }
template <class T>
T& Fld(uint8_t* base, uint32_t offset) { return *reinterpret_cast<T*>(base + offset); }

const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
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
    CMapCell* target = reinterpret_cast<CMapCell*>(Raw(gWorldMap) + static_cast<int32_t>(cell - 1) * 0x44);
    target->PlaceLitter(static_cast<CMapCell::LitterType>(rnd - quotient * kinds + 1));
}

}  // namespace

// 0x1011d400
void CPerson::ProcessStateWalksToLitterBin() {
    uint8_t* self = Raw(this);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks == 0) {
        if (Fld<uint16_t>(self, 0x1dc) == gNoLitterRide) {
            DropLitter(self);
        } else {
            uint16_t ride = Fld<uint16_t>(self, 0x1dc);
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
            uint8_t* object = Raw(gThingArray[ride].mThing);
            RSSE_SCRIPT* script = RSSE_GetScriptPointer(Fld<int32_t>(object, 0x20));
            RSSE_Setvar(script, 0, 1);
        }
        Fld<float>(self, 0x1b4) = 0.0f;
        SetState(static_cast<PersonState>(6));
    } else if (walks == 2) {
        DropLitter(self);
        Fld<float>(self, 0x1b4) = 0.0f;
        SetState(static_cast<PersonState>(6));
    }
}

// 0x1011d5ec
void CPerson::ProcessStateStandingInQueue() {
    uint8_t* self = Raw(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    uint16_t rideIndex = Fld<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[rideIndex].mThing);
    uint8_t* obj = Raw(object);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);

    if (Fld<uint8_t>(self, 0x1f1) == 0 && Fld<uint32_t>(self, 0x1f8) != 0) {
        if (object->ReadyToAdmit(me->GetThingId())) {
            Fld<uint32_t>(self, 0x1f8) = 0;
            MapPos first, second;
            object->GetStandPoint(0, &first, &second);
            if (reinterpret_cast<CPeep*>(this)->SetDest(first, second) != 0) {
                uint16_t ride = Fld<uint16_t>(self, 0x1dc);
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
    GAME_ASSERTMSG(Fld<uint32_t>(self, 0x1f8) == 0, kNotFirstText);
    if (Fld<uint8_t>(self, 0x1f1) == 0 && Fld<uint32_t>(self, 0x1f8) != 0) {
        if (!object->ReadyToAdmit(me->GetThingId())) {
            uint16_t ride = Fld<uint16_t>(self, 0x1dc);
            GAME_ASSERTMSG(0, kNotReadyText, Fld<uint32_t>(self, 0x1f8), static_cast<uint32_t>(ride));
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
    uint32_t mine = Fld<uint8_t>(self, 0x1f1);
    if (mine == place) {
        uint32_t ideal = object->GetIdealQueueLength();
        if (Fld<uint8_t>(self, 0x1f1) > ideal) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x10), false);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x15), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{Fld<uint16_t>(self, 0x1dc)});
            LeaveQueue();
            return;
        }
        bool leave = false;
        if (object->IsCoaster() != 0 && IsCoasterUseable(object) == 0) {
            leave = true;
        } else {
            RideInfo* info = object->GetDBInfo();
            // The WhichTrackType slot (the Mac's +0x9c) by name: the host's record is laid out wider.
            if (info->Value(kRideSlot_WhichTrackType).mInteger == 1 && Fld<int32_t>(obj, 0x28) == 0) leave = true;
        }
        if (leave) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xd), false);
            LeaveQueue();
            return;
        }
    } else if (Fld<uint32_t>(self, 0x1f4) == 0 || mine - place > 2u) {
        if (!object->IsBrokenDown() && FindQueueDestination() == 0) {
            CTRACE(2, kNoPositionText);
            LeaveQueue();
            return;
        }
    } else {
        Fld<uint32_t>(self, 0x1f4) = Fld<uint32_t>(self, 0x1f4) - 1;
    }

    CWorld* world = gWorld;
    uint32_t turn = world->mTurn;
    if (turn - Fld<uint32_t>(self, 0x208) > 0x1eu) {
        uint32_t happy = static_cast<uint32_t>(ppc_fctiwz(Fld<float>(self, 0x19c))) & 0xffu;
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
            uint32_t other = static_cast<uint32_t>(ppc_fctiwz(Fld<float>(self, 0x1ac))) & 0xffu;
            if (other > 0x50u) {
                thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(4), false);
                if ((Fld<uint16_t>(obj, 0x2e) & 1u) == 0) LeaveQueue();
            }
        }
    } else if (turn > Fld<uint32_t>(self, 0x1fc) + 100u) {
        uint16_t id = me->GetThingId().mIndex;
        CTRACE(2, kBoredText, static_cast<uint32_t>(id));
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(7), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{Fld<uint16_t>(self, 0x1dc)});
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xc), false);
        LeaveQueue();
    } else {
        uint32_t rnd = world->GetRand();
        if (rnd % 10u == 0) {
            int32_t angle = Fld<int32_t>(self, 0x1c);
            uint32_t rnd2 = gWorld->GetRand();
            int32_t sum = angle + static_cast<int32_t>(rnd2 % 800u - 400u);
            if (sum < 0) sum += 0x800;
            else if (sum >= 0x800) sum -= 0x800;
            Fld<int32_t>(self, 0x1c) = sum;
        }
    }
}

// 0x1011dfc8
void CPerson::ProcessStateWaitingForScriptToAdmitMe() {
    uint8_t* self = Raw(this);
    uint16_t rideIndex = Fld<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[rideIndex].mThing);
    if (object->FinishedAdmitting() != 0) {
        object->RemovePersonFromQueue(reinterpret_cast<CThing*>(this)->GetThingId());
        GAME_ASSERTMSG(gNoAdmitState == Fld<uint16_t>(self, 0x228), kNotRemovedText);
        SetState(static_cast<PersonState>(0x10));
    }
}
