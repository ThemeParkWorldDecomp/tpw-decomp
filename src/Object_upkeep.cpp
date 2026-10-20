// CObject, main app, unit w199 (the Mac files them under Object.cpp, 0x1010ae5c to 0x1010d9cc in pieces): the ride's wear, breakdown, maintenance, repair,
// upgrade and cleaning. GetProspectiveDamagePerTurn (0x1010ae5c, 816 bytes) and GetProspectiveReliability (0x1010b1cc, 60) estimate the wear a turn does;
// AddWearAndTear (0x1010b244, 840) applies it every 64th turn; RepairFully (0x1010b5bc, 1,940) repairs the ride and, when it was waiting for an upgrade,
// upgrades it a level; CleanFully (0x1010bda8, 228) cleans it; RequestMaintenance (0x1010beb8, 808) flags it for maintenance (closing it) or takes the flag
// off (opening it); the staff member the ride is assigned to (AssignStaffMember 0x1010c4d4, UnassignStaffMember 0x1010c554, GetAssignedStaffMember
// 0x1010c5a0, IsBeingWorkedUpon 0x1010c6d8); the one-line state tests IsCondemned (0x1010c8bc), IsDirty (0x1010c900), IsFlaggedForMaintenance (0x1010c980),
// RequiresUpgrade (0x1010c9c0), StartFixing (0x1010ca08), IsPlayingFixAnim (0x1010ca6c); RequestBreakdown (0x1010d90c) and SetWornFlag (0x1010d994).
// The other CObject members are in Object.cpp, Object_turn.cpp, Object_queries.cpp and Object_queue.cpp.
#include <cstdint>

#include "AdvisorQueue.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {
// The strings of the string pool at 0x1024294c (the offset after each). The ones with a %d that the original passes no number for print whatever the
// registers held; the host passes nothing either.
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x3cd
constexpr const char* kSpeed = "SPEED = %d\n";                                                             // +0x2e7
constexpr const char* kDuration = "DUR = %d\n";                                                            // +0x2f3
constexpr const char* kCapacity = "CAPACITY = %d\n";                                                       // +0x2fd
constexpr const char* kClosing = "Object %d: Closing...";                                                  // +0x699
constexpr const char* kPersonLoaded = "Object %d: person being loaded was %d (about to zeroise)\n";       // +0x6af
constexpr const char* kNonOpenable = "!!!!!!!!!! Opening non-openable ride! (non-fatal, can ignore) !!!!!!!!!! \n";   // +0x6e9
constexpr const char* kRepairing = "Object %d: repairing fully\n";                                         // +0x734
constexpr const char* kLevelThree = "Trying to upgrade a ride already at level three\n";                   // +0x750
constexpr const char* kWantsMaintenance = "Object %d: wants maintenance\n";                                // +0x781
constexpr const char* kCancelled = "Object %d: has cancelled its request for maintenance\n";               // +0x79f
constexpr const char* kTakenTooLong = "Ride: removing staff member from this ride, he's taken too long to fix it\n";   // +0x855
constexpr const char* kRequestedBreakdown = "Object %d: requested breakdown\n";                            // +0xb45

constexpr uint16_t kNoThing = 0;   // the halfword at 0x102a0bde, 0

// The data block RepairFully's upgrade effect spawns its sprite script with (0x1029fd30, 0x18 bytes of the sprite engine's data, its last word is its own
// Mac address; a twin sits at 0x1029fd18). The words are pointers into the TOC area and small numbers, meaning unknown. The host gives it an opaque block
// of the same words under an invented name.
uint32_t gUpgradeSpawnBlock[6] = {0x10266a50, 0, 0x10266a60, 0, 0x10266958, 0x1029fd30};

// The low byte of the float converted to a word the way fctiwz does (truncating, saturating, NaN as the smallest word).
uint32_t LowByte(float value) {
    return static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(value))) & 0xff;
}

// The ride record of the ride's object type.
RideInfo* InfoOf(const CObject& ride) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride.mObjectType});
}

// A slot of the record: the first upgrade level's slot moved up by the ride's level (each level is 16 slots, 0x40 bytes of the Mac record).
int32_t UpgradeValue(const RideInfo* info, RideBalanceSlot slot, uint8_t level) {
    return info->Value(static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16)).mInteger;
}

