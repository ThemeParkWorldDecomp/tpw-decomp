// Test library for the mesh file loader (src/engine/EngineUtil.cpp, src/engine/M3D2Load.cpp, src/game/park/TPHField.cpp; libtpw_unit_M3D2.dylib; see
// tests/test_M3D2.py). The loader runs for real over the test's file bytes; the engine's
// allocator, the four file callbacks and the debug log are recording fakes. The block
// comes from the arena (the file's structures have 32-bit pointer fields), at a distance
// from an 8-byte boundary that the test chooses.
#include <cstdarg>
#include <cstring>

#include "Wavelet.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {
const uint8_t* gFile;
uint32_t gFileSize, gFilePosition;
char gFileHandle;
int32_t gSkew;
void* gArenaBlock;   // what the arena gave
void* gBlock;        // what the loader got: gArenaBlock + gSkew

void* FakeMalloc(uint32_t size) {
    ev("Malloc", {size});
    gArenaBlock = tpw::ArenaAllocZeroed(size + 16);
    gBlock = static_cast<uint8_t*>(gArenaBlock) + gSkew;
    return gBlock;
}
void FakeFree(void*& block) {
    ev("Free", {block == gBlock ? 1 : -1});
    tpw::ArenaFree(gArenaBlock);
    gArenaBlock = gBlock = nullptr;
    block = nullptr;
}
void* FakeOpen(const char* name, const char* mode) {
    ev("Open", {}, {name, mode});
    gFilePosition = 0;
    return ret("Open", 1) ? &gFileHandle : nullptr;
}
int32_t FakeSize(void* file) {
    ev("Size", {file == &gFileHandle});
    return static_cast<int32_t>(gFileSize);
}
uint32_t FakeRead(void* dest, uint32_t size, uint32_t count, void* file) {
    ev("Read", {size, count, file == &gFileHandle,
                static_cast<int64_t>(static_cast<uint8_t*>(dest) - static_cast<uint8_t*>(gBlock))});
    uint32_t bytes = size * count;
    if (bytes > gFileSize - gFilePosition) bytes = gFileSize - gFilePosition;
    std::memcpy(dest, gFile + gFilePosition, bytes);
    gFilePosition += bytes;
    return size != 0 ? bytes / size : 0;
}
uint32_t FakeClose(void* file) {
    ev("Close", {file == &gFileHandle});
    return 0;
}
}  // namespace

TbMallocFn gMalloc = FakeMalloc;
TbFreeFn gFree = FakeFree;
TbFileOpenFn gFileOpen = FakeOpen;
TbFileTellFn gFileTell = FakeSize;
TbFileReadFn gFileRead = FakeRead;
TbFileCloseFn gFileClose = FakeClose;

namespace SamsUtilities {
// The engine's own copy of UDebug is an empty function; here it shows which message the
// loader chose and for which file.
void UDebug(const char* format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const char* name = va_arg(arguments, const char*);
    va_end(arguments);
    ev("UDebug", {}, {format, name});
}
// A little-endian host has the file's words the right way round already.
void _USwapBlock16(uint16_t*, int32_t) {}
void _USwapBlock32(uint32_t*, int32_t) {}
}  // namespace SamsUtilities
void sMatrix::Byteswap() { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&m[0][0]), 16); }

extern "C" {
// Loads `file`. Returns the header's address, 0 for a refused file.
void* tpw_m3_load(const uint8_t* file, uint32_t size, uint32_t flags, int32_t skew, const char* name) {
    gFile = file;
    gFileSize = size;
    gSkew = skew;
    gArenaBlock = gBlock = nullptr;
    char text[64];
    if (name != nullptr) std::strncpy(text, name, sizeof text - 1);
    text[sizeof text - 1] = 0;
    return LoadM3D2Header(name != nullptr ? text : nullptr, flags);
}
// The block the loader got (0 once freed) and the arena's base, to read pointer fields by.
void tpw_m3_free(void* header) { FreeM3D2Header(static_cast<tag_sM3D2Header*>(header)); }
void* tpw_m3_block() { return gBlock; }
void* tpw_m3_arena_base() { return tpw::ArenaBase(); }
void tpw_m3_release() {
    tpw::ArenaFree(gArenaBlock);
    gArenaBlock = gBlock = nullptr;
}
// Keeps the arena's first megabyte away from the test's blocks, so that a file offset can
// never look like a relocated pointer.
void tpw_m3_reserve() {
    static void* reserved = nullptr;
    if (reserved == nullptr) reserved = tpw::ArenaAlloc(2 << 20);
}

void tpw_m3_align_value(uint32_t value, uint32_t moved, int32_t withMoved, uint32_t* out) {
    AlignTo64Bit(value, withMoved ? &moved : nullptr);
    out[0] = value;
    out[1] = moved;
}
// The address as a number: the function only does arithmetic on it.
void tpw_m3_align_address(uint32_t address, uint32_t moved, int32_t withMoved, uint32_t* out) {
    void* pointer = reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    AlignTo64Bit(pointer, withMoved ? &moved : nullptr);
    out[0] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
    out[1] = moved;
}
}
