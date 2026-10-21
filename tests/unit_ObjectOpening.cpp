// Test library for the ride's open, close, upgrade and setting steps, the blocked-exit message and the ride messages (unit w200;
// libtpw_unit_ObjectOpening.dylib, tests/test_ObjectOpening.py), main app:
//   src/game/Object.cpp   CObject::IsOpenable, IsCloseable, CloseObject, Reopen, IsRunning, IsAvailableToChoose, RequestUpgrade, SetUpgradeParent,
//                            GetUpgradeParent, NotifyExitBlocked, GetDBInfo, SetOperatingSpeed, SetOperatingDuration, SetOperatingCapacity, ReceiveMessage
//   src/game/Object.cpp, src/game/ui/MessageCentre.cpp   the constructors, GetType functions and destructors of the ride messages (CMsgNewRide ... CMsgBlockedExit)
//   src/game/rides/RideDB.cpp    CRideDatabase::GetRideInfo (it runs for real: the test builds the ride database it walks)
//
// Every other callee is a fake that records what the real one reads and answers from the test's queue: the ride questions (IsBrokenDown, IsClosed,
// IsCondemned, IsFlaggedForMaintenance, RequiresUpgrade, IsBOQConnected, IsCoaster, IsCoasterUseable, GetBackOfQueue, HasFreeQueueSpace), SetState,
// UnassignStaffMember, GetThingId, the hoardings, the script functions, CTRACE, CMessageCentre::PostMessage, the nothrow operator new with the message
// centre's header constructor, and CHistory::SetFromTemp. The globals of other files the unit reads (the message centre pointer, the ride database and the
// ride anim set table) are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "History.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "calllog.h"
#include "platform/arena.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
CMessageCentre* gMessageCentre;
CRideDatabase gRideDatabase;
tag_sRideAnimSet* gRideAnimSets[8];

namespace {

// The ride's script: any block will do, the code only passes it on. Its address is the label the log shows for the script.
unsigned char gScriptBlock[16];

// The message centre the fake nothrow operator new handed out last (its first header's constructor logs the construction).
void* gNewCentre = nullptr;

// Where the object under test starts: SetFromTemp logs the offset of the history it is called on.
const char* gObjectBase = nullptr;

// A pointer the compiler cannot prove anything about (the fakes read blocks the arena handed out).
template <class T>
T* Launder(T* pointer) {
    __asm__ volatile("" : "+r"(pointer));
    return pointer;
}

// The number of int arguments the Mac passes after each trace text. It is the Mac's own count, not the format's: "Cannot upgrade this object" has no %d but
// the original passes the id, and the staff member text has a %d but the original passes nothing (it leaves the register as it was).
int TraceArguments(const char* format) {
    struct Entry {
        const char* text;
        int count;
    };
    static const Entry entries[] = {
        {"Object %d: Closing...", 1},
        {"Object %d: person being loaded was %d (about to zeroise)\n", 2},
        {"!!!!!!!!!! Opening non-openable ride! (non-fatal, can ignore) !!!!!!!!!! \n", 0},
        {"Object %d: wants maintenance\n", 1},
        {"Cannot upgrade this object\n", 1},
        {"Object %d: Cancelling request for upgrade\n", 1},
        {"Object %d: Never requested upgrading in the first place\n", 1},
        {"Object %d: staff member assigned to service me no longer exists\n", 0},
        {"SPEED = %d\n", 1},
        {"DUR = %d\n", 1},
        {"CAPACITY = %d\n", 1},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.text, format) == 0) return entry.count;
    return -1;
}

// The message kinds, in the order of the module's MESSAGES table.
enum Kind : uint32_t { kNewRide, kBrokenDown, kCondemned, kWantsUpgrade, kDeleted, kBlockedExit };

