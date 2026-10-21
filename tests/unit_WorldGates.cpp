// Test entry points for the park gates and the arrival vehicle (library unit_WorldGates: src/World_gates.cpp, unit w193): CWorld::
// SetParkOpenOrClosed, ParkGates_Open, ParkGates_Close, ArrivalVehicle_DoNextLeg, _GetStatus, _CrossingIsClear, _PeopleWaitingToLeave,
// _PeopleLeftToDropOff and _IsSeagoing.
//
// Every callee outside those nine is a fake that records the same event as the PowerPC side's stub: the asserts and traces, the thing
// virtuals of CObject (CloseObject, IsOpenable, Reopen) and CThing::GetThingId, the park analyser's visitors, ParkGates_GetStatus and
// IsParkClosed (queues), the script pointer and variables, CMsgEvent and CMessageCentre::PostMessage, the nothrow new of the message
// centre, the ride database walk, _wcsicmp, CObject's constructor and the map's bus stop. Operator new logs its size for the two block
// sizes the functions allocate (0x1d0 centre by nothrow new, 0x448 object).
//
// The world is raw memory (a case chooses its size and fill; the tests write the few words the functions use), the thing table and the
// ride database are the unit's own globals, the thing objects are blocks of 0x240 bytes over the fill 0xa5.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
void observe_allocation(std::size_t size, bool array, void* block);
}
#define TPW_ALLOCATION_OBSERVER observe_allocation
#include "calllog.cpp"
#include "platform/arena.h"

#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/macdoze.h"
#include "rsse.h"

using namespace calllog;

// The unit makes its things on the game heap (the Mac's operator new), which the host takes from the arena (unit v5L31). Here the arena is
// the call log's operator new and delete, so a thing's block is made, filled, labelled and freed as the Mac's is.
namespace tpw {
void* ArenaAlloc(size_t size) { return ::operator new(size); }
bool InArena(const void* address) { return address != nullptr; }
void ArenaFree(void* block) { ::operator delete(block); }
}  // namespace tpw

namespace {

constexpr int32_t kMaxBlocks = 16;
constexpr std::size_t kBlockSize = 0x240;
constexpr int32_t kMaxRides = 4;
constexpr std::size_t kWorldMax = 0x460000;
constexpr int32_t kLabelWorld = 1, kLabelScript = 3, kLabelCentre = 6, kLabelBlock = 10;

alignas(16) unsigned char gWorldBytes[kWorldMax];
alignas(16) unsigned char gWorldInitial[kWorldMax];
std::size_t gWorldSize;
alignas(16) unsigned char gBlocks[kMaxBlocks][kBlockSize];
alignas(16) unsigned char gBlocksInitial[kMaxBlocks][kBlockSize];
int32_t gBlockCount;
alignas(16) unsigned char gScriptBytes[0x40];
alignas(16) unsigned char gCentre[0x1d0];
void* gNewCentre;
void* gNewObject;
alignas(RideInfo) unsigned char gRideStorage[kMaxRides][sizeof(RideInfo)];
RideInfo* gRideRecords[kMaxRides];
int32_t gRideCount, gRideCursor;

CWorld* World() { return reinterpret_cast<CWorld*>(gWorldBytes); }

template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

void Narrow16(const char16_t* text, char* out, std::size_t size) {
    std::size_t i = 0;
    for (; text != nullptr && text[i] != 0 && i + 1 < size; ++i) out[i] = static_cast<char>(text[i]);
    out[i] = 0;
}

void observe_allocation(std::size_t size, bool, void* block) {
    if (!on()) return;
    if (size == 0x448) {
        std::memset(block, 0xa5, size);
        gNewObject = block;
        ev("new", {static_cast<int64_t>(size)});
    }
}

}  // namespace

// ---- The unit's globals (the launcher gets zero definitions) -----------------------------------------------------------------------
CThingArrayEntry gThingArray[kThingArraySize];
CMessageCentre* gMessageCentre;
CRideDatabase gRideDatabase;

// The formatters the string classes declare (never called here).
template <> void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <> void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <> void TbStaticStringTemplate<char>::Format(const char*, ...) {}

// ---- The fakes --------------------------------------------------------------------------------------------------------------------------
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

// The level, and the %s argument when the text takes one.
void CTRACE(uint32_t level, const char* format, ...) {
    if (std::strstr(format, "%s") != nullptr) {
        va_list list;
        va_start(list, format);
        const char* argument = va_arg(list, const char*);
        va_end(list);
        ev("CTRACE", {level}, {format, argument});
    } else {
        ev("CTRACE", {level}, {format});
    }
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {static_cast<uint32_t>(script)});
    return reinterpret_cast<RSSE_SCRIPT*>(gScriptBytes);
}
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index});
    return static_cast<int32_t>(ret("var", 1));
}
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value}); }

