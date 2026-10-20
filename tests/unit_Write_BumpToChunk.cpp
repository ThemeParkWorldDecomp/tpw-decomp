// Test library for the advisor chunk writer (unit v5247; libtpw_unit_Write_BumpToChunk.dylib, tests/test_Write_BumpToChunk.py), main app:
//   src/host_leaves_Write_BumpToChunk.cpp   Write_BumpToChunk, Write_CloseChunk
//
// The callees outside the unit are fakes recording what the real ones read: LbFile_Write (also keeps the buffer), LbFile_GetPosition,
// LbFile_Seek, TRACE, NS_MacDoze::GlobalFree.
#include <cstdarg>
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

static uint8_t gWrote[0xb0];

// ---- the fakes
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), transferred != nullptr});
    std::memset(gWrote, 0, sizeof gWrote);
    std::memcpy(gWrote, buffer, length < sizeof gWrote ? length : sizeof gWrote);
    if (transferred) *transferred = static_cast<uint32_t>(ret("xfer"));
    return TbError(static_cast<int32_t>(ret("err")));
}

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {lab(file), offset, static_cast<int64_t>(whence)});
    return 0;
}

void TRACE(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int32_t a = va_arg(args, int32_t);
    int32_t b = va_arg(args, int32_t);
    va_end(args);
    ev("TRACE", {a, b}, {format});
}

namespace NS_MacDoze {
InternalHandle* GlobalFree(InternalHandle* handle) {
    ev("GlobalFree", {lab(handle)});
    return nullptr;
}
}  // namespace NS_MacDoze

extern "C" {

uint64_t tpw_wr_addr(const char* name) {
    if (std::strcmp(name, "wrote") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gWrote));
    return 0;
}

void tpw_wr_clear() { std::memset(gWrote, 0, sizeof gWrote); }

uint64_t tpw_wr_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_wr_off(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(address)); }

int32_t tpw_wr_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, file));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, tag));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, dataSize));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, physicalSize));
    out[n++] = static_cast<int64_t>(sizeof(tabBMPCHUNK));
    return n;
}

uint32_t tpw_wr_bump(uint64_t chunk, uint64_t bump) {
    return Write_BumpToChunk(reinterpret_cast<tabBMPCHUNK*>(chunk), *reinterpret_cast<tagBUMP*>(bump));
}

int32_t tpw_wr_close(uint64_t chunk) { return Write_CloseChunk(reinterpret_cast<tabBMPCHUNK*>(chunk)); }

}  // extern "C"
