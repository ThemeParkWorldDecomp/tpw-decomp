// Differential library for unit w202, the ride's admittance and queue length (src/game/Object.cpp): the eleven CObject members HasFreeQueueSpace,
// GetIdealQueueLength, QueueTooLong, AdmitPerson, FinishedAdmitting, ReadyToAdmit, PersonLeftDuringAdmittance, LetPeopleOn, LetPeopleOff,
// ProcessStateFunctioning and GoFirstPerson.
//
// ONE static block (gRegion) holds everything the functions touch, at the offsets the PowerPC side uses in its own mapped region (RB + offset): the call
// trace (a count at 0 and (tag, value) pairs from 0x100), the ride object (0x448 bytes between guard bytes), 64 guest blocks of 0x240 bytes, the ride
// record, a script block, 64 anim set blocks and a world-sized block (gWorld; only mTurn is read). Every fake appends (tag, value) to the trace; a pointer
// is recorded as its offset in the block, so the two sides' traces compare word for word. The ride record's value slots are 4 bytes on the Mac and 8 on
// the host (BalanceValue holds a pointer), so the test writes each side's record at that side's own offsets. SetState is a fake here (the real one is
// another unit's); CThing::GetThingId (src/game/Thing.cpp, another unit) is a copy. The float global is the one the unit defines.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "RideDB.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"

namespace {

constexpr uint32_t kTraceCap = 0x1000;
constexpr uint32_t kObj = 0x10000;
constexpr uint32_t kPers = 0x11000;
constexpr uint32_t kRec = 0x1a000;
constexpr uint32_t kScript = 0x1b000;
constexpr uint32_t kSets = 0x1c000;
constexpr uint32_t kWorld = 0x20000;
constexpr uint32_t kRegionSize = 0x200000;
constexpr int32_t kSetCount = 64;
constexpr int32_t kSlots = 64;
constexpr uint32_t kGuestSize = 0x240;
constexpr uint32_t kSetSize = 0x40;

alignas(4096) unsigned char gRegion[kRegionSize];
uint32_t gTraceTag[kTraceCap];
uint32_t gTraceVal[kTraceCap];
uint32_t gTraceN;

// The case's answers: the script's variables, the queue's count, the coaster test, the camera mesh finders, the tour, and per guest block what
// ReadyForAdmitting and ExitRide answer.
int32_t gVars[16];
int32_t gCount;
int32_t gCoaster;
int32_t gBmp;
int32_t gFly;
int32_t gTour;
int32_t gReady[kSlots];
int32_t gExit[kSlots];

using ThingTag = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

uint32_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(gRegion)); }

void Tr(uint32_t tag, uint32_t value) {
    if (gTraceN < kTraceCap) {
        gTraceTag[gTraceN] = tag;
        gTraceVal[gTraceN] = value;
    }
    gTraceN++;
}

uint32_t TextHash(const char* text) {
    uint32_t h = 0;
    for (; *text; text++) h = h * 31u + static_cast<uint8_t>(*text);
    return h;
}

// One trace entry for each conversion of the format (a %s as the hash of its text).
void TraceArguments(uint32_t tag, const char* format, va_list args) {
    for (const char* p = format; *p; p++) {
        if (*p != '%') continue;
        p++;
        if (*p == '\0') break;
        if (*p == '%') continue;
        if (*p == 's') {
            Tr(tag, TextHash(va_arg(args, const char*)));
        } else {
            Tr(tag, va_arg(args, uint32_t));
        }
    }
}

// The guest block a person pointer is in (the answers of ReadyForAdmitting and ExitRide are per block).
uint32_t GuestSlot(const CPerson* person) {
    const uint32_t slot = (Off(person) - kPers) / kGuestSize;
    return slot < static_cast<uint32_t>(kSlots) ? slot : 0;
}

CObject* Obj() { return reinterpret_cast<CObject*>(gRegion + kObj); }

}  // namespace

