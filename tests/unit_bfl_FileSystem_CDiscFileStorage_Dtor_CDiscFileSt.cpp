// Test library for the ltms CDiscFileStorage destructor (unit v5110), main app:
//   src/TbIResourceIndex_bfl_FileSystem_CDiscFileStorage_Dtor_CDiscFileSt.cpp
//
// The base destructor is a fake recording the same event as the PowerPC stub. The library's global operator delete is
// the real one (defined elsewhere in this library), so the test allocates the object with operator new and sees the delete
// by asking the heap whether the block is still live (malloc_size is 0 once freed).
#include <malloc/malloc.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "bfl_FileSystem.h"

using namespace calllog;

// The base destructor (a bullfrog import): records the object. Defined by its symbol so that no vtable of
// CFileStorage is needed.
extern "C" {
void tpw_fake_base_dtor_d1(void* self) __asm__("__ZN3bfl10FileSystem12CFileStorageD1Ev");
void tpw_fake_base_dtor_d1(void* self) { ev("CFileStorage_dtor", {lab(self)}); }
}

extern "C" {

uint64_t tpw_cdfs_addr(const char*) { return 0; }

// A block from the heap that operator delete frees.
uint64_t tpw_cdfs_alloc(uint32_t size) {
    void* p = ::operator new(size);
    std::memset(p, 0, size);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p));
}

int32_t tpw_cdfs_layout(int64_t*) { return 0; }

// Returns this (or 0); *freed is 1 when the heap no longer holds the block afterwards.
uint64_t tpw_cdfs_dtor(uint64_t self, int32_t flag, int32_t* freed) {
    using bfl::FileSystem::CDiscFileStorage;
    void* p = reinterpret_cast<void*>(static_cast<uintptr_t>(self));
    CDiscFileStorage* r = CDiscFileStorage::DestroyLtms(static_cast<CDiscFileStorage*>(p), static_cast<int16_t>(flag));
    *freed = (p != nullptr && malloc_size(p) == 0) ? 1 : 0;
    return reinterpret_cast<uintptr_t>(r);
}

}  // extern "C"
