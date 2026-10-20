// CObject, main app, unit w202 (the Mac files them under Object.cpp, the pieces 0x101085a0 to 0x10108a0c and 0x1010d0e0 to 0x1010ee94): the ride judges
// its queue (HasFreeQueueSpace, GetIdealQueueLength, QueueTooLong), admits a guest (AdmitPerson, FinishedAdmitting, ReadyToAdmit,
// PersonLeftDuringAdmittance), takes its guests on and off (LetPeopleOn, LetPeopleOff, ProcessStateFunctioning) and shows the first person view
// (GoFirstPerson). src/Object.cpp, src/Object_turn.cpp, src/Object_queries.cpp, src/Object_queue.cpp, src/Object_takings.cpp, src/Object_upkeep.cpp
// and src/Object_opening.cpp hold other CObject members. The guests are CPerson, a placeholder class: the ride reads one word of a guest (+0x220, his
// state) and calls five of his members, which stay untranslated (the launcher answers zero).
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
#include "ppc_math.h"
#include "rsse.h"

// The five CPerson members the ride calls, against their Mac names as src/Object.cpp and src/Object_turn.cpp do (CPerson is a placeholder class).
// ReadyForAdmitting is const on the Mac; ReadyForAdmitting and ExitRide answer a word that the ride tests whole.
void ObjectForceRideAdmittance(CPerson*) __asm__("__ZN7CPerson19ForceRideAdmittanceEv");   // 0x1011df70
void ObjectDismissFromQueue(CPerson*) __asm__("__ZN7CPerson16DismissFromQueueEv");         // 0x1011f1b8
int32_t ObjectReadyForAdmitting(CPerson*) __asm__("__ZNK7CPerson17ReadyForAdmittingEv");   // 0x1011f0b8
void ObjectOnAdmittance(CPerson*) __asm__("__ZN7CPerson12OnAdmittanceEv");                 // 0x1011f114
int32_t ObjectExitRide(CPerson*) __asm__("__ZN7CPerson8ExitRideEv");                       // 0x1011f5a4

// 0x1034c35c: the float GetIdealQueueLength and QueueTooLong store the ratio of the ride's speed to its record's initial speed in (the name is mine).
float gObjectQueueSpeedRatio = 0.0f;

namespace {
using ThingId = tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>;

// The strings of the string pool at 0x1024294c (the offset after each). The ones without a line end are the Mac's.
constexpr const char* kNoQueueConstant = "No queue constant entered in SAM file for object number %d at upgrade level %d\n";   // +0x30c
constexpr const char* kZeroDuration = "Operating duration set to zero - shouldn't have a zero duration for a ride!";          // +0x35c
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";                       // +0x3cd
constexpr const char* kForcing = "Object %d: script admitted person %d but he doesn't know yet, forcing him onto ride\n";     // +0x8a0
constexpr const char* kCannotAdmit = "Object %d (type %d): cannot admit person %d, broke down or closed!";                    // +0x8f5
constexpr const char* kLoadedIs = "Object %d (type %d): mPersonBeingLoaded is %d, but admitting person %d (%d)\n";             // +0x938
constexpr const char* kWrongPerson = "admitting wrong person - check debug output\n";                                         // +0x985
constexpr const char* kAdmitting = "Object %d::AdmitPerson - person being loaded is %d\n";                                    // +0x9b2
constexpr const char* kChangedMind = "Object %d (type %d): cannot admit person %d, script changed its mind about admission! (%d)";   // +0x9e6
constexpr const char* kNotExpecting = "Person %d: telling object %d about leaving during admittance, but object not expecting me (expecting %d instead)\n";   // +0xa41
constexpr const char* kLeftAfter = "Object %d: person %d left after I'd admitted him.  Fine, stuff you then.";                // +0xab3
constexpr const char* kLetMeOn = "Object %d: person %d is in LETMEON but wants to leave.  Zeroing LETMEON\n";                 // +0xafc
constexpr const char* kZeroCapacity = "Object has zero capacity\n";                                                           // +0xbcd
constexpr const char* kLoading = "Object %d::LetMeOn - person being loaded is %d\n";                                          // +0xbe7
constexpr const char* kThinksOtherwise = "Object %d thinks person %d is being loaded, person thinks otherwise\n";             // +0xc17
constexpr const char* kNotOnRide = "Object %d: trying to dismiss person %d, but they're not on this ride!";                   // +0xc5c
constexpr const char* kBreakingDown = "Object %d: Setting state BROKEN_DOWN\n";                                               // +0xca2
constexpr const char* kCondemning = "Object %d: Setting state CONDEMNED\n";                                                   // +0xcc8

// The halfwords the Mac compares the thing ids against, one in the pool for each address the code names; all of them are 0.
constexpr uint16_t kNoThing = 0;            // 0x102a0be2: LetPeopleOn's dismissal of the queue's head
constexpr uint16_t kNobodyLoading = 0;      // 0x102a0be6: LetPeopleOn, nobody being loaded
constexpr uint16_t kNobodyQueued = 0;       // 0x102a0be8: LetPeopleOn, nobody at the head of the queue
constexpr uint16_t kNobodyLoaded = 0;       // 0x102a0bea: LetPeopleOn, the check of the person being loaded
constexpr uint16_t kNobodyToLetOff = 0;     // 0x102a0bec: LetPeopleOff and ProcessStateFunctioning, nobody to let off

constexpr uint32_t kGuestState = 0x220;     // the offset of a guest's state word
constexpr uint32_t kBeingLoaded = 0xd;      // the guest is being loaded
constexpr uint32_t kAdmittedUnaware = 0xe;  // the guest is admitted and does not know it yet
constexpr uint32_t kOnTheRide = 0x10;       // the guest is on the ride

uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint32_t LowByte(float value) {
    return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xff;
}

// The ride record of the ride's object type (a call each time: the Mac fetches it again for every field it reads, GoFirstPerson apart).
RideInfo* InfoOf(const CObject& ride) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride.mObjectType});
}

