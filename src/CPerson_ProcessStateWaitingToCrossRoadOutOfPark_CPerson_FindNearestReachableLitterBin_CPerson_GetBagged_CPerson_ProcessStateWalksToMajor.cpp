#include <cstdint>
#include "ThingReceivers.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "ThingParts2.h"
#include "TPSound.h"
#include "ppc_math.h"

// Globals the original has in its initialised data (names invented, zero at start).
uint16_t gNoMajorRide = 0;   // 0x102a0ff8
uint16_t gNoMajorCell = 0;   // 0x102a0ffa

namespace {

template <class T> T& Fld(uint8_t* base, uint32_t offset) { return *reinterpret_cast<T*>(base + offset); }

const char kRowZeroMajor[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
const char kQueueTooLongText[] = "Person %d: ride %d's queue is too long!\n";                       // 0x10244c21
const char kNoPlaceText[] = "Person %d: Couldn't get to my place in the queue, leaving!\n";         // 0x10244c4a
const char kNotExcitingText[] = "Person: ride is not exciting enough!\n";                          // 0x10244c86
const char kTooExcitingText[] = "Person: ride is too exciting!\n";                                 // 0x10244cac
const char kQueueMovedText[] = "The back of the queue has moved while I was walking here\n";       // 0x10244ccb
const char kStuckText[] = "The person has become stuck on their way to the ride they were interested in\n";   // 0x10244d05
const char kParkClosedText[] = "The park has closed underneath me!\n";                             // 0x10244d53

// fctiwz, then the low byte (clrlwi 24)
uint32_t Low8(float value) { return static_cast<uint32_t>(ppc_fctiwz(value)) & 0xffu; }

}  // namespace

// 0x1011cf18
void CPerson::ProcessStateWalksToMajor() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CThing* me = reinterpret_cast<CThing*>(this);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingTag;
    typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapTag;

    uint16_t ride = Fld<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroMajor);
    CObject* object = reinterpret_cast<CObject*>(gThingArray[ride].mThing);
    int32_t walks = reinterpret_cast<CPeep*>(this)->PeepWalks();
    if (walks != 0) {
        if (walks == 2) {
            CTRACE(2, kStuckText);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(3), ThingTag{Fld<uint16_t>(self, 0x1dc)});
            LowerHappiness(static_cast<Amount>(2));
            Fld<uint16_t>(self, 0x1dc) = 0;
            SetState(static_cast<PersonState>(6));
            return;
        }
        if (gWorld->IsParkClosed() != 0) {
            CTRACE(2, kParkClosedText);
            LowerHappiness(static_cast<Amount>(2));
            Fld<uint16_t>(self, 0x1dc) = 0;
            SetState(static_cast<PersonState>(6));
            return;
        }
        Fld<uint8_t>(self, 0x2c) = static_cast<uint8_t>(Fld<uint8_t>(self, 0x2c) + 1);
        if (Fld<uint8_t>(self, 0x2c) <= 0xb) return;
        Fld<uint8_t>(self, 0x2c) = 0;
        LookForMinorDest();
        return;
    }
    if (Fld<uint16_t>(self, 0x1dc) != gNoMajorRide) {
        uint16_t place = static_cast<uint16_t>(Fld<uint8_t>(self, 4) + (static_cast<uint32_t>(Fld<uint8_t>(self, 6)) << 7) + 1);
        MapTag back = object->GetBackOfQueue();
        if (back.mIndex == place) {
            if (object->HasFreeQueueSpace() != 0) {
                int32_t score = ScoreOnCloserInspection(object);
                uint8_t excitement = object->GetExcitementLevel(true);
                int32_t magnitude = score < 0 ? -score : score;
                if (excitement != 0 && magnitude >= 0x2d) {
                    if (score < 0) {
                        CTRACE(2, kNotExcitingText);
                        thoughts->RecordEvent(static_cast<CThoughts::EventType>(5), ThingTag{Fld<uint16_t>(self, 0x1dc)});
                        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xc), false);
                    } else {
                        CTRACE(2, kTooExcitingText);
                        thoughts->RecordEvent(static_cast<CThoughts::EventType>(4), ThingTag{Fld<uint16_t>(self, 0x1dc)});
                        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0xf), false);
                    }
                    AddRideToRecentTemporaryHistory(ThingTag{Fld<uint16_t>(self, 0x1dc)});
                    Fld<uint16_t>(self, 0x1dc) = 0;
                    Fld<uint32_t>(self, 0x1fc) = 0;
                    SetState(static_cast<PersonState>(6));
                    return;
                }
                if (object->QueueTooLong()) {
                    uint16_t objectId = reinterpret_cast<CThing*>(object)->GetThingId().mIndex;
                    uint16_t personId = me->GetThingId().mIndex;
                    CTRACE(2, kQueueTooLongText, static_cast<uint32_t>(personId), static_cast<uint32_t>(objectId));
                    thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x15), ThingTag{Fld<uint16_t>(self, 0x1dc)});
                    AddRideToRecentTemporaryHistory(ThingTag{Fld<uint16_t>(self, 0x1dc)});
                    thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x10), false);
                    Fld<uint16_t>(self, 0x1dc) = 0;
                    SetState(static_cast<PersonState>(6));
                    return;
                }
                Fld<uint32_t>(self, 0x20c) = Fld<uint32_t>(self, 0x19c);
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

// 0x10117fd4
void CPerson::DoSomeThinking() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    uint32_t thought;
    if (Low8(Fld<float>(self, 0x1a8)) > 0x5a && Low8(Fld<float>(self, 0x1a4)) > 0x5a) {
        thought = 3;
    } else if (static_cast<float>(Low8(Fld<float>(self, 0x1a8))) > 99.0f) {
        thought = 1;
    } else if (static_cast<float>(Low8(Fld<float>(self, 0x1a4))) > 99.0f) {
        thought = 2;
    } else if (static_cast<float>(Low8(Fld<float>(self, 0x1ac))) > 99.0f) {
        thought = 4;
    } else if (static_cast<float>(Low8(Fld<float>(self, 0x1b0))) > 99.0f) {
        thought = 0xe;
    } else {
        uint16_t cellId = static_cast<uint16_t>(Fld<uint8_t>(self, 4) + (static_cast<uint32_t>(Fld<uint8_t>(self, 6)) << 7) + 1);
        CParkAnalyser* analyser = gWorld->GetParkAnalyser();
        CParkAnalyserCell* cell = analyser->GetAnalyserCell(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cellId});
        float walkable = static_cast<float>(cell->GetNumWalkableCells(4));
        float littered = static_cast<float>(cell->GetNumLitteredCells(4));
        float ratio = littered / walkable;
        if (ratio > 0.2f) {
            thought = 7;
        } else {
            uint32_t happy = Low8(Fld<float>(self, 0x19c));
            if (static_cast<float>(happy) > 99.0f) {
                thought = 9;
            } else if (static_cast<float>(happy) < 10.0f) {
                thought = 0xb;
            } else {
                return;
            }
        }
    }
    uint16_t id = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    if ((id & 0xf) == 0) {
        CPeep* peep = reinterpret_cast<CPeep*>(this);
        if (thought == 0xd) peep->PlaySound(gSoundBanks[1], 0x7f);
        else if (thought < 0xd) {
            if (thought == 4) peep->PlaySound(gSoundBanks[1], 0x85);
            else if (thought >= 0xc) peep->PlaySound(gSoundBanks[1], 0x7e);
        } else if (thought == 0xf) peep->PlaySound(gSoundBanks[1], 0x85);
    }
    reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(thought), false);
}
