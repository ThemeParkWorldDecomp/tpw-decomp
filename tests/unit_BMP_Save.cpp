// Test library for the advisor save (unit v5630; libtpw_unit_BMP_Save.dylib, tests/test_BMP_Save.py), main app:
//   src/game/rides/BumperRide.cpp   BMP_Save, gBMPSaveTime, gBMPSaveDate
//
// The callees outside the unit (NS_MacDoze::GlobalAlloc, LbFile_GetPosition, LbFile_Write, Write_CloseChunk, Write_BumpToChunk) are
// fakes that record the arguments the real ones read and answer from the test's queues; GlobalAlloc hands out the test's pool blocks.
// The world global 0x102b56c0 (gColList, of other files) is defined here with room for the 0x24 bytes BMP_Save reads.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "BMP_InsideArea.h"
#include "LbFile.h"
#include "platform/macdoze.h"

using namespace calllog;

// ---- globals of other files the unit reads, defined here: gColList by its symbol, with room after the Mac's 0x3c bytes
alignas(8) uint32_t gBumpSaveWorldStorage[16] __asm__("_gColList");

namespace {

uint64_t gPool[128];
int32_t gPoolCount = 0;
int32_t gPoolNext = 0;

// The chunk header OpenChunk writes raw from its pool block (Write_CloseChunk rewrites it in file order later).
bool InPool(const void* buffer) {
    uintptr_t at = reinterpret_cast<uintptr_t>(buffer);
    for (int32_t i = 0; i < gPoolCount; i++)
        if (at >= gPool[i] && at < gPool[i] + 0x18) return true;
    return false;
}

// The bytes of a written buffer (what the file gets) as big-endian words, the PowerPC side's native reading, so both sides
// compare the file's bytes (v5L15: the host no longer swaps the values; the text flag is kept for the event's shape). A raw
// chunk header is the native words on both sides.
void BufferWords(const void* buffer, uint32_t length, bool text, int64_t* out, int32_t* n) {
    (void)text;
    uint8_t bytes[12] = {};
    uint32_t count = length < 12 ? length : 12;
    std::memcpy(bytes, buffer, count);
    bool raw = InPool(buffer);
    for (uint32_t i = 0; i < (count + 3) / 4; i++) {
        uint32_t word = (uint32_t(bytes[4 * i]) << 24) | (uint32_t(bytes[4 * i + 1]) << 16) | (uint32_t(bytes[4 * i + 2]) << 8) |
                        bytes[4 * i + 3];
        if (raw) std::memcpy(&word, bytes + 4 * i, 4);
        out[(*n)++] = word;
    }
}

}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
namespace NS_MacDoze {
InternalHandle* GlobalAlloc(uint32_t flags, uint32_t size) {
    ev("GlobalAlloc", {static_cast<int64_t>(flags), static_cast<int64_t>(size)});
    if (ret("GlobalAlloc", 1) == 0) return nullptr;
    if (gPoolNext >= gPoolCount) {
        ev("POOL_EXHAUSTED", {});
        return nullptr;
    }
    return reinterpret_cast<InternalHandle*>(static_cast<uintptr_t>(gPool[gPoolNext++]));
}
}  // namespace NS_MacDoze

int32_t LbFile_GetPosition(void* file) {
    ev("LbFile_GetPosition", {lab(file)});
    return static_cast<int32_t>(ret("LbFile_GetPosition"));
}

TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    (void)transferred;
    bool text = buffer == gBMPSaveTime || buffer == gBMPSaveDate;
    int64_t words[3];
    int32_t n = 0;
    BufferWords(buffer, length, text, words, &n);
    if (n == 0) ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), text ? 1 : 0});
    else if (n == 1) ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), text ? 1 : 0, words[0]});
    else if (n == 2) ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), text ? 1 : 0, words[0], words[1]});
    else ev("LbFile_Write", {lab(file), static_cast<int64_t>(length), text ? 1 : 0, words[0], words[1], words[2]});
    return TbError(static_cast<int32_t>(ret("LbFile_Write")));
}

int32_t Write_CloseChunk(tabBMPCHUNK* chunk) {
    ev("Write_CloseChunk", {lab(chunk), chunk->start, lab(chunk->file.get()), static_cast<int64_t>(chunk->pad08),
                            static_cast<int64_t>(chunk->tag), chunk->dataSize, chunk->physicalSize});
    return static_cast<int32_t>(ret("Write_CloseChunk"));
}

uint32_t Write_BumpToChunk(tabBMPCHUNK* chunk, tagBUMP& bump) {
    ev("Write_BumpToChunk", {lab(chunk), lab(&bump), chunk->start, lab(chunk->file.get()), static_cast<int64_t>(chunk->pad08),
                             static_cast<int64_t>(chunk->tag), chunk->dataSize, chunk->physicalSize});
    return static_cast<uint32_t>(ret("Write_BumpToChunk"));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_bs_addr(const char* name) {
    if (std::strcmp(name, "world") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gBumpSaveWorldStorage));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_bs_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena base Ptr32 fields count from.
uint64_t tpw_bs_arena_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The blocks GlobalAlloc hands out, in order.
void tpw_bs_pool(const uint64_t* blocks, int32_t count) {
    gPoolCount = count < 128 ? count : 128;
    for (int32_t i = 0; i < gPoolCount; i++) gPool[i] = blocks[i];
    gPoolNext = 0;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_bs_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tabBMPCHUNK));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, file));
    out[n++] = static_cast<int64_t>(offsetof(tabBMPCHUNK, tag));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSAVEWORLD));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPSAVEWORLD, table));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSAVETRACK));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPSAVETRACK, next));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSAVELINK));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPSAVELINK, next));
    out[n++] = static_cast<int64_t>(sizeof(tagBUMPSAVEENTRY));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMPSAVEENTRY, ride));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, col));
    out[n++] = static_cast<int64_t>(offsetof(tagBUMP, owner));
    return n;
}

// ---- the function
int32_t tpw_bs_save(uint64_t file) { return BMP_Save(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

}  // extern "C"