// ---- the unit's globals (the launcher gets zero definitions)
CThingArrayEntry gThingArray[kThingArraySize];
CWorld* gWorld;
tag_sRideAnimSet* gRideAnimSets[kSetCount];
alignas(16) unsigned char gRideDatabaseStorage[sizeof(CRideDatabase)] __asm__("_gRideDatabase");   // raw bytes: the fake lookup never touches it

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

// ---- the fakes (the same tags and values as the PowerPC side's stubs)
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    Tr(20, static_cast<uint32_t>(condition));
    Tr(21, TextHash(format));
    va_list args;
    va_start(args, format);
    TraceArguments(22, format, args);
    va_end(args);
}

void CTRACE(uint32_t level, const char* format, ...) {
    Tr(11, level);
    Tr(12, TextHash(format));
    va_list args;
    va_start(args, format);
    TraceArguments(13, format, args);
    va_end(args);
}

RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t handle) {
    Tr(30, static_cast<uint32_t>(handle));
    return reinterpret_cast<RSSE_SCRIPT*>(gRegion + kScript);
}

int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    Tr(31, Off(script));
    Tr(32, static_cast<uint32_t>(index));
    return index >= 0 && index < 16 ? gVars[index] : 0;
}

void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value) {
    Tr(33, Off(script));
    Tr(34, static_cast<uint32_t>(index));
    Tr(35, static_cast<uint32_t>(value));
}

int32_t RSSE_GetTourBaseID(RSSE_SCRIPT* script) {
    Tr(36, Off(script));
    return gTour;
}

int32_t CObject::GetNumPeopleInQueue() const {
    Tr(40, Off(this));
    return gCount;
}

void CObject::RemovePersonFromQueue(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    Tr(41, Off(this));
    Tr(42, person.mIndex);
}

int32_t CObject::IsCoaster() const {
    Tr(43, Off(this));
    return gCoaster;
}

void CObject::SetState(ObjectState state) {
    Tr(44, Off(this));
    Tr(45, static_cast<uint32_t>(state));
}

// The five CPerson members, against the Mac names the unit's source declares them by.
void ObjectForceRideAdmittance(CPerson* person) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");
void ObjectForceRideAdmittance(CPerson* person) { Tr(46, Off(person)); }

void ObjectDismissFromQueue(CPerson* person) __asm__("__ZN7CPerson16DismissFromQueueEv");
void ObjectDismissFromQueue(CPerson* person) { Tr(47, Off(person)); }

int32_t ObjectReadyForAdmitting(CPerson* person) __asm__("__ZNK7CPerson17ReadyForAdmittingEv");
int32_t ObjectReadyForAdmitting(CPerson* person) {
    Tr(48, Off(person));
    return gReady[GuestSlot(person)];
}

void ObjectOnAdmittance(CPerson* person) __asm__("__ZN7CPerson12OnAdmittanceEv");
void ObjectOnAdmittance(CPerson* person) { Tr(49, Off(person)); }

int32_t ObjectExitRide(CPerson* person) __asm__("__ZN7CPerson8ExitRideEv");
int32_t ObjectExitRide(CPerson* person) {
    Tr(50, Off(person));
    return gExit[GuestSlot(person)];
}

int32_t BMP_GetCameraMesh(int32_t ride) {
    Tr(51, static_cast<uint32_t>(ride));
    return gBmp;
}

int32_t FLY_GetCameraMesh(int32_t tour) {
    Tr(52, static_cast<uint32_t>(tour));
    return gFly;
}

void ToggleRideRideIn1stPerson(int32_t ride, int32_t a, int32_t b, int32_t* out, int32_t node, int32_t c) {
    Tr(53, static_cast<uint32_t>(ride));
    Tr(54, static_cast<uint32_t>(a));
    Tr(55, static_cast<uint32_t>(b));
    Tr(56, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(out)));
    Tr(57, static_cast<uint32_t>(node));
    Tr(58, static_cast<uint32_t>(c));
}

// The ride record lookup: the database as a label (1 for the global), the type, and the case's record.
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    Tr(60, this == &gRideDatabase ? 1u : 0u);
    Tr(61, id.mIndex);
    return reinterpret_cast<RideInfo*>(gRegion + kRec);
}

void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag) {
    Tr(62, Off(set));
    Tr(63, flag);
}

