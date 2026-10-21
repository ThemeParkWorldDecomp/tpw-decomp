// GoldenTickets.cpp: name proven by __sinit_GoldenTickets_cpp in the main application.

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "CMessageCentre.h"
#include "CVariableTime.h"
#include "Debug.h"
#include "fixmath.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "helpers/GoldenTicketsHelpers.h"
#include "HLSprite.h"
#include "keytable_actions.h"
#include "lobby.h"
#include "m3d2.h"
#include "MainBalance.h"
#include "MsgAdvisor.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "platform/macdoze.h"
#include "RideDB.h"
#include "rsse.h"
#include "TbDynamicStringTemplate.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ui_main.h"
#include "UIInterface.h"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace {
Fixed saturate(int64_t v) {
    if (v > INT32_MAX) return INT32_MAX;
    if (v < INT32_MIN) return INT32_MIN;
    return static_cast<Fixed>(v);
}
}  // namespace

Fixed FixMul(Fixed a, Fixed b) {
    int64_t p = static_cast<int64_t>(a) * static_cast<int64_t>(b);
    // round to nearest (half away from zero), then drop 16 fraction bits
    p += (p >= 0) ? 0x8000 : -0x8000;
    return saturate(p >> 16);
}

Fixed FixDiv(Fixed a, Fixed b) {
    if (b == 0) return (a >= 0) ? INT32_MAX : INT32_MIN;
    int64_t n = static_cast<int64_t>(a) << 16;
    int64_t q = n / b;
    int64_t r = n % b;
    // round to nearest, half away from zero
    if (std::llabs(r) * 2 >= std::llabs(static_cast<int64_t>(b))) q += ((n < 0) != (b < 0)) ? -1 : 1;
    return saturate(q);
}

uint32_t isqrt(uint32_t x) {
    double s = std::sqrt(static_cast<double>(x));
    if (s <= 0.0) return 0;
    if (s >= 4294967296.0) return 0xFFFFFFFFu;
    return static_cast<uint32_t>(s);   // truncation, as fctiwz
}

// CGoldenTicketControl::CheckForTickets (main app, 0x100fbd2c, 144 bytes; unit w129).

// The game type object is a function-local static of an inline accessor (TheGameType, include/FS_Online.h): the guard byte at 0x102b3aa4 is tested as a
// signed byte and CGameType's constructor (0x1016f5d8) runs on first use. Only the normal game (type 0, compared after the construction) checks the local,
// global and secret tickets, in that order.
void CGoldenTicketControl::CheckForTickets() {
    if (TheGameType().mType == 0) {
        CheckForLocal();
        CheckForGlobal();
        CheckForSecret();
    }
}

// CGoldenTicketControl::TellTheWorld (main app; unit w183): the three overloads (local 0x100fcbb0, global 0x100fcf1c, secret 0x100fd4f4) that
// announce a won golden ticket. Each traces what was won, then (by what EarnedTicket answered) posts one CMsgAdvisor to the message centre, and
// always ends with TellIfFirst. Kept out of src/GoldenTicket.cpp, whose test fakes the three.

namespace {

// The message centre, made on the first use at each post (0x1d0 bytes, the array of 0x1d subscription headers), as src/GoldenTicket.cpp does.
CMessageCentre* TicketMessageCentre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// One announcement: a request (thing id 0, secondary 0, the response number, not override-only-once, score -1) wrapped in a CMsgAdvisor and
// posted; the Mac's inlined destructor is the one store of the vtable. `id` is the AdvisorMessage number (names unknown).
void Announce(int32_t id, int32_t responseNo) {
    const CAdvisorRequest request(static_cast<AdvisorMessage>(id), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, responseNo, false, -1);
    CMsgAdvisor message(request);
    TicketMessageCentre()->PostMessage(message);
}

}  // namespace

