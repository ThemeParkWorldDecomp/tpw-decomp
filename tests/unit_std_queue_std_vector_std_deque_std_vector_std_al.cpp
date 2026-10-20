// Test library for the std::queue<std::vector<Argument>> destructor (unit v5725), main app:
//   src/COnlinePerson_SetDest_GotoPark_OK_std_queue_std_vector_std_deque_std_vector_std_al.cpp
//
// Callees: deque tear_down and operator delete, fakes that record which object they got (the queue = 1, its block table = 2, else 0).
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {
uintptr_t gThis = 0;
uintptr_t gBlock = 0;
int64_t Cls(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    return v == gThis ? 1 : (v == gBlock ? 2 : 0);
}
}  // namespace

namespace online_callee {
void DequeTearDown(void* self) { ev("tear_down", {Cls(self)}); }
void OperatorDelete(void* p) { ev("delete", {Cls(p), Cls(p) == 0 ? static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu) : 0}); }
}  // namespace online_callee

extern "C" {

uint64_t tpw_qd_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_qd_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(MslArgumentQueue, mBlocks));
    return n;
}

uint64_t tpw_qd_dtor(uint64_t object, int32_t flag) {
    MslArgumentQueue* q = reinterpret_cast<MslArgumentQueue*>(static_cast<uintptr_t>(object));
    gThis = static_cast<uintptr_t>(object);
    gBlock = q ? static_cast<uintptr_t>(q->mBlocks) : 0;
    MslArgumentQueue* r = ArgumentQueueDestruct(q, static_cast<int16_t>(flag));
    return r == q ? 1 : 0;
}

}  // extern "C"
