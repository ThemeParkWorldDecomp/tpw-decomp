// Test library for the ride scripts' load (unit w167; libtpw_unit_RideLoad.dylib, tests/test_RideLoad.py), main app:
//   src/game/rides/rsse.cpp   RSSE_Load and RSSE_WALK::Byteswap (and w166's RSSE_Save, linked and not called)
//
// Every callee outside the unit is a fake that records what the PowerPC stub of the module records: LbFile_Read (the file, the length, whether a
// transferred-bytes pointer was given; the bytes come from the test's stream, zeros past its end), CTRACE and GAME_ASSERTMSG (the texts), KillScript
// (the script's handle and the flag; answers the script's next), RSSE_Init (sets the state to its start values), SamsUtilities::_USwapBlock32 (the
// count and whether a block was given; the host does not swap, like the platform's on a little-endian host), GetRideMasterBoundArea and
// SOUND_AddObject (answers from the queues). The script system's state and the other globals are storage here. The unit's build does not define
// TPW_SWAP_FILE_WORDS: the load swaps nothing and never calls RSSE_WALK::Byteswap or the thing id's swap.
#include <cstdarg>
#include <cstddef>
#include <cstdint>
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

// ---- the globals of other files the unit uses, defined here
RSSEState gRSSEState;   // 0x102f7bec
int32_t gRSSEMallocCount;   // 0x102f7bdc
int32_t gRSSEF7BE0;   // 0x102f7be0
tag_sRideAnimSet* gRideAnimSets[16];   // 0x102c79a4

namespace {
std::vector<void*> gArenaBlocks;       // the blocks the test asked for, freed at the next reset
std::vector<uint8_t> gStream;          // the file
size_t gPos = 0;
bool gLeakNames = false;               // the test left a stale word in a record's name: do not free it

constexpr int kSlots = 4;
tag_sRideAnimSet gSets[kSlots];
tag_sMesh gMeshes[kSlots];
tag_sM3D2Header* gHeaders[kSlots];
void* gNodes[kSlots];

void FreeBlock(const void* block) {
    if (block != nullptr) tpw::ArenaFree(const_cast<void*>(block));
}
}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    uint8_t* out = static_cast<uint8_t*>(buffer);
    for (uint32_t i = 0; i < length; ++i) out[i] = gPos + i < gStream.size() ? gStream[gPos + i] : 0;
    gPos += length;
    if (transferred != nullptr) *transferred = length;
    return TbError(0);
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int32_t>(level), value}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
RSSE_SCRIPT* KillScript(RSSE_SCRIPT* script, int32_t flag) {
    ev("KillScript", {script->mHandle, flag});
    return script->mNext.get();
}
void RSSE_Init() {
    ev("RSSE_Init");
    gRSSEState.mInitialised = 1;
    gRSSEState.mF04 = 0;
    gRSSEState.mF08 = 1;
    gRSSEState.mScriptCount = 0;
    gRSSEState.mScripts = nullptr;
}
int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    ev("GetRideMasterBoundArea", {ride});
    const int32_t answer = static_cast<int32_t>(ret("grmba", 0));
    if (answer != 0) {
        for (int i = 0; i < 3; ++i) {
            char key[8] = {'b', 'x', static_cast<char>('0' + i), 0};
            const uint32_t lo = static_cast<uint32_t>(ret(key));
            key[1] = 'y';
            const uint32_t hi = static_cast<uint32_t>(ret(key));
            std::memcpy(&box->mMin[i], &lo, 4);
            std::memcpy(&box->mMax[i], &hi, 4);
        }
    }
    return answer;
}
int32_t SOUND_AddObject(float a, float b, float c, float d, float e) {
    ev("SOUND_AddObject", {fbits(a), fbits(b), fbits(c), fbits(d), fbits(e)});
    return static_cast<int32_t>(ret("SOUND_AddObject", 0));
}

// RSSE_Save's callees: linked (the file holds RSSE_Save too) and never called.
TbError LbFile_Write(void*, const void*, uint32_t, uint32_t*) { return TbError(0); }

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) { ev("USwapBlock32", {count, words != nullptr}); }
uint32_t* _USwapCopy32(const uint32_t*, int32_t) { return nullptr; }
void _USwapDelete(void*) {}
void* _USwapAllocate(uint32_t) { return nullptr; }
}  // namespace SamsUtilities

