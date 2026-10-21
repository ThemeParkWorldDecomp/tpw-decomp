// Test library for the park entry leaves (libtpw_unit_ParkEntryLeaves.dylib; see tests/test_ParkEntryLeaves.py, unit w114):
//   src/game/World.cpp       CWorld::DoIHaveTheseRides 0x10140fac (and the masks gRidesOwned, gParkRequiredRides)
//   src/game/rides/RideDB.cpp       CRideDatabase::GetFirstRide 0x101547b4, GetNextRide 0x10154800
//   src/game/rides/FlyingRide.cpp  FLY_RemoveBase 0x100781f4
//   src/game/TPEmbed.cpp   DisableRideHead 0x1020039c, TPDestroyEmbeddedData 0x101ffa58
//   src/game/Script.cpp     SCR_KillScript 0x100d9478
//   src/game/park/TPHField.cpp    LoadProceduralTextureSet 0x1005b8fc
//   src/game/MeshLoad.cpp      CorrectOrientationFlagsForDiffProceduralMode 0x100b7fa0
//
// Real under each other: the walk under DoIHaveTheseRides, DisableRideHead and SCR_KillScript under FLY_RemoveBase, the corrector under
// LoadProceduralTextureSet. Fakes that record through calllog: FindDataNodeIndexByID (answers from a queue), DestroyRideInstance,
// PAR_KillEmitter, SOUND_StopSoundEffect, LbMem_Free and MReleaseFreeF (they free nothing: the block's label is recorded, and for a script block the
// script table's entry at that moment), GetMeshPath
// (the case's text or NULL), PT_Load (the path, a hash of it and the mode) and PT_Disable.
//
// The scene the head functions share: two ride sets (instances 1 and 5 of gRideAnimSets), each with a mesh whose embedded set has eight
// records, and the script system's table of 128 words (a script is a word 0x40000000 + 16 * handle, labelled 100 + handle; nothing
// dereferences it).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "HLSprite.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "TPSound.h"
#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "rsse.h"
#include "tb_hfield.h"

using namespace calllog;

// ---- globals of other files
CRideDatabase gRideDatabase;
void* gFlyBases[100];
tag_sRideAnimSet* gRideAnimSets[kMaxRideInstances];
uint32_t gScriptInitialised;
int32_t gScriptTableSize;
void* gScriptTable;
uint32_t gGameOptions;
tag_sM3D2Header* gHeightfieldHeader;
sHField* gHeightfieldCopy;

namespace {

constexpr int32_t kPool = 8;          // ride records
constexpr int32_t kRecords = 8;       // records of an embedded set
constexpr int32_t kTableWords = 128;  // the script table
constexpr int32_t kSetInts = 2 + 2 * kRecords;
constexpr int32_t kSetLabel = 20;     // ride set k is labelled 20 + k
constexpr int32_t kScriptLabel = 100;
constexpr int32_t kFlyLabel = 30;     // the base being removed; 31 the other one
constexpr uint32_t kScriptBase = 0x40000000;

alignas(RideInfo) char gPoolStorage[kPool][sizeof(RideInfo)];
RideInfo& Pool(int32_t k) { return *reinterpret_cast<RideInfo*>(gPoolStorage[k]); }

struct HostSet {
    tag_sRideAnimSet set;
    tag_sMesh mesh;
    sEmbeddedSet embedded;
    sEmbeddedRecord records[kRecords];
};
HostSet gSets[2];
uint32_t gTable[kTableWords];
std::string gPath;                    // the folder GetMeshPath answers
bool gHasPath;
void* gScriptBlocks[kTableWords];     // the script blocks (small arena blocks, label kScriptLabel + handle); gTable holds their Ptr32 offsets
alignas(16) char gFlyBlocks[2][0x122c];
sHField* gFieldBlock;                 // the height field chain, from the arena
sHField* gCopyBlock;
tag_sM3D2Header* gHeaderBlock;

// ---- destroy test objects
alignas(16) char gMeshBlock[sizeof(tag_sMesh)];
alignas(16) char gDestroySet[sizeof(sEmbeddedSet)];
alignas(16) char gRecordsBlock[16];
alignas(16) char gBlocksBlock[16];

uint64_t Fnv(const char* text) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (; *text != 0; ++text) hash = (hash ^ static_cast<uint8_t>(*text)) * 0x100000001b3ull;
    return hash & 0xffffffffull;
}

int32_t PoolIndex(const void* p) {
    if (p == nullptr) return -1;
    for (int32_t k = 0; k < kPool; ++k) {
        if (p == &Pool(k)) return k;
    }
    return -2;
}

}  // namespace

