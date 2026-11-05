// A jump to a host function, placed in another 4 GB window than the function: a function pointer whose upper half is not
// that of the test library's code, as when a program's code straddles a 4 GB boundary. Where the jump cannot be made
// (Windows, another processor, the system refuses the address) the function itself comes back.
#pragma once
#include <cstdint>
#include <cstring>

#if !defined(_WIN32) && (defined(__aarch64__) || defined(__x86_64__))
#include <sys/mman.h>
#define TPW_FAR_CODE_STUB 1
#endif

inline void* FarCodeStub(void* target) {
#ifdef TPW_FAR_CODE_STUB
    const uintptr_t address = reinterpret_cast<uintptr_t>(target);
    // Not on a round address: a low half of 0 is a word meaning no callback.
    const uintptr_t hint = (((address >> 32) + 0x100) << 32) | 0x10000000u;
    void* page = mmap(reinterpret_cast<void*>(hint), 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (page == MAP_FAILED) return target;
    if ((reinterpret_cast<uintptr_t>(page) >> 32) == (address >> 32) || static_cast<uint32_t>(reinterpret_cast<uintptr_t>(page)) == 0) {
        munmap(page, 0x1000);
        return target;
    }
    unsigned char* code = static_cast<unsigned char*>(page);
#if defined(__aarch64__)
    const uint32_t jump[2] = {0x58000050u, 0xD61F0200u};   // ldr x16, #8; br x16
    std::memcpy(code, jump, sizeof(jump));
    std::memcpy(code + sizeof(jump), &address, sizeof(address));
#else
    const unsigned char load[2] = {0x48, 0xB8};            // movabs rax, address
    const unsigned char jump[2] = {0xFF, 0xE0};            // jmp rax
    std::memcpy(code, load, sizeof(load));
    std::memcpy(code + sizeof(load), &address, sizeof(address));
    std::memcpy(code + sizeof(load) + sizeof(address), jump, sizeof(jump));
#endif
    if (mprotect(page, 0x1000, PROT_READ | PROT_EXEC) != 0) {
        munmap(page, 0x1000);
        return target;
    }
    __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(code) + 0x20);
    return page;
#else
    return target;
#endif
}
