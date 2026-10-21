// Test library for the golden ticket control and the advisor message (unit w180; libtpw_unit_GoldenTicket.dylib, tests/test_GoldenTicket.py), main app:
//   src/game/park/GoldenTickets.cpp   CGoldenTicketControl::HowManyTickets, SpendTicketOn, HasBeenBought, CanIAfford, CheckForLocal, CheckForGlobal, CheckForSecret,
//                          EarnedGoldenTicket (three), TellIfFirst
//   src/game/park/GoldenTickets.cpp, src/game/ui/MessageCentre.cpp     CMsgAdvisor's constructor, GetType and destructor
//
// Every callee outside the unit is a fake that records what the real one reads and answers from the test's queues: the master save (its constructor
// and eleven members), the ride database's GetRideInfo (answering one ride record), the game type's constructor, the park analyser (ten questions),
// the world's GetParkAnalyser and GetBankAccount, the bank account's GetProfitThisYear, the park time's GetFunnyTimeSinceSessionStart, the three
// TellTheWorld, CTRACE, the advisor request's two constructors, the message centre (the nothrow new of its 0x1d0 bytes, the array construction of
// its headers, PostMessage). The globals of other files are defined here, zero at start like the Mac's bss.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "CSubscriptionHeader.h"
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
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

namespace {

alignas(16) unsigned char gWorldBlock[0x400];
alignas(16) unsigned char gControlBlock[0x40];
alignas(16) unsigned char gAnalyserBlock[0x40];
alignas(16) unsigned char gBankBlock[0x40];
alignas(16) unsigned char gMasterBlock[0x40];
alignas(16) unsigned char gCentreBlock[0x1d0];
alignas(16) unsigned char gRideBlock[sizeof(RideInfo)];   // zero at start; RideInfo's destructor is never needed
void* gNewCentre;
void* gAllocated[8];
int32_t gAllocatedCount;
int32_t gCtorType;

int64_t CanonBits(float value) {
    uint32_t bits = fbits(value);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu) != 0) bits = 0x7fc00000u;
    return bits;
}

float FloatOf(int64_t bits) {
    const uint32_t b = static_cast<uint32_t>(bits);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}

RideInfo& Ride() { return *reinterpret_cast<RideInfo*>(gRideBlock); }
CGoldenTicketControl* Control() { return reinterpret_cast<CGoldenTicketControl*>(gControlBlock); }

}  // namespace

// ---- globals of other files, defined here -----------------------------------------------------------------------------------------------------
CWorld* gWorld;
CWorld* gGameWorld;
CMessageCentre* gMessageCentre;
CFSGlobalMasterSave* gMasterSave;
CRideDatabase gRideDatabase;
void CBalanceTable::Init() {}
CMainBalance::~CMainBalance() {}
const BalanceDescription* CMainBalance::GetBalanceDescription(int32_t) { return nullptr; }
BalanceValue* CMainBalance::GetBalanceValuesStart() { return mValues; }
CMainBalance gMainBalance;

// ---- the fakes ---------------------------------------------------------------------------------------------------------------------------------
CGameType::CGameType() {
    ev("CGameType", {1});
    mType = static_cast<GameType>(gCtorType);
}

RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return &Ride();
}

// The master save: the constructor takes a new label; the answers come from queues in call order.
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}
CFSGlobalMasterSave::~CFSGlobalMasterSave() {}
CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("CFSGlobalMasterSave", {newlab(this)}); }
int32_t CFSGlobalMasterSave::HowManyTickets() const {
    ev("HowManyTickets", {lab(this)});
    return static_cast<int32_t>(ret("howmany", 0));
}
int32_t CFSGlobalMasterSave::HowManyTicketsIncludingSecrets() const {
    ev("HowManyTicketsIncludingSecrets", {lab(this)});
    return static_cast<int32_t>(ret("inclsec", 0));
}
int32_t CFSGlobalMasterSave::SpendingPower() const {
    ev("SpendingPower", {lab(this)});
    return static_cast<int32_t>(ret("power", 0));
}
bool CFSGlobalMasterSave::SpendTicketOn(uint16_t thing, int32_t cost) {
    ev("MasterSpendTicketOn", {lab(this), thing, cost});
    return (ret("mspend", 0) & 0xff) != 0;
}
bool CFSGlobalMasterSave::HasBeenBought(uint16_t thing) const {
    ev("MasterHasBeenBought", {lab(this), thing});
    return (ret("bought", 0) & 0xff) != 0;
}
bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeLocal type) const {
    ev("HaveTicketLocal", {lab(this), static_cast<int32_t>(type)});
    return (ret("have", 0) & 0xff) != 0;
}
bool CFSGlobalMasterSave::HaveTicket(GoldenTicketTypeSecret type) const {
    ev("HaveTicketSecret", {lab(this), static_cast<int32_t>(type)});
    return (ret("have", 0) & 0xff) != 0;
}
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeLocal type) {
    ev("EarnedTicketLocal", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("won", 0));
}
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeGlobal type, int32_t score) {
    ev("EarnedTicketGlobal", {lab(this), static_cast<int32_t>(type), score});
    return static_cast<int32_t>(ret("won", 0));
}
int32_t CFSGlobalMasterSave::EarnedTicket(GoldenTicketTypeSecret type) {
    ev("EarnedTicketSecret", {lab(this), static_cast<int32_t>(type)});
    return static_cast<int32_t>(ret("won", 0));
}

