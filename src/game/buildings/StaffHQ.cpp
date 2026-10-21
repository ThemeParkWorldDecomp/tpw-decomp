// StaffHQ.cpp: name proven by __sinit_StaffHQ_cpp in the main application.

#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "helpers/ThingHelpers.h"
#include "MapCell.h"
#include "ParkMessages.h"
#include "ParkTime.h"
#include "ppc_math.h"
#include "RegionEffectCell.h"
#include "Serialise.h"
#include "tb_rain.h"
#include "TbDynamicStringTemplate.h"
#include "TbTimeDiff.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// The constructors of the things a new level makes (unit w122a, main app): CThing::CThing (Thing.cpp) and the constructors of CStaffHQ,
// CMechanicHQ, CAdvisor, CTagSystem, CResearchLab, CWeather and CUIMsgReceiver. The six leaf constructors they build their parts with
// (CTrainingCentre, CWeatherForecast, CStaffHQ::StrikeType, AdvisorHistory (host: AdvisorMessageState, written in CAdvisor's loop),
// CResearchProject, CResearchCategory) are inline in include/ThingParts.h and include/ThingReceivers.h.
//
// Every derived constructor builds its CThing first (the host holds it as the first member, which is constructed before the others) with
// map cell id 1 and the model of its class, then its own fields, then subscribes to the message centre. The Mac's constructors leave most
// bytes of the object alone (a new object keeps whatever the allocator gave); so do these, and no member has an initialiser.

// 0x1012a7fc (304 bytes, StaffHQ.cpp), model 9. mTraining (0x1012ffb0) and mStrikes (0x1012a950) are built by their own constructors.
CStaffHQ::CStaffHQ() : mBase(FirstCell(), static_cast<CThing::ThingModel>(9)) {
    mF60 = 0;
    mF64 = 0;
    Subscribe(mBase, 0xc);
    Subscribe(mBase, 0xe);
}

// Unit v51052: small functions of the staff HQ, tag system, thoughts, UI receiver, research lab message, Vector2d and the steering behaviours.
// The destructors are the Mac's deleting forms (the flag rides in r4): free functions here, because the host classes have no matching
// destructors. The vtable stores of the originals have no host counterpart and are not written.

// ---- globals the static initialisers store (names invented)
uint32_t gStaffHQSinitHigh;         // 0x1034c6e8
uint32_t gStaffHQSinitLow;          // 0x1034c6ec
void CStaffHQ::FinalDestroy() { mBase.FinalDestroy(); }

// Unit v5918: CStaffHQ::Serialise 0x1012ac08, ReceiveMessage 0x1012a9e8, UpdateMonthly 0x1012ae40, FindNearestAvailableGuard 0x1012be78.

// Halfwords of initialised data (all zero at start; names invented). The monthly check compares the world's staff list heads with
// them (index = staff type), ReceiveMessage compares the guard found with gStaffHQNoGuard, the guard walk ends at gStaffHQGuardListEnd.
uint16_t gStaffHQNoGuard;          // 0x102a11bc
uint16_t gStaffHQNoStaff[5];       // 0x102a11be, 0x102a11c0, 0x102a11c2, 0x102a11c4, 0x102a11c6
uint16_t gStaffHQGuardListEnd;     // 0x102a11dc

namespace {
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";                                     // 0x10245ff2
const char kAnySecurity[] = "Any security level will do, no probability yet - later, make prob. of detection prop. to sec. level\n";   // +0x43
const char kNoGuards[] = "No guards available to deal with pranker, he's got away with it!\n";                                      // +0xa8
const char kSiccing[] = "Siccing a guard on this pranker\n";                                                                      // +0xea
const char kNoSecurity[] = "There's no security level on this cell, this kid has got away with it!\n";                             // +0x10b
const char kParkClosed[] = "The park is closed, so I'm not considering striking\n";                                                // +0x185
const char kForceStrike[] = "mForceStrike";                                                                                       // +0x153
const char kPickedUp[] = "mStaffMemberPickedUp";                                                                                  // +0x160
const char kStrikeLevel[] = "mStrikeLevel[i]";                                                                                    // +0x175

typedef tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ThingIdTag;
typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> MapIdTag;

// (x + y * 128 + 1) as the 16 bit map cell id of a thing (its bytes at +4 and +6)
uint16_t CellOfThing(const CThing* thing) {
    return static_cast<uint16_t>(thing->mCellX + (static_cast<uint16_t>(thing->mCellY * 0x80) + 1));
}
}  // namespace