// The ride may be opened: it is not broken down, condemned, flagged for maintenance or waiting for an upgrade, its queue is connected, and a coaster has
// a usable track. The calls are made in this order and stop at the first failure.
bool CanOpen(CObject* ride) {
    if (ride->IsBrokenDown() || ride->IsCondemned() != 0 || ride->IsFlaggedForMaintenance() != 0 || ride->RequiresUpgrade() != 0 ||
        ride->IsBOQConnected() == 0) {
        return false;
    }
    return ride->IsCoaster() == 0 || IsCoasterUseable(ride) != 0;
}

// Opens the ride: the ride is not told when it cannot be opened (five level 2 traces say so, with no number for their %d), mF64 goes to 1, the hoardings
// go off, the script's variable 6 to 0 and the ride's state to normal. RepairFully and RequestMaintenance both open the ride this way.
void OpenRide(CObject* ride, bool openable) {
    if (!openable) {
        for (int32_t i = 0; i < 5; ++i) CTRACE(2, kNonOpenable);
    }
    ride->mF64 = 1;
    DeActivateHoardings(gRideAnimSets[ride->mF1C]);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(ride->mF20)), 6, 0);
    ride->SetState(CObject::kStateNormal);
}

// Closes the ride: says so, mF64 to 0, says who was being loaded and forgets him, the script's variable 6 to 1 and the hoardings on.
void CloseRide(CObject* ride) {
    const uint16_t closing = ride->mBase.GetThingId().mIndex;
    CTRACE(2, kClosing, static_cast<int32_t>(closing));
    ride->mF64 = 0;
    const uint16_t loaded = ride->mPersonLoaded;
    const uint16_t id = ride->mBase.GetThingId().mIndex;
    CTRACE(2, kPersonLoaded, static_cast<int32_t>(id), static_cast<int32_t>(loaded));
    ride->mPersonLoaded = 0;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(ride->mF20)), 6, 1);
    ActivateHoardings(gRideAnimSets[ride->mF1C], 1);
}
}  // namespace

// The estimate of the damage a turn does to the ride, with the ride's operating speed, the number of people it takes and the duration (the third is never
// read). Two halves, each in 0.1 to 1.1 times a ratio: the speed over the record's maximum speed, and the people over the record's maximum capacity (the
// capacity argument when `useCapacity`, else the script's variable 5, the people on the ride, clamped to 0 up to the maximum); a half is 0.1 plus its ratio
// when below its upgrade level's red line (the speed, and the people divided by 10), else half of 1.1 plus its ratio. The answer is the mean of the halves
// times the level's wear rate. All in single precision, the conversions from words signed.
double CObject::GetProspectiveDamagePerTurn(float speed, float capacity, float duration, bool useCapacity) const {
    (void)duration;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    const int32_t onRide = RSSE_Getvar(script, 5);
    int32_t people = ppc_fctiwz(static_cast<double>(static_cast<float>(onRide)));
    if (people < 0) {
        people = 0;
    } else if (people > InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger) {
        people = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
    }
    const float maxSpeed = static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxSpeed).mInteger);
    const float speedRatio = 0.9f * (speed / maxSpeed);
    const float redLineSpeed = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_RedLineSpeed, mF4C));
    float speedHalf;
    if (speed < redLineSpeed) {
        speedHalf = 0.1f + speedRatio;
    } else {
        speedHalf = (1.1f + speedRatio) * 0.5f;
    }
    float peopleRatio;
    if (useCapacity) {
        peopleRatio = 0.9f * (capacity / static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger));
    } else {
        peopleRatio = 0.9f * (static_cast<float>(people) / static_cast<float>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger));
    }
    const float load = static_cast<float>(people) / 10.0f;
    const float redLineCapacity = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_RedLineCapacity, mF4C));
    float peopleHalf;
    if (load < redLineCapacity) {
        peopleHalf = 0.1f + peopleRatio;
    } else {
        peopleHalf = (1.1f + peopleRatio) * 0.5f;
    }
    const float mean = (speedHalf + peopleHalf) * 0.5f;
    const float wearRate = static_cast<float>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_WearRate, mF4C));
    return static_cast<double>(mean * wearRate);
}

