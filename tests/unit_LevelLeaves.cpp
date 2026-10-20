// Test entry points for the leaves under LevelInit2 and LevelDeInit (library unit_LevelLeaves: LLCellMouse.cpp, LLCellClicks.cpp,
// LevelLeaves.cpp, tb_hfield_texrange.cpp). Every callee outside those files is a fake that records what it is given: the
// directory set-up, LoadRideType, GetUniqueID, get_theme_path, DestroyRideInstance and the allocator. The tables and words are the
// unit's own, reached by name (tpw_ll_addr) or through the entry accessors (a host entry holds a native pointer, so it is 16
// bytes, not the Mac's 12); the names the tables point at come from one pool of strings both sides share by index.
#include <cstdio>
#include <cstring>

#include "calllog.h"

#include "Debug.h"
#include "LLCellSystem.h"
#include "LevelSystems.h"
#include "Main.h"
#include "analyser.h"
#include "Quickload.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "tb_hfield.h"

using namespace calllog;

namespace {

constexpr int32_t kPoolSize = 16;
// The name pool (set by the test per case), the theme text, the allocator's blocks, the test-owned counters and texture set.
char gPool[kPoolSize][160];
char gThemeText[1100];
alignas(16) unsigned char gBlocks[4][64];
uint32_t gObjWords[3];
sDTextureSet gTestSet;
sDTextureEntry gTestEntries[2];
alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];

int64_t Number(const void* p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }
int64_t Length(const char* text) { return static_cast<int64_t>(std::strlen(text)); }
// The tail of a folder text (its last 150 characters), where the suffix the loader appended is.
const char* Tail(const char* text) {
    const std::size_t length = std::strlen(text);
    return length > 150 ? text + length - 150 : text;
}
const char* Text(const char* text) { return text != nullptr ? text : "(null)"; }

struct Table {
    sLLCMMesh* entries;
    int32_t count;
};
Table TableOf(int32_t which) {
    switch (which) {
        case 0: return {gLLCMQMeshes, 8};
        case 1: return {gLLCMKTrackMeshes, 20};
        case 2: return {gLLCMWTrackMeshes, 25};
        default: return {gLLCMHoardingMeshes, 28};
    }
}

}  // namespace

// ---- The fakes ----------------------------------------------------------------------------------------------------------------
// get_theme_path records the two size pairs as they are when it is called (the loaders store theirs first).
char* get_theme_path() {
    ev("get_theme_path", {gLLCMQSize[0], gLLCMQSize[1], gLLCMTrackSize[0], gLLCMTrackSize[1]});
    return gThemeText;
}
// The directory set-up: the file, the folder (its head, its tail and its length) and the third argument as a raw number.
int32_t SetupDirectoryForMeshLoad(char* file, char* folder, char* sharedTextureFolder) {
    ev("SetupDirectoryForMeshLoad", {Number(sharedTextureFolder), Length(folder), file != nullptr}, {Text(file), folder, Tail(folder)});
    return static_cast<int32_t>(ret("setup", 1));
}
// What the real one answers: the class base plus the index.
uint32_t GetUniqueID(eIDClass idClass, uint32_t index) {
    ev("GetUniqueID", {idClass, index});
    return static_cast<uint32_t>(idClass) + index;
}
// LoadRideType: the size pair labels (1 the Q pair, 2 the track pair) and the pair's two words as they are at the call.
uint32_t LoadRideType(char* folder, char* name, void* a, void* b, uint32_t flags, uint32_t id, uint32_t c, const char16_t* sign1,
                      const char16_t* sign2, int32_t d, int32_t e) {
    const int32_t* pair = static_cast<const int32_t*>(a);
    ev("LoadRideType", {lab(a), lab(b), pair[0], pair[1], flags, id, c, sign1 != nullptr, sign2 != nullptr, d, e, Length(folder)},
       {Text(name), folder, Tail(folder)});
    return static_cast<uint32_t>(ret("RideType", 7));
}
void DestroyRideInstance(uint32_t instance) { ev("DestroyRideInstance", {instance}); }
// The allocator: the size, the flags and the extruder words as they are at the call (the count, then the two blocks as labels);
// answers the block the case's queue "alloc" names (0 to 3), or NULL for a negative number.
void* MReleaseAllocF(uint32_t size, uint32_t flags) {
    ev("MReleaseAllocF", {size, flags, gExtrudeFaceStoreCount, lab(gExtrudeFaceStoreB), lab(gExtrudeFaceStoreA)});
    const int64_t which = ret("alloc", 0);
    return which < 0 ? nullptr : gBlocks[which & 3];
}

