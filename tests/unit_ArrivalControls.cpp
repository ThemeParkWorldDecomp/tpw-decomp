// Differential library for unit w176: the arrivals' people and the control manager (src/Arrival_people.cpp, src/ControlManager.cpp, src/WorldSavvy.cpp).
//
// Every callee outside the unit is a fake that records the same event as the PowerPC side's stub: the arrival vehicle, the park analyser and weather, the
// map's bus stop, CPerson's constructor (and the allocation before it), the trace and assert functions, the serialise log, the file functions, the basic
// LOADVAR / SAVEVAR instances (the unit's Serialise functions call them by name; their own bodies are w146 / w160's) and the ride database's walk.
// The object control file functions are the unit's own and run on both sides.
//
// The world is a tail block of 0x80 bytes (the turn and the 0x1da738 word): a world pointer is the tail's address minus 0x1da6f0, on both sides.
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "Serialise.h"
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* v, const char* n, int32_t g);
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* v, const char* n, int32_t g);
extern uint32_t gArrivalFileWord1034C1A4, gArrivalFileWord1034C1A8;
#include "Arrival.h"
#include "Debug.h"
#include "FS_Online.h"
#include "History.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "TimeSig.h"
#include "World.h"
#include "analyser.h"

#pragma clang diagnostic ignored "-Winvalid-offsetof"

// The allocation observer of calllog.cpp: the Mac's CPerson is 0x22c bytes.
namespace {
bool gTrack;
void Allocated(std::size_t size, bool array, void* block);
}  // namespace
#define TPW_CALLLOG_QUEUE_VALUES 256
#define TPW_ALLOCATION_OBSERVER(size, array, p) Allocated(size, array, p)
#include "calllog.cpp"
#include "platform/arena.h"

using namespace calllog;

// The unit makes its things on the game heap (the Mac's operator new), which the host takes from the arena (unit v5L31). Here the arena is
// the call log's operator new and delete, so a thing's block is made, filled, labelled and freed as the Mac's is.
namespace tpw {
void* ArenaAlloc(size_t size) { return ::operator new(size); }
bool InArena(const void* address) { return address != nullptr; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

namespace {
using ObjectId = tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>;
using MapId = tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>;

constexpr int32_t kTail = 0x1da6f0;
constexpr int32_t kTailSize = 0x80;
constexpr int32_t kGuard = 0x40;
constexpr int32_t kArrivalSize = 0x14;
constexpr int32_t kControlSize = 0x12cc;
constexpr int32_t kAnalyserSize = 0x20d00;
constexpr int32_t kWeatherSize = 0x40;
constexpr int32_t kMaxRides = 24;

alignas(16) unsigned char gTail[kTailSize];
alignas(16) unsigned char gArrival[kGuard + kArrivalSize + kGuard];
alignas(16) unsigned char gControl[kGuard + kControlSize + kGuard];
alignas(16) unsigned char gAnalyser[kAnalyserSize];
alignas(16) unsigned char gWeather[kWeatherSize];
alignas(16) unsigned char gMap[0x40];
alignas(16) unsigned char gSpare[0xa0];
char gFileHandle;
std::vector<uint8_t> gFileData;
const unsigned char* gBase;   // the block whose offsets the VAR events log

RideInfo* gRides[kMaxRides];
int32_t gRideCount;
int32_t gRideAt;

CWorld* WorldPtr() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gTail) - kTail); }

std::string Hex(const void* data, uint32_t size) {
    static const char kDigits[] = "0123456789abcdef";
    const auto* p = static_cast<const uint8_t*>(data);
    std::string out;
    for (uint32_t i = 0; i < size; ++i) {
        out += kDigits[p[i] >> 4];
        out += kDigits[p[i] & 15];
    }
    return out;
}

void Allocated(std::size_t size, bool array, void* block) {
    std::memset(block, 0xA5, size != 0 ? size : 1);
    if (!on() || !gTrack || array) return;
    if (size == 0x22c) ev("new", {0x22c, newlab(block)});
}
}  // namespace

// ---- The unit's globals --------------------------------------------------------------------------------------------------------------
CWorld* gWorld;
CMap* gWorldMap;
CSaveLoad gSaveLoad;
alignas(16) char gRideDBBlock[sizeof(CRideDatabase)] __asm__("_gRideDatabase");

