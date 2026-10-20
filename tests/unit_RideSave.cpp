// Test library for the ride scripts' save and rsse.cpp's swap helpers (unit w166; libtpw_unit_RideSave.dylib, tests/test_RideSave.py), main app:
//   src/rsse_save.cpp   RSSE_Save and the eleven RsseSwap helpers
//
// Every callee outside the unit is a fake that records what the PowerPC stub of the module records: LbFile_Write (the file, the length, whether a
// bytes-done pointer was given; the bytes go to a list of chunks the test reads back) and the SamsUtilities copy, allocate and delete (the host's
// copy is not swapped, like the platform's on a little-endian host; each block gets a label). The script system's state is storage here. The
// script records, their tables, names and objects are arena blocks the test makes (their links are 32-bit offsets). The thing id's Byteswap is not
// linked: the unit's build does not define TPW_SWAP_FILE_WORDS, so RSSE_Save never calls it.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "Debug.h"
#include "LbFile.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

// ---- the global of another file the unit reads, defined here
RSSEState gRSSEState;   // 0x102f7bec
int32_t gRSSEMallocCount;   // 0x102f7bdc  (RSSE_Load's, unit w167: linked and never called here)
int32_t gRSSEF7BE0;   // 0x102f7be0
tag_sRideAnimSet* gRideAnimSets[16];   // 0x102c79a4

namespace {
std::vector<void*> gArenaBlocks;                // the blocks the test asked for, freed at the next reset
std::vector<std::vector<uint8_t>> gOut;         // the chunks the writes got
}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (blocks as labels)
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gOut.emplace_back(bytes, bytes + length);
    if (transferred != nullptr) *transferred = length;
    return TbError(0);
}

// RSSE_Load's callees (unit w167): linked because the file holds RSSE_Load too, never called by these tests.
TbError LbFile_Read(void*, void*, uint32_t, uint32_t*) { return TbError(0); }
void CTRACE(uint32_t, const char*, ...) {}
void GAME_ASSERTMSG(int32_t, const char*, ...) {}
RSSE_SCRIPT* KillScript(RSSE_SCRIPT*, int32_t) { return nullptr; }
void RSSE_Init() {}
int32_t GetRideMasterBoundArea(uint32_t, sAlignedBox*) { return 0; }
int32_t SOUND_AddObject(float, float, float, float, float) { return 0; }

namespace SamsUtilities {
void _USwapBlock32(uint32_t*, int32_t) {}
uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    uint32_t* copy = static_cast<uint32_t*>(std::malloc(4 * static_cast<size_t>(count > 0 ? count : 1)));
    for (int32_t i = 0; i < count; ++i) copy[i] = words[i];
    ev("USwapCopy32", {count, count > 0 ? static_cast<int64_t>(words[0]) : 0, newlab(copy)});
    return copy;
}
void _USwapDelete(void* copy) {
    ev("USwapDelete", {lab(copy)});
    std::free(copy);
}
void* _USwapAllocate(uint32_t bytes) {
    void* block = std::malloc(bytes > 0 ? bytes : 1);
    std::memset(block, 0xA5, bytes);
    ev("USwapAllocate", {static_cast<int64_t>(bytes), newlab(block)});
    return block;
}
}  // namespace SamsUtilities

extern "C" {

// A zeroed arena block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rv_alloc(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size > 0 ? size : 4);
    gArenaBlocks.push_back(block);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}
uint64_t tpw_rv_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// The start of a case: the arena blocks of the last one are freed, the chunks and the state cleared.
void tpw_rv_reset() {
    for (void* block : gArenaBlocks) tpw::ArenaFree(block);
    gArenaBlocks.clear();
    gOut.clear();
    gRSSEState = RSSEState{};
}
// The state: mInitialised, mF04, mF08, mScriptCount and the head of the list (a host address, 0 for none).
void tpw_rv_state(int32_t initialised, int32_t f04, int32_t f08, int32_t count, uint64_t head) {
    gRSSEState.mInitialised = initialised;
    gRSSEState.mF04 = f04;
    gRSSEState.mF08 = f08;
    gRSSEState.mScriptCount = count;
    gRSSEState.mScripts = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(head));
}
int32_t tpw_rv_out_count() { return static_cast<int32_t>(gOut.size()); }
int32_t tpw_rv_out_length(int32_t i) { return static_cast<int32_t>(gOut[static_cast<size_t>(i)].size()); }
const uint8_t* tpw_rv_out_data(int32_t i) { return gOut[static_cast<size_t>(i)].data(); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rv_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSaveWords));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSaveBlock));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSaveWordCount));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mSaveBlockBytes));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_OBJECT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_WALK));
    return n;
}

int32_t tpw_rv_RSSE_Save(uint64_t file) { return RSSE_Save(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

// The swap helpers on host memory (addresses as uint64_t); the pointer ones take and answer the word.
void tpw_rv_USwapInplace32Int(uint64_t p) { RsseSwap::USwapInplace32Int(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(p))); }
void tpw_rv_USwapInplace32UInt(uint64_t p) { RsseSwap::USwapInplace32UInt(*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(p))); }
void tpw_rv_USwapInplace32ULong(uint64_t p) { RsseSwap::USwapInplace32ULong(*reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(p))); }
void tpw_rv_USwapInplace32Long(uint64_t p) { RsseSwap::USwapInplace32Long(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(p))); }
void tpw_rv_USwapInplace16UShort(uint64_t p) { RsseSwap::USwapInplace16UShort(*reinterpret_cast<uint16_t*>(static_cast<uintptr_t>(p))); }
void tpw_rv_USwapInplace16Short(uint64_t p) { RsseSwap::USwapInplace16Short(*reinterpret_cast<int16_t*>(static_cast<uintptr_t>(p))); }
uint32_t tpw_rv_USwap32ObjectPtr(uint32_t v) { return RsseSwap::USwap32ObjectPtr(v); }
uint32_t tpw_rv_USwap32CharPtr(uint32_t v) { return RsseSwap::USwap32CharPtr(v); }
uint32_t tpw_rv_USwap32WalkPtr(uint32_t v) { return RsseSwap::USwap32WalkPtr(v); }
uint32_t tpw_rv_USwap32LongPtr(uint32_t v) { return RsseSwap::USwap32LongPtr(v); }
uint32_t tpw_rv_USwap32VoidPtr(uint32_t v) { return RsseSwap::USwap32VoidPtr(v); }

}  // extern "C"