// Builds a message of the kind in the block (placement new; the block is arena storage that is never freed) and sets the flags word and the thing id.
CMsg* MakeMessage(uint32_t kind, void* block, uint32_t flags, uint16_t id) {
    const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> tag{id};
    CMsg* message = nullptr;
    switch (kind) {
        case kNewRide: {
            auto* typed = new (block) CMsgNewRide(tag);
            typed->mThingId = id;
            message = typed;
            break;
        }
        case kBrokenDown: {
            auto* typed = new (block) CMsgRideBrokenDown(tag);
            typed->mThingId = id;
            message = typed;
            break;
        }
        case kCondemned: {
            auto* typed = new (block) CMsgRideCondemned(tag);
            typed->mThingId = id;
            message = typed;
            break;
        }
        case kWantsUpgrade: {
            auto* typed = new (block) CMsgRideWantsUpgrade(tag);
            typed->mThingId = id;
            message = typed;
            break;
        }
        case kDeleted: {
            auto* typed = new (block) CMsgRideDeleted();
            typed->mThingId = id;
            message = typed;
            break;
        }
        default: {
            auto* typed = new (block) CMsgBlockedExit(tag);
            typed->mThingId = id;
            message = typed;
            break;
        }
    }
    message->mFlags = flags;
    return message;
}

// The message ReceiveMessage is given: a CMsgDeath whose GetType answers the test's queue (the real one always answers 0x1b).
class TestMessage : public CMsgDeath {
public:
    int32_t GetType() const override {
        ev("GetType", {lab(this)});
        return static_cast<int32_t>(ret("type", 0));
    }
};

CObject* ObjectAt(uint64_t address) { return reinterpret_cast<CObject*>(static_cast<uintptr_t>(address)); }
uint64_t AddressOf(const void* pointer) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer)); }
RideInfo* RideAt(uint64_t address) { return reinterpret_cast<RideInfo*>(static_cast<uintptr_t>(address)); }

}  // namespace

// ---- the key function and destructor of CMsgDeath (src/game/ui/MessageCentre.cpp, src/game/Thing.cpp is not compiled here): TestMessage derives from it
int32_t CMsgDeath::GetType() const { return 0x1b; }
CMsgDeath::~CMsgDeath() {}

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    const CThing* self = Launder(this);
    ev("GetThingId", {lab(self)});
    return {self->mThingId};
}

bool CObject::IsClosed() const {
    ev("IsClosed", {lab(this)});
    return ret("IsClosed", 0) != 0;
}

bool CObject::IsBrokenDown() const {
    ev("IsBrokenDown", {lab(this)});
    return ret("IsBrokenDown", 0) != 0;
}

int32_t CObject::IsCondemned() const {
    ev("IsCondemned", {lab(this)});
    return static_cast<int32_t>(ret("IsCondemned", 0));
}

int32_t CObject::IsFlaggedForMaintenance() const {
    ev("IsFlaggedForMaintenance", {lab(this)});
    return static_cast<int32_t>(ret("IsFlaggedForMaintenance", 0));
}

int32_t CObject::RequiresUpgrade() const {
    ev("RequiresUpgrade", {lab(this)});
    return static_cast<int32_t>(ret("RequiresUpgrade", 0));
}

int32_t CObject::IsBOQConnected() {
    ev("IsBOQConnected", {lab(this)});
    return static_cast<int32_t>(ret("IsBOQConnected", 0));
}

int32_t CObject::IsCoaster() const {
    ev("IsCoaster", {lab(this)});
    return static_cast<int32_t>(ret("IsCoaster", 0));
}

int32_t IsCoasterUseable(CObject* object) {
    ev("IsCoasterUseable", {lab(object)});
    return static_cast<int32_t>(ret("IsCoasterUseable", 0));
}

int32_t CObject::HasFreeQueueSpace() const {
    ev("HasFreeQueueSpace", {lab(this)});
    return static_cast<int32_t>(ret("HasFreeQueueSpace", 0));
}

tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CObject::GetBackOfQueue() {
    ev("GetBackOfQueue", {lab(this)});
    return {static_cast<uint16_t>(ret("GetBackOfQueue", 0))};
}

void CObject::UnassignStaffMember() { ev("UnassignStaffMember", {lab(this)}); }