extern "C" {

// A zeroed arena block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_rw_alloc(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size > 0 ? size : 4);
    gArenaBlocks.push_back(block);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block));
}
uint64_t tpw_rw_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }
uint32_t tpw_rw_blocksize(uint64_t block) { return static_cast<uint32_t>(tpw::ArenaSize(reinterpret_cast<void*>(static_cast<uintptr_t>(block)))); }

// The start of a case: what the last load made (walked from the state's list) and the blocks of the last one are freed, the state cleared.
void tpw_rw_reset() {
    for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr;) {
        RSSE_SCRIPT* next = script->mNext.get();
        RSSE_OBJECT* object = script->mObjects.get();
        while (object != nullptr) {
            RSSE_OBJECT* following = object->mNext.get();
            FreeBlock(object);
            object = following;
        }
        FreeBlock(script->mCode.get());
        FreeBlock(script->mVars.get());
        FreeBlock(script->mStack.get());
        FreeBlock(script->mLimbo.get());
        FreeBlock(script->mBounce.get());
        FreeBlock(script->mWalk.get());
        FreeBlock(script->mSaveWords.get());
        FreeBlock(script->mSaveBlock.get());
        if (!gLeakNames) FreeBlock(script->mName.get());
        FreeBlock(script);
        script = next;
    }
    for (void* block : gArenaBlocks) tpw::ArenaFree(block);
    gArenaBlocks.clear();
    gRSSEState = RSSEState{};
    gRSSEMallocCount = 0;
    gRSSEF7BE0 = 0;
    gStream.clear();
    gPos = 0;
    gLeakNames = false;
    std::memset(static_cast<void*>(gRideAnimSets), 0, sizeof(gRideAnimSets));
}
void tpw_rw_leaknames(int32_t leak) { gLeakNames = leak != 0; }
void tpw_rw_stream(const uint8_t* data, uint32_t length) { gStream.assign(data, data + length); gPos = 0; }

// The state before the call: mInitialised, mF04, mF08, mScriptCount and the head of the list (a host address, 0 for none).
void tpw_rw_state(int32_t initialised, int32_t f04, int32_t f08, int32_t count, uint64_t head) {
    gRSSEState.mInitialised = initialised;
    gRSSEState.mF04 = f04;
    gRSSEState.mF08 = f08;
    gRSSEState.mScriptCount = count;
    gRSSEState.mScripts = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(head));
}
// The state after the call: its four words, then the list head (a host address).
uint64_t tpw_rw_state_out(int32_t* words) {
    words[0] = gRSSEState.mInitialised;
    words[1] = gRSSEState.mF04;
    words[2] = gRSSEState.mF08;
    words[3] = gRSSEState.mScriptCount;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRSSEState.mScripts));
}
void tpw_rw_globals(int32_t mallocCount, int32_t f7be0) {
    gRSSEMallocCount = mallocCount;
    gRSSEF7BE0 = f7be0;
}
void tpw_rw_globals_out(int32_t* out) {
    out[0] = gRSSEMallocCount;
    out[1] = gRSSEF7BE0;
}

// Ride set `slot` (1 to 3): a set, its mesh and header, and the header's node mF78 with the float at +0x44 written from its bits.
void tpw_rw_ride(int32_t slot, uint32_t heightBits) {
    tpw::ArenaFree(gNodes[slot]);
    tpw::ArenaFree(gHeaders[slot]);
    gNodes[slot] = tpw::ArenaAllocZeroed(0x50);
    gHeaders[slot] = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    std::memcpy(static_cast<uint8_t*>(gNodes[slot]) + 0x44, &heightBits, 4);
    gHeaders[slot]->mF78.set(reinterpret_cast<tag_sNode*>(gNodes[slot]));
    std::memset(static_cast<void*>(&gMeshes[slot]), 0, sizeof(gMeshes[slot]));
    gMeshes[slot].mHeader = gHeaders[slot];
    std::memset(static_cast<void*>(&gSets[slot]), 0, sizeof(gSets[slot]));
    gSets[slot].mSelf = &gSets[slot];
    gSets[slot].mHeads = &gMeshes[slot];
    gRideAnimSets[slot] = &gSets[slot];
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_rw_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_OBJECT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_WALK));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mHandle));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFCC));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    return n;
}

int32_t tpw_rw_RSSE_Load(uint64_t file) { return RSSE_Load(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
void tpw_rw_Byteswap(uint64_t record) { reinterpret_cast<RSSE_WALK*>(static_cast<uintptr_t>(record))->Byteswap(); }

}  // extern "C"
