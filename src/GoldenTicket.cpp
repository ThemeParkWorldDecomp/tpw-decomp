// CGoldenTicketControl (main app; unit w180): the ticket counts, the spending of a ticket on a ride, the checks that earn the local, global and
// secret tickets, the three EarnedGoldenTicket and TellIfFirst. The master save (gMasterSave) is made on first use, and the original repeats that
// test before EVERY call into it, so the helper below is called at each place the listing tests it. The three TellTheWorld overloads are in src/GoldenTicket_tell.cpp (unit w183).
#include <new>

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "MainBalance.h"
#include "MsgAdvisor.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

namespace {

// The value slots of gMainBalance (MainBalance.h 30) the checks read: slot = (Mac offset - 8) / 4. The names are the description rows' (group
// names precede their members).
constexpr int kSlotVisitors = 466;                     // GoldenTicketLocal.Visitors +0x750
constexpr int kSlotPeopleInPark = 467;                 // .PeopleInPark +0x754
constexpr int kSlotHappiness = 468;                    // .Happiness +0x758
constexpr int kSlotAtLeastThisManyHappyPeople = 469;   // .AtLeastThisManyHappyPeople +0x75c
constexpr int kSlotProfitYear = 470;                   // .ProfitYear +0x760
constexpr int kSlotRecentVisitors = 471;               // .RecentVisitors +0x764
constexpr int kSlotRecentVisitorMonths = 472;          // .RecentVisitorMonths +0x768
constexpr int kSlotCoasterHeight = 473;                // GoldenTicketGlobal.CoasterHeight +0x76c
constexpr int kSlotGokartExcitement = 474;             // .GokartExcitement +0x770
constexpr int kSlotWaterLength = 475;                  // .WaterLength +0x774
constexpr int kSlotMinCellsCovered = 477;              // .MinCellsCovered +0x77c (MinCellsOwned, 476, is not read here)

constexpr int64_t kMonthTicks = 25920000000000LL;   // 0x1792f8648000: thirty days of 100 ns ticks

// The master save, made on the first use the way src/GameSys.cpp does (operator new 0x4c, the constructor 0x101694dc).
CFSGlobalMasterSave* TicketMasterSave() {
    if (gMasterSave == nullptr) gMasterSave = new CFSGlobalMasterSave;
    return gMasterSave;
}

// The message centre, made on the first use the way src/BankAccount.cpp does (0x1d0 bytes, the array of 0x1d subscription headers).
CMessageCentre* TicketMessageCentre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

int32_t Slot(int slot) { return gMainBalance.mValues[slot].mInteger; }

// 0x10237e1c: the signed 64 bit division of the span by thirty days; the Mac keeps the low word.
int32_t MonthsOf(const TbTimeDiff& span) { return static_cast<int32_t>(span.mTicks / kMonthTicks); }

}  // namespace

// 0x100fbdf8
int32_t CGoldenTicketControl::HowManyTickets() const { return TicketMasterSave()->HowManyTickets(); }

// 0x100fbea0: the cost is read first (the ride record is not checked for NULL), then the game type object is made on first use. The online game
// (type 1) spends nothing and answers true; otherwise CanIAfford (which reads the record again) decides.
bool CGoldenTicketControl::SpendTicketOn(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) {
    const int32_t cost = gRideDatabase.GetRideInfo(type)->Value(kRideSlot_GoldenTicketCost).mInteger;
    if (TheGameType().mType == 1) return true;
    if (!CanIAfford(type)) return false;
    return TicketMasterSave()->SpendTicketOn(type.mIndex, cost);
}

// 0x100fbfec
bool CGoldenTicketControl::HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    return TicketMasterSave()->HasBeenBought(type.mIndex);
}

// 0x100fc0c8: the cost is read before the master save test; a signed compare (the cost may not exceed the spending power).
bool CGoldenTicketControl::CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    const int32_t cost = gRideDatabase.GetRideInfo(type)->Value(kRideSlot_GoldenTicketCost).mInteger;
    return !(cost > TicketMasterSave()->SpendingPower());
}

