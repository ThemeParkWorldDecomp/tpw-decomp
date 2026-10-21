// Test library for the script interpreter's core (unit w163; libtpw_unit_ScriptInterp.dylib, tests/test_ScriptInterp.py), main app:
//   src/game/Script.cpp, src/game/ScriptOps.cpp   AssignSlot, InitScript, DoScript, SCR_Error, SCR_Push, SCR_Pop, SCR_SpawnScript, the setters and getters,
//                          SCR_WhichScriptHasThisSprite, ESP_InitScript, ESP_SpawnScript and PK_GOTO, NOP, END, WAIT, SETTEXT, SETVAR,
//                          PUSHVAR, POPVAR, GOSUB, RETURN
//
// Every callee outside the unit is a fake that records its arguments and answers from the test's queue: the trace and assertion
// reports, the clock read, the allocator (LbMem_Alloc, LbMem_Free and SCR_NewScriptBlock, which logs the Mac's LbMem_Alloc(0x118)), the
// sprites' two methods and the 32 PK_ commands of other units (each logs the current script's label, position and state, and the queue
// "pk" lets it set the state, move the position or switch the current script). The code array, its check table and the globals of the
// script system are storage here (the program's definitions are in files this library does not compile). The blocks the fakes allocate
// are remembered so the test can read them back.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "calllog.h"
#include "Clock.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "LbFile.h"
#include "platform/arena.h"
#include "analyser.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
namespace {
uint32_t gCode[883];
uint8_t gCheck[883];
}  // namespace
uint8_t* gScriptCheckTable = gCheck;                    // 0x102f7c10
const uint32_t* gScriptCodeWords = gCode;               // 0x1029f20c
const uint32_t* gScriptCodeWordsEnd = gCode + 883;      // 0x1029ffd8
uint32_t gScriptInitialised;                            // 0x102f7c28
uint32_t gScriptF7C2C;                                  // 0x102f7c2c
int32_t gScriptTableSize;                               // 0x102f7c14
void* gScriptTable;                                     // 0x102f7c18
CClock gClock;
ESP_CSprite* gESPSprites[kESPMaxSprites + 16];
ESPSpriteGroup gESPGroups[kESPGroups];

namespace {

struct Fresh {
    uint64_t address;
    uint32_t size;
};
Fresh gFresh[16];
int32_t gFreshCount;
char gDummy[32][16];       // the sprites (their members are never read)
char gNames[8][8];         // the groups' names
void* gSwitchTarget;       // the script the "pk" queue can make current

void* Fill(uint32_t size) {
    void* block = tpw::ArenaAllocZeroed(size);
    std::memset(block, 0xA5, size);
    if (gFreshCount < 16) gFresh[gFreshCount++] = {static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block)), size};
    return block;
}

// What a fake PK_ command does after it logged: the queue "pk" gives bits 0-1 the state (0 keep, 1 waits, 2 never run, 3 finished), bits
// 2-5 how far the position moves and bit 6 a switch of the current script.
void PkEvent(const char* name) {
    tagSCRIPT* current = gScriptCurrent;
    ev(name, {lab(current), current->mPos, current->mState});
    const int64_t v = ret("pk", 0);
    switch (v & 3) {
        case 1: current->mState = 2; break;
        case 2: current->mState = 3; break;
        case 3: current->mState = 4; break;
        default: break;
    }
    current->mPos += static_cast<int32_t>((v >> 2) & 15);
    if ((v >> 6) & 1) gScriptCurrent = static_cast<tagSCRIPT*>(gSwitchTarget);
}

}  // namespace

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void CTRACE(uint32_t level, const char* format, ...) { ev("CTRACE", {static_cast<int64_t>(level)}, {format}); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }
int32_t CClock::Read() {
    ev("Clock");
    return static_cast<int32_t>(ret("clock", 0));
}
void* LbMem_Alloc(uint32_t size) {
    void* block = Fill(size);
    ev("LbMem_Alloc", {static_cast<int64_t>(size), newlab(block)});
    return block;
}
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }
tagSCRIPT* SCR_NewScriptBlock() {
    void* block = Fill(sizeof(tagSCRIPT));
    ev("LbMem_Alloc", {static_cast<int64_t>(sizeof(tagSCRIPT)), newlab(block)});
    return static_cast<tagSCRIPT*>(block);
}
void SCR_FreeScriptBlock(tagSCRIPT* script) { ev("LbMem_Free", {lab(script)}); }
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

#define PKFAKE(N) void N() { PkEvent(#N); }
PKFAKE(PK_IF) PKFAKE(PK_ENDIF) PKFAKE(PK_ELSE) PKFAKE(PK_DO) PKFAKE(PK_LOOP) PKFAKE(PK_REPEAT) PKFAKE(PK_UNTIL) PKFAKE(PK_ADD)
PKFAKE(PK_SUB) PKFAKE(PK_AND) PKFAKE(PK_MUL) PKFAKE(PK_MULVAR) PKFAKE(PK_DIV) PKFAKE(PK_DIVVAR) PKFAKE(PK_MOD) PKFAKE(PK_MODVAR)
PKFAKE(PK_ADDVAR) PKFAKE(PK_COPYVAR) PKFAKE(PK_RANDOM) PKFAKE(PK_SPAWN) PKFAKE(PK_SET_OBJ_ID) PKFAKE(PK_SET_OBJ_ACT)
PKFAKE(PK_PutSpriteVAR) PKFAKE(PK_PutSpriteIM) PKFAKE(PK_Put3DSpriteIM) PKFAKE(PK_RemoveSprite) PKFAKE(PK_ResetScale)
PKFAKE(PK_ReduceScale) PKFAKE(PK_IncreaseScale) PKFAKE(PK_ResetRotation) PKFAKE(PK_ReduceRotation) PKFAKE(PK_IncreaseRotation)

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sj_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gScriptInitialised", &gScriptInitialised}, {"gScriptF7C2C", &gScriptF7C2C}, {"gScriptTableSize", &gScriptTableSize},
        {"gScriptTable", &gScriptTable}, {"gScriptCheckTable", &gScriptCheckTable}, {"gScriptCurrent", &gScriptCurrent},
        {"gScriptCodeBase", &gScriptCodeBase}, {"gScriptVars", &gScriptVars}, {"code", gCode}, {"check", gCheck},
        {"switch", &gSwitchTarget},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_sj_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }
uint64_t tpw_sj_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

// Forgets the blocks the fakes made (the start of a case) and restores the storage defaults.
void tpw_sj_reset() {
    gFreshCount = 0;
    gSwitchTarget = nullptr;
    gScriptCheckTable = gCheck;
    std::memset(gCode, 0, sizeof gCode);
    std::memset(gCheck, 0, sizeof gCheck);
    std::memset(gESPGroups, 0, sizeof gESPGroups);
    for (int32_t i = 0; i < 32; ++i) gESPSprites[i] = reinterpret_cast<ESP_CSprite*>(gDummy[i]);
}
int32_t tpw_sj_nfresh() { return gFreshCount; }
uint64_t tpw_sj_fresh(int32_t i) { return gFresh[i].address; }
uint32_t tpw_sj_fresh_size(int32_t i) { return gFresh[i].size; }
uint64_t tpw_sj_sprite(int32_t i) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gESPSprites[i])); }
void tpw_sj_group(int32_t set, int32_t first, int32_t count) {
    std::snprintf(gNames[set], sizeof gNames[set], "grp%d", set);
    gESPGroups[set].mName = gNames[set];
    gESPGroups[set].mFirst = first;
    gESPGroups[set].mCount = count;
}

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sj_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagSCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mVars));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mSprite));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mHasSprite));
    return n;
}

// ---- the functions, one wrapper each with C types only (pointers as uint64_t, code addresses as the Mac's)
static void* CodePointer(uint32_t address) {
    if (address <= 3) return reinterpret_cast<void*>(static_cast<uintptr_t>(address));
    return const_cast<uint32_t*>(gCode) + (address - kScriptCodeAddress) / 4;
}
static tagSCRIPT* S(uint64_t p) { return reinterpret_cast<tagSCRIPT*>(static_cast<uintptr_t>(p)); }
int32_t tpw_sj_AssignSlot(uint64_t script) { return AssignSlot(S(script)); }
void tpw_sj_InitScript(uint64_t script, uint32_t code, int32_t value, float x, float y, float z) {
    InitScript(S(script), CodePointer(code), value, x, y, z);
}
int32_t tpw_sj_DoScript(uint64_t script) { return DoScript(S(script)); }
void tpw_sj_SCR_Error() { SCR_Error("an error"); }
void tpw_sj_SCR_Push(int32_t value) { SCR_Push(value); }
int32_t tpw_sj_SCR_Pop() { return SCR_Pop(); }
int32_t tpw_sj_SCR_SpawnScript(uint32_t code, int32_t value, float x, float y, float z) {
    return SCR_SpawnScript(CodePointer(code), value, x, y, z);
}
void tpw_sj_SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) { SCR_SetScriptPosition(handle, x, y, z, mode); }
void tpw_sj_SCR_GetScriptPosition(int32_t handle, uint64_t x, uint64_t y, uint64_t z, uint64_t mode) {
    SCR_GetScriptPosition(handle, reinterpret_cast<float*>(static_cast<uintptr_t>(x)), reinterpret_cast<float*>(static_cast<uintptr_t>(y)),
                          reinterpret_cast<float*>(static_cast<uintptr_t>(z)), reinterpret_cast<int32_t*>(static_cast<uintptr_t>(mode)));
}
void tpw_sj_SCR_SetVariable(int32_t handle, uint32_t index, int32_t value) { SCR_SetVariable(handle, index, value); }
void tpw_sj_SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) { SCR_SetFloatVariable(handle, index, value); }
void tpw_sj_SCR_SetServiceInterval(int32_t handle, int32_t period) { SCR_SetServiceInterval(handle, period); }
int32_t tpw_sj_SCR_WhichScriptHasThisSprite(uint64_t sprite) {
    return SCR_WhichScriptHasThisSprite(reinterpret_cast<void*>(static_cast<uintptr_t>(sprite)));
}
void tpw_sj_ESP_InitScript(uint64_t script, uint32_t code, int32_t set, int32_t index, int32_t acts, float x, float y, float z, int32_t a,
                           int32_t b) {
    ESP_InitScript(S(script), CodePointer(code), static_cast<ESP_Sprites>(set), index, static_cast<ACTS>(acts), x, y, z, a, b);
}
int32_t tpw_sj_ESP_SpawnScript(uint32_t code, int32_t set, int32_t index, int32_t acts, float x, float y, float z) {
    return ESP_SpawnScript(CodePointer(code), static_cast<ESP_Sprites>(set), index, static_cast<ACTS>(acts), x, y, z);
}
// The commands: 0 GOTO, 1 NOP, 2 END, 3 WAIT, 4 SETTEXT, 5 SETVAR, 6 PUSHVAR, 7 POPVAR, 8 GOSUB, 9 RETURN.
void tpw_sj_PK(int32_t which) {
    void (*const commands[10])() = {PK_GOTO, PK_NOP, PK_END, PK_WAIT, PK_SETTEXT, PK_SETVAR, PK_PUSHVAR, PK_POPVAR, PK_GOSUB, PK_RETURN};
    commands[which]();
}

}  // extern "C"
