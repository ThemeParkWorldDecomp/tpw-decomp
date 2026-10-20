// Differential library for unit w126: CObject::CObject and CObject::SetState. Every callee outside the unit is a recording fake. Each fake logs its
// arguments and a hash of the object's fields at the call, so the order of the constructor's stores against its calls is compared too.
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.cpp"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
#include "Debug.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbTimeDiff.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "rsse.h"

using namespace calllog;

// The two CPerson members SetState reaches (declared against their Mac names in src/Object.cpp).
void ObjectForceRideAdmittance(CPerson*) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");
void ObjectDismissFromQueue(CPerson*) __asm__("__ZN7CPerson16DismissFromQueueEv");

CWorld* gWorld;
CMessageCentre* gMessageCentre;
CMap* gWorldMap;
CThingArrayEntry gThingArray[kThingArraySize];
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;

namespace {
constexpr int32_t kBlockSize = 0x280;
alignas(16) unsigned char gObject[0x448 + 0x40];
alignas(16) unsigned char gWorldTail[0x100];
alignas(16) unsigned char gCentreBlock[0x1d0];
alignas(16) unsigned char gRideBlock[sizeof(RideInfo) + 0x40];
alignas(16) unsigned char gMisc[24][kBlockSize];
char gRideName[0x80];
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gFieldOffset[256];
int32_t gFieldWidth[256];
int32_t gFieldCount;

// The blocks a case uses (the same numbering as BLOCKS in test_Object.py).
enum Block {
    kMap = 0, kBank, kAnalyser, kControl0, kControl1, kControl2, kPerson0, kPerson1, kPerson2, kScript0, kScript1, kAnim0, kAnim1, kAnim2, kAnim3
};
CWorld* WorldBase() { return reinterpret_cast<CWorld*>(reinterpret_cast<uintptr_t>(gWorldTail) - 0x1da6f0); }

// The object's fields as the case's table lists them: the hash that both sides compute at every fake call.
uint32_t Snap() {
    uint32_t h = 2166136261u;
    for (int32_t i = 0; i < gFieldCount; ++i) {
        uint64_t v = 0;
        std::memcpy(&v, gObject + gFieldOffset[i], static_cast<size_t>(gFieldWidth[i]));
        h = static_cast<uint32_t>((static_cast<uint64_t>(h) ^ v) * 16777619u);
    }
    return h;
}
int64_t Off(const void* p, const void* base) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(base)); }
int32_t CountArgs(const char* format) {
    int32_t n = 0;
    for (; *format != 0; ++format) n += *format == '%';
    return n > 2 ? 2 : n;
}
}  // namespace

// ---- the thing base, the world, the message centre
CThing::CThing(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell, ThingModel model) {
    ev("CThing", {lab(this), cell.mIndex, model, Snap()});
    std::memset(this, 0x5a, 0xc);
    mCellX = static_cast<uint8_t>(ret("cx", 0x5a));
    mCellY = static_cast<uint8_t>(ret("cy", 0x5a));
}
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const auto id = static_cast<uint16_t>(ret("id", 0x1234));
    ev("GetThingId", {lab(this), Snap()});
    return {id};
}
void CWorld::Register(CObject* object) { ev("Register", {lab(this), lab(object), Snap()}); }
bool CMessageCentre::Subscribe(const CThing& thing, MsgType type) {
    ev("Subscribe", {lab(this), lab(&thing), type, Snap()});
    if (ret("clear_sub", 0)) gMessageCentre = nullptr;
    return true;
}
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    uint16_t id = 0;
    switch (message.GetType()) {
    case 6: id = static_cast<const CMsgNewRide&>(message).mThingId; break;
    case 7: id = static_cast<const CMsgRideBrokenDown&>(message).mThingId; break;
    case 8: id = static_cast<const CMsgRideCondemned&>(message).mThingId; break;
    case 9: id = static_cast<const CMsgRideWantsUpgrade&>(message).mThingId; break;
    }
    ev("PostMessage", {lab(this), message.GetType(), message.mFlags, id, Snap()});
    return 0;
}
// The four ride messages: the unit's tests fake what the constructors write (the real ones are in src/ThingMessages2.cpp, unit w200).
CMsgNewRide::CMsgNewRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("CMsgNewRide", {id.mIndex, Snap()});
    mFlags = 0;
    mThingId = id.mIndex;
    if (ret("clear_msg", 0)) gMessageCentre = nullptr;
}
int32_t CMsgNewRide::GetType() const { return 6; }
CMsgRideBrokenDown::CMsgRideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("CMsgRideBrokenDown", {id.mIndex, Snap()});
    mFlags = 0;
    mThingId = id.mIndex;
    if (ret("clear_msg", 0)) gMessageCentre = nullptr;
}
int32_t CMsgRideBrokenDown::GetType() const { return 7; }
CMsgRideCondemned::CMsgRideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("CMsgRideCondemned", {id.mIndex, Snap()});
    mFlags = 0;
    mThingId = id.mIndex;
    if (ret("clear_msg", 0)) gMessageCentre = nullptr;
}
int32_t CMsgRideCondemned::GetType() const { return 8; }
CMsgRideWantsUpgrade::CMsgRideWantsUpgrade(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    ev("CMsgRideWantsUpgrade", {id.mIndex, Snap()});
    mFlags = 0;
    mThingId = id.mIndex;
    if (ret("clear_msg", 0)) gMessageCentre = nullptr;
}
int32_t CMsgRideWantsUpgrade::GetType() const { return 9; }
// The destructors the vtables name (the Mac's, which reset the vtable and delete the object for a positive flag, are in src/ThingMessages2.cpp).
CMsgNewRide::~CMsgNewRide() {}
CMsgRideBrokenDown::~CMsgRideBrokenDown() {}
CMsgRideCondemned::~CMsgRideCondemned() {}
CMsgRideWantsUpgrade::~CMsgRideWantsUpgrade() {}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    ++tpw_cl_heap[0];
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

