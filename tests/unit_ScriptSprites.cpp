// Test library for the script sprites and the script save and load (unit w165; libtpw_unit_ScriptSprites.dylib, tests/test_ScriptSprites.py),
// main app:
//   src/game/Script.cpp, src/game/ScriptOps.cpp  ESP_ForceGoto, ESP_RURunningThis, ESP_Spawn2DCopyOfScriptAt, ESP_Update2DScript, ESP_FixSpriteIDsFromDetail and the
//                          PK_ commands SPAWN, SET_OBJ_ID, SET_OBJ_ACT, PutSpriteVAR, PutSpriteIM, Put3DSpriteIM, RemoveSprite, ResetScale,
//                          ReduceScale, IncreaseScale, ResetRotation, ReduceRotation, IncreaseRotation
//   src/game/Script.cpp     SCR_SaveAllScripts, SCR_LoadAllScripts and the file swaps (namespace ScriptSaveSwap)
//
// Every callee outside the unit is a fake that records its arguments and answers from the test's queues: the allocator (LbMem_Alloc,
// LbMem_Free and the script block pair, which logs the Mac's LbMem_Alloc(0x118); the queue "alloc" can make an allocation fail), the file
// reads and writes (a byte stream in, the chunks written out), the swap helpers of platform/sams_core.cpp (no-ops here, like the host's
// little-endian build; _USwapCopy32 and _USwapDelete log their copy), the sprites' two methods, AssignSlot, SCR_SpawnScript, DrawHLSprite and
// ESP_GetNumberOfSprites. The code array and the globals of the script system are storage here. The blocks the fakes allocate are
// remembered so the test can read them back.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "calllog.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
namespace {
uint32_t gCode[883];
}  // namespace
const uint32_t* gScriptCodeWords = gCode;               // 0x1029f20c
const uint32_t* gScriptCodeWordsEnd = gCode + 883;      // 0x1029ffd8
uint32_t gScriptInitialised;                            // 0x102f7c28
int32_t gScriptTableSize;                               // 0x102f7c14
void* gScriptTable;                                     // 0x102f7c18
tagSCRIPT* gScriptCurrent;                              // 0x102f7c24
uint32_t gScriptCodeBase;                               // 0x102f7c20
int32_t* gScriptVars;                                   // 0x102f7c1c
const uint32_t gScriptStartAddresses[4] = {0x1029fe58, 0x1029fef0, 0x1029ff20, 0x1029ff50};
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
ESPSpriteGroup gESPGroups[kESPGroups];

namespace {

struct Fresh {
    uint64_t address;
    uint32_t size;
};
Fresh gFresh[64];
int32_t gFreshCount;
char gDummy[32][16];            // the sprites (their members are never read)
char gNames[8][8];              // the groups' names
std::vector<uint8_t> gIn;       // the file the reads take from
size_t gInPos;
std::vector<std::vector<uint8_t>> gOut;   // the chunks the writes got

void* Fill(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size);
    std::memset(block, 0xA5, size);
    if (gFreshCount < 64) gFresh[gFreshCount++] = {static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block)), size};
    return block;
}

}  // namespace

// The code array as the Mac's data (the host copies of src/game/Script.cpp, which this library does not link).
uint32_t SCR_CodeWord(uint32_t address) {
    const uint32_t index = (address - kScriptCodeAddress) / 4;
    if (index < static_cast<uint32_t>(gScriptCodeWordsEnd - gScriptCodeWords)) return gScriptCodeWords[index];
    return 0;
}
uint32_t SCR_CodeAddress(const void* code) {
    const uint8_t* byte = static_cast<const uint8_t*>(code);
    const uint8_t* first = reinterpret_cast<const uint8_t*>(gScriptCodeWords);
    const uint8_t* end = reinterpret_cast<const uint8_t*>(gScriptCodeWordsEnd);
    if (byte >= first && byte < end) return kScriptCodeAddress + static_cast<uint32_t>(byte - first);
    return 0;
}

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void* LbMem_Alloc(uint32_t size) {
    if (ret("alloc", 1) == 0) {
        ev("LbMem_Alloc", {static_cast<int64_t>(size), 0});
        return nullptr;
    }
    void* block = Fill(size);
    ev("LbMem_Alloc", {static_cast<int64_t>(size), newlab(block)});
    return block;
}
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }
tagSCRIPT* SCR_NewScriptBlock() {
    if (ret("alloc", 1) == 0) {
        ev("LbMem_Alloc", {static_cast<int64_t>(sizeof(tagSCRIPT)), 0});
        return nullptr;
    }
    void* block = Fill(sizeof(tagSCRIPT));
    ev("LbMem_Alloc", {static_cast<int64_t>(sizeof(tagSCRIPT)), newlab(block)});
    return static_cast<tagSCRIPT*>(block);
}
void SCR_FreeScriptBlock(tagSCRIPT* script) { ev("LbMem_Free", {lab(script)}); }

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    // A short file leaves the rest zero (the Mac's stack would hold garbage there).
    std::memset(buffer, 0, length);
    const size_t left = gInPos < gIn.size() ? gIn.size() - gInPos : 0;
    const size_t n = length < left ? length : left;
    if (n) std::memcpy(buffer, gIn.data() + gInPos, n);
    gInPos += length;
    return TbError(0);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gOut.emplace_back(bytes, bytes + length);
    return TbError(0);
}

