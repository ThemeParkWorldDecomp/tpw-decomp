// host_replay_missing_ride.cpp: a park's recording that selects a ride type the game's ride database does not have. An easy
// park's database holds only the ride types with an easy .sam file (CRideDatabase::ScanDirectory), so a park recorded in a full
// game, replayed into an easy park, can select one it lacks. The replay of the selection (action 5, LLCC_SystemSetRide) reads the
// missing type's record in CWorld::FlagItemAsSelected: GetRideInfo answers NULL and the port faulted at address 0x128 (0.4.0,
// coaster040: Addable13's park replayed into the easy Lost Kingdom with Ctrl+F9). The clicks that follow would place the ride
// selected before it. With the port's fixes (replayride, src/game/ActionRecorder.cpp) the replay plays neither the selection nor
// the clicks that place or move the selected ride until the next selection of a ride the database has:
//   - a ride the database has: selected (its required-ride bit set) and placed by its clicks, plain and with a modifier;
//   - a missing ride: nothing selected, no fault, its clicks in the ride modes (4, a ride placed; 0x3b, a ride moved) not played,
//     the clicks of the other modes (a path laid) played;
//   - the next ride the database has: selected and placed again.
// The program runs the real CActionRecorder::ProcessAction overloads of the selection and the clicks, LLCC_SystemSetRide,
// LLCC_SystemIsMode, CWorld::FlagItemAsSelected and CRideDatabase::GetRideInfo on a database of two ride types; the clicks reach
// stand-ins for LLCM_LeftMouseButtonDown and Up that note the ride selected. Made in both builds: without the port's fixes it
// faults at the first missing ride, as the game did, so ctest runs it ("replay_missing_ride") only in a build with them. Exits
// nonzero on a failure.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "World.h"
#include "analyser.h"

// ---- globals of other files, defined here (zero at start)
CActionRecorder gActionRecorder;
CRideDatabase gRideDatabase;
CWorld* gGameWorld = nullptr;
uint8_t gParkRequiredRides[0x20];
int32_t gLLCC480C;
uint16_t gLLCCDE0E2;
uint16_t gLLCCE0E0;
int32_t gLLCCE0DC;

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, long value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%ld)\n", what, value);
}

struct Click {
    int32_t action;   // 0xf down, 0x14 up (or 0x28, 0x2d with a modifier)
    int32_t x, y;
    uint16_t ride;    // the selected ride when the click was played
};
std::vector<Click> gClicks;

// Two ride types; the second is one of the downloaded rides (its DownloadedRideNumber is 3).
constexpr uint16_t kCoaster = 1203;
constexpr uint16_t kGold = 1100;
constexpr uint16_t kMissing = 1207;   // a type of the full game the easy database lacks

alignas(RideInfo) unsigned char gRecords[2][sizeof(RideInfo)];

RideInfo* Record(int i, uint16_t id, int32_t downloaded) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRecords[i]);
    std::memset(gRecords[i], 0, sizeof(RideInfo));
    ride->mId = id;
    ride->Value(kRideSlot_DownloadedRideNumber).mInteger = downloaded;
    return ride;
}

alignas(16) unsigned char gWorldStorage[64];   // FlagItemAsSelected reads nothing of its world

CActionRecorder* Recorder() { return &gActionRecorder; }

void SetRide(uint16_t id) { Recorder()->ProcessAction(5, id, static_cast<uint8_t>(0)); }

// A click as the replay plays it: down then up, at a cell, plain (kinds 0) or with a modifier (kinds 2).
void ClickAt(int32_t x, int32_t y, bool modifier) {
    if (modifier) {
        Recorder()->ProcessAction(0x28, static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint16_t>(0x10));
        Recorder()->ProcessAction(0x2d, static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint16_t>(0x10));
    } else {
        Recorder()->ProcessAction(0xf, static_cast<uint8_t>(x), static_cast<uint8_t>(y));
        Recorder()->ProcessAction(0x14, static_cast<uint8_t>(x), static_cast<uint8_t>(y));
    }
}

