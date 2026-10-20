// Test library for the UCommandLoadSound constructor and destructor (unit v5758), main app:
//   src/SamsUtilities_UCommandLoadSound_UCommandLoadSoun.cpp
//
// The callees are recording fakes; each also leaves a marker word in memory so the offsets the code passes are compared.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "CSprocketsModule.h"

using namespace calllog;

namespace SamsUtilities {

void UCommand_Construct(UCommand* command) {
    ev("UCommand", {lab(command)});
    uint32_t marker = 0x11111111u;
    std::memcpy(command, &marker, 4);
}

void USoundDescriptionCounter_Copy(UReferenceCounter<USoundDescription>* self, const UReferenceCounter<USoundDescription>* source) {
    ev("CounterCopy", {lab(self), lab(source)});
    std::memcpy(self, source, 4);
}

void USoundDescriptionCounter_Destruct(UReferenceCounter<USoundDescription>* self, int16_t flag) {
    ev("CounterDestruct", {lab(self), flag});
    uint32_t marker = 0xDEAD0000u | (static_cast<uint32_t>(flag) & 0xffffu);
    std::memcpy(self, &marker, 4);
}

}  // namespace SamsUtilities

void USoundChannel_Free(void* channel) { ev("Free", {lab(channel)}); }

extern "C" {

uint64_t tpw_lsc_addr(const char*) { return 0; }

uint64_t tpw_lsc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_lsc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(SamsUtilities::UCommandLoadSound, mVtable4));
    out[n++] = static_cast<int64_t>(offsetof(SamsUtilities::UCommandLoadSound, mDescription));
    return n;
}

void tpw_lsc_ctor(uint64_t object, uint64_t source) {
    new (reinterpret_cast<void*>(object))
        SamsUtilities::UCommandLoadSound(*reinterpret_cast<SamsUtilities::UReferenceCounter<SamsUtilities::USoundDescription>*>(source));
}

uint32_t tpw_lsc_dtor(uint64_t object, int16_t flag) {
    return reinterpret_cast<SamsUtilities::UCommandLoadSound*>(object)->Destruct(flag) != nullptr;
}

}  // extern "C"
