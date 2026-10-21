// arena.cpp: the allocator behind Ptr32 (include/platform/arena.h, include/Ptr32.h).
//
// One range of address space, reserved once and committed by the system as it is touched.
// Small blocks come from size classes with a free list each (16-byte steps up to 512
// bytes, then powers of two up to 1 MB); a class's memory is never given to another
// class. Larger blocks are cut from the range in 64 KB steps and kept, when freed, in a
// list that later large requests search first. Every block has a 16-byte header in front
// of it with its size and class.
//
// Self-contained (the C++ library and mmap only), so that test libraries can link it
// without the rest of the platform layer.
#include "platform/arena.h"

#include <sys/mman.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

namespace tpw {

uint8_t* gArenaBase = nullptr;

namespace {

constexpr size_t kArenaBytes = 0xF0000000u;   // under 4 GB: every offset fits in 32 bits
constexpr size_t kHeader = 16;
constexpr size_t kSmallStep = 16, kSmallLimit = 512;
constexpr size_t kLargeLimit = size_t(1) << 20, kLargeStep = size_t(1) << 16;
constexpr int kSmallClasses = kSmallLimit / kSmallStep;   // 16, 32, ... 512
constexpr int kPowerClasses = 11;                         // 1 KB, 2 KB, ... 1 MB
constexpr int kLargeClass = kSmallClasses + kPowerClasses;
constexpr uint32_t kLive = 0x4c495645, kFree = 0x46524545;

struct Header {
    uint64_t size;     // what the caller asked for
    uint32_t bin;      // size class, or kLargeClass
    uint32_t state;    // kLive or kFree
};
static_assert(sizeof(Header) == kHeader, "arena block header");

struct LargeHole {
    size_t offset, bytes;   // of the whole block, header included
};

// Never destroyed: the game's global objects free arena blocks from their destructors
// while the program exits.
std::mutex& gLock = *new std::mutex;
size_t gTop = 0;                               // first byte never handed out
size_t gLive = 0;                              // the sizes asked for in the live blocks
void* gFreeLists[kSmallClasses + kPowerClasses];
std::vector<LargeHole>* gHoles = nullptr;

[[noreturn]] void Fail(const char* what, const void* address) {
    std::fprintf(stderr, "arena: %s (%p)\n", what, address);
    std::abort();
}

void Reserve() {
    if (gArenaBase != nullptr) return;
    void* base = mmap(nullptr, kArenaBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
    if (base == MAP_FAILED) Fail("could not reserve the address range", nullptr);
    gArenaBase = static_cast<uint8_t*>(base);
    gTop = 4096;   // offset 0 is the null pointer; the first page stays unused
    gHoles = new std::vector<LargeHole>;
}

// The class of a small or medium size and the bytes a block of that class holds.
int ClassOf(size_t size, size_t& capacity) {
    if (size <= kSmallLimit) {
        const size_t steps = size == 0 ? 1 : (size + kSmallStep - 1) / kSmallStep;
        capacity = steps * kSmallStep;
        return static_cast<int>(steps) - 1;
    }
    int bin = kSmallClasses;
    capacity = 1024;
    while (capacity < size) {
        capacity <<= 1;
        ++bin;
    }
    return bin;
}

void* Carve(size_t bytes) {
    if (bytes > kArenaBytes - gTop) return nullptr;
    void* block = gArenaBase + gTop;
    gTop += bytes;
    return block;
}

Header* HeaderOf(const void* block) {
    return reinterpret_cast<Header*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(block)) - kHeader);
}

}  // namespace

uint8_t* ArenaBase() {
    std::lock_guard<std::mutex> lock(gLock);
    Reserve();
    return gArenaBase;
}

void* ArenaAlloc(size_t size) {
    std::lock_guard<std::mutex> lock(gLock);
    Reserve();
    Header* header = nullptr;
    if (size <= kLargeLimit) {
        size_t capacity;
        const int bin = ClassOf(size, capacity);
        if (gFreeLists[bin] != nullptr) {
            void* block = gFreeLists[bin];
            std::memcpy(&gFreeLists[bin], block, sizeof(void*));
            header = HeaderOf(block);
        } else {
            header = static_cast<Header*>(Carve(kHeader + capacity));
            if (header == nullptr) return nullptr;
        }
        header->bin = static_cast<uint32_t>(bin);
    } else {
        const size_t bytes = (kHeader + size + kLargeStep - 1) / kLargeStep * kLargeStep;
        // A freed large block is taken again when the request fills at least three
        // quarters of it; the smallest such one.
        size_t best = gHoles->size();
        for (size_t i = 0; i < gHoles->size(); ++i) {
            const LargeHole& hole = (*gHoles)[i];
            if (hole.bytes < bytes || hole.bytes - bytes > hole.bytes / 4) continue;
            if (best == gHoles->size() || hole.bytes < (*gHoles)[best].bytes) best = i;
        }
        if (best != gHoles->size()) {
            header = reinterpret_cast<Header*>(gArenaBase + (*gHoles)[best].offset);
            gHoles->erase(gHoles->begin() + static_cast<std::ptrdiff_t>(best));
        } else {
            header = static_cast<Header*>(Carve(bytes));
            if (header == nullptr) return nullptr;
        }
        header->bin = kLargeClass;
    }
    header->size = size;
    header->state = kLive;
    gLive += size;
    return reinterpret_cast<uint8_t*>(header) + kHeader;
}

void* ArenaAllocZeroed(size_t size) {
    void* block = ArenaAlloc(size);
    if (block != nullptr) std::memset(block, 0, size);
    return block;
}

void ArenaFree(void* block) {
    if (block == nullptr) return;
    std::lock_guard<std::mutex> lock(gLock);
    if (gArenaBase == nullptr || static_cast<uint8_t*>(block) < gArenaBase + kHeader ||
        static_cast<uint8_t*>(block) >= gArenaBase + gTop) {
        Fail("freeing a block that is not from the arena", block);
    }
    Header* header = HeaderOf(block);
    if (header->state != kLive) Fail(header->state == kFree ? "block freed twice" : "freeing what is not a block", block);
    header->state = kFree;
    gLive -= header->size;
    if (header->bin == kLargeClass) {
        const size_t bytes = (kHeader + header->size + kLargeStep - 1) / kLargeStep * kLargeStep;
        gHoles->push_back({static_cast<size_t>(reinterpret_cast<uint8_t*>(header) - gArenaBase), bytes});
    } else {
        std::memcpy(block, &gFreeLists[header->bin], sizeof(void*));
        gFreeLists[header->bin] = block;
    }
}

size_t ArenaSize(const void* block) {
    const Header* header = HeaderOf(block);
    if (!InArena(block) || header->state != kLive) Fail("size of what is not a live block", block);
    return header->size;
}

bool InArena(const void* address) {
    const uint8_t* p = static_cast<const uint8_t*>(address);
    return gArenaBase != nullptr && p >= gArenaBase && p < gArenaBase + kArenaBytes;
}

uint32_t ArenaOffset(const void* address) {
    if (!InArena(address)) Fail("an address outside the arena cannot be kept in a 32-bit pointer field", address);
    return static_cast<uint32_t>(static_cast<const uint8_t*>(address) - gArenaBase);
}

size_t ArenaLiveBytes() {
    std::lock_guard<std::mutex> lock(gLock);
    return gLive;
}

size_t ArenaHighWater() {
    std::lock_guard<std::mutex> lock(gLock);
    return gTop;
}

}  // namespace tpw
