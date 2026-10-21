// CPerson::EveryTimeMods 0x1011a364, UpdateTemporaryHistory 0x10119b68, UseObject 0x101196a0, GetTimeInPark 0x1011962c, LookForMinorDest 0x1011915c
// (unit v51016). The person and the objects are reached by Mac offset.
#include <cstdint>
#include "ThingReceivers.h"
#include "RideDB.h"
#include "Debug.h"
#include "analyser.h"
#include "MainBalance.h"
#include "World.h"
#include "WorldSavvy.h"
#include "TPEngine.h"
#include "TPCamera.h"
#include "ppc_math.h"

// Names invented, zero at start (the Mac's bss).
uint16_t gMinorNoRide;    // 0x102a0fb0
uint16_t gMinorNoQueue;   // 0x102a0fb2
uint16_t gMinorListEnd;   // 0x102a0fb4
uint16_t gMinorNoDest;    // 0x102a0fb6
uint16_t gMinorNoDest2;   // 0x102a0fb8

namespace {

using MapIdTag = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

// The texts of the original sit in one pool at 0x102442be; the offsets are in the comments.
const char kRowZeroText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x0
const char kMinorText[] = "Minor Decision: OID=%d, @=(%d, %d), sc=%d\n";                              // +0x4f4
const char kHappyText[] = "Person %d: Happiness changed by %d since using object %d\n";               // +0x51f

inline uint8_t* Raw(const void* p) { return reinterpret_cast<uint8_t*>(const_cast<void*>(p)); }
template <class T>
inline T& Fld(uint8_t* base, uint32_t offset) { return *reinterpret_cast<T*>(base + offset); }

// The rider counts and the opinion buckets switch on the ride record's +0x7a8 (the UI type).
// The ride record's words are read by name (the Mac's +0x7a8, +0xe8, +0x158, +0x15c): the host's record is laid out wider.
inline int32_t UIType(CObject* object) { return object->GetDBInfo()->mUIType; }

}  // namespace

// 0x1011a364
void CPerson::EveryTimeMods(CObject* object) {
    uint8_t* self = Raw(this);
    int32_t kind = UIType(object);
    if (kind == 2) {
        Fld<int32_t>(self, 0x1cc) = Fld<int32_t>(self, 0x1cc) + 1;
    } else if (kind < 2) {
        if (kind == 0) {
            Fld<int32_t>(self, 0x1c4) = Fld<int32_t>(self, 0x1c4) + 1;
        } else if (kind > 0) {
            Fld<int32_t>(self, 0x1c8) = Fld<int32_t>(self, 0x1c8) + 1;
        }
    }
    PayPricePerUseForObject(object);
    object->HadCustomer();
    float cost = static_cast<float>(object->GetDBInfo()->Value(kRideSlot_FatigueEffect).mInteger);
    float happy = Fld<float>(self, 0x1b8);
    float after = -cost + happy;
    if (after > 100.0f) {
        Fld<float>(self, 0x1b8) = 100.0f;
    } else if (!(after < 0.0f)) {   // a NaN lands here, as the listing's bge does
        Fld<float>(self, 0x1b8) = happy - cost;
    } else {
        Fld<float>(self, 0x1b8) = 0.0f;
    }
}

// 0x10119b68
void CPerson::UpdateTemporaryHistory() {
    uint8_t* self = Raw(this);
    uint32_t turn = gWorld->mTurn;
    if (turn != (turn / 20u) * 20u) return;
    for (int32_t i = 3; i > 0; i--) Fld<uint16_t>(self, 0x1e8 + i * 2) = Fld<uint16_t>(self, 0x1e6 + i * 2);
    Fld<uint16_t>(self, 0x1e8) = 0;
}

// 0x1011962c: the game turns the person has been in the park, in units of four.
uint32_t CPerson::GetTimeInPark() const {
    const CWorld& world = reinterpret_cast<const CWorldSavvy*>(this)->GetWorld();
    uint32_t turn = world.mTurn;
    return (turn - Fld<uint32_t>(Raw(this), 0x1d4)) >> 2;
}

// 0x101196a0
void CPerson::UseObject(CObject* object) {
    uint8_t* self = Raw(this);
    CThing* thingObject = reinterpret_cast<CThing*>(object);
    CThoughts* thoughts = reinterpret_cast<CThoughts*>(self + 0x30);
    AddRideToRecentHistory(thingObject->GetThingId());
    EveryTimeMods(object);
    if (Fld<uint8_t>(self, 0x1f1) == 0) {
        LosingMods(object);
        if (gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Fld<uint16_t>(Raw(object), 0xe)})->mUIType == 2) {
            thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(6), false);
            thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x19), thingObject->GetThingId());
        }
        return;
    }
    WinningMods(object);
    int32_t afterValue = ppc_fctiwz(static_cast<double>(Fld<float>(self, 0x20c)));
    int32_t beforeValue = ppc_fctiwz(static_cast<double>(Fld<float>(self, 0x19c)));
    int32_t change = (static_cast<int32_t>(static_cast<uint32_t>(beforeValue) & 0xffu) - static_cast<int32_t>(static_cast<uint32_t>(afterValue) & 0xffu)) * 3;
    uint16_t objectId = thingObject->GetThingId().mIndex;
    uint16_t personId = reinterpret_cast<CThing*>(this)->GetThingId().mIndex;
    CTRACE(2, kHappyText, static_cast<uint32_t>(personId), change, static_cast<uint32_t>(objectId));
    object->ExpressSatisfaction(change);
    uint8_t value = static_cast<uint8_t>(change + 0x32);
    int32_t kind = UIType(object);
    if (kind == 2) {
        gWorld->GetParkAnalyser()->OfferOpinionOn(0x12, value);
    } else if (kind < 2 && kind > 0) {
        int32_t a = object->GetDBInfo()->Value(kRideSlot_SpecialIngredient).mInteger;
        if (a == 2) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xb, value);
        } else if (a < 2) {
            if (a != 0 && a >= 0) gWorld->GetParkAnalyser()->OfferOpinionOn(0xa, value);
        } else if (a == 4) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xd, value);
        } else if (a < 4) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xc, value);
        }
        int32_t b = object->GetDBInfo()->Value(kRideSlot_AppearanceEffect).mInteger;
        if (b == 1) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0xf, value);
        } else if (b > 0 && b < 3) {
            gWorld->GetParkAnalyser()->OfferOpinionOn(0x10, value);
        }
    }
    object->HadAWinner();
    if (gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{Fld<uint16_t>(Raw(object), 0xe)})->mUIType == 2) {
        thoughts->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(5), false);
        thoughts->RecordEvent(static_cast<CThoughts::EventType>(0x18), thingObject->GetThingId());
    }
}

