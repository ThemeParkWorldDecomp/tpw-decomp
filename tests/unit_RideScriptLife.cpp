// Test library for the life of a ride script (unit w169; libtpw_unit_RideScriptLife.dylib, tests/test_RideScriptLife.py), main app:
//   src/game/rides/rsse.cpp   RSSE_InitScript, KillScript, RSSE_KillScript
//
// The callees outside the unit are fakes that record the arguments the real ones read (the same event names and arguments as the
// module's PowerPC stubs) and answer from the test's queues. The globals of other files are defined here. The script records, their
// tables and objects come from the arena (the module fills them by Mac offset); the ride sets are built here (their pointers are native).
#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "Debug.h"
#include "FS_System.h"
#include "HLSprite.h"
#include "LevelInit.h"
#include "RideDB.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here (the program's definitions are in files this library does not compile)
RSSEState gRSSEState;
int32_t gRSSEF7BE0;
uint32_t gRSSEScriptChecksum;
tag_sRideAnimSet* gRideAnimSets[16];
CRideDatabase gRideDatabase;
CThingArrayEntry gThingArray[kThingArraySize];

namespace {

alignas(16) unsigned char gInfoBlock[sizeof(RideInfo) + 0x40];
char gFileObject[16];
const uint8_t* gFileData = nullptr;
int32_t gFileLength = 0;
int32_t gFilePosition = 0;
RSSE_SCRIPT* gKnown[8];   // the scripts RSSE_GetScriptPointer answers, by handle
int32_t gKnownHandle[8];

void PutBits(float* where, int64_t bits) {
    const uint32_t word = static_cast<uint32_t>(bits);
    std::memcpy(where, &word, sizeof(word));
}

constexpr int kSlots = 4;
tag_sRideAnimSet gSets[kSlots];
tag_sMesh gMeshes[kSlots];
tag_sM3D2Header* gHeaders[kSlots];
void* gNodes[kSlots];

// Where a read lands: the offset in the newest record, 0x1000 + the offset of mCode or mSaveBlock when it is the table, else -1.
int64_t Destination(const void* dest) {
    const RSSE_SCRIPT* record = gRSSEState.mScripts;
    if (record == nullptr) return -1;
    const uint8_t* d = static_cast<const uint8_t*>(dest);
    const uint8_t* r = reinterpret_cast<const uint8_t*>(record);
    if (d >= r && d < r + sizeof(RSSE_SCRIPT)) return d - r;
    if (record->mCode.get() != nullptr && dest == record->mCode.get()) return 0x1000 + offsetof(RSSE_SCRIPT, mCode);
    if (record->mSaveBlock.get() != nullptr && dest == record->mSaveBlock.get()) return 0x1000 + offsetof(RSSE_SCRIPT, mSaveBlock);
    return -1;
}

struct ArenaHeader {   // a copy of the arena's block header (platform/arena.cpp)
    uint64_t size;
    uint32_t bin;
    uint32_t state;
};

}  // namespace

// ---- the fakes: one per callee outside the unit, the same event name and arguments as the module's PowerPC stub
void CTRACE(uint32_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int32_t value = va_arg(args, int32_t);
    va_end(args);
    ev("CTRACE", {static_cast<int32_t>(level), value}, {format});
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    gFilePosition = 0;
    return ret("TPFile_Open", 1) != 0 ? reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject) : nullptr;
}
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    ev("TPFile_Read", {Destination(dest), size, count, lab(file)});
    const uint32_t length = size * count;
    const uint32_t remaining = static_cast<uint32_t>(gFileLength - gFilePosition);
    const uint32_t done = std::min(length, remaining);
    if (length != 0) {
        std::memcpy(dest, gFileData + gFilePosition, done);
        std::memset(static_cast<uint8_t*>(dest) + done, 0, length - done);
    }
    gFilePosition += static_cast<int32_t>(done);
    return size != 0 ? done / size : 0;
}
uint32_t TPFile_Close(bfl::FileSystem::CFile* file) {
    ev("TPFile_Close", {lab(file)});
    return 0;
}
// The file's words are little-endian and the host is too: nothing to swap (the event shows the same buffer as the PowerPC side's).
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) { ev("SwapBlock32", {count, Destination(words)}); }

uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID"));
}
void DisableRideHead(tag_sMesh* mesh, uint32_t index) { ev("DisableRideHead", {lab(mesh), index}); }
RideInfo* CRideDatabase::GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id) {
    ev("GetRideInfo", {lab(this), id.mIndex});
    return reinterpret_cast<RideInfo*>(gInfoBlock);
}
int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box) {
    ev("GetRideMasterBoundArea", {ride});
    const int32_t answer = static_cast<int32_t>(ret("grmba", 0));
    if (answer != 0) {
        for (int i = 0; i < 3; ++i) {
            char key[8] = {'b', 'x', static_cast<char>('0' + i), 0};
            PutBits(&box->mMin[i], ret(key));
            key[1] = 'y';
            PutBits(&box->mMax[i], ret(key));
        }
    }
    return answer;
}
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) {
    ev("PAR_InitEmitter", {type, a, b, c});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}
void PAR_SetEmitterRandomPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterRandomPos", {handle, x, y, z}); }
void PAR_SetEmitterLife(int32_t handle, int32_t life) { ev("PAR_SetEmitterLife", {handle, life}); }
int32_t SOUND_AddObject(float a, float b, float c, float d, float e) {
    ev("SOUND_AddObject", {fbits(a), fbits(b), fbits(c), fbits(d), fbits(e)});
    return static_cast<int32_t>(ret("SOUND_AddObject"));
}
void SOUND_DelObject(uint32_t object) { ev("SOUND_DelObject", {object}); }
void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }
void SOUND_FadeSoundEffect(int32_t handle) { ev("SOUND_FadeSoundEffect", {handle}); }
void SND_DipMusic(int32_t level) { ev("SND_DipMusic", {level}); }
void FLY_RemoveBase(int32_t index) { ev("FLY_RemoveBase", {index}); }
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t handle) {
    ev("RSSE_GetScriptPointer", {handle});
    for (int i = 0; i < 8; ++i)
        if (gKnown[i] != nullptr && gKnownHandle[i] == handle) return gKnown[i];
    return nullptr;
}

extern "C" {

uint64_t tpw_rl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"F7BE0", &gRSSEF7BE0}, {"STATE", &gRSSEState}, {"SUM", &gRSSEScriptChecksum}, {"RIDEDB", &gRideDatabase},
        {"FILE", gFileObject},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}
uint64_t tpw_rl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_rl_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }
// 1 while the arena block is live; the size asked for through `size`.
int32_t tpw_rl_live(uint64_t block, uint64_t* size) {
    const ArenaHeader* header = reinterpret_cast<const ArenaHeader*>(static_cast<uintptr_t>(block) - 16);
    *size = header->size;
    return header->state == 0x4c495645u ? 1 : 0;
}
int32_t tpw_rl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(RSSE_SCRIPT));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_OBJECT));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mCode));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mObjects));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mThingId));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFCC));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFE6));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mFB9));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_SCRIPT, mF14));
    out[n++] = static_cast<int64_t>(offsetof(RSSEState, mScripts));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mObjectType));
    out[n++] = static_cast<int64_t>(offsetof(CObject, mF1C));
    out[n++] = static_cast<int64_t>(offsetof(tag_sM3D2Header, mF78));
    out[n++] = static_cast<int64_t>(sizeof(RSSE_WALK));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_WALK, mKind));
    out[n++] = static_cast<int64_t>(offsetof(RSSE_WALK, mState));
    RideInfo* info = reinterpret_cast<RideInfo*>(gInfoBlock);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uint8_t*>(&info->Value(kRideSlot_CreateParticleEffect).mInteger) - gInfoBlock);
    out[n++] = static_cast<int64_t>(reinterpret_cast<uint8_t*>(&info->Value(kRideSlot_DestroyParticleEffect).mInteger) - gInfoBlock);
    return n;
}