// The three TellTheWorld; with "tellnull" set each one empties the master save pointer (so the second test of the original shows).
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeLocal type, GoldenTicketTypeWon won) {
    ev("TellTheWorldLocal", {lab(this), static_cast<int32_t>(type), static_cast<int32_t>(won)});
    if (ret("tellnull", 0) != 0) gMasterSave = nullptr;
}
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeGlobal type, GoldenTicketTypeWon won) {
    ev("TellTheWorldGlobal", {lab(this), static_cast<int32_t>(type), static_cast<int32_t>(won)});
    if (ret("tellnull", 0) != 0) gMasterSave = nullptr;
}
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeSecret type, GoldenTicketTypeWon won) {
    ev("TellTheWorldSecret", {lab(this), static_cast<int32_t>(type), static_cast<int32_t>(won)});
    if (ret("tellnull", 0) != 0) gMasterSave = nullptr;
}

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const int32_t count = va_arg(ap, int32_t);
    va_end(ap);
    ev("CTRACE", {level, count}, {format});
}

// The world's two lookups and the bank and park time questions.
CParkAnalyser* CWorld::GetParkAnalyser() {
    ev("GetParkAnalyser", {lab(this)});
    return reinterpret_cast<CParkAnalyser*>(gAnalyserBlock);
}
CBankAccount* CWorld::GetBankAccount() {
    ev("GetBankAccount", {lab(this)});
    return reinterpret_cast<CBankAccount*>(gBankBlock);
}
int32_t CBankAccount::GetProfitThisYear() const {
    ev("GetProfitThisYear", {lab(this)});
    return static_cast<int32_t>(ret("profit", 0));
}
TbTimeDiff CParkTime::GetFunnyTimeSinceSessionStart() const {
    ev("GetFunnyTimeSinceSessionStart", {lab(this)});
    return TbTimeDiff{ret("span", 0)};
}

// The park analyser: the receiver and the arguments; the answers come from queues. The three that write the thing id take it from "thing" (the entry
// value of the id is logged by the last two only: the first call finds the stack's leftovers).
int32_t CParkAnalyser::NumberOfVisitorsSinceBeginning() const {
    ev("NumberOfVisitorsSinceBeginning", {lab(this)});
    return static_cast<int32_t>(ret("visitors", 0));
}
int32_t CParkAnalyser::NumberOfVisitorsInPark() const {
    ev("NumberOfVisitorsInPark", {lab(this)});
    return static_cast<int32_t>(ret("inpark", 0));
}
float CParkAnalyser::GetHappinessLevel(uint32_t unused) const {
    ev("GetHappinessLevel", {lab(this), unused});
    return FloatOf(ret("happy", 0));
}
bool CParkAnalyser::AllObjectsResearchedAndPresent() const {
    ev("AllObjectsResearchedAndPresent", {lab(this)});
    return (ret("research", 0) & 0xff) != 0;
}
int32_t CParkAnalyser::NumberOfVisitorsInLastMonths(int32_t back) const {
    ev("NumberOfVisitorsInLastMonths", {lab(this), back});
    return static_cast<int32_t>(ret("lastmonths", 0));
}
uint32_t CParkAnalyser::GetHighestCoaster(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> type) const {
    ev("GetHighestCoaster", {lab(this), type.mIndex});
    id.mIndex = static_cast<uint16_t>(ret("thing", 0));
    return static_cast<uint32_t>(ret("coaster", 0));
}
uint8_t CParkAnalyser::GetMostExcitingGokart(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) const {
    ev("GetMostExcitingGokart", {lab(this), id.mIndex});
    id.mIndex = static_cast<uint16_t>(ret("thing", 0));
    return static_cast<uint8_t>(ret("gokart", 0));
}
int32_t CParkAnalyser::GetLongestTrackRide(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id, int32_t track) const {
    ev("GetLongestTrackRide", {lab(this), id.mIndex, track});
    id.mIndex = static_cast<uint16_t>(ret("thing", 0));
    return static_cast<int32_t>(ret("longest", 0));
}
int32_t CParkAnalyser::NumberOfCellsCoveredByObjects() {
    ev("NumberOfCellsCoveredByObjects", {lab(this)});
    return static_cast<int32_t>(ret("cells", 0));
}
int32_t CParkAnalyser::PercentageOfParkCoveredByCameras() {
    ev("PercentageOfParkCoveredByCameras", {lab(this)});
    return static_cast<int32_t>(ret("cameras", 0));
}