// 100 minus nine times the estimated damage a turn (as a word, truncated), the people taken from the capacity argument.
int32_t CObject::GetProspectiveReliability(float speed, float capacity, float duration) const {
    const double damage = GetProspectiveDamagePerTurn(speed, capacity, duration, true);
    const float scaled = 9.0f * static_cast<float>(damage);
    return 100 - ppc_fctiwz(static_cast<double>(scaled));
}

// Every 64th world turn, for a ride that wears (its level's wear rate is not 0, and the online game type wears nothing) and runs (or runs continuously,
// mFlags 0x100): takes the estimated damage of the ride's script values (0 while the script's variable 5, the people on the ride, is 0) off the condition
// (mF40, clamped to 0 to 100) and a fiftieth of it off the remaining life (mF44). A condition below 20 sets the script's variable 8 (the worn flag); below
// 10 and the damage would reach 10 the advisor is asked (message 0x42, about this object type). A NaN damage takes the plain subtraction, as in the original.
void CObject::AddWearAndTear() {
    uint32_t wearRate = static_cast<uint32_t>(UpgradeValue(InfoOf(*this), kRideSlot_Upgrades_WearRate, mF4C));
    if (TheGameType().mType == kGameTypeOnline) wearRate = 0;
    if (wearRate == 0) return;
    if (IsRunning() == 0 && (mFlags & 0x100) == 0) return;
    if ((gWorld->mTurn & 0x3f) != 0) return;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    float damage;
    if (RSSE_Getvar(script, 5) == 0) {
        damage = 0.0f;
    } else {
        RSSE_SCRIPT* again = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
        const float speed = static_cast<float>(RSSE_GetRideSpeed(again));
        const float capacity = static_cast<float>(RSSE_Getvar(again, 2));
        const float duration = static_cast<float>(RSSE_Getvar(again, 3));
        damage = static_cast<float>(GetProspectiveDamagePerTurn(speed, capacity, duration, false));
    }
    const float lowered = -damage + mF40;
    if (!(lowered > 100.0f)) {
        if (!(lowered < 0.0f)) {
            mF40 = mF40 - damage;
        } else {
            mF40 = 0.0f;
        }
    } else {
        mF40 = 100.0f;
    }
    const float wear = 0.02f * damage;
    const float aged = -wear + mF44;
    if (!(aged > 100.0f)) {
        if (!(aged < 0.0f)) {
            mF44 = mF44 - wear;
        } else {
            mF44 = 0.0f;
        }
    } else {
        mF44 = 100.0f;
    }
    if (static_cast<float>(LowByte(mF40)) < 20.0f) RSSE_Setvar(script, 8, 1);
    if (static_cast<float>(LowByte(mF40)) < 10.0f) {
        if (damage + static_cast<float>(LowByte(mF40)) >= 10.0f) {
            const uint16_t type = mObjectType;
            CWorld* world = gWorld;
            const tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = mBase.GetThingId();
            const CAdvisorRequest request(static_cast<AdvisorMessage>(0x42), id, type, -1, false, -1);
            world->GetAdvisor()->QueueRequest(request);
        }
    }
}