void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance gMainBalance;

// ---- The fakes ------------------------------------------------------------------------------------------------------------------------
CGameType::CGameType() { mType = static_cast<GameType>(0); }

void CTRACE(uint32_t level, const char* format, ...) {
    int64_t value = 0;
    if (std::strstr(format, "%d") != nullptr) {
        va_list args;
        va_start(args, format);
        value = va_arg(args, int);
        va_end(args);
    }
    ev("CTRACE", {level, value}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
void CSaveLoad::WriteSerialiseLog(int32_t size, const char* name, LogfileState state) {
    ev("CSaveLoad::WriteSerialiseLog", {lab(this), size, state}, {name});
}

// The file: a read comes from the test's bytes ("LbFile_Read:xfer" and "LbFile_Read" replace the count and the result); a write is logged as hex with the
// five pad bytes of an object control (2, 3, 0x11 to 0x13: stack leftovers on the Mac) cleared.
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const uint32_t available = static_cast<uint32_t>(gFileData.size());
    uint32_t count = available < length ? available : length;
    const int64_t forced = ret("LbFile_Read:xfer", -1);
    if (forced >= 0) count = static_cast<uint32_t>(forced);
    *transferred = count;
    const int32_t result = static_cast<int32_t>(ret("LbFile_Read"));
    if (count == length && result >= 0) {
        std::vector<uint8_t> bytes(length, 0);
        std::memcpy(bytes.data(), gFileData.data(), available < length ? available : length);
        std::memcpy(buffer, bytes.data(), length);
    }
    // the file's bytes advance
    const uint32_t used = available < count ? available : count;
    gFileData.erase(gFileData.begin(), gFileData.begin() + used);
    return TbError(result);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    std::vector<uint8_t> copy(static_cast<const uint8_t*>(buffer), static_cast<const uint8_t*>(buffer) + length);
    if (length == 0x20) copy[2] = copy[3] = copy[0x11] = copy[0x12] = copy[0x13] = 0;
    ev("LbFile_Write", {lab(file), length}, {Hex(copy.data(), length).c_str()});
    const int64_t forced = ret("LbFile_Write:xfer", -1);
    *transferred = forced >= 0 ? static_cast<uint32_t>(forced) : length;
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

// The basic instances the Serialise functions call. Event "VAR": the kind (1 long, 2 int, 3 bool, 4 unsigned long, 5 object id struct), 1 for a save, the
// file label, the value's offset in the object at hand, the noLog flag, the value (what a save writes, what a load stored) and the answer; the name.
// A load stores the next value of "lv" unless the answer ("ans", consumed per call) is not 0.
namespace {
template<class Value> int32_t Var(int32_t kind, bool save, void* file, Value* value, const char* name, int32_t noLog) {
    const int32_t answer = static_cast<int32_t>(ret("ans", 0));
    int64_t shown;
    if (save) {
        Value copy;
        std::memcpy(&copy, value, sizeof(Value));
        shown = static_cast<int64_t>(static_cast<uint64_t>(sizeof(Value) == 1 ? *reinterpret_cast<uint8_t*>(&copy)
                                                          : sizeof(Value) == 2 ? *reinterpret_cast<uint16_t*>(&copy) : *reinterpret_cast<uint32_t*>(&copy)));
    } else {
        const uint32_t loaded = static_cast<uint32_t>(ret("lv", 0));
        shown = answer != 0 ? 0 : loaded;
        if (answer == 0) {
            if (sizeof(Value) == 1) *reinterpret_cast<uint8_t*>(value) = static_cast<uint8_t>(loaded);
            else if (sizeof(Value) == 2) *reinterpret_cast<uint16_t*>(value) = static_cast<uint16_t>(loaded);
            else std::memcpy(value, &loaded, 4);
            if (sizeof(Value) == 1) shown = static_cast<uint8_t>(loaded);
            else if (sizeof(Value) == 2) shown = static_cast<uint16_t>(loaded);
        }
    }
    ev("VAR", {kind, save ? 1 : 0, lab(file), reinterpret_cast<const unsigned char*>(value) - gBase, noLog, shown, answer}, {name});
    return answer;
}
}  // namespace
template<> int32_t LOADVAR<MacLong>(void* file, MacLong* v, const char* n, int32_t g) { return Var(1, false, file, v, n, g); }
template<> int32_t SAVEVAR<MacLong>(void* file, MacLong* v, const char* n, int32_t g) { return Var(1, true, file, v, n, g); }
template<> int32_t LOADVAR<int32_t>(void* file, int32_t* v, const char* n, int32_t g) { return Var(2, false, file, v, n, g); }
template<> int32_t SAVEVAR<int32_t>(void* file, int32_t* v, const char* n, int32_t g) { return Var(2, true, file, v, n, g); }
template<> int32_t LOADVAR<bool>(void* file, bool* v, const char* n, int32_t g) { return Var(3, false, file, v, n, g); }
template<> int32_t SAVEVAR<bool>(void* file, bool* v, const char* n, int32_t g) { return Var(3, true, file, v, n, g); }
template<> int32_t LOADVAR<MacULong>(void* file, MacULong* v, const char* n, int32_t g) { return Var(4, false, file, v, n, g); }
template<> int32_t SAVEVAR<MacULong>(void* file, MacULong* v, const char* n, int32_t g) { return Var(4, true, file, v, n, g); }
template<> int32_t LOADVARSTRUCT<ObjectId>(void* file, ObjectId* v, const char* n, int32_t g) { return Var(5, false, file, v, n, g); }
template<> int32_t SAVEVARSTRUCT<ObjectId>(void* file, ObjectId* v, const char* n, int32_t g) { return Var(5, true, file, v, n, g); }

// The world: the arrival vehicle's answers come from the queues "status", "wait" and "sea"; the leg change records the leg.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CWorld::ArrivalVehicle_DoNextLeg(int32_t leg) {
    ev("DoNextLeg", {lab(this), leg});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0};
}
int32_t CWorld::ArrivalVehicle_GetStatus() {
    ev("GetStatus", {lab(this)});
    return static_cast<int32_t>(ret("status", 0));
}
int32_t CWorld::ArrivalVehicle_PeopleWaitingToLeave() {
    ev("PeopleWaitingToLeave", {lab(this)});
    return static_cast<int32_t>(ret("wait", 0));
}
int32_t CWorld::ArrivalVehicle_IsSeagoing() {
    ev("IsSeagoing", {lab(this)});
    return static_cast<int32_t>(ret("sea", 0));
}
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyser);
}
CWeather* CWorld::GetWeather() {
    ev("GetWeather", {lab(this)});
    return reinterpret_cast<CWeather*>(gWeather);
}
int32_t CParkAnalyser::GetVisitorsInPark(uint32_t arg) const {
    ev("GetVisitorsInPark", {lab(this), arg});
    return static_cast<int32_t>(ret("visitors", 0));
}
int32_t CParkAnalyser::GetTotalParkAttractionPoints() {
    ev("GetTotalParkAttractionPoints", {lab(this)});
    return static_cast<int32_t>(ret("points", 0));
}
template<> void CHistory<int, 5>::SetAtEnd(int value) {
    ev("SetAtEnd", {reinterpret_cast<const unsigned char*>(this) - gAnalyser, value});
}
MapId CMap::GetBusStopPos(int32_t which) const {
    ev("GetBusStopPos", {lab(this), which});
    MapId cell;
    cell.mIndex = static_cast<uint16_t>(ret("cell", 0));
    return cell;
}
CPerson::CPerson(MapId cell) { ev("CPerson", {lab(this), cell.mIndex}); }

