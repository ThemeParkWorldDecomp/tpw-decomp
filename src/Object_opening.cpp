// The ride's open and close steps, whether it runs, may be chosen or upgraded, its operating speed, duration and capacity, the blocked exit and the messages
// it receives (main app, Object.cpp range; unit w200). The members are the Mac's CObject members; the names of the fields are the header's. The ride is
// closed while mF64 is 0, running while its script's variable 9 is above 0, and its state word mState says whether it wants an upgrade.
#include <cstdint>
#include <new>

#include "CMessageCentre.h"
#include "CMsgDeath.h"
#include "Debug.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "rsse.h"

namespace {
// The strings of the string pool at 0x1024294c (the offset after each). The staff member text has a %d that the original passes no number for (it prints
// whatever the register held); the host passes nothing either. "Cannot upgrade this object" has no %d but the original passes the id.
constexpr const char* kStaffGone = "Object %d: staff member assigned to service me no longer exists\n";   // +0x2a6
constexpr const char* kSpeed = "SPEED = %d\n";                                                             // +0x2e7
constexpr const char* kDuration = "DUR = %d\n";                                                            // +0x2f3
constexpr const char* kCapacity = "CAPACITY = %d\n";                                                       // +0x2fd
constexpr const char* kClosing = "Object %d: Closing...";                                                  // +0x699
constexpr const char* kPersonLoaded = "Object %d: person being loaded was %d (about to zeroise)\n";       // +0x6af
constexpr const char* kNonOpenable = "!!!!!!!!!! Opening non-openable ride! (non-fatal, can ignore) !!!!!!!!!! \n";   // +0x6e9
constexpr const char* kWantsMaintenance = "Object %d: wants maintenance\n";                                // +0x781 (the Mac's text for an upgrade request)
constexpr const char* kCannotUpgrade = "Cannot upgrade this object\n";                                     // +0x7d5
constexpr const char* kCancelling = "Object %d: Cancelling request for upgrade\n";                         // +0x7f1
constexpr const char* kNeverRequested = "Object %d: Never requested upgrading in the first place\n";       // +0x81c

constexpr uint32_t kTrace = 2;            // the level of the open, close and upgrade traces
constexpr uint32_t kSettingTrace = 0x7c;  // the level of the operating settings and of the upgrade request's first line
constexpr uint16_t kNoMapCell = 0;        // the halfword at 0x102a0bb6, 0 (GetBackOfQueue answers it for a ride without a queue end)

constexpr int32_t kStaffDeath = 0x1b;     // the message type CMsgDeath answers
constexpr int32_t kMonthEnd = 0xb;        // the message type that closes the six histories at the end of a month (the class that carries it is not known)

// The message centre the Mac builds on first use (src/BankAccount.cpp has the same helper).
CMessageCentre* MessageCentreForPost() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// The ride record of the ride's object type (a call each time: the Mac fetches it again for every field it reads).
RideInfo* InfoOf(const CObject& ride) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{ride.mObjectType});
}

int32_t ThingId(const CObject& ride) {
    return static_cast<int32_t>(ride.mBase.GetThingId().mIndex);
}
}  // namespace

// 0x1010aaac (192 bytes). Each question is a call, in this order, and the first bad answer stops the walk. A coaster also needs its track to be usable.
int32_t CObject::IsOpenable() {
    if (IsBrokenDown()) return 0;
    if (IsCondemned() != 0) return 0;
    if (IsFlaggedForMaintenance() != 0) return 0;
    if (RequiresUpgrade() != 0) return 0;
    if (IsBOQConnected() == 0) return 0;
    if (IsCoaster() != 0) {
        if (IsCoasterUseable(this) == 0) return 0;
    }
    return 1;
}

// 0x1010ab98 (8 bytes).
int32_t CObject::IsCloseable() { return 1; }

// 0x1010abcc (228 bytes). Says so, stops the ride, says who was being loaded and forgets him, sets the script's variable 6 and puts the hoardings on.
void CObject::CloseObject() {
    CTRACE(kTrace, kClosing, ThingId(*this));
    mF64 = 0;
    const uint16_t loaded = mPersonLoaded;
    CTRACE(kTrace, kPersonLoaded, ThingId(*this), static_cast<int32_t>(loaded));
    mPersonLoaded = 0;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 6, 1);
    ActivateHoardings(gRideAnimSets[mF1C], 1);
}

// 0x1010acdc (344 bytes). The Mac writes IsOpenable's calls out again (the same questions in the same order); when the ride is not openable it says so
// five times and opens it all the same. Then mF64 to 1, the hoardings off, the script's variable 6 to 0 and the state back to normal.
void CObject::Reopen() {
    if (IsOpenable() == 0) {
        for (int32_t i = 0; i < 5; ++i) CTRACE(kTrace, kNonOpenable);
    }
    mF64 = 1;
    DeActivateHoardings(gRideAnimSets[mF1C]);
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 6, 0);
    SetState(kStateNormal);
}

// 0x1010cb28 (76 bytes). Only a positive variable counts (the Mac builds it with the carry of a subtraction).
int32_t CObject::IsRunning() const {
    return RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 9) > 0 ? 1 : 0;
}

// 0x101083e0 (396 bytes). A choosable ride that is working, open, has a queue end and room in its queue (and, for a coaster, a usable track). The ride
// record is fetched twice at the end: a record whose WhichTrackType is 1 refuses the ride while mF28 is 0, and (the second fetch) so does a 2.
int32_t CObject::IsAvailableToChoose() {
    if ((mFlags & 4) == 0) return 0;
    if (IsBrokenDown()) return 0;
    if (IsClosed()) return 0;
    if (IsCondemned() != 0) return 0;
    if (GetBackOfQueue().mIndex == kNoMapCell) return 0;
    if (HasFreeQueueSpace() == 0) return 0;
    if (IsCoaster() != 0) {
        if (IsCoasterUseable(this) == 0) return 0;
    }
    if (InfoOf(*this)->Value(kRideSlot_WhichTrackType).mInteger == 1) {
        if (mF28 == 0) return 0;
    }
    if (InfoOf(*this)->Value(kRideSlot_WhichTrackType).mInteger == 2) {
        if (mF28 == 0) return 0;
    }
    return 1;
}