void CObject::SetState(ObjectState state) { ev("SetState", {lab(this), static_cast<int64_t>(state)}); }

void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) { ev("ActivateHoardings", {lab(set), static_cast<int64_t>(flag)}); }

void DeActivateHoardings(tag_sRideAnimSet* set) { ev("DeActivateHoardings", {lab(set)}); }

// The script handle is logged as the unsigned word the Mac passes in a register.
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script) {
    ev("RSSE_GetScriptPointer", {static_cast<int64_t>(static_cast<uint32_t>(script))});
    return reinterpret_cast<RSSE_SCRIPT*>(gScriptBlock);
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    ev("RSSE_Getvar", {lab(script), static_cast<int64_t>(index)});
    return static_cast<int32_t>(ret("var", 0));
}

void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    ev("RSSE_Setvar", {lab(script), static_cast<int64_t>(index), static_cast<int64_t>(value)});
}

void RSSE_SetRideSpeed(RSSE_SCRIPT* script, int32_t speed) { ev("RSSE_SetRideSpeed", {lab(script), static_cast<int64_t>(speed)}); }

// CTRACE logs the level, the ints the Mac passes for the text (as unsigned words) and the text; an unknown text is logged under its own name.
void CTRACE(uint32_t level, const char* format, ...) {
    const int count = TraceArguments(format);
    if (count < 0) {
        ev("CTRACE unknown", {}, {format});
        return;
    }
    int64_t first = 0;
    int64_t second = 0;
    va_list arguments;
    va_start(arguments, format);
    if (count >= 1) first = static_cast<uint32_t>(va_arg(arguments, int));
    if (count >= 2) second = static_cast<uint32_t>(va_arg(arguments, int));
    va_end(arguments);
    const int64_t at = level;
    if (count == 0) ev("CTRACE", {at}, {format});
    if (count == 1) ev("CTRACE", {at, first}, {format});
    if (count == 2) ev("CTRACE", {at, first, second}, {format});
}

// PostMessage logs the centre, the message's type, flags and thing id (the only message it is given is the blocked exit).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {lab(this), static_cast<int64_t>(message.GetType()), static_cast<int64_t>(message.mFlags),
                       static_cast<int64_t>(static_cast<const CMsgBlockedExit&>(message).mThingId)});
    return 1;
}

// The message centre is allocated with the nothrow operator new (0x1d0 bytes, filled with 0xa5); the queue "newnull" says when it fails.
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), static_cast<int64_t>(fail)});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    return gNewCentre;
}

// The Mac builds the 29 headers with one __construct_array call (logged once, with the array's new label); the first header stands for it.
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}

CSubscriptionHeader::~CSubscriptionHeader() {}

// SetFromTemp logs the offset of the history in the object and its temp word (the unsigned history as unsigned, the signed one as signed).
template <>
void CHistory<uint32_t, 30>::SetFromTemp() {
    ev("SetFromTemp", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - gObjectBase), static_cast<int64_t>(mTemp)});
}

template <>
void CHistory<int32_t, 30>::SetFromTemp() {
    ev("SetFromTempS", {static_cast<int64_t>(reinterpret_cast<const char*>(this) - gObjectBase), static_cast<int64_t>(mTemp)});
}

extern "C" {

// The host address of a global by the module's name for it.
uint64_t tpw_oop_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"script", gScriptBlock},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return AddressOf(entry.address);
    return 0;
}

// A zeroed block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_oop_alloc(uint32_t size) { return AddressOf(tpw::ArenaAllocZeroed(size)); }

