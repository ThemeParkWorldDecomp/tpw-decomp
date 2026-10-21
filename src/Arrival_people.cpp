// CArrival (main app of SimTheme Park 2.04b3, Arrival.cpp; unit w176): the buses and their people. Serialise 0x100f2040 (596 bytes), CreatePeople
// 0x100f2350 (644), HowManyPeople 0x100f2638 (472), CreatePerson 0x100f2840 (216) and the file's static initializer __sinit_Arrival_cpp 0x100f29b0 (28).
// The arrival vehicle (CWorld::ArrivalVehicle_*), the park analyser's queries, the map's bus stop and CPerson's constructor are outside the unit.
#include <cstdint>
#include <new>

#include "Arrival.h"
#include "Debug.h"
#include "FS_Online.h"
#include "History.h"
#include "MainBalance.h"
#include "Serialise.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"

// __sinit_Arrival_cpp 0x100f29b0 (28 bytes): the Mac stores 0 at 0x1034c1a8, then 0x10000 at 0x1034c1a4: two words of this file's data that nothing in the
// binary reads (names mine). Constant initialisers need no code on the host.
uint32_t gArrivalFileWord1034C1A4 = 0x10000;
uint32_t gArrivalFileWord1034C1A8 = 0;

namespace {
// The strings are the Mac's: the member names from 0x102404cb (the Serialise names), the trace texts after them.
constexpr char kNameArrivalRate[] = "mArrivalRate";                          // 0x102404cb
constexpr char kNameTargetVehicleCapacity[] = "mTargetVehicleCapacity";       // 0x102404d8
constexpr char kNamePeopleOnBus[] = "mPeopleOnBus";                           // 0x102404ef
constexpr char kNameOffloading[] = "mOffloading";                             // 0x102404fc
constexpr char kNameGatesOpen[] = "mGatesOpen";                               // 0x10240508
constexpr char kTraceBusArriving[] = "Bus about to arrive with %d people\n";   // 0x10240513
constexpr char kAssertBadStatus[] = "Shouldn't get this return value...\n";    // 0x10240537
constexpr char kTraceDroppingOff[] = "Bus: dropping off kids\n";                 // 0x1024055b
constexpr char kTraceCapOnline[] = "Capping the number of people in online park";      // 0x10240573
constexpr char kTraceCapOffline[] = "Capping the number of people in offline park";    // 0x1024059f
constexpr char kTracePeopleCount[] = "Number of people is %d.\n";                      // 0x102405cc

// The main balance's value slots (slot = (Mac offset - 8) / 4).
int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }
constexpr int kMinPeopleSlot = 178;            // +0x2d0
constexpr int kTimeBetweenArrivalsSlot = 179;  // +0x2d4
constexpr int kNewParkBonusSlot = 181;         // +0x2dc
constexpr int kPointsPerVisitorSlot = 182;     // +0x2e0

// The arrivals' time stamp (this + 4) is a CTimeSig.
CTimeSig* TimeSigOf(CArrival* arrival) { return reinterpret_cast<CTimeSig*>(&arrival->mF04); }

// fctiwz of a single: truncates toward zero, saturating (the NaN of a PowerPC compare-free converter is the minimum, which the product never is).
int32_t TruncateToInt(float value) {
    if (value >= 2147483648.0f) return INT32_MAX;
    if (value < -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(value);
}
}  // namespace

// 0x100f2040 (596 bytes): the rate, the time stamp, the vehicle capacity, the people on the bus, offloading and gates open; versions 0 and 1.
int32_t CArrival::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (save == 0) {
        if (version < 2 && -1 < version) {
            if (LOADVAR(file, reinterpret_cast<MacLong*>(&mArrivalRate), kNameArrivalRate, 0) != 0) return 1;
            if (TimeSigOf(this)->Serialise(file, 0, version, unused) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<MacLong*>(&mTargetVehicleCapacity), kNameTargetVehicleCapacity, 0) != 0) return 1;
            if (LOADVAR(file, &mPeopleOnBus, kNamePeopleOnBus, 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<bool*>(&mOffloading), kNameOffloading, 0) != 0) return 1;
            if (LOADVAR(file, reinterpret_cast<bool*>(&mGatesOpen), kNameGatesOpen, 0) != 0) return 1;
        }
    } else if (version < 2 && -1 < version) {
        if (SAVEVAR(file, reinterpret_cast<MacLong*>(&mArrivalRate), kNameArrivalRate, 0) != 0) return 1;
        if (TimeSigOf(this)->Serialise(file, save, version, unused) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<MacLong*>(&mTargetVehicleCapacity), kNameTargetVehicleCapacity, 0) != 0) return 1;
        if (SAVEVAR(file, &mPeopleOnBus, kNamePeopleOnBus, 0) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mOffloading), kNameOffloading, 0) != 0) return 1;
        if (SAVEVAR(file, reinterpret_cast<bool*>(&mGatesOpen), kNameGatesOpen, 0) != 0) return 1;
    }
    return 0;
}