// The slot of the record's upgrade array for the ride's upgrade level: 16 slots (0x40 bytes) to a level.
RideBalanceSlot AtLevel(RideBalanceSlot slot, uint8_t level) {
    return static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16);
}

// The guest of a thing id: the Mac asserts that row 0 of the thing array is empty before every fetch.
CPerson* GuestOf(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    return static_cast<CPerson*>(gThingArray[id].mThing);
}

ThingId IdOfGuest(CPerson* guest) {
    return reinterpret_cast<CThing*>(guest)->GetThingId();
}

// The ideal length of the queue of a ride that has one (mFlags 8), the steps GetIdealQueueLength and QueueTooLong share (the Mac repeats them in each).
// The speed ratio (the operating speed over the record's initial speed of the upgrade level, 1 for a ride without a speed) goes to the global, which
// is read back for the length: the queue wait constant of the level times the ratio times the capacity, over the duration, at least 4, as a word.
uint32_t IdealLength(const CObject& ride) {
    if (ride.mF54 != 0) {
        const float speed = static_cast<float>(ride.mF54);
        const float initial = static_cast<float>(InfoOf(ride)->Value(AtLevel(kRideSlot_Upgrades_InitSpeed, ride.mF4C)).mInteger);
        gObjectQueueSpeedRatio = speed / initial;
    } else {
        gObjectQueueSpeedRatio = 1.0f;
    }
    const float constant = InfoOf(ride)->Value(AtLevel(kRideSlot_Upgrades_QueueWaitTimeConstant, ride.mF4C)).mFloat;
    GAME_ASSERTMSG(constant > 0.0f, kNoQueueConstant, ride.mObjectType, ride.mF4C);
    GAME_ASSERTMSG(ride.mF58 != 0, kZeroDuration);
    const float capacity = static_cast<float>(ride.mF59);
    const float duration = static_cast<float>(ride.mF58);
    const float length = (constant * (gObjectQueueSpeedRatio * capacity)) / duration;
    return cw_cvt_fp2unsigned(static_cast<double>(length > 4.0f ? length : 4.0f));
}

// The steps LetPeopleOff takes once it has the ride's script (ProcessStateFunctioning has them inline): the script's variable 1 names the guest to let
// off; he must be on the ride, and the variable is cleared when he has gone.
void LetGuestOff(const CObject& ride, RSSE_SCRIPT* script) {
    const uint16_t id = static_cast<uint16_t>(RSSE_Getvar(script, 1));
    if (id == kNobodyToLetOff) return;
    CPerson* guest = GuestOf(id);
    const ThingId personId = IdOfGuest(guest);
    const ThingId rideId = ride.mBase.GetThingId();
    GAME_ASSERTMSG(WordAt(guest, kGuestState) == kOnTheRide, kNotOnRide, rideId.mIndex, personId.mIndex);
    if (ObjectExitRide(guest) != 0) RSSE_Setvar(script, 1, 0);
}

// Whether LetPeopleOn may load the queue's head, once the script's variable 0 is 0: the load (variable 5) is below the capacity (variable 2), or the
// ride is a coaster or a track ride of type 2; the ride runs continuously or its variable 9 is 0; nobody is being loaded. Each call in the Mac's order.
bool MayLoadNext(const CObject& ride, RSSE_SCRIPT* script) {
    const int32_t capacity = RSSE_Getvar(script, 2);
    const int32_t load = RSSE_Getvar(script, 5);
    if (load >= capacity) {
        if (ride.IsCoaster() == 0) {
            if (InfoOf(ride)->Value(kRideSlot_WhichTrackType).mInteger != 2) return false;
        }
    }
    if (RSSE_Getvar(script, 9) != 0 && (ride.mFlags & 0x100) == 0) return false;
    return ride.mPersonLoaded == kNobodyLoading;
}
}  // namespace