// 0x1012a9e8 (492 bytes). Message type 12 is the monthly update, 14 a prank (the perp's thing id at +8); the rest are ignored.
void CStaffHQ::ReceiveMessage(const CMsg& message) {
    int32_t type = message.GetType();
    if (type == 13) return;
    if (type < 13) {
        if (type >= 12) UpdateMonthly();
        return;
    }
    if (type >= 15) return;
    uint16_t perp = static_cast<const CMsgStaffHQPrank&>(message).mPerp;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    MapIdTag cell;
    cell.mIndex = CellOfThing(static_cast<const CThing*>(gThingArray[perp].mThing));
    CRegionEffectCell* effect = gWorldMap->GetRegionalEffectCell(cell);
    if (effect->mpEffect[3] <= 0) {
        CTRACE(2, kNoSecurity);
        return;
    }
    CTRACE(2, kAnySecurity);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    MapIdTag where;
    where.mIndex = CellOfThing(static_cast<const CThing*>(gThingArray[perp].mThing));
    ThingIdTag guard = FindNearestAvailableGuard(where);
    if (guard.mIndex == gStaffHQNoGuard) {
        CTRACE(2, kNoGuards);
        return;
    }
    CTRACE(2, kSiccing);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    ThingIdTag perpTag;
    perpTag.mIndex = perp;
    static_cast<CGuard*>(gThingArray[guard.mIndex].mThing)->ProsecutePerp(perpTag);
}

// 0x1012ac08 (520 bytes)
int32_t CStaffHQ::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    if (mBase.Serialise(file, save, version, unused) != 0) return 1;
    if (save != 0) {
        if (version >= 2 || version < 0) return 0;
        if (SAVEVAR(file, reinterpret_cast<int32_t*>(&mF64), kForceStrike, 0) != 0) return 1;
        if (SAVEVARSTRUCT(file, reinterpret_cast<ThingIdTag*>(&mF60), kPickedUp, 0) != 0) return 1;
        for (int32_t i = 0; i < 5; ++i)
            if (SAVEVARSTRUCT(file, &mStrikes[i], kStrikeLevel, 0) != 0) return 1;
        if (mTraining.Serialise(file, save, version, unused) != 0) return 1;
    } else {
        if (version >= 2 || version < 0) return 0;
        if (LOADVAR(file, reinterpret_cast<int32_t*>(&mF64), kForceStrike, 0) != 0) return 1;
        if (LOADVARSTRUCT(file, reinterpret_cast<ThingIdTag*>(&mF60), kPickedUp, 0) != 0) return 1;
        for (int32_t i = 0; i < 5; ++i)
            if (LOADVARSTRUCT(file, &mStrikes[i], kStrikeLevel, 0) != 0) return 1;
        if (mTraining.Serialise(file, save, version, unused) != 0) return 1;
    }
    return 0;
}

// 0x1012ae40 (532 bytes): the park analyser is only asked when a strike is not forced
void CStaffHQ::UpdateMonthly() {
    mTraining.UpdateMonthly();
    if (mF64 == 0) {
        if (gWorld->IsParkClosed() != 0 || gWorld->GetParkAnalyser()->GetVisitorsInPark(5) != 0) {
            CTRACE(2, kParkClosed);
            return;
        }
    }
    for (int32_t i = 0; i < 5; ++i) {
        uint16_t head;
        switch (i) {
            case 0: head = gWorld->mF1DA73C; break;
            case 1: head = gWorld->mF1DA73E; break;
            case 2: head = gWorld->mF1DA740; break;
            case 3: head = gWorld->mF1DA744; break;
            default: head = gWorld->mF1DA742; break;
        }
        if (head == gStaffHQNoStaff[i]) continue;
        if (mStrikes[i].mF08 == gWorld->mTurn) continue;
        mStrikes[i].mF08 = gWorld->mTurn;
        if (IsStriking(static_cast<StaffType>(i)) != 0)
            StopStrikeInProgress(static_cast<StaffType>(i));
        else
            ConsiderStriking(static_cast<StaffType>(i));
    }
}

// CStaffHQ::ConsiderStriking (unit v5681), 0x1012b084.

// For the program; the test library defines its own (recording) versions, which win over these weak ones.
__attribute__((weak)) void* staffhq_callee::OperatorNew(uint32_t size) { return ::operator new(size); }
__attribute__((weak)) void staffhq_callee::ConstructSubscriptions(void* mem) { new (mem) CMessageCentre; }

namespace {

// The five copies of the original's "create the message centre if there is none, then post".
void PostStaffInfo(StaffType type, StaffSituation situation) {
    CMsgStaffInfo message(type, situation);
    if (gMessageCentre == nullptr) {
        void* mem = staffhq_callee::OperatorNew(0x1d0);
        if (mem != nullptr) staffhq_callee::ConstructSubscriptions(mem);
        gMessageCentre = static_cast<CMessageCentre*>(mem);
    }
    gMessageCentre->PostMessage(message);
}

}  // namespace

