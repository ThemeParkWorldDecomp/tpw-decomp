// ui_ErrorDisplay.cpp's static initialiser (0x101f499c, unit v5134).
#include <cstdint>
#include <cstring>

#include "UI_pump.h"
#include "std_deque_errorthingy.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940

// The registered destructors' descriptors (the Mac's TVectors 0x102685e0 ~queue, 0x102686a8 ~bthread_mutex) and the chain nodes
// (0x10396470, 0x10396448; 12 bytes each). The host stands in with its own data.
uint32_t gSinitErrorQueueDtor[2];    // 0x102685e0
uint32_t gSinitErrorMutexDtor[2];    // 0x102686a8
uint32_t gSinitErrorQueueNode[3];    // 0x10396470
uint32_t gSinitErrorMutexNode[3];    // 0x10396448

// Weak stand-ins so the launcher links; the static initialiser never runs on the host (the test library defines the real fakes).
namespace errsinit_callee {
__attribute__((weak)) void DequeCtor(void*, void*) {}
__attribute__((weak)) void DequeCopy(void*, const void*) {}
__attribute__((weak)) void* BthreadMutexCtor(void* self) { return self; }
}  // namespace errsinit_callee

void __sinit_ui_ErrorDisplay_cpp() {
    uint32_t temp[7];       // r1+0x3c: the temporary deque (0x1c bytes); word 4 (+0x10) is the block table pointer
    uint32_t allocator;     // r1+0x38
    errsinit_callee::DequeCtor(temp, &allocator);
    errsinit_callee::DequeCopy(&gUIErrorQueue, temp);
    errdeque_callee::TearDown(reinterpret_cast<MslErrorDeque*>(temp));
    if (temp[4] != 0) errdeque_callee::OperatorDelete(reinterpret_cast<void*>(static_cast<uintptr_t>(temp[4])));
    __register_global_object(&gUIErrorQueue, gSinitErrorQueueDtor, gSinitErrorQueueNode);
    void* mutex = errsinit_callee::BthreadMutexCtor(&gUIErrorMutex);
    __register_global_object(mutex, gSinitErrorMutexDtor, gSinitErrorMutexNode);
}