// ---- the fakes
uint32_t FindDataNodeIndexByID(tag_sRideAnimSet* set, uint32_t mask, uint32_t id) {
    ev("FindDataNodeIndexByID", {lab(set), mask, id});
    return static_cast<uint32_t>(ret("FindDataNodeIndexByID", -1));
}
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }
void SOUND_StopSoundEffect(int32_t handle) { ev("SOUND_StopSoundEffect", {handle}); }
// (a script block's free also records the table entry of that script at that moment: the original clears the entry first)
void LbMem_Free(void* block) {
    const uintptr_t word = reinterpret_cast<uintptr_t>(block);
    int64_t entry = -1;
    if (word >= kScriptBase && word < kScriptBase + 16u * kTableWords && (word - kScriptBase) % 16 == 0) entry = gTable[(word - kScriptBase) / 16];
    ev("LbMem_Free", {lab(block), entry});
}
// SCR_KillScript frees a script block through SCR_FreeScriptBlock (host only, src/game/park/analyser.cpp): the same event as the Mac's LbMem_Free
// of the block, with the table entry of that script (as the Mac's word, kScriptBase + 16 * handle, or 0) at that moment
void SCR_FreeScriptBlock(tagSCRIPT* script) {
    int64_t entry = -1;
    for (int32_t h = 0; h < kTableWords; ++h)
        if (script != nullptr && gScriptBlocks[h] == static_cast<void*>(script)) entry = gTable[h] != 0 ? kScriptBase + 16u * static_cast<uint32_t>(h) : 0;
    ev("LbMem_Free", {lab(script), entry});
}
void* MReleaseFreeF(void* block) {
    ev("MReleaseFreeF", {lab(block)});
    return nullptr;
}
char* GetMeshPath() {
    ev("GetMeshPath");
    return gHasPath ? const_cast<char*>(gPath.c_str()) : nullptr;
}
uint32_t PT_Load(char* path, int32_t mode) {
    ev("PT_Load", {mode, static_cast<int64_t>(std::strlen(path)), static_cast<int64_t>(Fnv(path))}, {path});
    return 0;
}
void PT_Disable() { ev("PT_Disable"); }