// Repairs the ride fully (the staff member is not read): the condition to 100, the worn flag (script variable 8) off, the staff member and the maintenance
// flag off. A ride that waits for an upgrade (state 2) goes up a level (asserting it was below level three), takes its upgrade effect's old sprite script
// away, shows the effect (RSSE_UpgradeRideEffect, and a new sprite script at level 1 and 2 over the front of the physical queue; the rotations other than
// 0, 90, 180 and 270 leave the position in registers the original never set), pays the level's cost of upgrade and takes the level's speed, capacity and
// duration (each only when above 0; the capacity and the duration are clamped to the record's minimum and maximum, the capacity only when they add up
// above 0, and cut to a byte). Any other ride sets the script's variable 4 to 0 and takes its hoardings off. Last, in all cases, the ride is opened (see
// OpenRide), and a coaster without a usable track is closed again (see CloseRide).
void CObject::RepairFully(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) {
    (void)staff;
    RSSE_SCRIPT* script = RSSE_GetScriptPointer(static_cast<int32_t>(mF20));
    CTRACE(0xc, kRepairing);
    mF40 = 100.0f;
    RSSE_Setvar(script, 8, 0);
    mF5A = 0;
    mF60 = 0;
    if (mState == kStateWantsUpgrade) {
        GAME_ASSERTMSG(static_cast<int32_t>((static_cast<uint32_t>(mF4C) - 3u) >> 31), kLevelThree);
        mF4C = static_cast<uint8_t>(mF4C + 1);
        if (mF50 != 0) SCR_KillScript(static_cast<int32_t>(mF50));
        // The original tests mFlags & 8 before each of the next two calls and makes the same call on both sides.
        const uint16_t frontX = GetFrontOfPhysicalQueue().mIndex;
        const float cellX = 0.5f + static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(frontX) - 1u) & 0x7f));
        const uint16_t frontZ = GetFrontOfPhysicalQueue().mIndex;
        const float cellZ = 0.5f + static_cast<float>(static_cast<int32_t>((static_cast<uint32_t>(frontZ) - 1u) >> 7));
        float x = 0.0f;
        float z = 0.0f;
        switch (mRotation) {
            case 0:
            case 0x5a:
            case 0xb4:
            case 0x10e:
                x = 10.0f * cellX;
                z = 10.0f * cellZ;
                break;
            default:
                break;   // the original leaves two callee-saved registers as its caller had them: 0 stands in
        }
        RSSE_UpgradeRideEffect(static_cast<int32_t>(mF20));
        if (mF4C == 1 || mF4C == 2) {
            mF50 = static_cast<uint32_t>(ESP_SpawnScript(gUpgradeSpawnBlock, static_cast<ESP_Sprites>(0xc), 0, static_cast<ACTS>(0), x, 0.0f, z));
        }
        RideInfo* info = InfoOf(*this);
        gWorld->GetBankAccount()->MakeWithdrawal(static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_CostOfUpgrade, mF4C)));
        const int32_t speed = UpgradeValue(info, kRideSlot_Upgrades_InitSpeed, mF4C);
        if (speed > 0) {
            RSSE_SetRideSpeed(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), speed);
            CTRACE(0x7c, kSpeed, speed);
            mF54 = static_cast<uint32_t>(speed);
        }
        uint32_t capacity = static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_InitCapacity, mF4C));
        if (static_cast<int32_t>(capacity) > 0) {
            capacity &= 0xff;
            const int32_t maxCapacity = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
            const int32_t minCapacity = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
            if (minCapacity + maxCapacity > 0) {
                if (static_cast<int32_t>(capacity) < InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger) {
                    capacity = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger);
                } else if (InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger < static_cast<int32_t>(capacity)) {
                    capacity = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger);
                }
                capacity &= 0xff;
            }
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 2, static_cast<int32_t>(capacity));
            CTRACE(0x7c, kCapacity, static_cast<int32_t>(capacity));
            mF59 = static_cast<uint8_t>(capacity);
        }
        uint32_t duration = static_cast<uint32_t>(UpgradeValue(info, kRideSlot_Upgrades_InitDuration, mF4C));
        if (static_cast<int32_t>(duration) > 0) {
            duration &= 0xff;
            if (static_cast<int32_t>(duration) < InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger) {
                duration = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger);
            } else if (InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger < static_cast<int32_t>(duration)) {
                duration = static_cast<uint32_t>(InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger);
            }
            RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 3, static_cast<int32_t>(duration & 0xff));
            CTRACE(0x7c, kDuration, static_cast<int32_t>(duration & 0xff));
            mF58 = static_cast<uint8_t>(duration);
        }
    } else {
        RSSE_Getvar(script, 7);   // both reads are dead in the original, and so is the script they read
        RSSE_Getvar(script, 4);
        RSSE_Setvar(script, 4, 0);
        DeActivateHoardings(gRideAnimSets[mF1C]);
    }
    OpenRide(this, CanOpen(this));
    if (IsCoaster() != 0 && IsCoasterUseable(this) == 0) CloseRide(this);
}

// Cleans the ride: a dirty one (see IsDirty) takes the litter effect (kind 6) out of its map cell and relief (kind 1) in; the worn flag (script variable 8)
// goes off, the condition to 100, and the staff member and the maintenance flag off. The cell is the map cell of the ride's offset (1 based id).
void CObject::CleanFully() {
    if (IsDirty() != 0) {
        const uint16_t cell = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
        gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(6), tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{cell});   // 6 has no name (a dirt effector)
        const uint16_t again = static_cast<uint16_t>(mBase.mCellX + ((static_cast<uint32_t>(mBase.mCellY) << 7) + 1));
        gWorldMap->AddRegionalEffector(kRegionEffectorRelief, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{again});
    }
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 0);
    mF40 = 100.0f;
    mF5A = 0;
    mF60 = 0;
}

