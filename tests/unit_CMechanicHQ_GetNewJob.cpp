// Test library for the mechanic HQ (unit v5889; libtpw_unit_CMechanicHQ_GetNewJob.dylib, tests/test_CMechanicHQ_GetNewJob.py), main app:
//   src/game/buildings/MechanicHQ.cpp   CMechanicHQ::GetNewJob, Serialise, ReassignJobsAfterDrop, ReceiveMessage; the eleven id constants
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues or from the objects' own
// fields (the ride's state, flags, staff member, the mechanic's staff state). Texts are recorded as a 32 bit FNV hash. The globals gWorld and
// gThingArray (other files) are defined here.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "AdvisorQueue.h"
#include "CMsg.h"
#include "Debug.h"
#include "Serialise.h"
#include "ThingMessages2.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files
CWorld* gWorld;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

int64_t U(uint32_t v) { return static_cast<int64_t>(v); }

int64_t Fnv(const char* text) {
    uint32_t h = 2166136261u;
    for (; *text; ++text) h = (h ^ static_cast<uint8_t>(*text)) * 16777619u;
    return h;
}

uint16_t IdOf(const void* thing) { return *static_cast<const uint16_t*>(thing); }

template <class T>
T At(const void* base, size_t offset) {
    T v;
    std::memcpy(&v, static_cast<const char*>(base) + offset, sizeof(T));
    return v;
}

struct TestMsg : CMsg {
    uint16_t mThingId = 0;
    int32_t GetType() const override {
        ev("GetType", {0});
        return static_cast<int32_t>(ret("GetType"));
    }
};

}  // namespace

// ---- the fakes
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {U(condition), Fnv(format)}); }

void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t arg = va_arg(ap, int32_t);
    va_end(ap);
    ev("CTRACE", {U(level), Fnv(format), U(arg)});
}

bool CObject::IsBrokenDown() const {
    ev("IsBrokenDown", {U(mBase.mThingId)});
    return mState == 1;
}

int32_t CObject::IsFlaggedForMaintenance() const {
    ev("IsFlaggedForMaintenance", {U(mBase.mThingId)});
    return static_cast<int32_t>(mF60);
}

int32_t CObject::RequiresUpgrade() const {
    ev("RequiresUpgrade", {U(mBase.mThingId)});
    return mState == 2;
}

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CObject::GetAssignedStaffMember() {
    ev("GetAssignedStaffMember", {U(mBase.mThingId)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{mF5A};
}

void CObject::UnassignStaffMember() { ev("UnassignStaffMember", {U(mBase.mThingId)}); }

tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const {
    ev("GetThingId", {U(mThingId)});
    return tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{mThingId};
}

int32_t CThing::Serialise(void* file, int32_t save, int32_t version, uint32_t unused) {
    ev("CThing::Serialise", {U(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file))), U(save), U(version), U(unused)});
    return static_cast<int32_t>(ret("CThing::Serialise"));
}

int32_t CStaff::GetGeneralStaffState() const {
    ev("GetGeneralStaffState", {U(IdOf(this))});
    return At<int32_t>(this, 0x100);
}

CAdvisor* CWorld::GetAdvisor() {
    ev("GetAdvisor", {this == gWorld ? 1 : 0});
    return reinterpret_cast<CAdvisor*>(static_cast<uintptr_t>(0x5000));
}

CAdvisorRequest::CAdvisorRequest(AdvisorMessage message, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thingId, uint32_t secondary,
                                 int32_t responseNo, bool overrideOnlyOnce, int32_t score) {
    ev("CAdvisorRequest", {U(static_cast<int32_t>(message)), U(thingId.mIndex), U(secondary), U(responseNo), overrideOnlyOnce ? 1 : 0, U(score)});
    mThingId = thingId.mIndex;
    mSecondary = secondary;
    mMessage = message;
}

int32_t CAdvisor::QueueRequest(const CAdvisorRequest& request) {
    ev("QueueRequest", {U(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this))), U(request.mThingId), U(request.mSecondary),
                        U(static_cast<int32_t>(request.mMessage))});
    return 1;
}

template <>
int32_t SAVEVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* file, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* value,
                                                                        const char* name, int32_t noLog) {
    ev("SAVEVARSTRUCT", {U(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file))), U(value->mIndex), Fnv(name), U(noLog)});
    return static_cast<int32_t>(ret("SAVEVARSTRUCT"));
}

template <>
int32_t LOADVARSTRUCT<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(void* file, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>* value,
                                                                        const char* name, int32_t noLog) {
    ev("LOADVARSTRUCT", {U(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(file))), U(value->mIndex), Fnv(name), U(noLog)});
    value->mIndex = static_cast<uint16_t>(ret("LoadValue"));
    return static_cast<int32_t>(ret("LOADVARSTRUCT"));
}

extern "C" {

uint64_t tpw_hq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gWorld", &gWorld},
        {"arr", &gThingArray[0]},
        {"n9a", &gMechHQId102a0b9a}, {"n9c", &gMechHQId102a0b9c}, {"n9e", &gMechHQId102a0b9e}, {"na0", &gMechHQId102a0ba0},
        {"na2", &gMechHQId102a0ba2}, {"na4", &gMechHQId102a0ba4}, {"na6", &gMechHQId102a0ba6}, {"na8", &gMechHQId102a0ba8},
        {"naa", &gMechHQId102a0baa}, {"nac", &gMechHQId102a0bac}, {"nae", &gMechHQId102a0bae},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_hq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The last entry is the stride of the thing table on the host.
int32_t tpw_hq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF5A));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF60));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mState));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellX));
    out[n++] = static_cast<int64_t>(offsetof(CThing, mCellY));
    out[n++] = static_cast<int64_t>(offsetof(CMechanic, mNext));
    out[n++] = static_cast<int64_t>(offsetof(CMechanic, mObjectToRepair));
    out[n++] = static_cast<int64_t>(offsetof(CMechanicHQ, mF0C));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA73E));
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mF1DA746));
    out[n++] = static_cast<int64_t>(sizeof(CObject));
    out[n++] = static_cast<int64_t>(sizeof(CThingArrayEntry));
    return n;
}

uint32_t tpw_hq_getnewjob(uint64_t hq, uint32_t mechanic) {
    auto* self = reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(hq));
    return self->GetNewJob(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{static_cast<uint16_t>(mechanic)}).mIndex;
}

int32_t tpw_hq_serialise(uint64_t hq, uint32_t file, int32_t save, int32_t version, uint32_t unused) {
    auto* self = reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(hq));
    return self->Serialise(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), save, version, unused);
}

void tpw_hq_reassign(uint64_t hq, uint64_t mechanic, uint32_t cell) {
    auto* self = reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(hq));
    self->ReassignJobsAfterDrop(*reinterpret_cast<CMechanic*>(static_cast<uintptr_t>(mechanic)),
                                tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>{static_cast<uint16_t>(cell)});
}

void tpw_hq_receive(uint64_t hq, uint32_t thingId) {
    auto* self = reinterpret_cast<CMechanicHQ*>(static_cast<uintptr_t>(hq));
    TestMsg message;
    message.mFlags = 0;
    message.mThingId = static_cast<uint16_t>(thingId);
    self->ReceiveMessage(message);
}

}  // extern "C"