// 0x101085a0 (80 bytes). The queue is full when it holds four people to each unit of mF3C (an unsigned compare). The Mac answers a byte.
int32_t CObject::HasFreeQueueSpace() const {
    return static_cast<uint32_t>(GetNumPeopleInQueue()) < (mF3C << 2);
}

// 0x10108624 (448 bytes). 100 for a ride without a queue.
uint32_t CObject::GetIdealQueueLength() const {
    if ((mFlags & 8) == 0) return 100;
    return IdealLength(*this);
}

// 0x10108818 (484 bytes). The queue is too long when the people in it reach the ideal length (100 for a ride without a queue); the count is asked last.
bool CObject::QueueTooLong() const {
    uint32_t ideal = 100;
    if ((mFlags & 8) != 0) ideal = IdealLength(*this);
    return static_cast<uint32_t>(GetNumPeopleInQueue()) >= ideal;
}

// 0x1010d0e0 (548 bytes). A ride that is broken down, condemned or closed refuses (a trace of level 0xc). Otherwise the person should be the one being
// loaded (a trace and an assert say so when he is not), the ride forgets the person being loaded and gives the script's variable 0 to the person, when
// the script holds nobody; else it refuses with a trace of level 0xc.
int32_t CObject::AdmitPerson(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    const int32_t state = mState;
    if (state == kStateBrokenDown || mF64 == 0 || state == kStateCondemned) {
        const uint16_t who = person.mIndex;
        const uint16_t type = mObjectType;
        const ThingId id = mBase.GetThingId();
        CTRACE(0xc, kCannotAdmit, id.mIndex, type, who);
        return 0;
    }
    if (mPersonLoaded != person.mIndex) {
        const uint32_t turn = gWorld->mTurn;
        const uint16_t who = person.mIndex;
        const uint16_t loaded = mPersonLoaded;
        const uint16_t type = mObjectType;
        const ThingId id = mBase.GetThingId();
        CTRACE(2, kLoadedIs, id.mIndex, type, loaded, who, turn);
        GAME_ASSERTMSG(0, kWrongPerson);
    }
    const uint16_t loaded = mPersonLoaded;
    const ThingId id = mBase.GetThingId();
    CTRACE(2, kAdmitting, id.mIndex, loaded);
    mPersonLoaded = 0;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (RSSE_Getvar(script, 0) == 0) {
        RSSE_Setvar(script, 0, person.mIndex);
        return 1;
    }
    const uint32_t turn = gWorld->mTurn;
    const uint16_t who = person.mIndex;
    const uint16_t type = mObjectType;
    const ThingId again = mBase.GetThingId();
    CTRACE(0xc, kChangedMind, again.mIndex, type, who, turn);
    return 0;
}

// 0x1010d35c (96 bytes). Admitting is finished when the script's variable 0 is no longer the head of the queue.
int32_t CObject::FinishedAdmitting() {
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    return static_cast<uint32_t>(RSSE_Getvar(script, 0)) != mQueuedPerson;
}

// 0x1010d3ec (32 bytes). The Mac answers a byte and calls nothing.
bool CObject::ReadyToAdmit(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    return mPersonLoaded == person.mIndex;
}

// 0x1010d464 (360 bytes). The person should be the one being loaded (an assert says so when he is not). The ride says so, forgets the person being loaded
// and, when the script's variable 0 still holds the person, says so again and clears the variable.
void CObject::PersonLeftDuringAdmittance(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> person) {
    const uint16_t loaded = mPersonLoaded;
    const ThingId id = mBase.GetThingId();
    GAME_ASSERTMSG(mPersonLoaded == person.mIndex, kNotExpecting, person.mIndex, id.mIndex, loaded);
    const ThingId again = mBase.GetThingId();
    CTRACE(2, kLeftAfter, again.mIndex, person.mIndex);
    mPersonLoaded = 0;
    const uint16_t who = person.mIndex;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (static_cast<uint32_t>(RSSE_Getvar(script, 0)) != who) return;
    const ThingId last = mBase.GetThingId();
    CTRACE(2, kLetMeOn, last.mIndex, who);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0, 0);
}