extern "C" {

// ---- the ride database and the masks

// Ride record k of the pool: the two words DoIHaveTheseRides reads.
void tpw_pe_ride(int32_t k, int32_t bit, int32_t preview) {
    std::memset(static_cast<void*>(&Pool(k)), 0, sizeof(RideInfo));
    Pool(k).Value(kRideSlot_DownloadedRideNumber).mInteger = bit;
    Pool(k).Value(kRideSlot_IsPreviewRide).mInteger = preview;
}

// The database: `count`, the position `cursor`, and slot i holds pool record slots[i] (-1 for NULL); slots from n on are NULL.
void tpw_pe_db(int32_t count, int32_t cursor, const int32_t* slots, int32_t n) {
    std::memset(static_cast<void*>(&gRideDatabase), 0, sizeof(gRideDatabase));
    for (int32_t i = 0; i < n && i < 0x14a; ++i) gRideDatabase.mRides[i] = slots[i] < 0 ? nullptr : &Pool(slots[i]);
    gRideDatabase.mRideCount = count;
    gRideDatabase.mF72C = cursor;
}

// The masks start as given (the owned one is the case's, the required one is not touched) and are read back.
int32_t tpw_pe_do_i_have(const char* required, int32_t previews, uint8_t* owned, int32_t* cursor) {
    std::memset(gParkRequiredRides, 0xc3, 0x20);   // the function must leave the required mask (the next global) alone
    std::memcpy(gRidesOwned, owned, 0x20);
    const int32_t result = reinterpret_cast<CWorld*>(gPoolStorage)->DoIHaveTheseRides(required, previews);
    std::memcpy(owned, gRidesOwned, 0x20);
    *cursor = gRideDatabase.mF72C;
    return result;
}

// The required mask as DoIHaveTheseRides left it (it is primed with 0xc3 bytes by tpw_pe_do_i_have).
void tpw_pe_required_after(uint8_t* out) { std::memcpy(out, gParkRequiredRides, 0x20); }

// GetFirstRide (next == 0) or GetNextRide: the answer's pool index (-1 NULL, -2 other) and the position after.
int32_t tpw_pe_walk(int32_t next, int32_t* cursor) {
    RideInfo* answer = next == 0 ? gRideDatabase.GetFirstRide() : gRideDatabase.GetNextRide();
    *cursor = gRideDatabase.mF72C;
    return PoolIndex(answer);
}

// ---- the scene: the script system and the two ride sets

// sets: for each of the two ride sets kSetInts ints: the embedded set's flags and counter, then each record's flags and script handle.
void tpw_pe_scene(int32_t on, int32_t size, const int32_t* table, const int32_t* sets) {
    gScriptInitialised = static_cast<uint32_t>(on);
    gScriptTableSize = size;
    gScriptTable = gTable;
    for (int32_t h = 0; h < kTableWords; ++h) {
        if (gScriptBlocks[h] == nullptr) gScriptBlocks[h] = tpw::ArenaAllocZeroed(16);
        gTable[h] = table[h] != 0 ? tpw::ArenaOffset(gScriptBlocks[h]) : 0;
        tpw_cl_label(kScriptLabel + h, gScriptBlocks[h]);
    }
    std::memset(gRideAnimSets, 0, sizeof gRideAnimSets);
    for (int32_t s = 0; s < 2; ++s) {
        HostSet& host = gSets[s];
        std::memset(static_cast<void*>(&host), 0, sizeof host);
        host.set.mHeads = &host.mesh;
        host.mesh.mF28 = &host.embedded;
        host.embedded.mRecords = host.records;
        const int32_t* in = sets + s * kSetInts;
        host.embedded.mFlags = static_cast<uint32_t>(in[0]);
        host.embedded.mF08 = static_cast<uint32_t>(in[1]);
        for (int32_t r = 0; r < kRecords; ++r) {
            host.records[r].mFlags = static_cast<uint32_t>(in[2 + 2 * r]);
            host.records[r].mF0C = static_cast<uint32_t>(in[3 + 2 * r]);
        }
        tpw_cl_label(kSetLabel + s, &host.set);
    }
    gRideAnimSets[1] = &gSets[0].set;
    gRideAnimSets[5] = &gSets[1].set;
}

void tpw_pe_scene_get(int32_t* table, int32_t* sets) {
    for (int32_t h = 0; h < kTableWords; ++h) table[h] = gTable[h] != 0 ? static_cast<int32_t>(kScriptBase + 16u * static_cast<uint32_t>(h)) : 0;
    for (int32_t s = 0; s < 2; ++s) {
        const HostSet& host = gSets[s];
        int32_t* out = sets + s * kSetInts;
        out[0] = static_cast<int32_t>(host.embedded.mFlags);
        out[1] = static_cast<int32_t>(host.embedded.mF08);
        for (int32_t r = 0; r < kRecords; ++r) {
            out[2 + 2 * r] = static_cast<int32_t>(host.records[r].mFlags);
            out[3 + 2 * r] = static_cast<int32_t>(host.records[r].mF0C);
        }
    }
}

void tpw_pe_disable_head(int32_t set, uint32_t index) { DisableRideHead(&gSets[set].mesh, index); }
void tpw_pe_kill_script(int32_t handle) { SCR_KillScript(handle); }

// ---- FLY_RemoveBase

// The table of fly bases: entry i is NULL (0), the block being removed (1) or the other block (2); block 0 is `words` (0x122c bytes, as
// words in this machine's order). After the call the block is read back into `words` and the table into `labels` (0, 1, 2).
void tpw_pe_fly(int32_t index, const int32_t* kinds, uint32_t* words) {
    std::memcpy(gFlyBlocks[0], words, 0x122c);
    std::memset(gFlyBlocks[1], 0x5a, 0x122c);
    tpw_cl_label(kFlyLabel, gFlyBlocks[0]);
    tpw_cl_label(kFlyLabel + 1, gFlyBlocks[1]);
    for (int32_t i = 0; i < 100; ++i) gFlyBases[i] = kinds[i] == 0 ? nullptr : gFlyBlocks[kinds[i] - 1];
    // In the host the word after gFlyBases is gRideAnimSets[0] (which no case uses): for the index 100 it holds a live base, so that a
    // bound `index > 100` instead of `index >= 100` would read it as entry 100 and tear the base down (the Mac returns).
    tag_sRideAnimSet* const savedSet0 = gRideAnimSets[0];
    if (index == 100) gRideAnimSets[0] = reinterpret_cast<tag_sRideAnimSet*>(gFlyBlocks[0]);
    FLY_RemoveBase(index);
    gRideAnimSets[0] = savedSet0;
    std::memcpy(words, gFlyBlocks[0], 0x122c);
}

void tpw_pe_fly_table(int32_t* kinds) {
    for (int32_t i = 0; i < 100; ++i) {
        kinds[i] = gFlyBases[i] == nullptr ? 0 : (gFlyBases[i] == gFlyBlocks[0] ? 1 : (gFlyBases[i] == gFlyBlocks[1] ? 2 : 3));
    }
}

// ---- TPDestroyEmbeddedData: a mesh with a set (or none) whose blocks and records may be NULL; labels 60 mesh, 61 set, 62 records,
// 63 blocks. out: the mesh's word, the set's records and blocks as labels (the set's are -1 when there is no set).
void tpw_pe_destroy(int32_t hasSet, int32_t hasRecords, int32_t hasBlocks, int32_t* out) {
    tag_sMesh* mesh = reinterpret_cast<tag_sMesh*>(gMeshBlock);
    std::memset(gMeshBlock, 0, sizeof gMeshBlock);
    sEmbeddedSet* set = reinterpret_cast<sEmbeddedSet*>(gDestroySet);
    std::memset(gDestroySet, 0, sizeof gDestroySet);
    set->mFlags = 0x1234;
    set->mRecords = hasRecords ? reinterpret_cast<sEmbeddedRecord*>(gRecordsBlock) : nullptr;
    set->mF08 = 7;
    set->mBlockCount = 3;
    set->mBlocks = hasBlocks ? reinterpret_cast<uint8_t*>(gBlocksBlock) : nullptr;
    mesh->mF28 = hasSet ? set : nullptr;
    tpw_cl_label(60, mesh);
    tpw_cl_label(61, set);
    tpw_cl_label(62, gRecordsBlock);
    tpw_cl_label(63, gBlocksBlock);
    TPDestroyEmbeddedData(mesh);
    out[0] = lab(mesh->mF28);
    out[1] = lab(set->mRecords);
    out[2] = lab(set->mBlocks);
    out[3] = static_cast<int32_t>(set->mFlags);
    out[4] = static_cast<int32_t>(set->mF08);
    out[5] = static_cast<int32_t>(set->mBlockCount);
}

// ---- the height field chain and the corrector

// Builds the chain from the arena: quads (count of them, as pairs of halfwords) and a copy of `copyCount` quads.
void tpw_pe_hfield(int32_t count, int32_t copyCount, const uint16_t* quads, const uint16_t* copy) {
    tpw::ArenaFree(gFieldBlock);
    tpw::ArenaFree(gCopyBlock);
    tpw::ArenaFree(gHeaderBlock);
    gHeaderBlock = static_cast<tag_sM3D2Header*>(tpw::ArenaAllocZeroed(sizeof(tag_sM3D2Header)));
    gFieldBlock = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    gCopyBlock = static_cast<sHField*>(tpw::ArenaAllocZeroed(sizeof(sHField)));
    sHFieldQuad* q = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(sizeof(sHFieldQuad) * (count > 0 ? count : 1)));
    sHFieldQuad* c = static_cast<sHFieldQuad*>(tpw::ArenaAllocZeroed(sizeof(sHFieldQuad) * (copyCount > 0 ? copyCount : 1)));
    std::memcpy(static_cast<void*>(q), quads, sizeof(sHFieldQuad) * count);
    std::memcpy(static_cast<void*>(c), copy, sizeof(sHFieldQuad) * copyCount);
    gFieldBlock->mQuadCount = static_cast<uint16_t>(count);
    gFieldBlock->mQuads = q;
    gCopyBlock->mQuadCount = static_cast<uint16_t>(copyCount);
    gCopyBlock->mQuads = c;
    gHeaderBlock->mHField = gFieldBlock;
    gHeightfieldHeader = gHeaderBlock;
    gHeightfieldCopy = gCopyBlock;
}

void tpw_pe_hfield_get(int32_t count, int32_t copyCount, uint16_t* quads, uint16_t* copy) {
    std::memcpy(quads, static_cast<const void*>(gFieldBlock->mQuads.get()), sizeof(sHFieldQuad) * count);
    std::memcpy(copy, static_cast<const void*>(gCopyBlock->mQuads.get()), sizeof(sHFieldQuad) * copyCount);
}

void tpw_pe_correct(int32_t mode) { CorrectOrientationFlagsForDiffProceduralMode(mode); }

// LoadProceduralTextureSet with the options word and the mesh folder (NULL for none).
void tpw_pe_proctex(int32_t mode, uint32_t options, const char* path) {
    gGameOptions = options;
    gHasPath = path != nullptr;
    if (path != nullptr) gPath = path;
    LoadProceduralTextureSet(mode);
}

}  // extern "C"