bool Placed(size_t from, uint16_t ride, size_t count) {
    if (gClicks.size() != from + count) return false;
    for (size_t i = from; i < gClicks.size(); ++i) {
        if (gClicks[i].ride != ride) return false;
    }
    return true;
}

}  // namespace

// ---- stand-ins for the callees outside the test
void CTRACE(uint32_t, const char*, ...) {}
void CActionRecorder::RecordAction(int32_t, uint16_t, uint8_t) {}   // the replay records nothing (mRecording)
void LLCM_LeftMouseButtonDown(int32_t x, int32_t y, uint16_t shift, int32_t) {
    gClicks.push_back({shift != 0 ? 0x28 : 0xf, x, y, gLLCCE0E0});
}
void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t) {
    gClicks.push_back({shift != 0 ? 0x2d : 0x14, x, y, gLLCCE0E0});
}

int main() {
    gGameWorld = reinterpret_cast<CWorld*>(gWorldStorage);
    gRideDatabase.mRides[0] = Record(0, kCoaster, 0);
    gRideDatabase.mRides[1] = Record(1, kGold, 3);
    gRideDatabase.mRideCount = 2;
    gRideDatabase.mCachedId = 0;
    gRideDatabase.mCachedRide = nullptr;

    {   // a ride the database has: selected, then placed by its clicks
        gLLCC480C = 4;
        const size_t from = gClicks.size();
        SetRide(kGold);
        Check(gLLCCE0E0 == kGold, "a ride the database has: selected", gLLCCE0E0);
        Check((gParkRequiredRides[0] & 0x04) != 0, "a ride the database has: its required-ride bit", gParkRequiredRides[0]);
        ClickAt(45, 19, false);
        ClickAt(46, 19, true);
        Check(Placed(from, kGold, 4), "a ride the database has: its clicks played", static_cast<long>(gClicks.size() - from));
        std::printf("%-44s %zu clicks played\n", "a ride the database has", gClicks.size() - from);
    }
    {   // a ride the database lacks: nothing selected, no fault, the ride modes' clicks not played
        gLLCC480C = 4;
        const size_t from = gClicks.size();
        uint8_t required[0x20];
        std::memcpy(required, gParkRequiredRides, sizeof required);
        SetRide(kMissing);
        Check(gLLCCE0E0 == kGold, "a missing ride: the ride before it stays selected", gLLCCE0E0);
        Check(std::memcmp(required, gParkRequiredRides, sizeof required) == 0, "a missing ride: no required-ride bit");
        ClickAt(49, 19, false);
        ClickAt(50, 19, true);
        gLLCC480C = 0x3b;
        ClickAt(51, 20, false);
        Check(gClicks.size() == from, "a missing ride: its clicks not played", static_cast<long>(gClicks.size() - from));
        std::printf("%-44s %zu clicks played\n", "a missing ride, ride modes", gClicks.size() - from);
    }
    {   // a missing ride still selected: a path laid (mode 1) is played
        gLLCC480C = 1;
        const size_t from = gClicks.size();
        ClickAt(30, 31, false);
        ClickAt(31, 31, true);
        Check(gClicks.size() == from + 4, "a missing ride: a path's clicks played", static_cast<long>(gClicks.size() - from));
        std::printf("%-44s %zu clicks played\n", "a missing ride, a path", gClicks.size() - from);
    }
    {   // the next ride the database has: selected and placed again
        gLLCC480C = 4;
        const size_t from = gClicks.size();
        SetRide(kCoaster);
        Check(gLLCCE0E0 == kCoaster, "the next ride: selected", gLLCCE0E0);
        ClickAt(52, 22, false);
        gLLCC480C = 0x3b;
        ClickAt(53, 22, true);
        Check(Placed(from, kCoaster, 4), "the next ride: its clicks played", static_cast<long>(gClicks.size() - from));
        std::printf("%-44s %zu clicks played\n", "the next ride the database has", gClicks.size() - from);
    }

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