// The ride database's walk over the unit's own table.
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {});
    gRideAt = 0;
    return gRideCount > 0 ? gRides[0] : nullptr;
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {});
    if (gRideAt < gRideCount - 1) {
        ++gRideAt;
        return gRides[gRideAt];
    }
    return nullptr;
}

// ---- The test entry points -----------------------------------------------------------------------------------------------------------
extern "C" {

// The address of a block: 0 the arrival block (the object is at +0x40), 1 the world's tail, 2 the analyser, 3 the weather, 4 the control block (the manager
// is at +0x40), 5 the map, 6 the file, 7 gSaveLoad, 8 the spare block.
void* tpw_avc_addr(int32_t which) {
    switch (which) {
    case 0: return gArrival;
    case 1: return gTail;
    case 2: return gAnalyser;
    case 3: return gWeather;
    case 4: return gControl;
    case 5: return gMap;
    case 6: return &gFileHandle;
    case 7: return &gSaveLoad;
    case 8: return gSpare;
    default: return nullptr;
    }
}
void* tpw_avc_world() { return WorldPtr(); }
// The globals and which block the VAR events measure from (0 the arrival object, 1 the control manager).
void tpw_avc_config(int32_t base, int32_t track) {
    gWorld = WorldPtr();
    gWorldMap = reinterpret_cast<CMap*>(gMap);
    gBase = base == 0 ? gArrival + kGuard : gControl + kGuard;
    gTrack = track != 0;
}
void tpw_avc_set_file(const char* data, int32_t size) { gFileData.assign(data, data + size); }
void tpw_avc_balance(int32_t slot, int32_t value) { gMainBalance.mValues[slot].mInteger = value; }
void tpw_avc_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
// The ride i: the id, FatigueEffect, RipOffOK, CostOfResearch and CostOfUpgrade; count rides in the walk.
void tpw_avc_ride(int32_t i, int32_t id, int32_t fatigue, int32_t ripoff, int32_t research, int32_t cost) {
    if (gRides[i] == nullptr) gRides[i] = static_cast<RideInfo*>(std::calloc(1, sizeof(RideInfo)));
    RideInfo* ride = gRides[i];
    ride->mId = static_cast<uint16_t>(id);
    ride->Value(kRideSlot_FatigueEffect).mInteger = fatigue;
    ride->Value(kRideSlot_RipOffOK).mInteger = ripoff;
    ride->Value(kRideSlot_Upgrades_CostOfResearch).mInteger = research;
    ride->Value(kRideSlot_Upgrades_CostOfUpgrade).mInteger = cost;
}
void tpw_avc_ride_count(int32_t count) { gRideCount = count; }

CArrival* arrival() { return reinterpret_cast<CArrival*>(gArrival + kGuard); }
CControlManager* controls() { return reinterpret_cast<CControlManager*>(gControl + kGuard); }

int32_t tpw_avc_arrival_serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return arrival()->Serialise(file, save, version, unused); }
void tpw_avc_create_people() { arrival()->CreatePeople(); }
int32_t tpw_avc_how_many() { return arrival()->HowManyPeople(); }
int32_t tpw_avc_create_person() { return arrival()->CreatePerson(); }
// The static initializer's two words.
void tpw_avc_sinit(uint32_t* out) {
    out[0] = gArrivalFileWord1034C1A4;
    out[1] = gArrivalFileWord1034C1A8;
}

