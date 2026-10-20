// Test library for the coaster queues and script commands (unit v5269; libtpw_unit_AllocateQueue.dylib, tests/test_AllocateQueue.py), main app:
//   src/AllocateQueue.cpp   AllocateQueue, CoasterAddPeep, HACKCoasterReturnLostPeep, CoasterGetQueueSpace, CoasterGetPeep,
//                           CoasterSetRepairState, CoasterSetSpeed, CoasterSetOpenClosedState; defines gCoasterQueueSpaceLimit
//
// Fakes: MReleaseAllocF, SetCoasterState and the 3-argument CoasterSetSpeed record their arguments (the instance as a label); the globals
// of other files (gObjectTable, gActionRecorder) are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ActionRecorder.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "rsse.h"

using namespace calllog;

void* gObjectTable[0x100];
CActionRecorder gActionRecorder;

void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {static_cast<int64_t>(size), static_cast<int64_t>(flags)});
    return tpw::ArenaAllocZeroed(size < 4 ? 4 : size);
}

void SetCoasterState(tag_sCoasterInstance* instance, uint32_t state) {
    ev("SetCoasterState", {lab(instance), static_cast<int64_t>(state)});
}

void CoasterSetSpeed(tag_sCoasterInstance* instance, uint32_t speed, int32_t flag) {
    ev("CoasterSetSpeed", {lab(instance), static_cast<int64_t>(speed), static_cast<int64_t>(flag)});
}

extern "C" {

uint64_t tpw_aq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"table", &gObjectTable[0]},
        {"lim", &gCoasterQueueSpaceLimit},
        {"rec", &gActionRecorder.mRecording},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_aq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// flags, state, f30, queue B0, queue D0, sizeof instance, queue block, queue mF08, sizeof queue
int32_t tpw_aq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mFlags));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mState));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mF30));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mQueueB0));
    out[n++] = static_cast<int64_t>(offsetof(tag_sCoasterInstance, mQueueD0));
    out[n++] = static_cast<int64_t>(sizeof(tag_sCoasterInstance));
    out[n++] = static_cast<int64_t>(offsetof(sWraparoundQueue, mBlock));
    out[n++] = static_cast<int64_t>(offsetof(sWraparoundQueue, mF08));
    out[n++] = static_cast<int64_t>(sizeof(sWraparoundQueue));
    return n;
}

uint32_t tpw_aq_alloc_queue(uint64_t queue, uint32_t capacity) {
    AllocateQueue(reinterpret_cast<sWraparoundQueue*>(static_cast<uintptr_t>(queue)), capacity);
    return 0;
}
uint32_t tpw_aq_add(uint32_t coaster, uint32_t peep) { return CoasterAddPeep(coaster, peep); }
uint32_t tpw_aq_hack(uint64_t instance, uint32_t peep) {
    HACKCoasterReturnLostPeep(reinterpret_cast<tag_sCoasterInstance*>(static_cast<uintptr_t>(instance)), peep);
    return 0;
}
uint32_t tpw_aq_space(uint32_t coaster) { return CoasterGetQueueSpace(coaster); }
uint32_t tpw_aq_get(uint32_t coaster) { return CoasterGetPeep(coaster); }
uint32_t tpw_aq_repair(uint32_t coaster, uint32_t state) { return CoasterSetRepairState(coaster, state); }
uint32_t tpw_aq_speed(uint32_t coaster, uint32_t speed) {
    CoasterSetSpeed(coaster, speed);
    return 0;
}
uint32_t tpw_aq_open(uint32_t coaster, int32_t state) { return CoasterSetOpenClosedState(coaster, state); }

}  // extern "C"