// The advisor request: the six-argument constructor sets the eight fields as src/game/advisor/Advisor.cpp does; the copy constructor copies the eight fields
// and leaves both padding regions alone (it logs the source's fields).
CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary, int32_t responseNo,
                                 bool overrideOnlyOnce, int32_t score) {
    ev("RequestCtor", {message, thingId.mIndex, secondary, responseNo, overrideOnlyOnce, score});
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    mResponseNo = responseNo;
    mMessage = message;
    mScore = score;
    mValid = 1;
    mOverrideOnlyOnce = overrideOnlyOnce;
    mScoreOverridden = 0;
}
CAdvisorRequest::CAdvisorRequest(const CAdvisorRequest& other) {
    ev("RequestCopy", {other.mThingId, other.mSecondary, other.mResponseNo, other.mMessage, other.mScore, other.mValid, other.mOverrideOnlyOnce,
                       other.mScoreOverridden});
    mThingId = other.mThingId;
    mSecondary = other.mSecondary;
    mResponseNo = other.mResponseNo;
    mMessage = other.mMessage;
    mScore = other.mScore;
    mValid = other.mValid;
    mOverrideOnlyOnce = other.mOverrideOnlyOnce;
    mScoreOverridden = other.mScoreOverridden;
}

// The message centre: the posted message by its type, flags and the request's eight fields; the lazy allocation of the centre (the nothrow new and the
// first header's constructor, which stands for the array construction).
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    const CAdvisorRequest& r = static_cast<const CMsgAdvisor&>(message).mRequest;
    ev("PostMessage", {lab(this), message.GetType(), message.mFlags, r.mThingId, r.mSecondary, r.mResponseNo, r.mMessage, r.mScore, r.mValid,
                       r.mOverrideOnlyOnce, r.mScoreOverridden});
    return 1;
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    const bool fail = ret("newnull", 0) != 0;
    ev("new", {static_cast<int64_t>(size), fail});
    if (fail) return nullptr;
    gNewCentre = std::malloc(size);
    std::memset(gNewCentre, 0xa5, size);
    gAllocated[gAllocatedCount++] = gNewCentre;
    return gNewCentre;
}
CSubscriptionHeader::CSubscriptionHeader() {
    if (this == gNewCentre) ev("CMessageCentre", {newlab(this), 1, 1, 0x10, 0x1d});
}
CSubscriptionHeader::~CSubscriptionHeader() {}

extern "C" {

// The host address of a block by the module's name for it.
uint64_t tpw_gt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", gWorldBlock},       {"control", gControlBlock}, {"analyser", gAnalyserBlock}, {"bank", gBankBlock},       {"master", gMasterBlock},
        {"centre", gCentreBlock},     {"ride", gRideBlock},           {"database", &gRideDatabase}, {"gWorld", &gWorld},        {"gMasterSave", &gMasterSave},
        {"gMessageCentre", &gMessageCentre},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable).
uint64_t tpw_gt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT): the park time's offset in the world, the ride cost slot's Mac offset, the request's offset in
// the message, the message's size.
int32_t tpw_gt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mParkTime));
    out[n++] = static_cast<int64_t>(reinterpret_cast<const unsigned char*>(&Ride().mBalance.mValues[kRideSlot_GoldenTicketCost]) -
                                    reinterpret_cast<const unsigned char*>(&Ride()));
    out[n++] = static_cast<int64_t>(sizeof(BalanceValue));
    out[n++] = static_cast<int64_t>(offsetof(CMsgAdvisor, mRequest));
    out[n++] = static_cast<int64_t>(sizeof(CMsgAdvisor));
    out[n++] = static_cast<int64_t>(sizeof(CAdvisorRequest));
    return n;
}

