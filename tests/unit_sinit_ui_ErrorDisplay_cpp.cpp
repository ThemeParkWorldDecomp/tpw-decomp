// Test library for ui_ErrorDisplay.cpp's static initialiser (unit v5134; libtpw_unit_sinit_ui_ErrorDisplay_cpp.dylib).
//   src/sinit_ui_ErrorDisplay_cpp.cpp   __sinit_ui_ErrorDisplay_cpp
// The deque and mutex constructors, tear_down, operator delete and __register_global_object are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>

#include "calllog.h"
#include "platform/arena.h"
#include "UI_pump.h"
#include "std_deque_errorthingy.h"

using namespace calllog;

std::deque<errorthingy> gUIErrorQueue;
bthread_mutex gUIErrorMutex;

extern uint32_t gSinitErrorQueueDtor[2], gSinitErrorMutexDtor[2], gSinitErrorQueueNode[3], gSinitErrorMutexNode[3];

namespace {
int64_t Word4(const void* p) {
    uint32_t w;
    std::memcpy(&w, static_cast<const char*>(p) + 0x10, 4);
    return w;
}
}  // namespace

namespace errsinit_callee {
void DequeCtor(void* self, void*) {
    ev("DequeCtor", {});
    uint32_t w = static_cast<uint32_t>(ret("tmpword"));
    std::memcpy(static_cast<char*>(self) + 0x10, &w, 4);
}
void DequeCopy(void* self, const void* other) { ev("DequeCopy", {lab(self), Word4(other)}); }
void* BthreadMutexCtor(void* self) {
    ev("MutexCtor", {lab(self)});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("mutexret") & 0xffffffffu));
}
}  // namespace errsinit_callee

namespace errdeque_callee {
void TearDown(MslErrorDeque* self) { ev("TearDown", {Word4(self)}); }
void OperatorDelete(void* p) { ev("OperatorDelete", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(p))}); }
}  // namespace errdeque_callee

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("Register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_sinit_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"queue", &gUIErrorQueue},
        {"mutex", &gUIErrorMutex},
        {"qdtor", gSinitErrorQueueDtor},
        {"mdtor", gSinitErrorMutexDtor},
        {"qnode", gSinitErrorQueueNode},
        {"mnode", gSinitErrorMutexNode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sinit_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sinit_layout(int64_t*) { return 0; }

void tpw_sinit_run() { __sinit_ui_ErrorDisplay_cpp(); }

}  // extern "C"