// 0x1011915c: looks around the cells beside the person for a ride closer to the one he is queueing for.
void CPerson::LookForMinorDest() {
    uint8_t* self = Raw(this);
    if (Fld<uint16_t>(self, 0x1dc) == gMinorNoRide) return;
    uint16_t bestId = 0;
    int32_t bestScore = 0;
    uint16_t here = static_cast<uint16_t>(Fld<uint8_t>(self, 4) + (Fld<uint8_t>(self, 6) * 128 + 1));
    int32_t x0 = static_cast<int32_t>((static_cast<uint32_t>(here) - 1u) & 0x7fu);
    int32_t y0 = static_cast<int32_t>((static_cast<uint32_t>(here) - 1u) >> 7);
    uint16_t current = Fld<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag back = static_cast<CObject*>(gThingArray[current].mThing)->GetBackOfQueue();
    if (back.mIndex == gMinorNoQueue) return;
    int32_t u = static_cast<int32_t>((static_cast<uint32_t>(back.mIndex) - 1u) & 0x7fu);
    int32_t v = static_cast<int32_t>((static_cast<uint32_t>(back.mIndex) - 1u) >> 7);
    int32_t limit = (x0 - u) * (x0 - u) + (y0 - v) * (y0 - v);
    uint8_t* map = Raw(gWorldMap);
    for (int32_t dx = -2; dx < 2; dx++) {
        for (int32_t dy = -2; dy < 2; dy++) {
            int32_t cx = x0 + dx;
            int32_t cy = y0 + dy;
            if (cx < 0 || cx >= 0x80 || cy < 0 || cy >= 0x80) continue;
            if ((cx - u) * (cx - u) + (cy - v) * (cy - v) >= limit) continue;
            uint16_t cell = static_cast<uint16_t>(cx + (cy * 128 + 1));
            uint16_t who = Fld<uint16_t>(map + static_cast<int32_t>(cell - 1) * 0x44, 0x24);
            while (who != gMinorListEnd) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
                CObject* object = static_cast<CObject*>(gThingArray[who].mThing);
                uint8_t* o = Raw(object);
                if (Fld<uint8_t>(o, 2) == 3 && who != Fld<uint16_t>(self, 0x1dc) && object->IsAvailableToChoose() != 0) {
                    int32_t score = static_cast<int32_t>(CalculateOptionScore(object));
                    if (score > bestScore || (score == bestScore && (gWorld->mTurn & 1u) != 0)) {
                        bestId = who;
                        bestScore = score;
                    }
                }
                who = Fld<uint16_t>(o, 0xa);
            }
        }
    }
    if (bestId == gMinorNoDest) return;
    uint16_t ride = Fld<uint16_t>(self, 0x1dc);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag rideCell{Fld<uint16_t>(Raw(gThingArray[ride].mThing), 0x32)};
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    MapIdTag destCell{Fld<uint16_t>(Raw(gThingArray[bestId].mThing), 0x32)};
    MapIdTag myCell{static_cast<uint16_t>(Fld<uint8_t>(self, 4) + (Fld<uint8_t>(self, 6) * 128 + 1))};
    NavMode mode = static_cast<NavMode>(Fld<int32_t>(self, 0x188));
    int32_t d1 = MAP_count_walking_distance(rideCell, myCell, mode);
    int32_t d2 = MAP_count_walking_distance(rideCell, destCell, mode);
    if (d1 == -1 || d2 == -1 || !(d1 > d2) || bestId == gMinorNoDest2) return;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZeroText);
    uint8_t* target = Raw(gThingArray[bestId].mThing);
    MapIdTag targetCell{Fld<uint16_t>(target, 0x32)};
    uint16_t pos = static_cast<uint16_t>(Fld<uint8_t>(target, 4) + (Fld<uint8_t>(target, 6) * 128 + 1));
    uint16_t pos2 = static_cast<uint16_t>(Fld<uint8_t>(target, 4) + (Fld<uint8_t>(target, 6) * 128 + 1));
    CTRACE(2, kMinorText, static_cast<uint32_t>(Fld<uint16_t>(target, 0xe)), static_cast<uint32_t>((static_cast<uint32_t>(pos2) - 1u) & 0x7fu),
           static_cast<uint32_t>((static_cast<uint32_t>(pos) - 1u) >> 7), bestScore);
    reinterpret_cast<CThoughts*>(self + 0x30)->RecordEvent(static_cast<CThoughts::EventType>(0x17), reinterpret_cast<CThing*>(target)->GetThingId());
    Fld<uint16_t>(self, 0x1de) = Fld<uint16_t>(self, 0x1dc);
    Fld<uint16_t>(self, 0x1dc) = bestId;
    reinterpret_cast<CPeep*>(this)->SetDest(targetCell);
}