// The per-case globals: the pointers the module cannot write as words (64-bit here), the balance slots 466 to 477, the ride's cost, the game type's
// constructor answer, the centre block's release.
void tpw_gt_prepare(uint64_t world, uint64_t master, uint64_t centre, const int32_t* slots, int32_t cost, int32_t ctortype) {
    for (int32_t i = 0; i < gAllocatedCount; ++i) std::free(gAllocated[i]);
    gAllocatedCount = 0;
    gNewCentre = nullptr;
    gWorld = reinterpret_cast<CWorld*>(static_cast<uintptr_t>(world));
    gMasterSave = reinterpret_cast<CFSGlobalMasterSave*>(static_cast<uintptr_t>(master));
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
    for (int32_t k = 0; k < 12; ++k) gMainBalance.mValues[466 + k].mInteger = slots[k];
    Ride().Value(kRideSlot_GoldenTicketCost).mInteger = cost;
    gCtorType = ctortype;
}
// The pointers after the call as labels, and the game type.
int64_t tpw_gt_master() { return lab(gMasterSave); }
int64_t tpw_gt_centre() { return lab(gMessageCentre); }
void tpw_gt_mode(int32_t type) { TheGameType().mType = static_cast<GameType>(type); }
int32_t tpw_gt_type() { return TheGameType().mType; }

// ---- the functions, one wrapper each with C types only
int32_t tpw_gt_howmany() { return Control()->HowManyTickets(); }
uint32_t tpw_gt_spend(uint32_t id) { return Control()->SpendTicketOn(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)}) ? 1 : 0; }
uint32_t tpw_gt_bought(uint32_t id) { return Control()->HasBeenBought(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)}) ? 1 : 0; }
uint32_t tpw_gt_afford(uint32_t id) { return Control()->CanIAfford(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{static_cast<uint16_t>(id)}) ? 1 : 0; }
void tpw_gt_check(int32_t which) {
    if (which == 0) Control()->CheckForLocal();
    else if (which == 1) Control()->CheckForGlobal();
    else Control()->CheckForSecret();
}
uint32_t tpw_gt_earned(int32_t which, int32_t type, int32_t score) {
    if (which == 0) return Control()->EarnedGoldenTicket(static_cast<GoldenTicketTypeLocal>(type)) ? 1 : 0;
    if (which == 1) return Control()->EarnedGoldenTicket(static_cast<GoldenTicketTypeGlobal>(type), score) ? 1 : 0;
    return Control()->EarnedGoldenTicket(static_cast<GoldenTicketTypeSecret>(type)) ? 1 : 0;
}
void tpw_gt_tellfirst() { Control()->TellIfFirst(); }

// The advisor message. The constructor on a block the module poisoned, from a request block (the 0x18 bytes in host order); out = the flags, the eight
// request fields, the three padding bytes (+2, +3, +0x17), 1 when the bytes after the object are untouched, 1 when the vtable is CMsgAdvisor's (GetType
// answers 5 through a CMsg pointer).
void tpw_gt_msg_ctor(unsigned char* block, int32_t blocksize, const unsigned char* request, uint32_t* out) {
    const CAdvisorRequest& source = *reinterpret_cast<const CAdvisorRequest*>(request);
    CMsgAdvisor* m = new (block) CMsgAdvisor(source);
    const CAdvisorRequest& r = m->mRequest;
    const unsigned char* raw = reinterpret_cast<const unsigned char*>(&r);
    const CMsg* base = m;
    out[0] = m->mFlags;
    out[1] = r.mThingId;
    out[2] = r.mSecondary;
    out[3] = static_cast<uint32_t>(r.mResponseNo);
    out[4] = static_cast<uint32_t>(r.mMessage);
    out[5] = static_cast<uint32_t>(r.mScore);
    out[6] = r.mValid;
    out[7] = r.mOverrideOnlyOnce;
    out[8] = r.mScoreOverridden;
    out[9] = raw[2];
    out[10] = raw[3];
    out[11] = raw[0x17];
    bool intact = true;
    for (int32_t i = static_cast<int32_t>(sizeof(CMsgAdvisor)); i < blocksize; ++i) intact = intact && block[i] == 0xa5;
    out[12] = intact ? 1 : 0;
    out[13] = base->GetType() == 5 ? 1 : 0;
}
int32_t tpw_gt_msg_type(uint32_t flags) {
    CAdvisorRequest request;
    CMsgAdvisor m(request);
    m.mFlags = flags;
    const CMsg* base = &m;
    return base->GetType();
}
// The destructor with the Mac's delete flag: a delete-expression for a positive (signed 16 bit) flag, else an explicit call; out = the flags left behind.
void tpw_gt_msg_dtor(int32_t flag, int32_t present, uint32_t* out) {
    out[0] = 0;
    if (present == 0) return;
    CAdvisorRequest request;
    if (static_cast<int16_t>(flag) > 0) {
        CMsg* m = new CMsgAdvisor(request);
        --tpw_cl_heap[0];
        tpw_cl_label(3, m);
        delete m;
        return;
    }
    alignas(16) static unsigned char block[0x40];
    std::memset(block, 0xa5, sizeof block);
    auto* m = new (block) CMsgAdvisor(request);
    m->mFlags = 0x11223344;
    m->~CMsgAdvisor();
    std::memcpy(&out[0], &m->mFlags, 4);
}

}  // extern "C"