extern "C" {

uint64_t tpw_oad_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRegion)); }

void tpw_oad_init() {
    gWorld = reinterpret_cast<CWorld*>(gRegion + kWorld);
    for (int32_t i = 0; i < kSetCount; i++) {
        gRideAnimSets[i] = reinterpret_cast<tag_sRideAnimSet*>(gRegion + kSets + kSetSize * static_cast<uint32_t>(i));
    }
}

// The case's answers: knobs 0 to 15 the script's variables, then the queue count, the coaster answer, BMP_GetCameraMesh, FLY_GetCameraMesh and the tour id;
// ready and exits the answers of ReadyForAdmitting and ExitRide for each of the 64 guest blocks; ratio the float global's bits before the call.
void tpw_oad_begin(const int32_t* knobs, const int32_t* ready, const int32_t* exits, uint32_t ratio) {
    for (int32_t i = 0; i < 16; i++) gVars[i] = knobs[i];
    gCount = knobs[16];
    gCoaster = knobs[17];
    gBmp = knobs[18];
    gFly = knobs[19];
    gTour = knobs[20];
    for (int32_t i = 0; i < kSlots; i++) {
        gReady[i] = ready[i];
        gExit[i] = exits[i];
    }
    std::memcpy(&gObjectQueueSpeedRatio, &ratio, sizeof ratio);
    gTraceN = 0;
}

// The thing table's first n rows: a guest block number, or -1 for no thing.
void tpw_oad_rows(const int32_t* slots, int32_t n) {
    for (int32_t row = 0; row < n; row++) {
        gThingArray[row].mThing = slots[row] >= 0 ? static_cast<void*>(gRegion + kPers + kGuestSize * static_cast<uint32_t>(slots[row])) : nullptr;
    }
}

uint32_t tpw_oad_ratio() {
    uint32_t bits;
    std::memcpy(&bits, &gObjectQueueSpeedRatio, sizeof bits);
    return bits;
}

uint32_t tpw_oad_trace_n() { return gTraceN; }
const uint32_t* tpw_oad_trace_tags() { return gTraceTag; }
const uint32_t* tpw_oad_trace_vals() { return gTraceVal; }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_oad_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mBase));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF20));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF24));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mQueuedPerson));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF3C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF44));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF4C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF54));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF58));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF59));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF64));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mPersonLoaded));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mState));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTurn));
    const RideInfo* info = reinterpret_cast<const RideInfo*>(gRegion + kRec);
    const unsigned char* start = reinterpret_cast<const unsigned char*>(info);
    out[n++] = static_cast<int64_t>(reinterpret_cast<const unsigned char*>(&info->Value(static_cast<RideBalanceSlot>(0))) - start);   // value slot 0
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(kRideSlot_SlotCount);
    return n;
}

// ---- the functions, one wrapper each with C types only. A tag is passed as a number, a bool answer is 0 or 1.
int32_t tpw_oad_has_free() { return Obj()->HasFreeQueueSpace(); }
uint32_t tpw_oad_ideal() { return Obj()->GetIdealQueueLength(); }
int32_t tpw_oad_too_long() { return Obj()->QueueTooLong() ? 1 : 0; }
int32_t tpw_oad_admit(uint32_t person) { return Obj()->AdmitPerson(ThingTag{static_cast<uint16_t>(person)}); }
int32_t tpw_oad_finished() { return Obj()->FinishedAdmitting(); }
int32_t tpw_oad_ready(uint32_t person) { return Obj()->ReadyToAdmit(ThingTag{static_cast<uint16_t>(person)}) ? 1 : 0; }
void tpw_oad_left(uint32_t person) { Obj()->PersonLeftDuringAdmittance(ThingTag{static_cast<uint16_t>(person)}); }
void tpw_oad_on() { Obj()->LetPeopleOn(); }
void tpw_oad_off() { Obj()->LetPeopleOff(); }
void tpw_oad_functioning() { Obj()->ProcessStateFunctioning(); }
void tpw_oad_first(int32_t on) { Obj()->GoFirstPerson(on); }

}  // extern "C"