// Flags the ride for maintenance (`on` not 0, when it is not flagged yet: closes it, see CloseRide) or takes the flag off (`on` 0, when it is flagged: opens
// the ride, see OpenRide, when it can be opened). Always answers 1.
int32_t CObject::RequestMaintenance(int32_t on) {
    if (on == 0) {
        const uint16_t id = mBase.GetThingId().mIndex;
        CTRACE(0x7c, kCancelled, static_cast<int32_t>(id));
        if (mF60 != 0) {
            mF60 = 0;
            if (CanOpen(this)) OpenRide(this, CanOpen(this));
        }
    } else if (mF60 == 0) {
        const uint16_t id = mBase.GetThingId().mIndex;
        CTRACE(0x7c, kWantsMaintenance, static_cast<int32_t>(id));
        mF60 = 1;
        CloseRide(this);
    }
    return 1;
}

// The staff member assigned to the ride, and the turn it was assigned on.
void CObject::AssignStaffMember(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) {
    mF5A = staff.mIndex;
    mF5C = gWorld->mTurn;
}

void CObject::UnassignStaffMember() {
    mF5A = 0;
    mF5C = 0;
}

// The assigned staff member (nobody is 0). One assigned more than 100 turns ago (the turn counter unsigned, the sum wrapping) who is not allocated to this
// ride any more is taken off, with a trace, and the answer is nobody.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    if (mF5A == kNoThing) return {kNoThing};
    if (static_cast<uint32_t>(mF5C + 100u) < gWorld->mTurn) {
        const uint16_t staffId = mF5A;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
        if (staff->IsAllocatedToRide(mBase.GetThingId()) == 0) {
            CTRACE(0xc, kTakenTooLong);
            mF5A = 0;
            mF5C = 0;
            return {kNoThing};
        }
    }
    return {mF5A};
}

// Whether the assigned staff member (the same timeout as GetAssignedStaffMember, inlined) works on this ride: nobody is 0, else the staff member's answer.
int32_t CObject::IsBeingWorkedUpon() {
    uint16_t assigned;
    if (mF5A == kNoThing) {
        assigned = kNoThing;
    } else {
        assigned = mF5A;
        if (static_cast<uint32_t>(mF5C + 100u) < gWorld->mTurn) {
            const uint16_t staffId = mF5A;
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
            CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
            if (staff->IsAllocatedToRide(mBase.GetThingId()) == 0) {
                CTRACE(0xc, kTakenTooLong);
                mF5A = 0;
                mF5C = 0;
                assigned = kNoThing;
            }
        }
    }
    if (assigned == kNoThing) return 0;
    const uint16_t staffId = mF5A;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CStaff* staff = static_cast<CStaff*>(gThingArray[staffId].mThing);
    return staff->IsWorkingOnAllocatedObject(mBase.GetThingId());
}

int32_t CObject::IsCondemned() const {
    return mState == kStateCondemned ? 1 : 0;
}

// A relief ride (mFlags 1) whose condition (the low byte of the float as fctiwz converts it) is below 25.
int32_t CObject::IsDirty() const {
    if ((mFlags & 1) != 0 && static_cast<float>(LowByte(mF40)) < 25.0f) return 1;
    return 0;
}

int32_t CObject::IsFlaggedForMaintenance() const {
    return static_cast<int32_t>(mF60);
}

int32_t CObject::RequiresUpgrade() const {
    return mState == kStateWantsUpgrade ? 1 : 0;
}

// The ride's script variable 4 is the breakdown request (RequestBreakdown sets it to 1, StartFixing to 0), 7 is 1 while the fixing animation plays and 8 is the
// worn flag (SetWornFlag).
void CObject::StartFixing() {
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 4, 0);
}

int32_t CObject::IsPlayingFixAnim() {
    return RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 7) > 0 ? 1 : 0;
}

void CObject::RequestBreakdown() {
    CTRACE(2, kRequestedBreakdown);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 4, 1);
}

void CObject::SetWornFlag() {
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 8, 1);
}
