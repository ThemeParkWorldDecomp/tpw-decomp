// Test library for CResearchLab::ReceiveMessage and CResearchLab::Serialise (unit v5024; libtpw_unit_CResearchLab_ReceiveMessage.dylib,
// tests/test_CResearchLab_ReceiveMessage.py), main app:
//   src/game/buildings/ResearchLab.cpp
//
// The callees outside the unit are fakes that record the arguments the real one reads and answer from the test's queues. The globals of
// other files (gWorld, gMessageCentre, gMasterSave, gRideDatabase) are defined here, zero at start like the Mac's bss. The global
// operator new is replaced: while a test call runs it logs its size and hands out 32-bit addressable arena memory.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "Serialise.h"
template<> int32_t SAVEVAR<int32_t>(void*, int32_t*, const char*, int32_t);
template<> int32_t SAVEVAR<MacULong>(void*, MacULong*, const char*, int32_t);
template<> int32_t SAVEVAR<bool>(void*, bool*, const char*, int32_t);
template<> int32_t LOADVAR<int32_t>(void*, int32_t*, const char*, int32_t);
template<> int32_t LOADVAR<MacULong>(void*, MacULong*, const char*, int32_t);
template<> int32_t LOADVAR<bool>(void*, bool*, const char*, int32_t);
#include "ThingReceivers.h"
#include "AdvisorQueue.h"
#include "CMessageCentre.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Serialise.h"
#include "TPEngine.h"
#include "World.h"

using namespace calllog;

CWorld* gWorld;
CMessageCentre* gMessageCentre;
CFSGlobalMasterSave* gMasterSave;
CRideDatabase gRideDatabase;
template<> TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {}   // the database's name builder holds strings; the program's is not linked

namespace {

// The blocks the test built before the call (a pointer in a global that is one of them is "another object", 2; null is 0; anything else
// was made by operator new during the call, 1). The shim has its own operator new, so the sizes of the allocations are not compared.
const void* gPresets[64];
int gPresetCount = 0;
const void* gPrevCtor = nullptr;
char gGuardDummy[8];
char gRideToken[16];

int64_t Kind(const void* p) {
    if (p == nullptr) return 0;
    for (int i = 0; i < gPresetCount; ++i)
        if (gPresets[i] == p) return 2;
    return 1;
}

}  // namespace

// ---- the fakes
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return ret("ride") ? reinterpret_cast<RideInfo*>(gRideToken) : nullptr;
}

char CResearchProject::SetupStatUpgrade(const RideInfo* ride) {
    ev("SetupStatUpgrade", {lab(this), lab(ride)});
    return static_cast<char>(ret("SetupStatUpgrade"));
}

void CResearchLab::UpdateHighestGroup(int32_t group) { ev("UpdateHighestGroup", {lab(this), group}); }

bool CResearchLab::IsAllResearchDone() const {
    ev("IsAllResearchDone", {lab(this)});
    return ret("IsAllResearchDone") != 0;
}

CMsgResearchCategoryUpdated::CMsgResearchCategoryUpdated(ResearchCat category) {
    ev("MsgCtor", {static_cast<int64_t>(category)});
    mCategory = category;
}

int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {Kind(this), static_cast<int64_t>(static_cast<const CMsgResearchCategoryUpdated&>(message).mCategory), message.GetType()});
    return 1;
}

CSubscriptionHeader::CSubscriptionHeader() {
    // the start of an array: the element after the previous one is not this one
    if (gPrevCtor == nullptr || static_cast<const char*>(gPrevCtor) + sizeof(CSubscriptionHeader) != static_cast<const void*>(this))
        ev("construct_array", {1, 1, 0x10, 0x1d, Kind(this)});
    gPrevCtor = this;
}

CSubscriptionHeader::~CSubscriptionHeader() {}

CFSGlobalMasterSave::CFSGlobalMasterSave() { ev("MSctor", {Kind(this)}); }

void CFSGlobalMasterSave::SetAllResearchCompletedInCurrentTheme() { ev("SetAllResearch", {Kind(this)}); }

