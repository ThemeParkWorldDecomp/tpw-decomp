// CObject, main app, unit w210 (its test; translated in unit v5L21; the Mac files them under Object.cpp, in pieces): a
// ride's names, kind, excitement and stand point, and its final destroy. UpgradesWhat (0x1011050c, 72 bytes), GetControl (0x10110580, 72), GetFileName
// (0x1011062c, 56), the sign names GetUIName (0x10110690, 264), GetSignNameA (0x101107c4, 232), GetSignNameB (0x101108dc, 60), SetSignNameA (0x10110948, 412)
// and SetSignNameB (0x10110b10, 412), the track ride test IsTrackRide (0x10110d50, 200; IsCoaster is in src/gameplay_leaves.cpp, unit v5L07), the excitement level
// (GetProspectiveExcitementLevel 0x1010cba0, 976; GetExcitementLevel 0x1010cfb0, 252), the stand point (GetStandPoint 0x1010a4fc, 1,028) and the final destroy
// (FinalDestroy 0x10107828, 724). src/Object.cpp, src/Object_turn.cpp, src/Object_queries.cpp, src/Object_queue.cpp, src/Object_takings.cpp,
// src/Object_upkeep.cpp, src/Object_opening.cpp and src/Object_admit.cpp hold other CObject members. The companions are CMsgRideDeleted's constructor
// (src/ThingMessages2.cpp) and CRideDatabase::GetFileName (src/RideDB_names.cpp).
#include <cmath>
#include <cstdint>
#include <new>

#include "ActionRecorder.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "ObjectControl.h"
#include "RideDB.h"
#include "TPAdvisor.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "rsse.h"

// The two scratch buffers of the sign names (0x21 wide characters each in the original's data; the names are mine).
char16_t gObjectSignNameBuffer[0x21];   // 0x1034c2d8: GetSignNameA's
char16_t gObjectUINameBuffer[0x21];     // 0x1034c31a: GetUIName's

namespace {

// The ride record of the object's type; every use fetches it again, as the original does.
RideInfo* RecordOfType(const CObject* object) {
    return gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{object->mObjectType});
}

// A slot of the record moved up by the ride's upgrade level (16 slots a level).
int32_t LevelSlot(const RideInfo* info, RideBalanceSlot slot, uint8_t level) {
    return info->Value(static_cast<RideBalanceSlot>(static_cast<int32_t>(slot) + static_cast<int32_t>(level) * 16)).mInteger;
}

// The ride's own cell as a 1 based map id (the effectors and the action recorder's value).
uint16_t InfoCell(const CObject* object) {
    return static_cast<uint16_t>(object->mBase.mCellX + object->mBase.mCellY * 0x80 + 1);
}

// A speed or duration ratio held to [0.75, 1.25] (fcmpo with bge and ble: a NaN stays NaN).
float ClampRatio(float ratio) {
    if (ratio < 0.75) return 0.75f;
    if (ratio > 1.25) return 1.25f;
    return ratio;
}

// One stand point coordinate is inside (0, 1); the second compare reads a second fetch, made only when the first passes
// (a NaN fails the first).
bool StandSlotInside(const CObject* object, RideBalanceSlot slot) {
    if (!(RecordOfType(object)->Value(slot).mFloat < 1.0)) return false;
    return RecordOfType(object)->Value(slot).mFloat > 0.0;
}

}  // namespace

// The type the ride upgrades to (its record's UpgradesId).
tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> CObject::UpgradesWhat() {
    return tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(RecordOfType(this)->Value(kRideSlot_UpgradesId).mInteger)};
}

// The control block of the ride's type.
ObjectControl* CObject::GetControl() {
    return gWorld->GetControlManager()->GetObjectControl(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType}, 0);
}

// The record's item name of the ride's type.
const char16_t* CObject::GetFileName() const {
    return gRideDatabase.GetFileName(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{mObjectType});
}