// 0x100f2350 (644 bytes).
void CArrival::CreatePeople() {
    if (mOffloading == 0) {
        const uint32_t seconds = static_cast<uint32_t>(TimeSigOf(this)->SecondsSinceLastUpdate());
        if (static_cast<uint32_t>(Slot(kTimeBetweenArrivalsSlot)) < seconds) {
            mPeopleOnBus = HowManyPeople();
            mOffloading = 1;
            CTRACE(0x17, kTraceBusArriving, mPeopleOnBus);
            gWorld->GetParkAnalyser()->mHistory216D8.SetAtEnd(mPeopleOnBus);
        }
    }
    if (mOffloading != 0) {
        const int32_t status = gWorld->ArrivalVehicle_GetStatus();
        if (status == 2) {
            CTRACE(2, kTraceDroppingOff);
            if (mPeopleOnBus > 0) {
                CreatePerson();
                mPeopleOnBus = mPeopleOnBus - 1;
                uint8_t* analyser = reinterpret_cast<uint8_t*>(gWorld->GetParkAnalyser());
                int32_t* counter = reinterpret_cast<int32_t*>(analyser + 0x20cc0);
                *counter = *counter + 1;   // a count in the analyser (not named yet)
            } else {
                TimeSigOf(this)->SetTimeSig();
                mOffloading = 0;
                gWorld->ArrivalVehicle_DoNextLeg(0);
            }
        } else if (status == -1 || status == 6) {
            if (status == 6) GAME_ASSERTMSG(0, kAssertBadStatus);
            int32_t leg;
            if (mPeopleOnBus <= 0x23) leg = 1;
            else if (mPeopleOnBus <= 0x3c) leg = 2;
            else leg = 3;
            gWorld->ArrivalVehicle_DoNextLeg(leg);
        }
    }
    if (gWorld->ArrivalVehicle_PeopleWaitingToLeave() == 0) {
        if (gWorld->ArrivalVehicle_GetStatus() == 4) gWorld->ArrivalVehicle_DoNextLeg(0);
    } else {
        const int32_t status = gWorld->ArrivalVehicle_GetStatus();
        if (status == 2) {
            if (mPeopleOnBus == 0) gWorld->ArrivalVehicle_DoNextLeg(0);
        } else if (status < 2) {
            if (status == 0) gWorld->ArrivalVehicle_DoNextLeg(0);
            else if (status < 0 && -2 < status) gWorld->ArrivalVehicle_DoNextLeg(0);
        }
    }
}

// 0x100f2638 (472 bytes).
int32_t CArrival::HowManyPeople() {
    if (gWorld->mF1DA738 == 4) return 0;
    const int32_t visitors = gWorld->GetParkAnalyser()->GetVisitorsInPark(0x14);
    int32_t divisor = Slot(kPointsPerVisitorSlot);
    if (divisor < 2) divisor = 1;
    const int32_t points = gWorld->GetParkAnalyser()->GetTotalParkAttractionPoints();
    const CWeather* weather = gWorld->GetWeather();
    const float factor = static_cast<int32_t>(weather->mF30) > 0 ? 0.8f : 1.2f;   // 0x3f4ccccd / 0x3f99999a
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(points) + static_cast<uint32_t>(Slot(kNewParkBonusSlot)));
    const float scaled = factor * static_cast<float>(sum);
    int32_t people = TruncateToInt(scaled) / divisor;
    if (Slot(kMinPeopleSlot) > people) people = Slot(kMinPeopleSlot);
    if (TheGameType().mType == 1 && static_cast<uint32_t>(visitors + people) > 0x1f4) {
        CTRACE(2, kTraceCapOnline);
        people = 0x1f4 - visitors;
    }
    if (TheGameType().mType != 1 && static_cast<uint32_t>(visitors + people) > 0x5dc) {
        CTRACE(2, kTraceCapOffline);
        people = 0x5dc - visitors;
    }
    CTRACE(3, kTracePeopleCount, people);
    return people;
}

// 0x100f2840 (216 bytes). A bus stop at sea is moved 256 cells back (-2 << 7). On land the cell is the answer as it stands.
int32_t CArrival::CreatePerson() {
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell;
    if (gWorld->ArrivalVehicle_IsSeagoing() != 0) {
        const int32_t offset = -2;
        const tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> stop = gWorldMap->GetBusStopPos(1);
        cell.mIndex = static_cast<uint16_t>(0 + static_cast<int32_t>(static_cast<uint32_t>(offset) << 7) + stop.mIndex);
    } else {
        cell = gWorldMap->GetBusStopPos(1);
    }
    void* memory = tpw::ArenaAlloc(0x22c);
    if (memory != nullptr) new (memory) CPerson(cell);
    return 1;
}