// The label the log gives an address (0 for NULL, -1 for an address no label names).
int64_t tpw_oop_lab(uint64_t address) { return lab(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }

// The offsets and sizes the module relies on (HOST_LAYOUT): the object's fields it writes by Mac offset, then the host message's flags, thing id and size.
int32_t tpw_oop_layout(int64_t* out) {
    using HistoryWord = CHistory<uint32_t, 30>;     // a name without a comma, for the offsetof macro
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF20));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF28));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF2C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF54));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF58));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF59));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF5A));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF64));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mPersonLoaded));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory6C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistoryF4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory1A4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory22C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory2B4));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mHistory33C));
    out[n++] = static_cast<int64_t>(offsetof(HistoryWord, mTemp));
    out[n++] = static_cast<int64_t>(offsetof(RideInfo, mId));
    CMsgNewRide probe{tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}};
    out[n++] = static_cast<int64_t>(reinterpret_cast<const char*>(&probe.mFlags) - reinterpret_cast<const char*>(&probe));
    out[n++] = static_cast<int64_t>(reinterpret_cast<const char*>(&probe.mThingId) - reinterpret_cast<const char*>(&probe));
    out[n++] = static_cast<int64_t>(sizeof(CMsgNewRide));
    return n;
}

// ---- the world: the message centre, the ride anim set table and the ride database (the Mac's CRideDatabase holds 4 byte pointers: the module writes its
// own layout in the emulator and goes through these on the host)
void tpw_oop_set_centre(uint64_t centre) { gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre)); }

uint64_t tpw_oop_centre() { return AddressOf(gMessageCentre); }

void tpw_oop_set_anim(int32_t index, uint64_t set) { gRideAnimSets[index] = reinterpret_cast<tag_sRideAnimSet*>(static_cast<uintptr_t>(set)); }

// A ride record of the id (the rest 0xa5), and a value slot of a record.
uint64_t tpw_oop_ride_new(uint32_t id) {
    void* block = tpw::ArenaAllocZeroed(sizeof(RideInfo));
    std::memset(block, 0xa5, sizeof(RideInfo));
    reinterpret_cast<RideInfo*>(block)->mId = static_cast<uint16_t>(id);
    return AddressOf(block);
}

void tpw_oop_ride_slot(uint64_t record, int32_t slot, int32_t value) {
    RideAt(record)->Value(static_cast<RideBalanceSlot>(slot)).mInteger = value;
}

// The database's count, cache and the first six slots; and the same read back (count, cached id, cached record, slots 0 to 5).
void tpw_oop_db_set(int32_t count, uint32_t cached_id, uint64_t cached_ride, const uint64_t* rides) {
    gRideDatabase.mRideCount = count;
    gRideDatabase.mCachedId = static_cast<uint16_t>(cached_id);
    gRideDatabase.mCachedRide = RideAt(cached_ride);
    for (int i = 0; i < 6; ++i) gRideDatabase.mRides[i] = RideAt(rides[i]);
}

void tpw_oop_db_read(int64_t* out) {
    out[0] = gRideDatabase.mRideCount;
    out[1] = gRideDatabase.mCachedId;
    out[2] = static_cast<int64_t>(AddressOf(gRideDatabase.mCachedRide));
    for (int i = 0; i < 6; ++i) out[3 + i] = static_cast<int64_t>(AddressOf(gRideDatabase.mRides[i]));
}

// ---- the messages: a block built as a message of the kind (flags and id set), the type its GetType answers, the constructor run on a block, the
// destructor run (the delete the Mac does for a positive flag is logged, the storage is the arena's), and the message ReceiveMessage is given
uint64_t tpw_oop_msg_make(uint32_t kind, uint64_t block, uint32_t flags, uint32_t id) {
    return AddressOf(MakeMessage(kind, reinterpret_cast<void*>(static_cast<uintptr_t>(block)), flags, static_cast<uint16_t>(id)));
}

int32_t tpw_oop_msg_type(uint64_t block) { return reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(block))->GetType(); }

// The constructor on a block; the thing id is the HIGH halfword of the register word the Mac passes.
uint64_t tpw_oop_msg_ctor(uint32_t kind, uint64_t block, uint32_t id_word) {
    void* where = reinterpret_cast<void*>(static_cast<uintptr_t>(block));
    const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> tag{static_cast<uint16_t>(id_word >> 16)};
    CMsg* message = nullptr;
    switch (kind) {
        case kNewRide: message = new (where) CMsgNewRide(tag); break;
        case kBrokenDown: message = new (where) CMsgRideBrokenDown(tag); break;
        case kCondemned: message = new (where) CMsgRideCondemned(tag); break;
        case kWantsUpgrade: message = new (where) CMsgRideWantsUpgrade(tag); break;
        default: message = new (where) CMsgBlockedExit(tag); break;
    }
    return AddressOf(message);
}