// ---- Entry points ---------------------------------------------------------------------------------------------------------------
extern "C" {

// The address of a named global; NULL for an unknown name.
void* tpw_ll_addr(const char* name) {
    struct Entry {
        const char* name;
        void* address;
    };
    const Entry entries[] = {
        {"gLLCMQSize", gLLCMQSize}, {"gLLCMTrackSize", gLLCMTrackSize}, {"gLLCMIllegalPlacement", &gLLCMIllegalPlacement},
        {"gLLCCClickStackCount", &gLLCCClickStackCount}, {"gExtrudeFaceStoreCount", &gExtrudeFaceStoreCount},
        {"gExtrudeFaceStoreB", &gExtrudeFaceStoreB}, {"gExtrudeFaceStoreA", &gExtrudeFaceStoreA}, {"gBlocks", gBlocks},
        {"gObjWords", gObjWords}, {"gTestSet", &gTestSet}, {"gTestEntries", gTestEntries},
        {"gRideHoardingTextures", &gRideHoardingTextures}, {"gDynamicCellTextures", &gDynamicCellTextures},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return entry.address;
    return nullptr;
}

// One string of the pool (the same strings are allocated on the PowerPC side).
void tpw_ll_set_pool(int32_t index, const char* text) { std::strncpy(gPool[index], text, sizeof gPool[index] - 1); }

// Per-case reset after the call log has begun: the theme text and the allocator's blocks.
void tpw_ll_begin(const char* themeText) {
    std::strncpy(gThemeText, themeText, sizeof gThemeText - 1);
    std::memset(gBlocks, 0xa5, sizeof gBlocks);
}

// Sets a table entry; nameIndex is a pool index, or -1 for NULL.
void tpw_ll_set_entry(int32_t table, int32_t i, uint32_t type, int32_t nameIndex, int32_t b8, int32_t b9, int32_t b10, int32_t b11) {
    sLLCMMesh& entry = TableOf(table).entries[i];
    entry.mRideType = type;
    entry.mName = nameIndex < 0 ? nullptr : gPool[nameIndex];
    entry.mTimeControls = static_cast<uint8_t>(b8);
    entry.mQueueFlag = static_cast<uint8_t>(b9);
    entry.mF0A = static_cast<uint8_t>(b10);
    entry.mF0B = static_cast<uint8_t>(b11);
}

// Reads a table entry: out = {type, name (pool index, -1 NULL, -2 other), b8, b9, b10, b11}; text = the name's text (empty for NULL).
void tpw_ll_get_entry(int32_t table, int32_t i, int64_t* out, char* text) {
    const sLLCMMesh& entry = TableOf(table).entries[i];
    out[0] = entry.mRideType;
    out[1] = entry.mName == nullptr ? -1 : -2;
    for (int32_t k = 0; k < kPoolSize && entry.mName != nullptr; ++k)
        if (entry.mName == gPool[k]) out[1] = k;
    out[2] = entry.mTimeControls;
    out[3] = entry.mQueueFlag;
    out[4] = entry.mF0A;
    out[5] = entry.mF0B;
    std::strncpy(text, entry.mName != nullptr ? entry.mName : "", 159);
    text[159] = 0;
}

// The test-owned texture set DMLoadTextures and DMFreeTextures get: flags, count, folder (pool index or -1), entries (1: the
// test's entry array, 0: NULL).
void tpw_ll_set_testset(uint32_t flags, int32_t count, int32_t folderIndex, int32_t entries) {
    gTestSet.mFlags = flags;
    gTestSet.mCount = static_cast<uint16_t>(count);
    gTestSet.mFolder = folderIndex < 0 ? nullptr : gPool[folderIndex];
    gTestSet.mEntries = entries != 0 ? gTestEntries : nullptr;
}
void tpw_ll_get_testset(int64_t* out) {
    out[0] = gTestSet.mFlags;
    out[1] = gTestSet.mCount;
    out[2] = gTestSet.mFolder == nullptr ? -1 : (gTestSet.mFolder >= gPool[0] && gTestSet.mFolder <= gPool[kPoolSize - 1] ? 1 : -2);
    out[3] = gTestSet.mEntries == nullptr ? 0 : (gTestSet.mEntries == gTestEntries ? 1 : -2);
}

// The three counters SetupObjectInstanceTable gets by reference (test-owned words).
void tpw_ll_set_obj(const uint32_t* words) { std::memcpy(gObjWords, words, sizeof gObjWords); }
void tpw_ll_get_obj(uint32_t* words) { std::memcpy(words, gObjWords, sizeof gObjWords); }

// The functions of the main app (op numbers in the test module); answers the function's own answer where it has one.
int64_t tpw_ll_call(int32_t op, uint32_t a, uint32_t b) {
    switch (op) {
        case 0: LLCM_LoadQMeshes(); return 0;
        case 1: LLCM_LoadKTrackMeshes(); return 0;
        case 2: LLCM_LoadWTrackMeshes(); return 0;
        case 3: LLCM_LoadHoardingMeshes(); return 0;
        case 4: LLCM_DestroyQMeshes(); return 0;
        case 5: LLCM_DestroyKTrackMeshes(); return 0;
        case 6: LLCM_DestroyWTrackMeshes(); return 0;
        case 7: LLCM_DestroyHoardingMeshes(); return 0;
        case 8: LLCM_SetIllegalPlacement(static_cast<int32_t>(a)); return 0;
        case 9: LLCC_SystemResetClickStack(); return 0;
        case 10: LLCC_ResetBuyLandList(); return 0;
        case 11: CreateExtrudeFaceStorage(a, b); return 0;
        case 12: SetupObjectInstanceTable(gObjWords[0], gObjWords[1], gObjWords[2]); return 0;
        case 13: return DMLoadTextures(&gTestSet);
        case 14: DMFreeTextures(&gTestSet); return 0;
    }
    return -1;
}

// The initial data of one of the two static texture sets (which 0: the hoardings', 1: the dynamic cells'): out = {flags, count,
// folder present, entries present}, then per entry {flags, +8, +0xc}; names = the folder text (64 bytes) then the entry names
// (32 bytes each, at most 20).
void tpw_ll_dump_set(int32_t which, int64_t* out, char* names) {
    const sDTextureSet& set = which == 0 ? gRideHoardingTextures : gDynamicCellTextures;
    out[0] = set.mFlags;
    out[1] = set.mCount;
    out[2] = set.mFolder != nullptr;
    out[3] = set.mEntries != nullptr;
    std::memset(names, 0, 64 + 20 * 32);
    if (set.mFolder != nullptr) std::strncpy(names, set.mFolder, 63);
    for (int32_t i = 0; i < set.mCount && i < 20; ++i) {
        out[4 + 3 * i] = set.mEntries[i].mFlags;
        out[5 + 3 * i] = set.mEntries[i].mF08;
        out[6 + 3 * i] = set.mEntries[i].mF0C;
        if (set.mEntries[i].mBlock != nullptr) std::strncpy(names + 64 + 32 * i, static_cast<const char*>(set.mEntries[i].mBlock), 31);
    }
}

// CHField::ProcTextureRange on an object of 0xa5 bytes: out = {mF234, mF238, mF23C, mF240, mPad244 as a word, the first word of
// mPrepareEntries}.
void tpw_ll_proc_range(const int32_t* args, uint32_t* out) {
    std::memset(gFieldStorage, 0xa5, sizeof gFieldStorage);
    CHField& field = *reinterpret_cast<CHField*>(gFieldStorage);
    field.ProcTextureRange(args[0], args[1], args[2], args[3]);
    out[0] = static_cast<uint32_t>(field.mF234);
    out[1] = static_cast<uint32_t>(field.mF238);
    out[2] = static_cast<uint32_t>(field.mF23C);
    out[3] = static_cast<uint32_t>(field.mF240);
    std::memcpy(&out[4], field.mPad244, 4);
    std::memcpy(&out[5], &field.mPrepareEntries[0], 4);
}

}  // extern "C"