// ---- debug output and assertions
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t n = CountArgs(format);
    const uint32_t a = n > 0 ? va_arg(args, uint32_t) : 0;
    const uint32_t b = n > 1 ? va_arg(args, uint32_t) : 0;
    va_end(args);
    ev("CTRACE", {level, n, a, b, Snap()}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition, Snap()}, {format}); }

// ---- the ride record, the map, the ticket and bank objects, the controls
RideInfo* CObject::GetDBInfo() const {
    ev("GetDBInfo", {lab(this), Snap()});
    return reinterpret_cast<RideInfo*>(gRideBlock);
}
MapDelta MapDelta::Rotate(int32_t rotation) const {
    auto* ride = reinterpret_cast<RideInfo*>(gRideBlock);
    int32_t which = -1;
    if (this == reinterpret_cast<const MapDelta*>(&ride->mMapOffsetX)) which = 0;
    if (this == reinterpret_cast<const MapDelta*>(&ride->mEntryX)) which = 1;
    if (this == reinterpret_cast<const MapDelta*>(&ride->mExitX)) which = 2;
    ev("Rotate", {which, mX, mY, rotation, Snap()});
    MapDelta out;
    out.mX = static_cast<int32_t>(ret("rx", 0));
    out.mY = static_cast<int32_t>(ret("ry", 0));
    out.mFlag = static_cast<uint8_t>(ret("rf", 0));
    return out;
}
void CMap::AddRegionalEffector(RegionEffector effector, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    ev("AddRegionalEffector", {lab(this), effector, cell.mIndex, Snap()});
}
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("HasBeenBought", {Off(this, WorldBase()), type.mIndex, Snap()});
    return ret("bought", 0) != 0;
}
bool CGoldenTicketControl::SpendTicketOn(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) {
    ev("SpendTicketOn", {Off(this, WorldBase()), type.mIndex, Snap()});
    return false;
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this), Snap()});
    return reinterpret_cast<CBankAccount*>(gMisc[kBank]);
}
void CBankAccount::MakeWithdrawal(uint32_t amount) { ev("MakeWithdrawal", {lab(this), amount, Snap()}); }
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this), Snap()});
    return reinterpret_cast<CParkAnalyser*>(gMisc[kAnalyser]);
}
void CParkAnalyser::BuiltSomething() { ev("BuiltSomething", {lab(this), Snap()}); }
TbTimeStamp CParkTime::GetFunnyTime(int32_t* a, int32_t* b, int32_t* c, int32_t* d, int32_t* e, int32_t* f, int32_t* g, int32_t* h) const {
    ev("GetFunnyTime", {lab(gWorld), Off(this, gWorld), a != nullptr, b != nullptr, c != nullptr, d != nullptr, e != nullptr, f != nullptr, g != nullptr,
                        h != nullptr, Snap()});
    return TbTimeStamp{ret("funny", 0)};
}
ObjectControl* CControlManager::GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type, int32_t flag) {
    const int32_t which = static_cast<int32_t>(ret("ctl", 0)) % 3;
    ev("GetObjectControl", {Off(this, WorldBase()), type.mIndex, flag, which, Snap()});
    return reinterpret_cast<ObjectControl*>(gMisc[kControl0 + which]);
}

// ---- the members of the object the constructor calls
void CObject::UpdateEngine(int32_t flag) { ev("UpdateEngine", {lab(this), flag, Snap()}); }
void CObject::CreateRideScript() { ev("CreateRideScript", {lab(this), Snap()}); }
void CObject::SetOperatingSpeed(uint32_t speed) { ev("SetOperatingSpeed", {lab(this), speed, Snap()}); }
void CObject::SetOperatingCapacity(uint8_t capacity) { ev("SetOperatingCapacity", {lab(this), capacity, Snap()}); }
void CObject::SetOperatingDuration(uint8_t duration) { ev("SetOperatingDuration", {lab(this), duration, Snap()}); }
void CObject::CloseObject() { ev("CloseObject", {lab(this), Snap()}); }
void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    ev("RemovePersonFromQueue", {lab(this), person.mIndex, Snap()});
}
void ObjectForceRideAdmittance(CPerson* person) { ev("ForceRideAdmittance", {lab(person), Snap()}); }
void ObjectDismissFromQueue(CPerson* person) { ev("DismissFromQueue", {lab(person), Snap()}); }