void tpw_rl_thing(uint32_t row, uint64_t thing) { gThingArray[row].mThing = reinterpret_cast<void*>(static_cast<uintptr_t>(thing)); }
// The two effect numbers of the ride record (the create and the destroy particle effect).
void tpw_rl_info(int32_t create, int32_t destroy) {
    RideInfo* info = reinterpret_cast<RideInfo*>(gInfoBlock);
    info->Value(kRideSlot_CreateParticleEffect).mInteger = create;
    info->Value(kRideSlot_DestroyParticleEffect).mInteger = destroy;
}
void tpw_rl_file(const uint8_t* data, int32_t length) {
    gFileData = data;
    gFileLength = length;
    gFilePosition = 0;
}
void tpw_rl_known(int32_t index, int32_t handle, uint64_t script) {
    gKnownHandle[index] = handle;
    gKnown[index] = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(script));
}
void tpw_rl_clear() {
    std::memset(static_cast<void*>(gRideAnimSets), 0, sizeof(gRideAnimSets));
    std::memset(static_cast<void*>(gKnown), 0, sizeof(gKnown));
    for (int i = 0; i < 8; ++i) gKnownHandle[i] = 0;
    std::memset(static_cast<void*>(&gRSSEState), 0, sizeof(gRSSEState));
}
// The head of the list is a native pointer on the host.
void tpw_rl_head(uint64_t head) { gRSSEState.mScripts = reinterpret_cast<RSSE_SCRIPT*>(static_cast<uintptr_t>(head)); }
uint64_t tpw_rl_gethead() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gRSSEState.mScripts)); }

// Builds ride set `slot` into gRideAnimSets[slot]: the 20 words d of the node (the header's mF78). Answers the addresses of the set and mesh.
void tpw_rl_ride(int32_t slot, const int64_t* d, uint64_t* out) {
    tpw::ArenaFree(gNodes[slot]);
    tpw::ArenaFree(gHeaders[slot]);
    gNodes[slot] = tpw::ArenaAllocZeroed(0x50);
    gHeaders[slot] = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    uint32_t words[20];
    for (int i = 0; i < 20; ++i) words[i] = static_cast<uint32_t>(d[i]);
    std::memcpy(gNodes[slot], words, sizeof(words));
    gHeaders[slot]->mF78.set(reinterpret_cast<tag_sNode*>(gNodes[slot]));
    std::memset(static_cast<void*>(&gMeshes[slot]), 0, sizeof(gMeshes[slot]));
    gMeshes[slot].mHeader = gHeaders[slot];
    std::memset(static_cast<void*>(&gSets[slot]), 0, sizeof(gSets[slot]));
    gSets[slot].mSelf = &gSets[slot];
    gSets[slot].mHeads = &gMeshes[slot];
    gRideAnimSets[slot] = &gSets[slot];
    out[0] = reinterpret_cast<uintptr_t>(&gSets[slot]);
    out[1] = reinterpret_cast<uintptr_t>(&gMeshes[slot]);
}

// ---- the functions, one wrapper each with the same five integer arguments (pointers as uint64_t)
#define PTR(T, x) reinterpret_cast<T*>(static_cast<uintptr_t>(x))
#define W(x) static_cast<uint64_t>(static_cast<uint32_t>(x))
#define P(x) static_cast<uint64_t>(reinterpret_cast<uintptr_t>(x))

uint64_t tpw_rl_RSSE_InitScript(uint64_t a, uint64_t b, uint64_t c, uint64_t, uint64_t) {
    return W(RSSE_InitScript(PTR(const char, a), static_cast<uint32_t>(b), static_cast<int32_t>(c)));
}
uint64_t tpw_rl_KillScript(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) {
    return P(KillScript(PTR(RSSE_SCRIPT, a), static_cast<int32_t>(b)));
}
uint64_t tpw_rl_RSSE_KillScript(uint64_t a, uint64_t b, uint64_t, uint64_t, uint64_t) {
    RSSE_KillScript(static_cast<int32_t>(a), static_cast<int32_t>(b));
    return 0;
}

}  // extern "C"