namespace SamsUtilities {
void _USwapBlock32(uint32_t* words, int32_t count) {
    (void)words;
    ev("USwapBlock32", {count});
}
uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    uint32_t* copy = static_cast<uint32_t*>(std::malloc(4 * (count > 0 ? count : 1)));
    for (int32_t i = 0; i < count; ++i) copy[i] = words[i];
    ev("USwapCopy32", {count, count > 0 ? static_cast<int64_t>(words[0]) : 0, newlab(copy)});
    return copy;
}
void _USwapDelete(void* copy) {
    ev("USwapDelete", {lab(copy)});
    std::free(copy);
}
}  // namespace SamsUtilities

int32_t AssignSlot(tagSCRIPT* script) {
    ev("AssignSlot", {lab(script)});
    return static_cast<int32_t>(ret("assign", 5));
}
int32_t SCR_SpawnScript(void* code, int32_t value, float x, float y, float z) {
    ev("SCR_SpawnScript", {SCR_CodeAddress(code), value, fbits(x), fbits(y), fbits(z)});
    return static_cast<int32_t>(ret("spawn", 7));
}
void DrawHLSprite(tagSCRIPT* script) { ev("DrawHLSprite", {lab(script)}); }
int32_t ESP_GetNumberOfSprites(ESP_Sprites set) {
    ev("ESP_GetNumberOfSprites", {static_cast<int32_t>(set)});
    return static_cast<int32_t>(ret("nsprites", 3));
}
uint8_t ESP_CSprite::GetActionFrames(int32_t action) const {
    ev("GetActionFrames", {lab(this), action});
    return static_cast<uint8_t>(ret("frames", 0));
}
void ESP_CSprite::GetCustomData(int32_t block, int32_t* first, ACTS* second, int32_t* third, int32_t* fourth) const {
    ev("GetCustomData", {lab(this), block});
    const int64_t mask = ret("cdmask", 15);
    if (mask & 1) *first = static_cast<int32_t>(ret("cd1", 0));
    if (mask & 2) *second = static_cast<ACTS>(ret("cd2", 0));
    if (mask & 4) *third = static_cast<int32_t>(ret("cd3", 0));
    if (mask & 8) *fourth = static_cast<int32_t>(ret("cd4", 0));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sg_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gScriptInitialised", &gScriptInitialised}, {"gScriptTableSize", &gScriptTableSize}, {"gScriptTable", &gScriptTable},
        {"gScriptCurrent", &gScriptCurrent}, {"gScriptCodeBase", &gScriptCodeBase}, {"gScriptVars", &gScriptVars}, {"code", gCode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_sg_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_sg_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// Forgets the blocks the fakes made (the start of a case), the file streams and restores the storage defaults.
void tpw_sg_reset() {
    gFreshCount = 0;
    gIn.clear();
    gInPos = 0;
    gOut.clear();
    std::memset(gCode, 0, sizeof gCode);
    std::memset(gESPGroups, 0, sizeof gESPGroups);
    for (int32_t i = 0; i < 32; ++i) gESPSprites[i] = reinterpret_cast<ESP_CSprite*>(gDummy[i]);
}
int32_t tpw_sg_nfresh() { return gFreshCount; }
uint64_t tpw_sg_fresh(int32_t i) { return gFresh[i].address; }
uint32_t tpw_sg_fresh_size(int32_t i) { return gFresh[i].size; }
uint64_t tpw_sg_sprite(int32_t i) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gESPSprites[i])); }
void tpw_sg_group(int32_t set, int32_t first, int32_t count) {
    std::snprintf(gNames[set], sizeof gNames[set], "grp%d", set);
    gESPGroups[set].mName = gNames[set];
    gESPGroups[set].mFirst = first;
    gESPGroups[set].mCount = count;
}
void tpw_sg_file_in(const uint8_t* data, int32_t length) { gIn.assign(data, data + length); }
int32_t tpw_sg_out_count() { return static_cast<int32_t>(gOut.size()); }
int32_t tpw_sg_out_length(int32_t i) { return static_cast<int32_t>(gOut[i].size()); }
const uint8_t* tpw_sg_out_data(int32_t i) { return gOut[i].data(); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sg_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagSCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mVars));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mSprite));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mHasSprite));
    return n;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t, code arguments as the Mac's: 0 to 3 or an address)