// 0x100fc1d0: six blocks, each earned when the ticket is not held yet and its value is GREATER than the balance slot (all signed int32). The
// analyser is asked first; gWorld is read again for the bank account and the park time.
void CGoldenTicketControl::CheckForLocal() {
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(0)) && analyser->NumberOfVisitorsSinceBeginning() > Slot(kSlotVisitors))
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(0));
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(1)) && analyser->NumberOfVisitorsInPark() > Slot(kSlotPeopleInPark))
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(1));
    // The integer slot becomes a single (fsubs against the 2^52 + 2^31 double), the compare is a float one (false for a NaN); the people count is
    // asked only when the happiness passed.
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(2)) &&
        analyser->GetHappinessLevel(0) > static_cast<float>(Slot(kSlotHappiness)) &&
        analyser->NumberOfVisitorsInPark() > Slot(kSlotAtLeastThisManyHappyPeople))
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(2));
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(3)) && analyser->AllObjectsResearchedAndPresent())
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(3));
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(4)) && gWorld->GetBankAccount()->GetProfitThisYear() > Slot(kSlotProfitYear))
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(4));
    // The months are asked of the analyser with the slot, then (only if the visitors passed) the park time's span since the session start is
    // divided into months; the slot is read again for the compare.
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeLocal>(5)) &&
        analyser->NumberOfVisitorsInLastMonths(Slot(kSlotRecentVisitorMonths)) > Slot(kSlotRecentVisitors) &&
        MonthsOf(gWorld->mParkTime.GetFunnyTimeSinceSessionStart()) > Slot(kSlotRecentVisitorMonths))
        EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(5));
}

// 0x100fc56c: no master save test here (the global EarnedTicket checks); the score is the value compared. The four calls share one thing id the
// analyser writes (the object type 0 rides in the high half of the word passed to GetHighestCoaster). The coaster height and the cells are
// compared as signed words although the coaster answer is declared unsigned; the gokart answer is a byte.
void CGoldenTicketControl::CheckForGlobal() {
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing;
    int32_t value = static_cast<int32_t>(analyser->GetHighestCoaster(thing, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{0}));
    if (value > Slot(kSlotCoasterHeight)) EarnedGoldenTicket(static_cast<GoldenTicketTypeGlobal>(0), value);
    value = analyser->GetMostExcitingGokart(thing);
    if (value > Slot(kSlotGokartExcitement)) EarnedGoldenTicket(static_cast<GoldenTicketTypeGlobal>(1), value);
    value = analyser->GetLongestTrackRide(thing, 2);
    if (value > Slot(kSlotWaterLength)) EarnedGoldenTicket(static_cast<GoldenTicketTypeGlobal>(2), value);
    value = analyser->NumberOfCellsCoveredByObjects();
    if (value > Slot(kSlotMinCellsCovered)) EarnedGoldenTicket(static_cast<GoldenTicketTypeGlobal>(3), value);
}

// 0x100fc6c4: the analyser first, then the ticket test; earned when exactly 100 percent of the park is covered by cameras.
void CGoldenTicketControl::CheckForSecret() {
    CParkAnalyser* analyser = gWorld->GetParkAnalyser();
    if (!TicketMasterSave()->HaveTicket(static_cast<GoldenTicketTypeSecret>(0)) && analyser->PercentageOfParkCoveredByCameras() == 100)
        EarnedGoldenTicket(static_cast<GoldenTicketTypeSecret>(0));
}

// 0x100fc7c4, 0x100fc918 and 0x100fca5c: the save's answer (0 when already won) is passed on to TellTheWorld, then the total is traced.
bool CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeLocal type) {
    const int32_t won = TicketMasterSave()->EarnedTicket(type);
    if (won == 0) return false;
    TellTheWorld(type, static_cast<GoldenTicketTypeWon>(won));
    CTRACE(3, "You have %d total golden tickets\n", TicketMasterSave()->HowManyTickets());
    return true;
}

bool CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeGlobal type, int32_t score) {
    const int32_t won = TicketMasterSave()->EarnedTicket(type, score);
    if (won == 0) return false;
    TellTheWorld(type, static_cast<GoldenTicketTypeWon>(won));
    CTRACE(3, "You have %d total golden tickets\n", TicketMasterSave()->HowManyTickets());
    return true;
}

bool CGoldenTicketControl::EarnedGoldenTicket(GoldenTicketTypeSecret type) {
    const int32_t won = TicketMasterSave()->EarnedTicket(type);
    if (won == 0) return false;
    TellTheWorld(type, static_cast<GoldenTicketTypeWon>(won));
    CTRACE(3, "You have %d total golden tickets\n", TicketMasterSave()->HowManyTickets());
    return true;
}

// 0x100fd7f0: with exactly one ticket held (secrets included) the advisor says two things. Each message is a request (thing id 0, secondary 0,
// response -1, not override-only-once, score -1) wrapped in a CMsgAdvisor, posted to the message centre (made on first use at each of the two
// places), then dropped: the Mac's inlined destructor is the one store of CMsgAdvisor's vtable (0x102a09a4), never used by the host.
void CGoldenTicketControl::TellIfFirst() {
    if (TicketMasterSave()->HowManyTicketsIncludingSecrets() != 1) return;
    {
        const CAdvisorRequest request(static_cast<AdvisorMessage>(0x137), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1);
        CMsgAdvisor message(request);
        TicketMessageCentre()->PostMessage(message);
    }
    {
        const CAdvisorRequest request(static_cast<AdvisorMessage>(0x13b), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, -1, false, -1);
        CMsgAdvisor message(request);
        TicketMessageCentre()->PostMessage(message);
    }
}