int32_t CWorld::IsParkClosed() const {
    ev("IsParkClosed", {lab(Launder(this))});
    return static_cast<int32_t>(ret("closed", 0));
}
int32_t CWorld::ParkGates_GetStatus() const {
    ev("ParkGates_GetStatus", {lab(Launder(this))});
    return static_cast<int32_t>(ret("gstatus", 1));
}
int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(Launder(this))});
    return static_cast<int32_t>(ret("visitors", 0));
}
void CObject::CloseObject() { ev("CloseObject", {lab(this)}); }
int32_t CObject::IsOpenable() {
    ev("IsOpenable", {lab(this)});
    return static_cast<int32_t>(ret("openable", 1));
}
void CObject::Reopen() { ev("Reopen", {lab(this)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CMap::GetBusStopPos(int32_t which) const {
    ev("GetBusStopPos", {static_cast<int64_t>(reinterpret_cast<const unsigned char*>(this) - gWorldBytes), which});
    return {static_cast<uint16_t>(ret("stop", 0))};
}

// The event message and the lazy message centre (as unit w173).
CMsgEvent::CMsgEvent(EventType type) : mEvent(type) { ev("CMsgEvent", {newlab(this), type}); }
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0; }
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), lab(&message), static_cast<const CMsgEvent&>(message).mEvent});
    return 1;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// The ride database walk.
RideInfo* CRideDatabase::GetFirstRide() {
    ev("GetFirstRide", {lab(this)});
    gRideCursor = 0;
    return gRideCount > 0 ? gRideRecords[0] : nullptr;
}
RideInfo* CRideDatabase::GetNextRide() {
    ev("GetNextRide", {lab(this)});
    ++gRideCursor;
    return gRideCursor < gRideCount ? gRideRecords[gRideCursor] : nullptr;
}
// The queue "wcsicmp" answers each call; with the queue empty the answer is 0 for texts that are equal ignoring case (ASCII), else 1.
int32_t NS_MacDoze::_wcsicmp(const char16_t* a, const char16_t* b) {
    char first[160];
    char second[160];
    Narrow16(a, first, sizeof first);
    Narrow16(b, second, sizeof second);
    ev("wcsicmp", {}, {first, second});
    const int64_t queued = ret("wcsicmp", INT64_MAX);
    if (queued != INT64_MAX) return static_cast<int32_t>(queued);
    for (std::size_t i = 0;; ++i) {
        char16_t x = a[i];
        char16_t y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char16_t>(x + 32);
        if (y >= 'A' && y <= 'Z') y = static_cast<char16_t>(y + 32);
        if (x != y) return 1;
        if (x == 0) return 0;
    }
}

// The new object: its id (queue "id") at +0 and its script handle (queue "handle") at +0x20.
CObject::CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t a,
                 int32_t b) {
    ev("CObject", {newlab(this), map.mIndex, type.mIndex, a, b});
    const uint16_t id = static_cast<uint16_t>(ret("id", 0x1234));
    const uint32_t handle = static_cast<uint32_t>(ret("handle", 0x777));
    std::memcpy(this, &id, 2);
    std::memcpy(reinterpret_cast<unsigned char*>(this) + 0x20, &handle, 4);
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const CThing* self = Launder(this);
    ev("GetThingId", {lab(self)});
    return {self->mThingId};
}