void CStaffHQ::ConsiderStriking(StaffType type) {
    CTRACE(2, "Considering striking on turn %d\n", gWorld->mTurn);
    TbTimeDiff since = gGameWorldRef->mParkTime.GetFunnyTimeSinceSessionStart();
    // 0x10237e1c: signed 64 bit division by 30 days of 100 ns ticks; only the low word of the quotient is compared.
    int64_t months = since.mTicks / 0x1792F8648000LL;
    if (static_cast<int32_t>(months) < 0x18) return;

    StrikeType& strike = mStrikes[type];
    if (StaffTypeHasAGrievance(type) == 0) {
        if (strike.mF00 == 1) PostStaffInfo(type, static_cast<StaffSituation>(3));
        strike.mF00 = 0;
        StopStrikeInProgress(type);
        return;
    }
    switch (static_cast<int32_t>(strike.mF00)) {
    case 0:
        CTRACE(0xc, "** Your staff have issued a strike warning!!! Clean your park up!! **\n");
        strike.mF00 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(0));
        break;
    case 1:
        CTRACE(0xc, "** Your staff are going on strike for one week!! **\n");
        strike.mF00 = 2;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 2:
        CTRACE(0xc, "** Your staff are going on strike for two weeks!! **\n");
        strike.mF00 = 3;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 3:
        CTRACE(0xc, "** Your staff are going on strike for a month!! **\n");
        strike.mF00 = 4;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    case 4:
        strike.mF00 = 4;
        strike.mF04 = 1;
        PostStaffInfo(type, static_cast<StaffSituation>(2));
        break;
    default:
        break;
    }
}

// The park analyser's three messages (main app, unit w161): the constructors 0x1015210c (CMsgAddToChallenge), 0x10151cfc (CMsgResearchCompleted) and
// 0x10151f14 (CMsgStaffInfo), the GetType functions 0x100ef1f4, 0x101244d0 and 0x1012c244, the destructors 0x100e4584, 0x1012412c and 0x1012b4f4, and
// CBankAccount::GetBalance 0x100f32b4. They live together because the native vtable needs a key function (GetType); the pattern is src/ThingMessages.cpp's.

CMsgStaffInfo::~CMsgStaffInfo() {}

// 0x102a11c8 .. 0x102a11da (names invented): the id that ends each staff list the grievance check walks (all zero at start).
uint16_t gStaffHQGrievanceEnd[10];

namespace {

const char kAssertText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";  // 0x10245ff2
const char kFatigue[] = "%ss are striking through fatigue\n";                                       // +0x2c1
const char kType[] = "##Type";                                                                      // +0x2e3
const char kUnhappy[] = "%ss are striking through unhappiness\n";                                   // +0x2ea
const char kUnknown[] = "Unknown staff type in StaffTypeHasAGrievance\n";                           // +0x310

// Walks the list from head (linked by the halfword at +0x20c) and averages the float at `offset` as the low byte of its
// truncated value. Answers 1 when the list has more than three members and the average is below 15.
int32_t Walk(uint16_t head, uint16_t end, uint32_t offset, const char* text) {
    uint32_t count = 0;
    uint32_t sum = 0;
    for (uint16_t cur = head; cur != end;) {
        count++;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAssertText);
        uint8_t* thing = reinterpret_cast<uint8_t*>(gThingArray[cur].mThing);
        float value = *reinterpret_cast<float*>(thing + offset);
        uint16_t next = *reinterpret_cast<uint16_t*>(thing + 0x20c);
        sum += static_cast<uint32_t>(ppc_fctiwz(value)) & 0xff;
        cur = next;
    }
    if (count > 3 && sum / count < 15) {
        CTRACE(2, text, kType);
        return 1;
    }
    return 0;
}

}  // namespace

int32_t CStaffHQ::StaffTypeHasAGrievance(StaffType type) {
    if (mF64 != 0) return 1;
    int32_t t = static_cast<int32_t>(type);
    uint16_t head;
    int32_t slot;
    switch (t) {
    case 0: {
        CParkAnalyser* analyser = gWorld->GetParkAnalyser();
        float walkable = static_cast<float>(analyser->GetNumWalkableCells(1));
        float littered = static_cast<float>(analyser->GetNumLitteredCells(1));
        if (littered / walkable > 0.2f) return 1;
        head = gWorld->mF1DA73C;
        slot = 0;
        break;
    }
    case 1: head = gWorld->mF1DA73E; slot = 2; break;
    case 2: head = gWorld->mF1DA740; slot = 4; break;
    case 3: head = gWorld->mF1DA744; slot = 6; break;
    case 4: head = gWorld->mF1DA742; slot = 8; break;
    default:
        GAME_ASSERTMSG(0, kUnknown);
        return 0;
    }
    if (Walk(head, gStaffHQGrievanceEnd[slot], 0x1f8, kFatigue)) return 1;
    if (Walk(head, gStaffHQGrievanceEnd[slot + 1], 0x1f4, kUnhappy)) return 1;
    return 0;
}

