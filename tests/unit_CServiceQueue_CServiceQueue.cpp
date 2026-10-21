// Test library for CServiceQueue (unit v5739; libtpw_unit_CServiceQueue_CServiceQueue.dylib, tests/test_CServiceQueue_CServiceQueue.py):
//   src/sound/ServiceQueue.cpp
//
// The glue imports (wait, set event, free event, free storage) and TbSoundPipeChannel::FillServiceBuffer are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "TbSoundSampleMPEG2.h"
#include "CSprocketsModule.h"
#include "TbSoundPipeChannel.h"

using namespace calllog;

namespace {
const void* gObj = nullptr;
const void* gArr = nullptr;
const char* gChan = nullptr;
}  // namespace

uint32_t USoundQueue_WaitAny(uint32_t count, void** events, uint32_t flags, uint32_t timeout) {
    ev("Wait", {count, events == gArr ? 1 : 0, flags, timeout});
    return static_cast<uint32_t>(ret("Wait"));
}

void USoundEvent_Set(void* event) { ev("Set", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(event))}); }

void USoundEvent_Free(void* event) { ev("EventFree", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(event))}); }

void USoundChannel_Free(void* object) { ev("Free", {object == gObj ? 1 : 0}); }

uint32_t TbSoundPipeChannel::FillServiceBuffer() {
    ev("Fill", {static_cast<int64_t>((reinterpret_cast<const char*>(this) - gChan) / static_cast<std::ptrdiff_t>(sizeof(TbSoundPipeChannel)))});
    return 0;
}

#pragma clang diagnostic ignored "-Winvalid-offsetof"

extern "C" {

uint64_t tpw_cq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// channel, slots, last, items, flags, size
int32_t tpw_cq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CServiceQueue, mChannel));
    out[n++] = static_cast<int64_t>(offsetof(CServiceQueue, mSlots));
    out[n++] = static_cast<int64_t>(offsetof(CServiceQueue, mLast));
    out[n++] = static_cast<int64_t>(offsetof(CServiceQueue, mItems));
    out[n++] = static_cast<int64_t>(offsetof(CServiceQueue, mFlags));
    out[n++] = static_cast<int64_t>(sizeof(CServiceQueue));
    return n;
}

void tpw_cq_ctor(uint64_t object, uint64_t channel, uint32_t last) {
    gObj = reinterpret_cast<void*>(object);
    new (reinterpret_cast<void*>(object)) CServiceQueue(reinterpret_cast<TbSoundPipeChannel*>(channel), last);
}

// Tells the fakes which object, array and channel block the next call works on.
void tpw_cq_prepare(uint64_t object) {
    CServiceQueue* q = reinterpret_cast<CServiceQueue*>(object);
    gObj = q;
    gArr = static_cast<void**>(q->mItems);
    gChan = reinterpret_cast<const char*>(q->mChannel);
}

void tpw_cq_close(uint64_t object) { reinterpret_cast<CServiceQueue*>(object)->Close(); }

void tpw_cq_poll(uint64_t object) { reinterpret_cast<CServiceQueue*>(object)->Poll(); }

// flag > 0 is the deleting form (r4 of the Mac's destructor, a short).
void tpw_cq_dtor(uint64_t object, int32_t flag) {
    CServiceQueue* q = reinterpret_cast<CServiceQueue*>(object);
    gObj = q;
    if (static_cast<int16_t>(flag) > 0)
        delete q;
    else
        q->~CServiceQueue();
}

uint64_t tpw_cq_vptr(uint64_t object) { return *reinterpret_cast<const uint64_t*>(object); }

}  // extern "C"