// 0x100fcbb0 (660 bytes): `type` is the ticket's number within its group (the response number), `won` what EarnedTicket answered (1 ticket, 2 and
// key, 3 and park). Any other won (0, negative, 4 and above) is traced as unrecognised and announces nothing.
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeLocal type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Local golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Local ticket only\n");
        Announce(0xd7, t);
    } else if (w == 2) {
        CTRACE(3, "Local ticket and key\n");
        Announce(0xd8, t);
    } else if (w == 3) {
        CTRACE(3, "Local ticket, key and park\n");
        Announce(0xd9, t);
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Local) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}

// CMsgAdvisor (main app; unit w180): the constructor 0x1015190c, GetType 0x100fd9cc and the destructor 0x100fcea8.

CMsgAdvisor::~CMsgAdvisor() {}

// 0x100fcf1c (1392 bytes): won 1 to 3 as the local one (ids 0xdc to 0xde); won 4 is the global award, "moved here", announced by `type` (0 coaster
// height 0xea, 1 gokart excitement 0xeb, 2 water length 0xec, 3 big park 0xed; response number -1; any other type nothing).
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeGlobal type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Global golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Global ticket only\n");
        Announce(0xdc, t);
    } else if (w == 2) {
        CTRACE(3, "Global ticket and key\n");
        Announce(0xdd, t);
    } else if (w == 3) {
        CTRACE(3, "Global ticket, key and park\n");
        Announce(0xde, t);
    } else if (w == 4) {
        CTRACE(3, "Global ticket award moved here\n");
        if (t == 0) {
            CTRACE(3, "Global ticket award coaster height\n");
            Announce(0xea, -1);
        } else if (t == 1) {
            CTRACE(3, "Global ticket award gokart excitement\n");
            Announce(0xeb, -1);
        } else if (t == 2) {
            CTRACE(3, "Global ticket award water length\n");
            Announce(0xec, -1);
        } else if (t == 3) {
            CTRACE(3, "Global ticket award big park\n");
            Announce(0xed, -1);
        }
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Global) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}

// 0x100fd4f4 (660 bytes): as the local one, ids 0xe1 to 0xe3.
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeSecret type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Secret golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Secret ticket only\n");
        Announce(0xe1, t);
    } else if (w == 2) {
        CTRACE(3, "Secret ticket and key\n");
        Announce(0xe2, t);
    } else if (w == 3) {
        CTRACE(3, "Secret ticket, key and park\n");
        Announce(0xe3, t);
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Secret) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}

int32_t CMsgAdvisor::GetType() const { return 5; }

// Unit v51077: leaf functions of the ride animation code (rsse.cpp, 0x100c3234..0x100c7b58), the fly accessors, the ride pointers
// (TPRide.cpp), Debug.cpp, keyboard actions and the static initialisers of lobby_screen, lobby_island, lobby_globe, BlockSave,
// GoldenTickets, Debug and TPRide.

// ---- the static initialisers: the wide strings the files hold (Mac 0x1023e9f0 and so on), the objects and the nodes

namespace {
using W = TbDynamicStringTemplate<char16_t>;
const char16_t kTicketW[] = u"gms.dat";            // 0x102417e4
}  // namespace

alignas(16) unsigned char gV51077TicketWc1e4[sizeof(W)];   // 0x1034c1e4
unsigned char gV51077TicketNodec1f8[16];                    // 0x1034c1f8
void __sinit_GoldenTickets_cpp() {
    void* object = new (static_cast<void*>(gV51077TicketWc1e4)) W(kTicketW);
    __register_global_object(object, reinterpret_cast<void*>(&UIMain_DestroyWString), gV51077TicketNodec1f8);
}

// CGoldenTicketControl (main app; unit w180): the ticket counts, the spending of a ticket on a ride, the checks that earn the local, global and
// secret tickets, the three EarnedGoldenTicket and TellIfFirst. The master save (gMasterSave) is made on first use, and the original repeats that
// test before EVERY call into it, so the helper below is called at each place the listing tests it. The three TellTheWorld overloads are in src/GoldenTicket_tell.cpp (unit w183).

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
