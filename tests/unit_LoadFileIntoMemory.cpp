// Test library for the ride height map loaders (unit v5127; libtpw_unit_LoadFileIntoMemory.dylib, tests/test_LoadFileIntoMemory.py), main app:
//   src/RideDestroy_LoadFileIntoMemory.cpp   LoadFileIntoMemory, LoadRideHeightMap, GetRideHeightMap
//
// The file system, the allocator, the byte swaps, the Byteswap of a box, GenerateRideHeightMap and SaveRideHeightMap are fakes that record
// what the real ones read; the file, the block and the generated map are kept in globals set by the test.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "FS_System.h"
#include "m3d2.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {
uintptr_t gSet = 0, gGen = 0, gShape = 0, gLast = 0;
std::vector<uint8_t> gData;

int64_t Cls(const void* p) {
    uintptr_t v = reinterpret_cast<uintptr_t>(p);
    if (v == 0) return 0;
    if (v == gLast) return 1;
    if (v == gGen) return 2;
    return 3;
}
int64_t Off(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p) - gLast); }
}  // namespace

// ---- the fakes
bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
    return reinterpret_cast<bfl::FileSystem::CFile*>(static_cast<uintptr_t>(ret("Open")));
}
int32_t TPFile_Size(bfl::FileSystem::CFile* file) {
    ev("Size", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    return static_cast<int32_t>(ret("Size"));
}
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("Alloc", {size, flags});
    if (ret("Alloc") == 0) return nullptr;
    void* p = tpw::ArenaAllocZeroed(size + 0x80);
    gLast = reinterpret_cast<uintptr_t>(p);
    return p;
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("Read", {size, count, static_cast<int64_t>(reinterpret_cast<uintptr_t>(file)), Cls(dest)});
    uint64_t n = static_cast<uint64_t>(size) * count;
    if (n > gData.size()) n = gData.size();
    if (dest != nullptr) std::memcpy(dest, gData.data(), n);
    return static_cast<uint32_t>(ret("Read"));
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("Close", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    return 0;
}
void* MReleaseFreeF(void* block) {
    ev("Free", {Cls(block)});
    return nullptr;
}
sRideHeightMap* GenerateRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, RideShape* shape, int32_t flag) {
    ev("Gen", {reinterpret_cast<uintptr_t>(set) == gSet, w, h, reinterpret_cast<uintptr_t>(shape) == gShape, flag});
    return reinterpret_cast<sRideHeightMap*>(ret("Gen") ? gGen : 0);
}
void SaveRideHeightMap(sRideHeightMap* map, char* path) {
    ev("Save", {Cls(map)}, {path});
}
void sAlignedBox::Byteswap() { ev("Byteswap", {Off(this)}); }
// The swaps record and leave the word as it is on both sides (the test writes the words already in their final form).
void USwapInplace32ULong_1005e658(uint32_t& word) { ev("Swap32", {Off(&word), word}); }
void USwapInplace16UShort_1005e610(uint16_t& half) { ev("Swap16", {Off(&half), half}); }
uint32_t USwap32UCharPtr_1005e5b4(uint32_t word) {
    ev("Swap32p", {word});
    return word;
}


extern "C" {

uint64_t tpw_lfm_addr(const char* name) { return 0; }

uint32_t tpw_lfm_offset(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

uint64_t tpw_lfm_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The fakes' state for a case: the set, the generated map, the shape the call passes and the file's bytes.
void tpw_lfm_state(uint64_t set, uint64_t gen, uint64_t shape, const uint8_t* data, uint32_t len) {
    gSet = static_cast<uintptr_t>(set);
    gGen = static_cast<uintptr_t>(gen);
    gShape = static_cast<uintptr_t>(shape);
    gLast = 0;
    gData.assign(data, data + len);
}
uint64_t tpw_lfm_last() { return gLast; }

// HOST_LAYOUT: the map's fixed offsets first, then the host's pointer-bearing offsets.
int32_t tpw_lfm_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mCells));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mMin));
    out[n++] = static_cast<int64_t>(offsetof(sRideHeightMap, mMax));
    out[n++] = static_cast<int64_t>(sizeof(sRideHeightMap));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeads));
    out[n++] = static_cast<int64_t>(offsetof(tag_sRideAnimSet, mHeightMap));
    out[n++] = static_cast<int64_t>(offsetof(tag_sMesh, mHeader));
    out[n++] = static_cast<int64_t>(sizeof(tag_sRideAnimSet));
    out[n++] = static_cast<int64_t>(sizeof(tag_sMesh));
    return n;
}

uint64_t tpw_lfm_lfm(uint64_t path, int32_t binary) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(LoadFileIntoMemory(reinterpret_cast<char*>(path), binary)));
}
uint64_t tpw_lfm_lrhm(uint64_t set, uint32_t w, uint32_t h, uint64_t path) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        LoadRideHeightMap(reinterpret_cast<tag_sRideAnimSet*>(set), w, h, reinterpret_cast<char*>(path))));
}
void tpw_lfm_grhm(uint64_t set, uint32_t w, uint32_t h, uint64_t path, uint64_t a, int32_t b) {
    GetRideHeightMap(reinterpret_cast<tag_sRideAnimSet*>(set), w, h, reinterpret_cast<char*>(path), reinterpret_cast<void*>(a), b);
}

}  // extern "C"