// 0x1012bde0
int32_t CStaffHQ::IsStriking(StaffType type) { return static_cast<int32_t>(mStrikes[static_cast<int32_t>(type)].mF04); }

// 0x1012be24: element at 0x24 + 0xc * type, second word.
void CStaffHQ::StopStrikeInProgress(StaffType type) { mStrikes[static_cast<int32_t>(type)].mF04 = 0; }

// 0x1012be78 (368 bytes; the result goes through a hidden pointer). The cell argument is never read: the distance is measured from
// this object's own cell bytes (+4 and +6), as the listing does.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CStaffHQ::FindNearestAvailableGuard(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    (void)cell;
    uint32_t best = 0xffffffffu;
    uint16_t current = gWorld->mF1DA744;
    uint16_t found = 0;
    while (current != gStaffHQGuardListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CGuard* guard = static_cast<CGuard*>(gThingArray[current].mThing);
        uint32_t g = static_cast<uint32_t>(CellOfThing(&guard->mThing)) - 1u;
        uint32_t m = static_cast<uint32_t>(CellOfThing(&mBase)) - 1u;
        int32_t dx = static_cast<int32_t>(labs(static_cast<int32_t>((g & 0x7f) - (m & 0x7f))));
        int32_t dy = static_cast<int32_t>(labs(static_cast<int32_t>((g >> 7) - (m >> 7))));
        uint32_t dist = static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) + static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy);
        bool available = guard->IsAvailable();
        if (available && dist < best) {
            found = current;
            best = dist;
        }
        current = guard->mNext.mIndex;
    }
    ThingIdTag result;
    result.mIndex = found;
    return result;
}

// CStaffHQ::DropStaff 0x1012c168 and CStaffHQ::ForceStrike 0x1012c04c (unit v5037).

void CStaffHQ::ForceStrike(StaffType type) {
    if (mF64 == 0 || static_cast<int32_t>(mStrikes[static_cast<int32_t>(type)].mF00) <= 1) {
        CTRACE(2, "Forcing strike\n");   // 0x1024634a
        mF64 = 1;
    } else {
        CTRACE(2, "Turning off force strike\n");   // 0x10246330
        mF64 = 0;
    }
}

// 0x1012c100: the id is the high halfword of r4, stored through the stack and reloaded.
void CStaffHQ::PickUpStaff(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff) { mF60 = staff.mIndex; }

void CStaffHQ::DropStaff(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> cell) {
    uint16_t row = mF60;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    static_cast<CStaff*>(gThingArray[row].mThing)->DropMe(cell);
    mF60 = 0;
}

int32_t CMsgStaffInfo::GetType() const { return 0x17; }

void __sinit_StaffHQ_cpp() {
    gStaffHQSinitLow = 0;
    gStaffHQSinitHigh = 0x10000;
}

// Unit v51002: the save-file instances for CStaffHQ::StrikeType (SAVEVARSTRUCT 0x1012c3d4, LOADVARSTRUCT 0x1012c27c) and CPerson::PersonState
// (SAVEVAR 0x1011b90c), and CPerson::PayPricePerUseForObject (0x1011a4ac). LOADVAR<ResearchCat> (0x10124554) is in
// src/ResearchLab_projects_CResearchProject_Serialise.cpp.

// A strike record is three words; the first is saved as it is, the other two are byte-reversed (the file is little-endian).
static void SwapForFile(CStaffHQ::StrikeType& value) {
    value.mF04 = serialise::SwapWord(value.mF04);
    value.mF08 = serialise::SwapWord(value.mF08);
}

static void UnswapForFile(CStaffHQ::StrikeType& value) {
    value.mF04 = serialise::UnswapWord(value.mF04);
    value.mF08 = serialise::UnswapWord(value.mF08);
}

template<> int32_t SAVEVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {   // 0x1012c3d4
    return serialise::SaveStruct(file, value, name, noLog);
}

template<> int32_t LOADVARSTRUCT<CStaffHQ::StrikeType>(void* file, CStaffHQ::StrikeType* value, const char* name, int32_t noLog) {   // 0x1012c27c
    return serialise::LoadStruct(file, value, name, noLog);
}