static void* CodePointer(uint32_t address) {
    if (address <= 3) return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    if (address < kScriptCodeAddress || address >= kScriptCodeAddress + 883 * 4) return nullptr;
    return const_cast<uint32_t*>(gCode) + (address - kScriptCodeAddress) / 4;
}
void tpw_sg_ESP_ForceGoto(int32_t handle, uint32_t code) { ESP_ForceGoto(handle, CodePointer(code)); }
int32_t tpw_sg_ESP_RURunningThis(int32_t handle, uint32_t code) { return ESP_RURunningThis(handle, CodePointer(code)); }
int32_t tpw_sg_ESP_Spawn2DCopyOfScriptAt(int32_t handle, float x, float y, float z, float scale) {
    return ESP_Spawn2DCopyOfScriptAt(handle, x, y, z, scale);
}
void tpw_sg_ESP_Update2DScript(int32_t handle, int32_t source) { ESP_Update2DScript(handle, source); }
void tpw_sg_ESP_FixSpriteIDsFromDetail() { ESP_FixSpriteIDsFromDetail(); }
void tpw_sg_SCR_SaveAllScripts(uint64_t file) { SCR_SaveAllScripts(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
int32_t tpw_sg_SCR_LoadAllScripts(uint64_t file) { return SCR_LoadAllScripts(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }
// The commands: 0 SPAWN, 1 SET_OBJ_ID, 2 SET_OBJ_ACT, 3 PutSpriteVAR, 4 PutSpriteIM, 5 Put3DSpriteIM, 6 RemoveSprite, 7 ResetScale,
// 8 ReduceScale, 9 IncreaseScale, 10 ResetRotation, 11 ReduceRotation, 12 IncreaseRotation.
void tpw_sg_PK(int32_t which) {
    void (*const commands[13])() = {PK_SPAWN,     PK_SET_OBJ_ID,   PK_SET_OBJ_ACT,   PK_PutSpriteVAR,   PK_PutSpriteIM,
                                    PK_Put3DSpriteIM, PK_RemoveSprite, PK_ResetScale, PK_ReduceScale, PK_IncreaseScale,
                                    PK_ResetRotation, PK_ReduceRotation, PK_IncreaseRotation};
    commands[which]();
}
// The file swaps, on host memory (addresses as uint64_t); the two pointer swaps take and answer the word.
void tpw_sg_USwapInplace32Int(uint64_t word) { ScriptSaveSwap::USwapInplace32Int(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(word))); }
void tpw_sg_USwapInplace32Long(uint64_t word) { ScriptSaveSwap::USwapInplace32Long(*reinterpret_cast<int32_t*>(static_cast<uintptr_t>(word))); }
void tpw_sg_USwapBlock32SLONGfloat(uint64_t words, int32_t count) {
    ScriptSaveSwap::USwapBlock32SLONGfloat(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(words)), count);
}
void tpw_sg_USwapBlock32Long(uint64_t words, int32_t count) {
    ScriptSaveSwap::USwapBlock32Long(reinterpret_cast<int32_t*>(static_cast<uintptr_t>(words)), count);
}
uint32_t tpw_sg_USwap32LongPtr(uint32_t value) { return ScriptSaveSwap::USwap32LongPtr(value); }
uint32_t tpw_sg_USwap32CharPtr(uint32_t value) { return ScriptSaveSwap::USwap32CharPtr(value); }
void tpw_sg_SpriteRecordByteswap(uint64_t record) { ScriptSaveSwap::SpriteRecordByteswap(reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(record))); }

}  // extern "C"