// ---- the name copy and the sign names
extern "C" char* strncpy(char* to, const char* from, size_t count) {
    static bool busy;
    if (!busy && from == gRideName && on()) {
        busy = true;
        ev("strncpy", {to != nullptr, static_cast<int64_t>(count)}, {from});
        busy = false;
    }
    size_t i = 0;
    for (; i < count && from[i] != 0; ++i) to[i] = from[i];
    for (; i < count; ++i) to[i] = 0;
    return to;
}
void CRideDatabase::GetSignNameA(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetSignNameA", {this == &gRideDatabase, id, Off(buffer, gObject), size, Snap()});
}
void CRideDatabase::GetSignNameB(uint16_t id, char16_t* buffer, uint32_t size) {
    ev("GetSignNameB", {this == &gRideDatabase, id, Off(buffer, gObject), size, Snap()});
}

// ---- the script and the hoardings
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    const int32_t which = static_cast<int32_t>(ret("scr", 0)) & 1;
    ev("RSSE_GetScriptPointer", {script, which, Snap()});
    return reinterpret_cast<RSSE_SCRIPT*>(gMisc[kScript0 + which]);
}
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), index, Snap()});
    return static_cast<int32_t>(ret("var", 0));
}
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) { ev("RSSE_Setvar", {lab(script), index, value, Snap()}); }
void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) { ev("ActivateHoardings", {lab(set), flag, Snap()}); }

extern "C" {
// Blocks by number (BLOCKS in test_Object.py): 0 object, 1 world base, 2 centre, 3 ride, 4 world tail, 5.. gMisc.
void* tpw_obj_ptr(int32_t k) {
    switch (k) {
    case 0: return gObject;
    case 1: return WorldBase();
    case 2: return gCentreBlock;
    case 3: return gRideBlock;
    case 4: return gWorldTail;
    default: return gMisc[k - 5];
    }
}
int32_t tpw_obj_size(int32_t k) {
    switch (k) {
    case 0: return sizeof gObject;
    case 2: return sizeof gCentreBlock;
    case 3: return sizeof gRideBlock;
    case 4: return sizeof gWorldTail;
    default: return kBlockSize;
    }
}
void tpw_obj_fields(const int32_t* pairs, int32_t n) {
    gFieldCount = n;
    for (int32_t i = 0; i < n; ++i) {
        gFieldOffset[i] = pairs[2 * i];
        gFieldWidth[i] = pairs[2 * i + 1];
    }
}
// The ride record: value slot, name, id and the three map offset triples (x, y, flag) in order (map offset, entry, exit).
void tpw_obj_ride_value(int32_t slot, int32_t value) { reinterpret_cast<RideInfo*>(gRideBlock)->Value(static_cast<RideBalanceSlot>(slot)).mInteger = value; }
void tpw_obj_ride_name(const char* text) {
    std::strncpy(gRideName, text, sizeof gRideName - 1);
    reinterpret_cast<RideInfo*>(gRideBlock)->Value(kRideSlot_Name).mString = gRideName;
}
void tpw_obj_ride_id(int32_t id) { reinterpret_cast<RideInfo*>(gRideBlock)->mId = static_cast<uint16_t>(id); }
void tpw_obj_ride_map(const int32_t* t) {
    auto* ride = reinterpret_cast<RideInfo*>(gRideBlock);
    ride->mMapOffsetX = t[0];
    ride->mMapOffsetY = t[1];
    ride->mEntryX = t[2];
    ride->mEntryY = t[3];
    ride->mExitX = t[4];
    ride->mExitY = t[5];
}
void tpw_obj_thing(int32_t index, int32_t block) { gThingArray[index].mThing = block < 0 ? nullptr : gMisc[block]; }
void tpw_obj_anim(int32_t index, int32_t block) { gRideAnimSets[index] = block < 0 ? nullptr : reinterpret_cast<tag_sRideAnimSet*>(gMisc[block]); }
void tpw_obj_prepare(int32_t centre) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gWorld = WorldBase();
    gWorldMap = reinterpret_cast<CMap*>(gMisc[kMap]);
    gMessageCentre = centre ? reinterpret_cast<CMessageCentre*>(gCentreBlock) : nullptr;
    std::memset(gRideBlock, 0, sizeof gRideBlock);
    for (int32_t i = 0; i < kThingArraySize; ++i) gThingArray[i].mThing = nullptr;
    for (auto& entry : gRideAnimSets) entry = nullptr;
}
void tpw_obj_globals(int64_t* out) {
    out[0] = lab(gWorld);
    out[1] = lab(gMessageCentre);
    out[2] = tpw_cl_heap[0];
}
// op 0: the constructor over the object block; op 1: SetState. a and b are the constructor's arguments.
void tpw_obj_call(int32_t op, uint32_t map, uint32_t type, int32_t rotation, int32_t b, int32_t state) {
    if (op == 0) {
        new (gObject) CObject(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(map)},
                              tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(type)}, rotation, b);
    } else {
        reinterpret_cast<CObject*>(gObject)->SetState(static_cast<CObject::ObjectState>(state));
    }
}
}