// The ride's full name: both sign lines joined with a blank, or without when the first ends with a hyphen (the hyphen is
// dropped, and the line is cut to 31 characters first); one line alone as it is.
void CObject::GetUIName(char16_t* out, uint32_t size) {
    const uint32_t length = wcslen(mSignA);
    if (length == 0) {
        wcsncpy(out, mSignB, size);
    } else if (mSignA[length - 1] == u'-') {
        gObjectUINameBuffer[0x20] = 0;
        swprintf(gObjectUINameBuffer, 0x20, u"%ls", mSignA);
        gObjectUINameBuffer[length - 1] = 0;
        swprintf(out, size, u"%ls%ls", gObjectUINameBuffer, mSignB);
    } else if (wcslen(mSignB) == 0) {
        wcsncpy(out, mSignA, size);
    } else {
        swprintf(out, size, u"%ls %ls", mSignA, mSignB);
    }
}

// The first sign line; a line of two or more characters ending in two hyphens loses the last one.
void CObject::GetSignNameA(char16_t* out, uint32_t size) {
    const uint32_t length = wcslen(mSignA);
    if (length == 0) {
        swprintf(out, size, u"");
        return;
    }
    if (static_cast<int32_t>(length) > 1 && mSignA[length - 1] == u'-' && mSignA[length - 2] == u'-') {
        gObjectSignNameBuffer[0x20] = 0;
        swprintf(gObjectSignNameBuffer, 0x21, u"%ls", mSignA);
        gObjectSignNameBuffer[length - 1] = 0;
        swprintf(out, size, u"%ls", gObjectSignNameBuffer);
        return;
    }
    swprintf(out, size, u"%ls", mSignA);
}

void CObject::GetSignNameB(char16_t* out, uint32_t size) {
    swprintf(out, size, u"%ls", mSignB);
}

// A new first sign line. Outside the online game the gates' line is the park's too (the master save is created on first
// use). The action recorder gets the gates as 60000 and any other ride as its cell.
void CObject::SetSignNameA(const char16_t* text) {
    wcsncpy(mSignA, text, 0x21);
    if (TheGameType().mType != kGameTypeOnline && mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        if (gMasterSave == nullptr) gMasterSave = new (std::nothrow) CFSGlobalMasterSave;
        gMasterSave->SetSignNameA(text);
    }
    if (mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x50), 60000, static_cast<uint8_t>(wcslen(mSignA)), text);
    } else {
        const uint16_t cell = InfoCell(this);
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x50), cell, static_cast<uint8_t>(wcslen(mSignA)), text);
    }
}

// The second line, as SetSignNameA (action 0x55).
void CObject::SetSignNameB(const char16_t* text) {
    wcsncpy(mSignB, text, 0x21);
    if (TheGameType().mType != kGameTypeOnline && mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        if (gMasterSave == nullptr) gMasterSave = new (std::nothrow) CFSGlobalMasterSave;
        gMasterSave->SetSignNameB(text);
    }
    if (mBase.GetThingId().mIndex == gWorld->mGatesThing) {
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x55), 60000, static_cast<uint8_t>(wcslen(mSignB)), text);
    } else {
        const uint16_t cell = InfoCell(this);
        gActionRecorder.RecordAction(static_cast<RecordedAction>(0x55), cell, static_cast<uint8_t>(wcslen(mSignB)), text);
    }
}

// A ride on a track: the record's WhichTrackType is 1, 2 or 3 (each test fetches the record again).
int32_t CObject::IsTrackRide() const {
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 1) return 1;
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 2) return 1;
    if (RecordOfType(this)->Value(kRideSlot_WhichTrackType).mInteger == 3) return 1;
    return 0;
}

