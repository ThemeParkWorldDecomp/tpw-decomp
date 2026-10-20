// Test-only replacement of the global operator new / new[] / delete / delete[] for the
// whole shim library, so a test can compare the allocations and frees of our code with
// the original's operator new / new[] / delete / delete[] imports (tpwtest keeps the
// PowerPC side's log in emu.alloc_log). Logging is off unless a test turns it on with
// tpw_alloc_log_on; blocks come from malloc either way.
#include <cstdint>
#include <cstdlib>
#include <new>

extern "C" {
int32_t tpw_alloc_log_on = 0;
int32_t tpw_alloc_n = 0;
// kind: 1 new, 2 new[], 3 delete, 4 delete[]
int32_t tpw_alloc_kind[1024];
uint64_t tpw_alloc_ptr[1024];
uint64_t tpw_alloc_size[1024];

void tpw_alloc_reset() { tpw_alloc_n = 0; }
}

namespace {

void note(int32_t kind, void* p, std::size_t n) {
    if (tpw_alloc_log_on == 0 || tpw_alloc_n >= 1024) return;
    tpw_alloc_kind[tpw_alloc_n] = kind;
    tpw_alloc_ptr[tpw_alloc_n] = reinterpret_cast<uintptr_t>(p);
    tpw_alloc_size[tpw_alloc_n] = n;
    tpw_alloc_n++;
}

void* allocate(std::size_t n) {
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

}  // namespace

void* operator new(std::size_t n) {
    void* p = allocate(n);
    note(1, p, n);
    return p;
}

void* operator new[](std::size_t n) {
    void* p = allocate(n);
    note(2, p, n);
    return p;
}

void operator delete(void* p) noexcept {
    note(3, p, 0);
    std::free(p);
}

void operator delete[](void* p) noexcept {
    note(4, p, 0);
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { operator delete[](p); }
