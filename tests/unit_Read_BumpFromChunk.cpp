// Test library for the advisor save's chunk reader (unit v51017; libtpw_unit_Read_BumpFromChunk.dylib, tests/test_Read_BumpFromChunk.py), main app:
//   src/game/rides/BumperRide.cpp   Read_OpenChunk, Read_DataFromChunk, Read_BumpFromChunk
//
// The callees outside the unit are recording fakes: BMPFile_Read (answers from the queues, fills the buffer with a pattern of the
// seed), LbFile_GetPosition, NS_MacDoze::GlobalAlloc (answers the block the test queued) and TRACE.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "platform/macdoze.h"

using namespace calllog;

void TRACE(const char* format, ...) { ev("TRACE", {}, {format}); }

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

namespace NS_MacDoze {
InternalHandle* GlobalAlloc(uint32_t flags, uint32_t size) {
    ev("GlobalAlloc", {static_cast<int64_t>(flags), static_cast<int64_t>(size)});
    return reinterpret_cast<InternalHandle*>(static_cast<uintptr_t>(ret("GlobalAlloc")));
}
}  // namespace NS_MacDoze

TbError BMPFile_Read(void* file, void* buffer, uint32_t size, uint32_t* transferred) {
    ev("BMPFile_Read", {lab(file), static_cast<int64_t>(size)});
    int32_t result = static_cast<int32_t>(ret("BMPFile_Read.res"));
    uint32_t count = static_cast<uint32_t>(ret("BMPFile_Read.n"));
    int64_t seed = ret("seed");
    if (count > size) count = size;
    uint8_t* bytes = static_cast<uint8_t*>(buffer);
    if (result < 0) count = 0;   // a failed read leaves the buffer alone
    for (uint32_t i = 0; i < count; i++) bytes[i] = static_cast<uint8_t>(seed + i * 13);
    *transferred = count;
    return TbError(result);
}

extern "C" {

uint64_t tpw_rbc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 field holds for an address.
uint32_t tpw_rbc_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_rbc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabBMPCHUNK));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, file));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, pad08));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, tag));
    return n;
}

uint32_t tpw_rbc_open(uint64_t file, uint64_t out) {
    return Read_OpenChunk(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), reinterpret_cast<Ptr32<tabBMPCHUNK>*>(static_cast<uintptr_t>(out)));
}

uint32_t tpw_rbc_data(uint64_t chunk, uint64_t buffer, uint32_t size) {
    return Read_DataFromChunk(reinterpret_cast<tabBMPCHUNK*>(static_cast<uintptr_t>(chunk)), reinterpret_cast<void*>(static_cast<uintptr_t>(buffer)), size);
}

uint32_t tpw_rbc_bump(uint64_t chunk, uint64_t bump) {
    return Read_BumpFromChunk(reinterpret_cast<tabBMPCHUNK*>(static_cast<uintptr_t>(chunk)), *reinterpret_cast<tagBUMP*>(static_cast<uintptr_t>(bump)));
}

}  // extern "C"
