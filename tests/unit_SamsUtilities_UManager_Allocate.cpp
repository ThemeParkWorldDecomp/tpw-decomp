// Test library for SamsUtilities::UManager<UAllocator>::Allocate (unit v5123; libtpw_unit_SamsUtilities_UManager_Allocate.dylib), main app:
//   src/TbMpegFilePlayer_SamsUtilities_UManager_Allocate.cpp
//
// The two callees (0x1002eac4 allocation, 0x1002ec8c initialiser) are recording fakes.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CMpegBase.h"

using namespace calllog;

namespace SamsUtilities {

void* UAllocatorMake(int32_t size) {
    ev("Make", {size});
    return reinterpret_cast<void*>(static_cast<uintptr_t>(ret("Make")));
}

void UAllocatorInit(void* object) {
    ev("Init", {lab(object)});
}

}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_uma_addr(const char*) { return 0; }

uint64_t tpw_uma_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_uma_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SamsUtilities::UManager<SamsUtilities::UAllocator>, mObject));
    return n;
}

void tpw_uma_allocate(uint64_t manager) {
    reinterpret_cast<SamsUtilities::UManager<SamsUtilities::UAllocator>*>(manager)->Allocate();
}

}  // extern "C"