extern "C" {

// A new case: the table empty, no blocks, no rides, no centre.
void tpw_wg_begin() {
    std::memset(gThingArray, 0, sizeof gThingArray);
    gBlockCount = 0;
    gRideCount = gRideCursor = 0;
    gNewCentre = gNewObject = nullptr;
    gMessageCentre = nullptr;
    std::memset(gScriptBytes, 0xa5, sizeof gScriptBytes);
    std::memset(gCentre, 0xa5, sizeof gCentre);
}

void* tpw_wg_ptr(int32_t label) {
    switch (label) {
    case kLabelWorld: return gWorldBytes;
    case kLabelScript: return gScriptBytes;
    case kLabelCentre: return gCentre;
    default: return label >= kLabelBlock && label < kLabelBlock + gBlockCount ? gBlocks[label - kLabelBlock] : nullptr;
    }
}

// The world: `size` bytes of `fill`; then words are written by offset.
void tpw_wg_world(int32_t size, int32_t fill) {
    gWorldSize = static_cast<std::size_t>(size);
    std::memset(gWorldBytes, fill, gWorldSize);
}
void tpw_wg_w16(int32_t offset, uint32_t value) {
    const uint16_t v = static_cast<uint16_t>(value);
    std::memcpy(gWorldBytes + offset, &v, 2);
}
void tpw_wg_w32(int32_t offset, uint32_t value) { std::memcpy(gWorldBytes + offset, &value, 4); }

// Thing block k (over 0xa5) and its fields.
void tpw_wg_block(int32_t k) {
    std::memset(gBlocks[k], 0xa5, kBlockSize);
    if (k + 1 > gBlockCount) gBlockCount = k + 1;
}
void tpw_wg_b8(int32_t k, int32_t offset, uint32_t value) { gBlocks[k][offset] = static_cast<unsigned char>(value); }
void tpw_wg_b16(int32_t k, int32_t offset, uint32_t value) {
    const uint16_t v = static_cast<uint16_t>(value);
    std::memcpy(gBlocks[k] + offset, &v, 2);
}
void tpw_wg_b32(int32_t k, int32_t offset, uint32_t value) { std::memcpy(gBlocks[k] + offset, &value, 4); }

// Table row `id` holds block k1 - 1 (0: NULL).
void tpw_wg_row(int32_t id, int32_t k1) { gThingArray[id].mThing = k1 == 0 ? nullptr : gBlocks[k1 - 1]; }

// The ride `index`: its type number and item name.
void tpw_wg_ride(int32_t index, uint16_t id, const uint16_t* name, int32_t n) {
    RideInfo* ride = reinterpret_cast<RideInfo*>(gRideStorage[index]);
    std::memset(static_cast<void*>(ride), 0xa5, sizeof(RideInfo));
    ride->mId = id;
    for (int32_t i = 0; i < n && i < 0x3f; ++i) ride->mItemName[i] = name[i];
    ride->mItemName[n < 0x3f ? n : 0x3f] = 0;
    gRideRecords[index] = ride;
}
void tpw_wg_ride_count(int32_t n) { gRideCount = n; }

void tpw_wg_centre(int32_t present) { gMessageCentre = present ? reinterpret_cast<CMessageCentre*>(gCentre) : nullptr; }

// The three vehicle names (the Mac's __sinit builds them; this unit's translation defines them).
void tpw_wg_names(const uint16_t* bus, int32_t nb, const uint16_t* seaplane, int32_t ns, const uint16_t* ferry, int32_t nf) {
    std::u16string b(reinterpret_cast<const char16_t*>(bus), nb), s(reinterpret_cast<const char16_t*>(seaplane), ns),
        f(reinterpret_cast<const char16_t*>(ferry), nf);
    gArrivalBusName = TbDynamicStringTemplate<char16_t>(b.c_str());
    gArrivalSeaplaneName = TbDynamicStringTemplate<char16_t>(s.c_str());
    gArrivalFerryName = TbDynamicStringTemplate<char16_t>(f.c_str());
}

// Keeps the world and the blocks as the case set them (the report compares against them).
void tpw_wg_snapshot() {
    std::memcpy(gWorldInitial, gWorldBytes, gWorldSize);
    std::memcpy(gBlocksInitial, gBlocks, sizeof gBlocks);
}

// The offsets of the fields the functions write (the world's start time, closed word, the four vehicle ids).
void tpw_wg_get_world(uint32_t* out) {
    World();
    std::memcpy(&out[0], gWorldBytes + 0x1da708, 4);
    std::memcpy(&out[1], gWorldBytes + 0x1da710, 4);
    for (int32_t i = 0; i < 4; ++i) {
        uint16_t v;
        std::memcpy(&v, gWorldBytes + 0x1da72a + 2 * i, 2);
        out[2 + i] = v;
    }
    static const std::size_t spans[][2] = {{0x1da708, 4}, {0x1da710, 4}, {0x1da72a, 8}};
    for (const auto& s : spans) {
        std::memset(gWorldBytes + s[0], 0, s[1]);
        std::memset(gWorldInitial + s[0], 0, s[1]);
    }
    out[6] = std::memcmp(gWorldBytes, gWorldInitial, gWorldSize) == 0;
}

// 1 per block that still holds what the case set.
void tpw_wg_get_blocks(uint32_t* out) {
    for (int32_t k = 0; k < gBlockCount; ++k) out[k] = std::memcmp(gBlocks[k], gBlocksInitial[k], kBlockSize) == 0;
}

// The object made by DoNextLeg: 1 if one was made, its id, its handle, and 1 when every other byte holds the fill.
void tpw_wg_get_new(uint32_t* out) {
    out[0] = gNewObject != nullptr;
    out[1] = out[2] = out[3] = 0;
    if (gNewObject == nullptr) return;
    const unsigned char* p = static_cast<const unsigned char*>(gNewObject);
    uint16_t id;
    std::memcpy(&id, p, 2);
    out[1] = id;
    std::memcpy(&out[2], p + 0x20, 4);
    bool fill = true;
    for (std::size_t i = 2; i < 0x448; ++i)
        if ((i < 0x20 || i >= 0x24) && p[i] != 0xa5) fill = false;
    out[3] = fill;
}

int64_t tpw_wg_centre_label() { return lab(gMessageCentre); }

// One function; the DoNextLeg answer is the vehicle id.
int64_t tpw_wg_call(int32_t op, int32_t a, int32_t b) {
    CWorld* w = World();
    switch (op) {
    case 0: w->SetParkOpenOrClosed(a, b); return 0;
    case 1: w->ParkGates_Open(); return 0;
    case 2: w->ParkGates_Close(a); return 0;
    case 3: return w->ArrivalVehicle_DoNextLeg(a).mIndex;
    case 4: return w->ArrivalVehicle_GetStatus();
    case 5: return w->ArrivalVehicle_CrossingIsClear();
    case 6: return w->ArrivalVehicle_PeopleWaitingToLeave();
    case 7: return w->ArrivalVehicle_PeopleLeftToDropOff();
    case 8: return w->ArrivalVehicle_IsSeagoing();
    default: return -1;
    }
}

}  // extern "C"