// 0x1010c214 (656 bytes). Asking (on is not 0): a choosable ride wants an upgrade (the state and the hoardings, flag 8); any other says it cannot (the
// first trace line is the Mac's maintenance text). Withdrawing: a ride that wanted one is opened again (Reopen, after the hoardings are taken off; the
// Mac writes Reopen out inline), any other says it never asked. Withdrawing always answers 1.
int32_t CObject::RequestUpgrade(int32_t on) {
    if (on != 0) {
        CTRACE(kSettingTrace, kWantsMaintenance, ThingId(*this));
        if ((mFlags & 4) != 0) {
            SetState(kStateWantsUpgrade);
            ActivateHoardings(gRideAnimSets[mF1C], 8);
            return 1;
        }
        CTRACE(kTrace, kCannotUpgrade, ThingId(*this));
        return 0;
    }
    if (RequiresUpgrade() != 0) {
        CTRACE(kTrace, kCancelling, ThingId(*this));
        DeActivateHoardings(gRideAnimSets[mF1C]);
        Reopen();
    } else {
        CTRACE(kTrace, kNeverRequested, ThingId(*this));
    }
    return 1;
}

// 0x1010d9f8 (16 bytes) and 0x1010da64 (12 bytes). The id goes by value in the HIGH halfword of a register; the answer through a hidden pointer.
void CObject::SetUpgradeParent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    mF2C = id.mIndex;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetUpgradeParent() const {
    return {mF2C};
}

// 0x1010a93c (200 bytes). A ride that is open tells the message centre (built first if the Mac has none; the allocation may fail and the Mac posts through
// the NULL then) and closes.
void CObject::NotifyExitBlocked() {
    if (IsClosed()) return;
    CMsgBlockedExit message(mBase.GetThingId());
    MessageCentreForPost()->PostMessage(message);
    CloseObject();
}

// 0x10107cd4 (56 bytes).
RideInfo* CObject::GetDBInfo() const {
    return InfoOf(*this);
}

// 0x10108068 (104 bytes).
void CObject::SetOperatingSpeed(uint32_t speed) {
    RSSE_SetRideSpeed(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), static_cast<int32_t>(speed));
    CTRACE(kSettingTrace, kSpeed, static_cast<int32_t>(speed));
    mF54 = speed;
}

// 0x10108104 (272 bytes). The byte is held between the record's minimum and maximum duration (signed compares; a bound is fetched from the record each time
// it is read: 2 fetches inside the range or below it, 3 above it) and masked to a byte again. Then the script's variable 3 and mF58.
void CObject::SetOperatingDuration(uint8_t duration) {
    int32_t value = duration;
    if (value >= InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger) {
        if (value > InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger) value = InfoOf(*this)->Value(kRideSlot_MaxDuration).mInteger;
    } else {
        value = InfoOf(*this)->Value(kRideSlot_MinDuration).mInteger;
    }
    value &= 0xff;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 3, value);
    CTRACE(kSettingTrace, kDuration, value);
    mF58 = static_cast<uint8_t>(value);
}

// 0x10108248 (356 bytes). The same with the record's capacity bounds, but only when their sum is above 0 (the sum wraps; the first fetch gives the maximum,
// the second the minimum; the clamp then fetches again: 4 fetches inside the range or below it, 5 above it, 2 when the sum is not above 0). Then the
// script's variable 2 and mF59.
void CObject::SetOperatingCapacity(uint8_t capacity) {
    const int32_t maximum = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
    const int32_t minimum = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
    int32_t value = capacity;
    if (static_cast<int32_t>(static_cast<uint32_t>(minimum) + static_cast<uint32_t>(maximum)) > 0) {
        if (value >= InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger) {
            if (value > InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger) value = InfoOf(*this)->Value(kRideSlot_MaxCapacity).mInteger;
        } else {
            value = InfoOf(*this)->Value(kRideSlot_MinCapacity).mInteger;
        }
    }
    value &= 0xff;
    RSSE_Setvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 2, value);
    CTRACE(kSettingTrace, kCapacity, value);
    mF59 = static_cast<uint8_t>(value);
}

// 0x10107ba0 (256 bytes). The type comes from the message's own GetType (a virtual call). A staff member's death (a CMsgDeath) takes him off the ride when he
// is the one assigned. The month end (type 0xb) closes the books of the six histories: each one's temp word is stored and then zeroed.
void CObject::ReceiveMessage(const CMsg& message) {
    const int32_t type = message.GetType();
    if (type == kStaffDeath) {
        if (static_cast<const CMsgDeath&>(message).mThingId == mF5A) {
            CTRACE(kTrace, kStaffGone);
            UnassignStaffMember();
        }
    } else if (type == kMonthEnd) {
        mHistory1A4.SetFromTemp();
        mHistory1A4.mTemp = 0;
        mHistory22C.SetFromTemp();
        mHistory22C.mTemp = 0;
        mHistory2B4.SetFromTemp();
        mHistory2B4.mTemp = 0;
        mHistory6C.SetFromTemp();
        mHistory6C.mTemp = 0;
        mHistoryF4.SetFromTemp();
        mHistoryF4.mTemp = 0;
        mHistory33C.SetFromTemp();
        mHistory33C.mTemp = 0;
    }
}