// The excitement a ride would have. A sideshow (a, b, c the prize value, the price and the chance of winning): 20 plus
// 0.8 of a tenth of c times the root of the prize's margin (held to 0..100), as a byte. A coaster: 50 plus half its
// second peep rating. Any other ride: the record's level (for a track ride 60 percent of it plus the weighted track
// sections, held to 0..40, the sum held to 0..100) times the speed and duration ratios to the level's initial ones, each
// held to [0.75, 1.25].
int32_t CObject::GetProspectiveExcitementLevel(int32_t a, int32_t b, int32_t c) const {
    if (RecordOfType(this)->mUIType == 2) {
        int32_t margin = a - b;
        if (margin < 0) margin = 0;
        else if (margin > 100) margin = 100;
        const double root = std::sqrt(static_cast<double>(margin));
        const float scaled = static_cast<float>((static_cast<double>(c) * root) / 10.0);
        const uint8_t level = static_cast<uint8_t>(ppc_fctiwz(0.8f * scaled));
        return static_cast<uint8_t>(level + 0x14);
    }
    if (IsCoaster() != 0) {
        const uint32_t handle = RSSE_GetCoasterHandle(static_cast<int32_t>(mF20));
        if (handle == 0) return 0;
        float first;
        float second;
        GetCoasterPeepStats(handle, &first, &second);
        return ppc_fctiwz(std::fmaf(second, 0.5f, 50.0f));
    }
    int32_t base = RecordOfType(this)->Value(kRideSlot_ExcitementLevel).mInteger;
    const int32_t speed = LevelSlot(RecordOfType(this), kRideSlot_Upgrades_InitSpeed, mF4C);
    const int32_t duration = LevelSlot(RecordOfType(this), kRideSlot_Upgrades_InitDuration, mF4C);
    if (mF24 != 0) {
        int32_t p = 0;
        int32_t q = 0;
        int32_t r = 0;
        int32_t s = 0;
        BMP_CountTrackSections(mF24, &p, &q, &r, &s);
        int32_t track = static_cast<int32_t>(static_cast<uint32_t>(r) * 3u + static_cast<uint32_t>(p) * 2u + static_cast<uint32_t>(q));
        if (track < 0) track = 0;
        else if (track > 40) track = 40;
        base = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(static_cast<uint32_t>(base) * 60u) / 100) +
                                    static_cast<uint32_t>(track));
        if (base < 0) base = 0;
        else if (base > 100) base = 100;
    }
    // Four more fetches whose answers are not used.
    RecordOfType(this);
    RecordOfType(this);
    RecordOfType(this);
    RecordOfType(this);
    const float speedRatio = ClampRatio(static_cast<float>(a) / static_cast<float>(speed));
    const float durationRatio = ClampRatio(static_cast<float>(b) / static_cast<float>(duration));
    const float ratio = speedRatio * durationRatio;
    return ppc_fctiwz(static_cast<float>(base) * ratio);
}

// The ride's excitement: with the flag the record's level as it is; else 0 for a level of 0, the sideshow's figure from its
// prize, price and chance of winning, or a ride's from its speed, duration and capacity (the word goes back as it is).
uint8_t CObject::GetExcitementLevel(bool flag) const {
    const int32_t level = RecordOfType(this)->Value(kRideSlot_ExcitementLevel).mInteger;
    if (flag) return static_cast<uint8_t>(level);
    if (level == 0) return 0;
    if (RecordOfType(this)->mUIType == 2) {
        const uint8_t chance = GetChanceOfWinning();
        const int32_t price = static_cast<int32_t>(GetPricePerUse());
        const int32_t prize = static_cast<int32_t>(GetPrizeValue());
        return static_cast<uint8_t>(GetProspectiveExcitementLevel(prize, price, chance));
    }
    return static_cast<uint8_t>(GetProspectiveExcitementLevel(static_cast<int32_t>(mF54), mF58, mF59));
}