bool CFSGlobalMasterSave::HasAllResearchBeenCompleted() {
    ev("HasAll", {Kind(this)});
    return ret("HasAll") != 0;
}

CGameType::CGameType() {}

CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("AdvisorRequest", {static_cast<int64_t>(message), thingId.mIndex, secondary, responseNo, overrideOnlyOnce ? 1 : 0, score});
}

CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {this == gWorld ? 1 : 0});
    return reinterpret_cast<CAdvisor*>(this);
}

int32_t CAdvisor::QueueRequest(const CAdvisorRequest&) {
    ev("QueueRequest", {reinterpret_cast<CWorld*>(this) == gWorld ? 1 : 0});
    return 1;
}

void CWorld::StartWinningSequence() { ev("StartWinning", {this == gWorld ? 1 : 0}); }

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("ThingSer", {lab(this), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)), save, version, unused});
    return static_cast<int32_t>(ret("ThingSer"));
}

int32_t CResearchProject::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("ProjectSer", {lab(this), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)), save, version, unused});
    return static_cast<int32_t>(ret("ProjectSer"));
}

int32_t CResearchCategory::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CategorySer", {lab(this), static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)), save, version, unused});
    return static_cast<int32_t>(ret("CategorySer"));
}

#define VARFAKE(FN, NAME, TYPE)                                                                                                    \
    template<> int32_t FN<TYPE>(void* file, TYPE* value, const char* name, int32_t noLog) {                                        \
        ev(NAME, {static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file)), lab(value), noLog}, {name});                           \
        return static_cast<int32_t>(ret(NAME));                                                                                    \
    }
VARFAKE(SAVEVAR, "SAVEVAR<int>", int32_t)
VARFAKE(SAVEVAR, "SAVEVAR<unsigned long>", MacULong)
VARFAKE(SAVEVAR, "SAVEVAR<bool>", bool)
VARFAKE(LOADVAR, "LOADVAR<int>", int32_t)
VARFAKE(LOADVAR, "LOADVAR<unsigned long>", MacULong)
VARFAKE(LOADVAR, "LOADVAR<bool>", bool)

namespace {
struct TestMsg : CMsgRideResearched {
    int32_t mType;
    int32_t GetType() const override { return mType; }
};
}  // namespace

extern "C" {

uint64_t tpw_rl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"gMessageCentre", &gMessageCentre},
        {"gMasterSave", &gMasterSave},
        {"gametype", &TheGameType().mType},
        {"guard", gGuardDummy},
        {"db", &gRideDatabase},
        {"token", gRideToken},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_rl_alloc(uint32_t size) {
    void* p = tpw::ArenaAllocZeroed(size);
    if (gPresetCount < 64) gPresets[gPresetCount++] = p;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

void tpw_rl_reset() {
    gWorld = nullptr;
    gMessageCentre = nullptr;
    gMasterSave = nullptr;
    gPresetCount = 0;
    gPrevCtor = nullptr;
}

int32_t tpw_rl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(CResearchLab));
    out[n++] = static_cast<int64_t>(offsetof(CResearchLab, mProjectCount));
    out[n++] = static_cast<int64_t>(offsetof(CResearchLab, mF1460));
    out[n++] = static_cast<int64_t>(offsetof(CResearchLab, mBestProject));
    out[n++] = static_cast<int64_t>(offsetof(CResearchLab, mHaveMechanic));
    return n;
}

void tpw_rl_receive(uint64_t labAddress, int32_t type, uint32_t id, int32_t category) {
    TestMsg msg;
    msg.mType = type;
    msg.mObjectId = static_cast<uint16_t>(id);
    msg.mCategory = category;
    reinterpret_cast<CResearchLab*>(static_cast<uintptr_t>(labAddress))->ReceiveMessage(msg);
}

uint32_t tpw_rl_serialise(uint64_t labAddress, int32_t save, int32_t version, uint32_t unused) {
    return static_cast<uint32_t>(reinterpret_cast<CResearchLab*>(static_cast<uintptr_t>(labAddress))
                                     ->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(0x12345670)), save, version, unused));
}

}  // extern "C"
