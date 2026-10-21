// Test library for the script interpreter's flow and arithmetic (unit w164; libtpw_unit_ScriptFlow.dylib, tests/test_ScriptFlow.py), main app:
//   src/game/Script.cpp, src/game/ScriptOps.cpp   SCR_FindNextEndif, SCR_FindNextElseOrEndif and PK_IF, ENDIF, ELSE, REPEAT, UNTIL, DO, LOOP, ADD, SUB, MUL, MULVAR,
//                        DIV, DIVVAR, MOD, AND, MODVAR, ADDVAR, COPYVAR, RANDOM
//
// Callees outside the unit are fakes that record what the real ones read: SCR_Error (the message), SCR_Push (the value), SCR_Pop (answers
// from the queue "pop") and CWorld::GetRand (the world's label; answers from the queue "GetRand"). The script system's globals and the code
// array are storage here (the program's definitions are in files this library does not compile); SCR_CodeWord is ScriptInterp.cpp's.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "World.h"
#include "analyser.h"
#include "calllog.h"
#include "platform/arena.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
namespace {
uint32_t gCode[883];
}  // namespace
const uint32_t* gScriptCodeWords = gCode;               // 0x1029f20c
const uint32_t* gScriptCodeWordsEnd = gCode + 883;      // 0x1029ffd8
tagSCRIPT* gScriptCurrent;                              // 0x102f7c24
uint32_t gScriptCodeBase;                               // 0x102f7c20
int32_t* gScriptVars;                                   // 0x102f7c1c
CWorld* gGameWorld;                                     // 0x1037ebfc

uint32_t SCR_CodeWord(uint32_t address) {
    const uint32_t index = (address - kScriptCodeAddress) / 4;
    if (index < static_cast<uint32_t>(gScriptCodeWordsEnd - gScriptCodeWords)) return gScriptCodeWords[index];
    return 0;
}

// ---- the fakes: the same event name and arguments as the module's PowerPC stub (pointers as lab(p))
void SCR_Error(const char* message) { ev("SCR_Error", {}, {message}); }
void SCR_Push(int32_t value) { ev("SCR_Push", {value}); }
int32_t SCR_Pop() {
    ev("SCR_Pop");
    return static_cast<int32_t>(ret("pop", 0));
}
uint32_t CWorld::GetRand() {
    ev("GetRand", {lab(this)});
    return static_cast<uint32_t>(ret("GetRand", 0));
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_sk_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gScriptCurrent", &gScriptCurrent}, {"gScriptCodeBase", &gScriptCodeBase}, {"gScriptVars", &gScriptVars},
        {"gGameWorld", &gGameWorld}, {"code", gCode},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_sk_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_sk_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(tagSCRIPT));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mPos));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mCodeBase));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mIfDepth));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mLoopDepth));
    out[n++] = static_cast<int64_t>(offsetof(tagSCRIPT, mVars));
    out[n++] = static_cast<int64_t>(kScriptCodeEnd);
    return n;
}

// ---- the functions: 0 FindNextEndif, 1 FindNextElseOrEndif, then PK_ IF, ENDIF, ELSE, REPEAT, UNTIL, DO, LOOP, ADD, SUB, MUL, MULVAR, DIV,
// DIVVAR, MOD, AND, MODVAR, ADDVAR, COPYVAR, RANDOM
void tpw_sk_call(int32_t which) {
    void (*const functions[21])() = {SCR_FindNextEndif, SCR_FindNextElseOrEndif, PK_IF,  PK_ENDIF, PK_ELSE,   PK_REPEAT, PK_UNTIL,
                                     PK_DO,             PK_LOOP,                 PK_ADD, PK_SUB,   PK_MUL,    PK_MULVAR, PK_DIV,
                                     PK_DIVVAR,         PK_MOD,                  PK_AND, PK_MODVAR, PK_ADDVAR, PK_COPYVAR, PK_RANDOM};
    functions[which]();
}

}  // extern "C"