int32_t tpw_avc_cm_serialise(void* file, int32_t save, int32_t version, uint32_t unused) { return controls()->Serialise(file, save, version, unused); }
// The control found, as its offset in the manager (-1 for NULL); `constant` selects the const overload.
int64_t tpw_avc_get(int32_t constant, uint32_t id, int32_t flag) {
    ObjectId key;
    key.mIndex = static_cast<uint16_t>(id);
    const unsigned char* found;
    if (constant != 0) found = reinterpret_cast<const unsigned char*>(static_cast<const CControlManager*>(controls())->GetObjectControl(key, flag));
    else found = reinterpret_cast<const unsigned char*>(controls()->GetObjectControl(key, flag));
    return found == nullptr ? -1 : found - reinterpret_cast<const unsigned char*>(controls());
}
void tpw_avc_check() { controls()->CheckAllControlsInitialised(); }
// The object control's file functions on the spare block (0x20 bytes at +0x10).
int32_t tpw_avc_struct(int32_t save, void* file, const char* name, int32_t noLog) {
    ObjectControl* control = reinterpret_cast<ObjectControl*>(gSpare + 0x40);
    return save != 0 ? SAVEVARSTRUCT(file, control, name, noLog) : LOADVARSTRUCT(file, control, name, noLog);
}
void tpw_avc_uswap(int32_t which, void* word) {
    if (which == 0) ControlSwap::USwapInplace32ULong(*static_cast<uint32_t*>(word));
    else ControlSwap::USwapInplace32Long(*static_cast<int32_t*>(word));
}

}  // extern "C"