uint64_t tpw_oop_msg_destroy(uint32_t kind, uint64_t block, int32_t flag) {
    CMsg* message = reinterpret_cast<CMsg*>(static_cast<uintptr_t>(block));
    switch (kind) {
        case kNewRide: static_cast<CMsgNewRide*>(message)->~CMsgNewRide(); break;
        case kBrokenDown: static_cast<CMsgRideBrokenDown*>(message)->~CMsgRideBrokenDown(); break;
        case kCondemned: static_cast<CMsgRideCondemned*>(message)->~CMsgRideCondemned(); break;
        case kWantsUpgrade: static_cast<CMsgRideWantsUpgrade*>(message)->~CMsgRideWantsUpgrade(); break;
        case kDeleted: static_cast<CMsgRideDeleted*>(message)->~CMsgRideDeleted(); break;
        default: static_cast<CMsgBlockedExit*>(message)->~CMsgBlockedExit(); break;
    }
    if (static_cast<int16_t>(flag) > 0) ev("delete", {lab(message)});
    return block;
}

uint64_t tpw_oop_test_message(uint64_t block, uint32_t flags, uint32_t id) {
    auto* message = new (reinterpret_cast<void*>(static_cast<uintptr_t>(block))) TestMessage();
    message->mFlags = flags;
    message->mThingId = static_cast<uint16_t>(id);
    return AddressOf(message);
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t, the id arguments as the register word whose HIGH halfword is the id)
int32_t tpw_oop_is_openable(uint64_t object) { return ObjectAt(object)->IsOpenable(); }

int32_t tpw_oop_is_closeable(uint64_t object) { return ObjectAt(object)->IsCloseable(); }

void tpw_oop_close_object(uint64_t object) { ObjectAt(object)->CloseObject(); }

void tpw_oop_reopen(uint64_t object) { ObjectAt(object)->Reopen(); }

int32_t tpw_oop_is_running(uint64_t object) { return ObjectAt(object)->IsRunning(); }

int32_t tpw_oop_is_available(uint64_t object) { return ObjectAt(object)->IsAvailableToChoose(); }

int32_t tpw_oop_request_upgrade(uint64_t object, int32_t on) { return ObjectAt(object)->RequestUpgrade(on); }

void tpw_oop_set_parent(uint64_t object, uint32_t id_word) {
    ObjectAt(object)->SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(id_word >> 16)});
}

uint32_t tpw_oop_get_parent(uint64_t object) { return ObjectAt(object)->GetUpgradeParent().mIndex; }

void tpw_oop_notify_blocked(uint64_t object) { ObjectAt(object)->NotifyExitBlocked(); }

uint64_t tpw_oop_get_db_info(uint64_t object) { return AddressOf(ObjectAt(object)->GetDBInfo()); }

void tpw_oop_set_speed(uint64_t object, uint32_t speed) { ObjectAt(object)->SetOperatingSpeed(speed); }

void tpw_oop_set_duration(uint64_t object, uint8_t duration) { ObjectAt(object)->SetOperatingDuration(duration); }

void tpw_oop_set_capacity(uint64_t object, uint8_t capacity) { ObjectAt(object)->SetOperatingCapacity(capacity); }

void tpw_oop_receive(uint64_t object, uint64_t message) {
    gObjectBase = reinterpret_cast<const char*>(static_cast<uintptr_t>(object));
    ObjectAt(object)->ReceiveMessage(*reinterpret_cast<const CMsg*>(static_cast<uintptr_t>(message)));
}

uint64_t tpw_oop_ride_info(uint32_t id_word) {
    return AddressOf(gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id_word >> 16)}));
}

}  // extern "C"