// Where a guest stands at the entry (which 0) or appears at the exit: the cell's column and row, and the record's offsets
// inside the cell (each asserted to be inside (0, 1)) turned by the ride's rotation, in 255ths.
void CObject::GetStandPoint(int32_t which, MapPos* x, MapPos* y) {
    const uint16_t cell = which == 0 ? mF32 : mF34;
    const RideBalanceSlot slotX = which == 0 ? kRideSlot_EntryCellStandPosX : kRideSlot_ExitCellAppearPosX;
    const RideBalanceSlot slotY = which == 0 ? kRideSlot_EntryCellStandPosY : kRideSlot_ExitCellAppearPosY;
    x->mCell = static_cast<uint8_t>((cell - 1) & 0x7f);
    y->mCell = static_cast<uint8_t>(static_cast<uint32_t>(cell - 1) >> 7);
    GAME_ASSERTMSG(StandSlotInside(this, slotX) ? 1 : 0,
                   which == 0 ? "Dodgy X stand point in SAM file for ride %d\n" : "Dodgy X exit point in SAM file for ride %d\n",
                   static_cast<int32_t>(mObjectType));
    GAME_ASSERTMSG(StandSlotInside(this, slotY) ? 1 : 0,
                   which == 0 ? "Dodgy Y stand point in SAM file for ride %d\n" : "Dodgy Y exit point in SAM file for ride %d\n",
                   static_cast<int32_t>(mObjectType));
    const float fx = RecordOfType(this)->Value(slotX).mFloat;
    const float fy = RecordOfType(this)->Value(slotY).mFloat;
    // Another rotation leaves the Mac's f30 and f31 as its caller had them; the host uses 0.
    float ox = 0.0f;
    float oy = 0.0f;
    switch (mRotation) {
        case 0: ox = fx; oy = fy; break;
        case 90: ox = fy; oy = 1.0f - fx; break;
        case 180: ox = 1.0f - fx; oy = 1.0f - fy; break;
        case 270: ox = 1.0f - fy; oy = fx; break;
        default: break;
    }
    x->mOffset = static_cast<uint8_t>(ppc_fctiwz(255.0 * static_cast<double>(ox)));
    y->mOffset = static_cast<uint8_t>(ppc_fctiwz(255.0 * static_cast<double>(oy)));
}

// The ride leaves the park: one fewer of its type, unregistered, the staff told (CMsgRideDeleted), its residual value
// refunded, its effectors removed, its scripts killed (the ride's own one quietly while the world is shutting down) and
// the UI's selection cleared if it is the ride; then the thing's own destroy.
void CObject::FinalDestroy() {
    GetControl()->mNumberInPark -= 1;
    gWorld->Unregister(this);
    CMsgRideDeleted message(mBase.GetThingId());
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    gMessageCentre->PostMessage(message);
    const uint32_t value = GetCurrentResidualValue();
    gWorld->GetBankAccount()->MakeDeposit(value);
    if ((mFlags & 0x10) != 0) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorSecurity, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
    }
    if ((mFlags & 0x80) != 0 && RSSE_Getvar(RSSE_GetScriptPointer(static_cast<int32_t>(mF20)), 0) < 2) {
        gWorldMap->RemoveRegionalEffector(kRegionEffectorFireworks, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
    }
    if ((mFlags & 1) != 0) {
        if (IsDirty() != 0) {
            gWorldMap->RemoveRegionalEffector(static_cast<RegionEffector>(6), tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
        } else {
            gWorldMap->RemoveRegionalEffector(kRegionEffectorRelief, tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{InfoCell(this)});
        }
    }
    if (mF50 != 0) SCR_KillScript(static_cast<int32_t>(mF50));
    if (gWorld->mF1DA738 == 2) RSSE_KillScript(static_cast<int32_t>(mF20), 0);
    else RSSE_KillScript(static_cast<int32_t>(mF20), gActionRecorder.mRecording != 0 ? 4 : 7);
    if (gUIInterface.GetSelectedObject() == this) gUIInterface.SetSelectedObject(nullptr);
    mBase.FinalDestroy();
}