// 0x1010e61c (952 bytes). A closed ride (mF64 0): when the script's variable 0 is not the head of the queue and the head is admitted but unaware (state
// 0xe), it forces him onto the ride and is done; otherwise it takes the head out of the queue and dismisses him. An open ride: the capacity must be
// above 0; when the script holds nobody and MayLoadNext agrees, it loads the head of the queue (ReadyForAdmitting and OnAdmittance) and makes him the
// person being loaded; in every other case it checks that the person being loaded is in the state of being loaded (0xd) and forgets him when he is not.
void CObject::LetPeopleOn() {
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (mF64 == 0) {
        const uint32_t held = static_cast<uint32_t>(RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0));
        const uint16_t queued = mQueuedPerson;
        if (held != queued) {
            CPerson* guest = GuestOf(queued);
            if (WordAt(guest, kGuestState) == kAdmittedUnaware) {
                const ThingId personId = IdOfGuest(guest);
                const ThingId rideId = mBase.GetThingId();
                CTRACE(2, kForcing, rideId.mIndex, personId.mIndex);
                ObjectForceRideAdmittance(guest);
                return;
            }
        }
        if (mQueuedPerson == kNoThing) return;
        const uint16_t head = mQueuedPerson;
        CPerson* guest = GuestOf(head);
        RemovePersonFromQueue(ThingId{mQueuedPerson});
        ObjectDismissFromQueue(guest);
        return;
    }
    GAME_ASSERTMSG(RSSE_Getvar(script, 2) > 0, kZeroCapacity);
    if (RSSE_Getvar(script, 0) == 0 && MayLoadNext(*this, script)) {
        if (mQueuedPerson == kNobodyQueued) return;
        const uint16_t head = mQueuedPerson;
        CPerson* guest = GuestOf(head);
        if (ObjectReadyForAdmitting(guest) == 0) return;
        ObjectOnAdmittance(guest);
        mPersonLoaded = mQueuedPerson;
        const uint16_t loaded = mPersonLoaded;
        const ThingId id = mBase.GetThingId();
        CTRACE(2, kLoading, id.mIndex, loaded);
        return;
    }
    if (mPersonLoaded == kNobodyLoaded) return;
    const uint16_t first = mPersonLoaded;
    CPerson* guest = GuestOf(first);
    if (WordAt(guest, kGuestState) == kBeingLoaded) return;
    const uint16_t loaded = mPersonLoaded;
    const ThingId id = mBase.GetThingId();
    GAME_ASSERTMSG(0, kThinksOtherwise, id.mIndex, loaded);
    mPersonLoaded = 0;
}

// 0x1010ea00 (280 bytes).
void CObject::LetPeopleOff() {
    LetGuestOff(*this, RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
}

// 0x1010eb44 (528 bytes). The ride loads its guests, then reads the script's variable 7: 0 lets the guests off (a second call for the script, as the
// Mac makes it); otherwise the ride breaks down while the low byte of mF44 (the remaining life) is not 0 and is condemned when it is, each with the hoardings
// of its anim set (2 and 4) and a trace of level 2.
void CObject::ProcessStateFunctioning() {
    LetPeopleOn();
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    if (RSSE_Getvar(script, 7) == 0) {
        LetGuestOff(*this, RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
        return;
    }
    if (LowByte(mF44) != 0) {
        const ThingId id = mBase.GetThingId();
        CTRACE(2, kBreakingDown, id.mIndex);
        ActivateHoardings(gRideAnimSets[mF1C], 2);
        SetState(kStateBrokenDown);
        return;
    }
    const ThingId id = mBase.GetThingId();
    CTRACE(2, kCondemning, id.mIndex);
    ActivateHoardings(gRideAnimSets[mF1C], 4);
    SetState(kStateCondemned);
}

// 0x1010ed8c (264 bytes). The ride record's word +0x80 (the first person sprites) goes to the camera in every case. A ride with a camera mesh of its own
// (mF24 not 0 and BMP_GetCameraMesh finds one) uses it and is done; a ride whose script drives a fly tour (RSSE_GetTourBaseID not 0) uses the tour's mesh;
// the others use their anim set's index (mF1C) and the record's word +0x11c (the constrain camera slot). `on` is the third argument in every case.
void CObject::GoFirstPerson(int32_t on) const {
    RideInfo* info = InfoOf(*this);
    const int32_t sprites = info->Value(kRideSlot_FirstPersonSprites).mInteger;
    if (mF24 != 0) {
        const int32_t mesh = BMP_GetCameraMesh(mF24);
        if (static_cast<uint32_t>(mesh) != 0) {
            ToggleRideRideIn1stPerson(mesh, sprites, on, nullptr, 1, 0);
            return;
        }
    }
    const int32_t tour = RSSE_GetTourBaseID(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)));
    if (tour != 0) {
        ToggleRideRideIn1stPerson(FLY_GetCameraMesh(tour), sprites, on, nullptr, 1, 0);
        return;
    }
    ToggleRideRideIn1stPerson(static_cast<int32_t>(mF1C), sprites, on, nullptr, 1, info->Value(kRideSlot_ConstrainCamera).mInteger);
}
