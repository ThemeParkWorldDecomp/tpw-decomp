// Script.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Clock.h"
#include "Debug.h"
#include "DynamicMesh.h"
#include "ESP_sprites.h"
#include "helpers/ScriptHelpers.h"
#include "helpers/ScriptOpsHelpers.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "Ptr32.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "World.h"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w163: the script interpreter's core. A script is a tagSCRIPT block
// (0x118 bytes) whose handle is its slot in gScriptTable; its program is a run of 32-bit words in ONE code array of the Mac's data segment
// (kScriptCodeAddress, 883 words): an instruction is the address of a command descriptor (42 of them from kScriptCommandAddress, 8 bytes
// each, the transition vector of a PK_ function) followed by its operand words (numbers, variable indexes, or code addresses for the jumps).
//
// Host design (area note ride-scripts 6.3): code addresses stay Mac addresses (uint32_t) in every 32-bit place, so the code words stay the
// Mac's. The array itself is gScriptCodeWords (src/ScriptCheckTable.cpp, the image's words since unit v5L39); SCR_CodeWord reads
// it by Mac address and SCR_CodeAddress turns a host pointer into it back into one. DoScript calls the PK_ functions through a table in
// descriptor order. HOST GUARD: a check table index outside the array stops DoScript as a 0 check byte would (the Mac reads whatever
// follows the table), so a script started on a block that is not in the array (the host copies of src/MapDelta_save.cpp and
// src/Mouse_staff.cpp) never runs on the host. Opcode functions 9 to 41 that are not translated yet are called through their stand-ins.

tagSCRIPT* gScriptCurrent;   // 0x102f7c24
uint32_t gScriptCodeBase;    // 0x102f7c20
int32_t* gScriptVars;        // 0x102f7c1c

// 0x1029ffdc: the start addresses of the scripts a sprite's custom data block names (ESP_SpawnScript).
const uint32_t gScriptStartAddresses[4] = {0x1029fe58, 0x1029fef0, 0x1029ff20, 0x1029ff50};

namespace {
constexpr uint32_t kPKEndWord = 0x102669b8;   // PK_END's descriptor (TOC 0x10261130)

// The command functions in descriptor order.
void (*const kCommands[42])() = {
    PK_GOTO,    PK_IF,       PK_ENDIF,       PK_ELSE,        PK_GOSUB,        PK_RETURN,     PK_DO,           PK_PUSHVAR,
    PK_POPVAR,  PK_LOOP,     PK_REPEAT,      PK_UNTIL,       PK_END,          PK_WAIT,       PK_NOP,          PK_SETTEXT,
    PK_SETVAR,  PK_ADD,      PK_SUB,         PK_AND,         PK_MUL,          PK_MULVAR,     PK_DIV,          PK_DIVVAR,
    PK_MOD,     PK_MODVAR,   PK_ADDVAR,      PK_COPYVAR,     PK_RANDOM,       PK_SPAWN,      PK_SET_OBJ_ID,   PK_SET_OBJ_ACT,
    PK_PutSpriteVAR, PK_PutSpriteIM, PK_Put3DSpriteIM, PK_RemoveSprite, PK_ResetScale, PK_ReduceScale, PK_IncreaseScale,
    PK_ResetRotation, PK_ReduceRotation, PK_IncreaseRotation};

// The table's entry for `index` (a 32-bit word the table is indexed by, re-read from the global each time as the Mac does).
Ptr32<tagSCRIPT> Entry(int32_t index) { return static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index]; }

// The script of a handle, as the setters and getters find it: 1 to the table's size - 1 (signed), else none.
tagSCRIPT* ScriptOfHandle(int32_t handle) {
    if (handle > 0 && handle < gScriptTableSize) return Entry(handle);
    return nullptr;
}

float WordFloat(int32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

// The start-up both InitScript and ESP_InitScript share, in the order of their listings.
}  // namespace

// The code word at a Mac code address.
uint32_t SCR_CodeWord(uint32_t address) {
    const uint32_t index = (address - kScriptCodeAddress) / 4;
    if (index < static_cast<uint32_t>(gScriptCodeWordsEnd - gScriptCodeWords)) return gScriptCodeWords[index];
    return 0;
}

// The Mac code address of a host pointer into the code array.
uint32_t SCR_CodeAddress(const void* code) {
    const uint8_t* byte = static_cast<const uint8_t*>(code);
    const uint8_t* first = reinterpret_cast<const uint8_t*>(gScriptCodeWords);
    const uint8_t* end = reinterpret_cast<const uint8_t*>(gScriptCodeWordsEnd);
    if (byte >= first && byte < end) return kScriptCodeAddress + static_cast<uint32_t>(byte - first);
    return 0;
}

// 0x100d8630 (228 bytes): puts the script in the first empty entry from 1 and answers the handle (also written to mSlot). A full table
// grows by 0x32 words first (a new block, zeroed, the old words copied, the old block freed) and the search runs again from 1.
int32_t AssignSlot(tagSCRIPT* script) {
    for (;;) {
        int32_t handle = 1;
        while (handle < gScriptTableSize) {
            Ptr32<tagSCRIPT>& entry = static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[handle];
            if (entry.mOffset == 0) {
                entry = script;
                script->mSlot = handle;
                return handle;
            }
            ++handle;
        }
        gScriptTableSize = gScriptTableSize + 0x32;
        void* grown = LbMem_Alloc(static_cast<uint32_t>(gScriptTableSize) * 4);
        std::memset(grown, 0, static_cast<size_t>(static_cast<uint32_t>(gScriptTableSize) * 4));
        std::memcpy(grown, gScriptTable, static_cast<size_t>(static_cast<uint32_t>(gScriptTableSize - 0x32) * 4));
        LbMem_Free(gScriptTable);
        gScriptTable = grown;
    }
}

// 0x100d8740 (4 bytes): an empty function in this build (the game's script error report was compiled out).
void SCR_Error(const char*) {}

// 0x100d8768 (280 bytes): sets a script block up to run the code at `code`: the state 1, the stack empty, the position the code's index
// from kScriptCodeAddress, the position x, y, z and the variables' defaults, the period 0x3e and the next time one period after the clock.
// `value` goes to variable 11. The trace line is a developer joke the original keeps.
void InitScript(tagSCRIPT* script, void* code, int32_t value, float x, float y, float z) {
    const uint32_t address = SCR_CodeAddress(code);
    CTRACE(6, "##### (InitScript) GONNA CRASH IN A SEC !!!!!\n");
    script->mCodeBase = kScriptCodeAddress;
    script->mStackTop = 0x14;
    script->mPos = PositionOf(address, script->mCodeBase);
    script->mState = 1;
    script->mIfDepth = 0;
    script->mGosubDepth = 0;
    script->mLoopDepth = 0;
    script->mStartPos = script->mPos;
    script->mVars[11] = value;
    script->mVars[1] = FloatWord(x);
    script->mVars[2] = FloatWord(y);
    script->mVars[3] = FloatWord(z);
    script->mVars[4] = FloatWord(0.0f);
    script->mVars[5] = FloatWord(0.0f);
    script->mVars[6] = FloatWord(0.0f);
    script->mVars[7] = 0xff;
    script->mVars[8] = FloatWord(1.0f);
    script->mVars[9] = FloatWord(1.0f);
    script->mVars[15] = 0;
    script->mVars[16] = 0;
    std::memset(script->mSprite + 0x28, 0, 4);
    script->mHasSprite = 0;
    script->mPeriod = 0x3e;
    script->mNextTime = static_cast<int32_t>(static_cast<uint32_t>(script->mPeriod) + static_cast<uint32_t>(gClock.Read()));
}

// 0x100d88b4 (276 bytes): runs the script's instructions until it stops waiting or finishes or the cap (gScriptF7C2C) is reached, and
// answers how many it ran. A script in state 3 (never run) answers 0. gScriptCurrent is read again after every instruction (a PK_
// function may spawn another script and move it). The instruction is skipped, and the run ends, when its check byte is 0.
int32_t DoScript(tagSCRIPT* script) {
    int32_t count = 0;
    gScriptCurrent = script;
    gScriptCodeBase = script->mCodeBase;
    gScriptVars = script->mVars;
    if (script->mState == 3) return count;
    script->mState = 1;
    for (;;) {
        tagSCRIPT* current = gScriptCurrent;
        ++count;
        const uint32_t address = gScriptCodeBase + static_cast<uint32_t>(current->mPos) * 4;
        const uint32_t word = SCR_CodeWord(address);
        const int32_t index = static_cast<int32_t>(address - kScriptCodeAddress) / 4;   // srawi + addze: rounds toward zero
        current->mLastPos = current->mPos;
        gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
        // The host guard: an index outside the table (or no table) counts as a 0 byte.
        const bool inTable = gScriptCheckTable != nullptr && index >= 0 && index < gScriptCodeWordsEnd - gScriptCodeWords;
        if (!inTable || gScriptCheckTable[index] == 0) return count;
        const uint32_t command = (word - kScriptCommandAddress) / 8;
        if ((word - kScriptCommandAddress) % 8 != 0 || command >= 42) {
            std::fprintf(stderr, "DoScript: code word 0x%08x is not a command descriptor\n", word);
            std::abort();
        }
        kCommands[command]();
        if (word == kPKEndWord) {
            gScriptCurrent->mState = 4;
            gScriptCurrent->mHasSprite = 0;
            return count;
        }
        if (gScriptCurrent->mState != 1) return count;
        if (static_cast<uint32_t>(count) >= gScriptF7C2C) return count;
    }
}

// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w164: the script interpreter's flow and arithmetic. The commands IF, ENDIF,
// ELSE, REPEAT, UNTIL, DO and LOOP, the two finders that skip code, and the arithmetic commands ADD, SUB, MUL, MULVAR, DIV, DIVVAR, MOD,
// AND, MODVAR, ADDVAR, COPYVAR and RANDOM. The model is unit w163's (src/ScriptInterp.cpp): every command reads gScriptCurrent,
// gScriptCodeBase and gScriptVars afresh, and an operand is the code word at gScriptCodeBase + 4 * mPos followed by ++mPos.
//
// Tests: a variable index is only ever 0 to 0x12 (the Mac writes outside the block for a larger one). Variables 1 to 6 are floats for the
// arithmetic commands and for IF / UNTIL (their "destination - 1 < 6" test), all others are integers.

namespace {

constexpr uint32_t kElseWord = 0x10266970;

}  // namespace

// 0x100d89f4 (432 bytes): moves mPos to the next ENDIF at the current nesting (the position of the ENDIF word itself), or to the code end.
// An IF on the way is skipped with its ENDIF. Only the third level of nesting reports a missing ENDIF (see WalkToEndif).
void SCR_FindNextEndif() { WalkToEndif(0); }

// 0x100d8bcc (224 bytes): moves mPos to the next ELSE or ENDIF at this nesting; an ELSE is stepped over (mPos after it), an ENDIF is not.
void SCR_FindNextElseOrEndif() {
    const uint32_t base = gScriptCurrent->mCodeBase;
    uint32_t address;
    for (;;) {
        address = AddressAt(base);
        if (!BeforeEnd(address)) break;
        const uint32_t word = SCR_CodeWord(address);
        if (word == kElseWord || word == kEndifWord) break;
        if (word == kIfWord) {
            gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth + 1;
            SCR_FindNextEndif();
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth - 1;
        }
        gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
    }
    if (BeforeEnd(address) && SCR_CodeWord(address) == kElseWord) gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
}

// 0x100d8cdc (56 bytes): pushes onto the current script's stack, which grows down from mStackTop = 0x14; does nothing when the top is 0 or
// less. A top above 0x14 stores past the array (the Mac does).
void SCR_Push(int32_t value) {
    if (gScriptCurrent->mStackTop <= 0) return;
    gScriptCurrent->mStackTop = gScriptCurrent->mStackTop - 1;
    int32_t* stack = gScriptCurrent->mStack;
    stack[gScriptCurrent->mStackTop] = value;
}

// 0x100d8d34 (64 bytes): pops the current script's stack (0 when it is empty, a top of 0x14 or more).
int32_t SCR_Pop() {
    const int32_t top = gScriptCurrent->mStackTop;
    if (top >= 0x14) return 0;
    gScriptCurrent->mStackTop = top + 1;
    const int32_t* stack = gScriptCurrent->mStack;
    return stack[gScriptCurrent->mStackTop - 1];
}

// The script system's start and the dynamic cell meshes' set-up and teardown (main application, analyser.cpp range; unit w111).

uint32_t gScriptInitialised;
uint32_t gScriptF7C2C;
int32_t gScriptTableSize;
void* gScriptTable;
// 0x100d8d94 (140 bytes): only the first call does anything.
void SCR_InitScriptSystem() {
    if (gScriptInitialised == 0) {
        gScriptInitialised = 1;
        gScriptF7C2C = 0x32;
        gScriptTableSize = 100;
        gScriptTable = LbMem_Alloc(400);
        std::memset(gScriptTable, 0, static_cast<uint32_t>(gScriptTableSize) << 2);
        SCR_InitCheckTable();
    }
}

// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w132: the script system's tick (SCR_ScriptTick, 0x100d907c, 224 bytes), its
// close (SCR_CloseScriptSystem, 0x100d8e4c, 148 bytes) and the list of the sprites the scripts own (SCR_BuildScriptSpriteList, 0x100d9184,
// 152 bytes). The table gScriptTable holds 32-bit words (100 of them, SCR_InitScriptSystem); here a word is an arena offset (Ptr32), as the
// records of the ride scripts' list are. SCR_KillScript (src/analyser_kill.cpp, unit w114) still reads a word as a raw address: no translated
// code stores a script block in the table, so the two never meet yet. The interpreter (DoScript), the sprite list functions and the
// clock are untranslated or other units'.

int32_t gScriptTickTotal;   // 0x102f7c08
int32_t gScriptTickTime;    // 0x102f7c0c

// With the system on: clears the flag, kills every entry (SCR_KillScript does nothing now that the flag is clear), frees the table, forgets
// it and its size, and closes the check table. Off: nothing.
void SCR_CloseScriptSystem() {
    if (gScriptInitialised != 0) {
        gScriptInitialised = 0;
        for (int32_t index = 0; index < gScriptTableSize; ++index) SCR_KillScript(index);
        LbMem_Free(gScriptTable);
        gScriptTable = nullptr;
        gScriptTableSize = 0;
        SCR_CloseCheckTable();
    }
}

// The script system's check table (main app of SimTheme Park 2.04b3, analyser.cpp in the inventory; unit w112c): one byte for each
// word of the compiled scripts' code array, 1 where the word is a command descriptor (a word between the smallest and the largest
// descriptor), else 0. The interpreter (DoScript 0x100d88b4, untranslated) stops at an instruction whose byte is 0.
//
// The code array and the descriptor list are in the Mac's data segment; the host holds them as the program image's words (unit v5L39;
// they were one zero word each before, so the table had no entries and no script ran).

namespace {
// 0x1029f20c: the compiled scripts' code, 883 words (unit v5L39, from the program image). Descriptor and code words are Mac addresses.
const uint32_t kScriptCode[883] = {
    0x000f423f, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000001, 0x10266a60, 0x00000000,   // 0x1029f20c
    0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002,   // 0x1029f22c
    0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004,   // 0x1029f24c
    0x10266a60, 0x00000004, 0x10266a60, 0x00000005, 0x10266a60, 0x00000005, 0x10266a60, 0x00000006,   // 0x1029f26c
    0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266a60, 0x00000007, 0x10266958, 0x1029f210,   // 0x1029f28c
    0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000001, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f2ac
    0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60,   // 0x1029f2cc
    0x00000005, 0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266958, 0x1029f2ac, 0x102669d8,   // 0x1029f2ec
    0x00000010, 0x00001200, 0x10266a50, 0x00000002, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001,   // 0x1029f30c
    0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60, 0x00000005,   // 0x1029f32c
    0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266958, 0x1029f308, 0x102669d8, 0x00000010,   // 0x1029f34c
    0x00001200, 0x10266a50, 0x00000000, 0x10266a60, 0x00000000, 0x10266958, 0x1029f364, 0x102669d8,   // 0x1029f36c
    0x00000010, 0x00001200, 0x10266a50, 0x0000000e, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f38c
    0x10266a60, 0x00000001, 0x10266a60, 0x00000001, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f3ac
    0x10266a60, 0x00000001, 0x10266a60, 0x00000001, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f3cc
    0x10266a60, 0x00000001, 0x10266a60, 0x00000001, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f3ec
    0x10266a60, 0x00000001, 0x10266a60, 0x00000001, 0x10266958, 0x1029f364, 0x102669d8, 0x00000010,   // 0x1029f40c
    0x00001200, 0x10266a50, 0x0000000c, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60,   // 0x1029f42c
    0x00000002, 0x10266a60, 0x00000003, 0x10266958, 0x1029f364, 0x102669d8, 0x00000010, 0x00001200,   // 0x1029f44c
    0x10266a50, 0x0000000b, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002,   // 0x1029f46c
    0x10266a60, 0x00000003, 0x10266958, 0x1029f364, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50,   // 0x1029f48c
    0x00000004, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60,   // 0x1029f4ac
    0x00000003, 0x10266958, 0x1029f364, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000006,   // 0x1029f4cc
    0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003,   // 0x1029f4ec
    0x10266958, 0x1029f364, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000004, 0x10266a60,   // 0x1029f50c
    0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60,   // 0x1029f52c
    0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029f54c
    0x1029f514, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000004, 0x10266a60, 0x00000000,   // 0x1029f56c
    0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004,   // 0x1029f58c
    0x10266a60, 0x00000005, 0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266a60, 0x00000000,   // 0x1029f5ac
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f5cc
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f5ec
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f60c
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f62c
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f64c
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266958, 0x1029f570,   // 0x1029f66c
    0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000004, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f68c
    0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60,   // 0x1029f6ac
    0x00000005, 0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266a60, 0x00000008, 0x10266a60,   // 0x1029f6cc
    0x00000009, 0x10266a60, 0x0000000a, 0x10266a60, 0x0000000b, 0x10266a60, 0x0000000c, 0x10266a60,   // 0x1029f6ec
    0x0000000d, 0x10266a60, 0x0000000e, 0x10266a60, 0x0000000f, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f70c
    0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f72c
    0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266958, 0x1029f68c, 0x102669d8,   // 0x1029f74c
    0x00000010, 0x00001200, 0x10266a50, 0x00000006, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001,   // 0x1029f76c
    0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266958, 0x1029f768, 0x102669d8, 0x00000010,   // 0x1029f78c
    0x00001200, 0x10266a50, 0x00000004, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f7ac
    0x00000001, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000002, 0x10266a60,   // 0x1029f7cc
    0x00000003, 0x10266a60, 0x00000003, 0x10266958, 0x1029f7a4, 0x102669d8, 0x00000010, 0x00001200,   // 0x1029f7ec
    0x10266a50, 0x00000004, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002,   // 0x1029f80c
    0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60, 0x00000005, 0x10266a60, 0x00000006,   // 0x1029f82c
    0x10266a60, 0x00000007, 0x10266958, 0x1029f800, 0x102669d8, 0x00000010, 0x00001200, 0x10266a50,   // 0x1029f84c
    0x00000003, 0x10266a60, 0x00000000, 0x10266958, 0x1029f85c, 0x102669d8, 0x00000010, 0x00001200,   // 0x1029f86c
    0x10266a50, 0x00000005, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002,   // 0x1029f88c
    0x10266a60, 0x00000003, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000,   // 0x1029f8ac
    0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266a60, 0x00000000, 0x10266958, 0x1029f880,   // 0x1029f8cc
    0x102669d8, 0x00000010, 0x00001200, 0x10266a50, 0x00000007, 0x10266a60, 0x00000000, 0x10266a60,   // 0x1029f8ec
    0x00000001, 0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60,   // 0x1029f90c
    0x00000005, 0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266958, 0x1029f948, 0x102669d8,   // 0x1029f92c
    0x00000010, 0x00001200, 0x10266a50, 0x00000008, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001,   // 0x1029f94c
    0x10266a60, 0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60, 0x00000005,   // 0x1029f96c
    0x10266a60, 0x00000006, 0x10266a60, 0x00000007, 0x10266958, 0x1029f948, 0x102669d8, 0x00000010,   // 0x1029f98c
    0x00001200, 0x10266a50, 0x00000009, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60,   // 0x1029f9ac
    0x00000002, 0x10266a60, 0x00000003, 0x10266a60, 0x00000004, 0x10266a60, 0x00000005, 0x10266a60,   // 0x1029f9cc
    0x00000006, 0x10266a60, 0x00000007, 0x10266958, 0x1029f364, 0x10266a50, 0x00000000, 0x102669d8,   // 0x1029f9ec
    0x00000010, 0x00080000, 0x10266a60, 0x00000000, 0x10266958, 0x1029fa00, 0x10266a50, 0x00000001,   // 0x1029fa0c
    0x102669d8, 0x00000010, 0x00080000, 0x10266a60, 0x00000000, 0x10266958, 0x1029fa24, 0x10266a50,   // 0x1029fa2c
    0x00000002, 0x102669d8, 0x00000010, 0x00080000, 0x10266a60, 0x00000000, 0x10266958, 0x1029fa48,   // 0x1029fa4c
    0x10266a50, 0x00000003, 0x102669d8, 0x00000010, 0x00080000, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fa6c
    0x1029fa6c, 0x10266a50, 0x00000004, 0x102669d8, 0x00000010, 0x00080000, 0x10266a60, 0x00000000,   // 0x1029fa8c
    0x10266958, 0x1029fa90, 0x10266a50, 0x00000005, 0x102669d8, 0x00000010, 0x00080000, 0x10266a60,   // 0x1029faac
    0x00000000, 0x10266958, 0x1029fab4, 0x10266a50, 0x00000006, 0x102669d8, 0x00000010, 0x00080000,   // 0x1029facc
    0x10266a60, 0x00000000, 0x10266958, 0x1029fad8, 0x10266a50, 0x00000007, 0x102669d8, 0x00000010,   // 0x1029faec
    0x00080000, 0x10266a60, 0x00000000, 0x10266958, 0x1029fafc, 0x10266a50, 0x00000000, 0x10266a60,   // 0x1029fb0c
    0x00000000, 0x10266958, 0x1029fb20, 0x10266a50, 0x00000001, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fb2c
    0x1029fb38, 0x10266a50, 0x00000002, 0x10266a60, 0x00000000, 0x10266958, 0x1029fb50, 0x10266a50,   // 0x1029fb4c
    0x00000003, 0x10266a60, 0x00000000, 0x10266958, 0x1029fb68, 0x10266a50, 0x00000004, 0x10266a60,   // 0x1029fb6c
    0x00000000, 0x10266958, 0x1029fb80, 0x10266a50, 0x00000005, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fb8c
    0x1029fb98, 0x10266a50, 0x00000006, 0x10266a60, 0x00000000, 0x10266958, 0x1029fbb0, 0x10266a50,   // 0x1029fbac
    0x00000007, 0x10266a60, 0x00000000, 0x10266958, 0x1029fbc8, 0x10266a50, 0x00000008, 0x10266a60,   // 0x1029fbcc
    0x00000000, 0x10266958, 0x1029fbe0, 0x10266a50, 0x00000009, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fbec
    0x1029fbf8, 0x10266a50, 0x0000000a, 0x10266a60, 0x00000000, 0x10266958, 0x1029fc10, 0x10266a50,   // 0x1029fc0c
    0x0000000b, 0x10266a60, 0x00000000, 0x10266958, 0x1029fc28, 0x10266a50, 0x0000000c, 0x10266a60,   // 0x1029fc2c
    0x00000000, 0x10266958, 0x1029fc40, 0x10266a50, 0x0000000d, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fc4c
    0x1029fc58, 0x10266a50, 0x0000000e, 0x10266a60, 0x00000000, 0x10266958, 0x1029fc70, 0x10266a50,   // 0x1029fc6c
    0x0000000f, 0x10266a60, 0x00000000, 0x10266958, 0x1029fc88, 0x10266a50, 0x00000015, 0x10266a60,   // 0x1029fc8c
    0x00000000, 0x10266958, 0x1029fca0, 0x10266a50, 0x00000010, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fcac
    0x1029fcb8, 0x10266a50, 0x00000011, 0x10266a60, 0x00000000, 0x10266958, 0x1029fcd0, 0x10266a50,   // 0x1029fccc
    0x00000012, 0x10266a60, 0x00000000, 0x10266958, 0x1029fce8, 0x10266a50, 0x00000013, 0x10266a60,   // 0x1029fcec
    0x00000000, 0x10266958, 0x1029fd00, 0x10266a50, 0x00000014, 0x10266a60, 0x00000000, 0x10266958,   // 0x1029fd0c
    0x1029fd18, 0x10266a50, 0x00000000, 0x10266a60, 0x00000000, 0x10266958, 0x1029fd30, 0x10266a60,   // 0x1029fd2c
    0x00000000, 0x10266958, 0x1029fd48, 0x102669a8, 0x10266a60, 0x00000001, 0x102669e8, 0x00000007,   // 0x1029fd4c
    0x00000014, 0x102669b0, 0x00000007, 0x00000008, 0x00000000, 0x102669b8, 0x102669d8, 0x00000007,   // 0x1029fd6c
    0x000000fa, 0x10266958, 0x1029fd58, 0x10266a58, 0x0000000d, 0x10266958, 0x1029fd98, 0x10266a50,   // 0x1029fd8c
    0x00000000, 0x102669d8, 0x0000000d, 0x00000000, 0x102669d8, 0x00000010, 0x03000080, 0x10266978,   // 0x1029fdac
    0x1029fd98, 0x102669b8, 0x10266a30, 0x0000000d, 0x00000011, 0x102669a8, 0x10266a58, 0x0000000d,   // 0x1029fdcc
    0x102669e0, 0x0000000d, 0x00000001, 0x102669b0, 0x0000000d, 0x00000001, 0x0000000e, 0x10266960,   // 0x1029fdec
    0x00000012, 0x00000007, 0x00000000, 0x102669d8, 0x00000000, 0x00000000, 0x102669a8, 0x10266a60,   // 0x1029fe0c
    0x00000000, 0x102669e0, 0x00000000, 0x00000001, 0x102669b0, 0x00000000, 0x00000003, 0x00000012,   // 0x1029fe2c
    0x10266968, 0x10266958, 0x1029fdd4, 0x102669d8, 0x00000010, 0x00001200, 0x102669d8, 0x0000000d,   // 0x1029fe4c
    0x00000000, 0x10266960, 0x00000011, 0x00000007, 0x00000000, 0x102669a8, 0x10266a58, 0x0000000d,   // 0x1029fe6c
    0x102669e0, 0x0000000d, 0x00000001, 0x102669b0, 0x0000000d, 0x00000003, 0x00000011, 0x10266968,   // 0x1029fe8c
    0x10266958, 0x1029fdd4, 0x102669d8, 0x0000000d, 0x00000000, 0x102669a8, 0x10266a58, 0x0000000d,   // 0x1029feac
    0x102669e0, 0x0000000d, 0x00000001, 0x102669b0, 0x0000000d, 0x00000003, 0x0000000e, 0x10266958,   // 0x1029fecc
    0x1029feb4, 0x102669d8, 0x00000010, 0x00001200, 0x102669d8, 0x0000000d, 0x00000000, 0x10266958,   // 0x1029feec
    0x1029feb4, 0x10266a60, 0x00000002, 0x10266958, 0x1029ff10, 0x102669d8, 0x00000010, 0x00001200,   // 0x1029ff0c
    0x102669d8, 0x0000000d, 0x00000000, 0x10266958, 0x1029ff10, 0x10266a60, 0x00000002, 0x10266958,   // 0x1029ff2c
    0x1029ff40, 0x102669d8, 0x00000010, 0x00001200, 0x102669d8, 0x0000000d, 0x00000000, 0x10266958,   // 0x1029ff4c
    0x1029ff40, 0x102669d8, 0x00000010, 0x00051000, 0x10266a50, 0x00000001, 0x102669d8, 0x0000000f,   // 0x1029ff6c
    0x00000005, 0x10266a60, 0x00000000, 0x10266a60, 0x00000001, 0x10266a60, 0x00000002, 0x10266a60,   // 0x1029ff8c
    0x00000003, 0x10266a60, 0x00000004, 0x10266a60, 0x00000005, 0x10266a60, 0x00000006, 0x10266a60,   // 0x1029ffac
    0x00000007, 0x10266958, 0x1029ff70,   // 0x1029ffcc
};
// 0x1029ffec: the 42 command descriptors in order (kScriptCommandAddress + 8 * index), ended by a 0.
const uint32_t kScriptCommands[43] = {
    0x10266958, 0x10266960, 0x10266968, 0x10266970, 0x10266978, 0x10266980, 0x10266988, 0x10266990,
    0x10266998, 0x102669a0, 0x102669a8, 0x102669b0, 0x102669b8, 0x102669c0, 0x102669c8, 0x102669d0,
    0x102669d8, 0x102669e0, 0x102669e8, 0x102669f0, 0x102669f8, 0x10266a00, 0x10266a08, 0x10266a10,
    0x10266a18, 0x10266a20, 0x10266a28, 0x10266a30, 0x10266a38, 0x10266a40, 0x10266a48, 0x10266a50,
    0x10266a58, 0x10266a60, 0x10266a68, 0x10266a70, 0x10266a78, 0x10266a80, 0x10266a88, 0x10266a90,
    0x10266a98, 0x10266aa0, 0x00000000,
};
}  // namespace

uint8_t* gScriptCheckTable = nullptr;                          // 0x102f7c10
const uint32_t* gScriptCodeWords = kScriptCode;                // 0x1029f20c
const uint32_t* gScriptCodeWordsEnd = kScriptCode + 883;       // 0x1029ffd8
const uint32_t* gScriptCommandList = kScriptCommands;          // 0x1029ffec
uint32_t gScriptCommandFirst = kScriptCommandAddress;          // TOC 0x1026111c holds 0x10266958

// 0x100d900c (68 bytes).
void SCR_CloseCheckTable() {
    if (gScriptCheckTable != nullptr) {
        delete[] gScriptCheckTable;
        gScriptCheckTable = nullptr;
    }
}

// 0x100d8f0c (212 bytes). The range starts as the single word gScriptCommandFirst and grows over the list (two independent unsigned
// compares for each word; the ending 0 is not used). The table has (end - begin) / 4 entries, the division rounding toward zero
// (srawi and addze) on the byte difference taken in 32 bits, and is not cleared before it is filled.
void SCR_InitCheckTable() {
    uint32_t lowest = gScriptCommandFirst;
    uint32_t highest = gScriptCommandFirst;
    const uint32_t* const words = gScriptCodeWords;
    const uint32_t* const end = gScriptCodeWordsEnd;
    SCR_CloseCheckTable();
    for (const uint32_t* command = gScriptCommandList; *command != 0; ++command) {
        const uint32_t word = *command;
        if (word < lowest) lowest = word;
        if (word > highest) highest = word;
    }
    const int32_t bytes = static_cast<int32_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(end)) -
                                               static_cast<uint32_t>(reinterpret_cast<uintptr_t>(words)));
    const int32_t count = bytes / 4;
    gScriptCheckTable = new uint8_t[count];
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t word = words[i];
        if (word < lowest || word > highest) {
            gScriptCheckTable[i] = 0;
        } else {
            gScriptCheckTable[i] = 1;
        }
    }
}

// Zeroes the total and, with the system on, reads the clock once and visits entries 1 to the table's size (re-read each time): an entry that is
// set and whose next time is before the clock (signed) is killed when its state is 4 (finished), else run with DoScript, the result added to
// the total, and given its next time (the clock plus its period).
void SCR_ScriptTick() {
    gScriptTickTotal = 0;
    if (gScriptInitialised != 0) {
        gScriptTickTime = gClock.Read();
        for (int32_t index = 1; index < gScriptTableSize; ++index) {
            tagSCRIPT* script = Entry(index);
            if (script != nullptr && gScriptTickTime > script->mNextTime) {
                if (script->mState == 4) {
                    SCR_KillScript(index);
                } else {
                    const int32_t result = DoScript(script);
                    gScriptTickTotal = static_cast<int32_t>(static_cast<uint32_t>(gScriptTickTotal) + static_cast<uint32_t>(result));
                    script->mNextTime = static_cast<int32_t>(static_cast<uint32_t>(gScriptTickTime) + static_cast<uint32_t>(script->mPeriod));
                }
            }
        }
    }
}

// With the system on: resets the high level sprites and adds the sprite of every script of entries 1 to the table's size. The sprites are
// drawn whether the system is on or not.
void SCR_BuildScriptSpriteList() {
    if (gScriptInitialised != 0) {
        ResetHLSprites();
        for (int32_t index = 1; index < gScriptTableSize; ++index) {
            tagSCRIPT* script = Entry(index);
            if (script != nullptr) ESP_AddHLSpriteFromScript(script);
        }
    }
    DrawHLSprites();
}

// 0x100d924c (180 bytes): makes a script on the code at `code` and answers its handle; 0 when the script system is off. A block that could
// not be made is not initialised but still goes to AssignSlot (which then writes through NULL: the Mac crashes).
int32_t SCR_SpawnScript(void* code, int32_t value, float x, float y, float z) {
    if (gScriptInitialised == 0) return 0;
    tagSCRIPT* script = SCR_NewScriptBlock();
    if (script != nullptr) InitScript(script, code, value, x, y, z);
    return AssignSlot(script);
}

// 0x100d932c (92 bytes): moves a script: its position x, y, z (variables 1 to 3) and its mode (variable 15).
void SCR_SetScriptPosition(int32_t handle, float x, float y, float z, int32_t mode) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised == 0) return;
    if (script == nullptr) return;
    script->mVars[1] = FloatWord(x);
    script->mVars[2] = FloatWord(y);
    script->mVars[3] = FloatWord(z);
    script->mVars[15] = mode;
}

// 0x100d93b8 (140 bytes): reads a script's position and mode into the pointers that are not NULL.
void SCR_GetScriptPosition(int32_t handle, float* x, float* y, float* z, int32_t* mode) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised == 0) return;
    if (script == nullptr) return;
    if (x != nullptr) *x = WordFloat(script->mVars[1]);
    if (y != nullptr) *y = WordFloat(script->mVars[2]);
    if (z != nullptr) *z = WordFloat(script->mVars[3]);
    if (mode != nullptr) *mode = script->mVars[15];
}

// analyser.cpp (main app 0x100d9478, unit w114): SCR_KillScript.

// Frees the script block of `handle` and clears its table entry (first), when the script system is on and 0 < handle < the table's size
// (signed compares: a negative size lets nothing through). The table holds 32-bit words (Ptr32 offsets, like src/ScriptTick.cpp reads them);
// the block goes back through SCR_FreeScriptBlock (the Mac's LbMem_Free, src/ScriptBlocks.cpp).
void SCR_KillScript(int32_t handle) {
    if (gScriptInitialised != 0 && handle > 0 && handle < gScriptTableSize) {
        Ptr32<tagSCRIPT>* entry = static_cast<Ptr32<tagSCRIPT>*>(gScriptTable) + handle;
        tagSCRIPT* script = *entry;
        if (script != nullptr) {
            entry->mOffset = 0;
            SCR_FreeScriptBlock(script);
        }
    }
}

// 0x100d9514 (96 bytes): sets variable `index` (0 to 0x12) of a script.
void SCR_SetVariable(int32_t handle, uint32_t index, int32_t value) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised == 0) return;
    if (script == nullptr) return;
    if (index >= 0x13) return;
    script->mVars[index] = value;
}

// 0x100d95a0 (96 bytes): sets float variable `index` (0 to 0x12) of a script.
void SCR_SetFloatVariable(int32_t handle, uint32_t index, float value) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised == 0) return;
    if (script == nullptr) return;
    if (index >= 0x13) return;
    script->mVars[index] = FloatWord(value);
}

// 0x100d9630 (80 bytes): sets how many clock ticks pass between two runs of a script.
void SCR_SetServiceInterval(int32_t handle, int32_t period) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised == 0) return;
    if (script == nullptr) return;
    script->mPeriod = period;
}

// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w165: the script system's save and load, and the file swaps they use.
//
// The file holds the tag 'SCPT' (0x53435054), the record size 0x118, the table size, the table (size words; the entries only matter as
// zero or not) and then one 0x118 byte record for each non-zero entry from 1 on. The file's words are little-endian: the Mac swaps them
// (the main app's own instances of the SamsUtilities templates, which swap unconditionally), a little-endian host does not. The save works
// on a copy of each block; the load fixes the code base to the code array's address and a position of 0x61 that was last at 0x5f to 0x5f.
//
// Host design: the table words the load reads are the file's (unused but for the zero test); each block is a SCR_NewScriptBlock (an arena
// block, so that the table's Ptr32 words reach it), the table itself a LbMem_Alloc block. The record constructor the Mac calls first
// (SSpriteGameData::SSpriteGameData, 0x1002f144) only builds a local record that the block copy then overwrites: not called.

// The swaps are real only where the host's order differs from the file's (or when a test asks).
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define SCRIPT_SWAP_FILE_WORDS 1
#endif

namespace ScriptSaveSwap {

namespace {
uint32_t Swap32(uint32_t v) {
#ifdef SCRIPT_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
}  // namespace

// 0x100d9920 (12 bytes)
void USwapInplace32Int(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }

// 0x100d9c2c (12 bytes)
void USwapInplace32Long(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }

// 0x100d9b00 (36 bytes)
void USwapBlock32SLONGfloat(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

// 0x100d9b74 (36 bytes)
void USwapBlock32Long(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

// 0x100d9bd0 (40 bytes): a pointer field as its 32-bit word.
uint32_t USwap32LongPtr(uint32_t value) { return Swap32(value); }

// 0x100d9c70 (40 bytes)
uint32_t USwap32CharPtr(uint32_t value) { return Swap32(value); }

// 0x100d9964 (364 bytes): SSpriteGameData::Byteswap on the 0x44 byte sprite record: the words at 0 to 0x34 (14 of them), the half word at
// 0x38 and the word at 0x40 (the bytes from 0x3a to 0x3f stay).
void SpriteRecordByteswap(uint8_t* record) {
    for (int32_t offset = 0; offset <= 0x34; offset += 4) {
        uint32_t word;
        std::memcpy(&word, record + offset, 4);
        word = Swap32(word);
        std::memcpy(record + offset, &word, 4);
    }
#ifdef SCRIPT_SWAP_FILE_WORDS
    uint16_t half;
    std::memcpy(&half, record + 0x38, 2);
    half = static_cast<uint16_t>(half >> 8 | half << 8);
    std::memcpy(record + 0x38, &half, 2);
#endif
    uint32_t word;
    std::memcpy(&word, record + 0x40, 4);
    word = Swap32(word);
    std::memcpy(record + 0x40, &word, 4);
}

}  // namespace ScriptSaveSwap

namespace {

// The block's fields in the file's order, in the original's order (the save swaps a copy, the load the block read).
void SwapBlock(tagSCRIPT* block) {
    using namespace ScriptSaveSwap;
    block->mText = USwap32CharPtr(block->mText);
    USwapInplace32Long(block->mSlot);
    USwapInplace32Long(block->mPos);
    USwapInplace32Long(block->mStartPos);
    USwapInplace32Long(block->mLastPos);
    block->mCodeBase = USwap32LongPtr(block->mCodeBase);
    USwapInplace32Long(block->mState);
    USwapInplace32Long(block->mStackTop);
    USwapBlock32Long(block->mStack, 0x14);
    USwapInplace32Long(block->mIfDepth);
    USwapInplace32Long(block->mGosubDepth);
    USwapInplace32Long(block->mLoopDepth);
    USwapInplace32Long(block->mNextTime);
    USwapInplace32Long(block->mPeriod);
    USwapBlock32SLONGfloat(block->mVars, 0x13);
    SpriteRecordByteswap(block->mSprite);
    USwapInplace32Int(block->mHasSprite);
}

// The table's entry for `index` (a Ptr32 word the table is indexed by, re-read from the global each time as the Mac does).
Ptr32<tagSCRIPT>& EntryRef(int32_t index) { return static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index]; }

}  // namespace

// 0x100d96b0 (580 bytes): writes the script system to a file (nothing with the system off): the tag, the record size and the table size
// as swapped words (each from a _USwapCopy32 copy that is deleted again), the table as it is, and a swapped copy of each block from 1 on.
void SCR_SaveAllScripts(void* file) {
    if (gScriptInitialised != 0) {
        uint32_t word = 0x53435054;
        uint32_t transferred;
        uint32_t* copy = SamsUtilities::_USwapCopy32(&word, 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        word = 0x118;
        copy = SamsUtilities::_USwapCopy32(&word, 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        copy = SamsUtilities::_USwapCopy32(reinterpret_cast<const uint32_t*>(&gScriptTableSize), 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        LbFile_Write(file, gScriptTable, static_cast<uint32_t>(gScriptTableSize) << 2, nullptr);
        for (int32_t i = 1; i < gScriptTableSize; ++i) {
            tagSCRIPT* block = EntryRef(i);
            if (block != nullptr) {
                tagSCRIPT local;
                std::memcpy(&local, block, sizeof local);
                SwapBlock(&local);
                LbFile_Write(file, &local, 0x118, nullptr);
            }
        }
    }
}

// 0x100d9ccc (784 bytes): reads the script system back; answers 0 when it is loaded, 1 when not (system off, a bad file header, a failed
// allocation). Frees the old blocks and the old table first; a bad header leaves a fresh empty table of 100 entries. A block whose
// position is 0x61 and whose last position 0x5f gets position 0x5f.
int32_t SCR_LoadAllScripts(void* file) {
    if (gScriptInitialised == 0) return 1;
    for (int32_t i = 0; i < gScriptTableSize; ++i) {
        if (gScriptInitialised != 0 && i > 0 && i < gScriptTableSize) {
            tagSCRIPT* old = EntryRef(i);
            if (old != nullptr) {
                EntryRef(i) = nullptr;
                SCR_FreeScriptBlock(old);
            }
        }
    }
    LbMem_Free(gScriptTable);
    uint32_t tag;
    uint32_t size;
    uint32_t transferred;
    LbFile_Read(file, &tag, 4, &transferred);
    SamsUtilities::_USwapBlock32(&tag, 1);
    LbFile_Read(file, &size, 4, &transferred);
    SamsUtilities::_USwapBlock32(&size, 1);
    if (tag == 0x53435054 && size == 0x118) {
        LbFile_Read(file, &gScriptTableSize, 4, &transferred);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&gScriptTableSize), 1);
        gScriptTable = LbMem_Alloc(static_cast<uint32_t>(gScriptTableSize) << 2);
        if (gScriptTable == nullptr) return 1;
        LbFile_Read(file, gScriptTable, static_cast<uint32_t>(gScriptTableSize) << 2, nullptr);
        EntryRef(0).mOffset = 0;
        for (int32_t i = 1; i < gScriptTableSize; ++i) {
            if (EntryRef(i).mOffset != 0) {
                tagSCRIPT* block = SCR_NewScriptBlock();
                if (block == nullptr) return 1;
                EntryRef(i) = block;
                LbFile_Read(file, block, 0x118, nullptr);
                SwapBlock(block);
                block->mCodeBase = kScriptCodeAddress;
                if (block->mPos == 0x61 && block->mLastPos == 0x5f) block->mPos = 0x5f;
            }
        }
        return 0;
    }
    gScriptTableSize = 100;
    gScriptTable = LbMem_Alloc(400);
    std::memset(gScriptTable, 0, static_cast<uint32_t>(gScriptTableSize) << 2);
    return 1;
}

// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w165: the script sprites. The ESP_ functions that restart, compare, copy
// and repair the scripts of sprites, and the PK_ commands that set the sprite set, the sprite and its action, show and remove the sprite,
// and change the scale and the rotation (variables 8 and 9). The script block, its table and the code array are those of
// src/ScriptInterp.cpp (unit w163); the variable window gScriptVars is the current script's mVars.
//
// A variable is a 32-bit word; the float ones (1 to 6, 8, 9) are read and written with std::memcpy. The Mac functions do not guard their
// handles: a handle outside the table finds a NULL script, which the ones that dereference it would read at address 0 (not reproduced; the
// host faults the same way on the NULL pointer). A custom data block number comes in as a code argument of 0 to 3.

namespace {

// divw: the quotient rounded toward zero; the PowerPC leaves a zero divisor and INT_MIN / -1 undefined (the emulator the tests compare with
// answers 0 for a zero divisor and INT_MIN for INT_MIN / -1).
int32_t PpcDivw(int32_t a, int32_t b) {
    if (b == 0) return 0;
    if (a == INT32_MIN && b == -1) return INT32_MIN;
    return a / b;
}

// The sprite of a script (its set in variable 10, its number in 11).
ESP_CSprite* SpriteOf(const tagSCRIPT* script) { return gESPSprites[script->mVars[11] + gESPGroups[script->mVars[10]].mFirst]; }

// A custom data block's start address for the script (variable 12 takes the block's action): the address table is indexed by the block's
// first value without a range check on the Mac; an index outside 0 to 3 answers 0 here.
uint32_t CustomStart(tagSCRIPT* script, int32_t block, ESP_CSprite** spriteOut) {
    int32_t first = 0;
    ACTS acts = static_cast<ACTS>(0);
    int32_t third = 0;
    int32_t fourth = 0;
    ESP_CSprite* sprite = SpriteOf(script);
    sprite->GetCustomData(block, &first, &acts, &third, &fourth);
    script->mVars[12] = static_cast<int32_t>(acts);
    if (spriteOut != nullptr) *spriteOut = sprite;
    return static_cast<uint32_t>(first) < 4 ? gScriptStartAddresses[first] : 0;
}

bool IsCustomBlock(void* code) { return reinterpret_cast<uintptr_t>(code) <= 3; }

}  // namespace

// 0x100da318 (300 bytes): restarts a script at `code` (a code pointer, or a custom data block number 0 to 3: then the start address and the
// action come from the sprite's block and variable 14 gets the action's frame count). Resets the stack and the nesting counters.
void ESP_ForceGoto(int32_t handle, void* code) {
    tagSCRIPT* script = ScriptOfHandle(handle);
    if (gScriptInitialised != 0 && script != nullptr) {
        uint32_t address;
        if (IsCustomBlock(code)) {
            ESP_CSprite* sprite = nullptr;
            address = CustomStart(script, static_cast<int32_t>(reinterpret_cast<uintptr_t>(code)), &sprite);
            script->mVars[14] = static_cast<int32_t>(sprite->GetActionFrames(script->mVars[12]));
        } else {
            address = SCR_CodeAddress(code);
        }
        script->mStackTop = 0x14;
        script->mPos = static_cast<int32_t>(address - script->mCodeBase) >> 2;
        script->mIfDepth = 0;
        script->mGosubDepth = 0;
        script->mLoopDepth = 0;
        script->mStartPos = script->mPos;
    }
}

// 0x100da46c (248 bytes): 1 when the script was started at `code` (or at the custom block's start address), else 0. A custom block also
// sets variable 12 to the block's action.
int32_t ESP_RURunningThis(int32_t handle, void* code) {
    if (gScriptInitialised != 0) {
        tagSCRIPT* script = ScriptOfHandle(handle);
        if (script != nullptr) {
            uint32_t address;
            if (IsCustomBlock(code)) {
                address = CustomStart(script, static_cast<int32_t>(reinterpret_cast<uintptr_t>(code)), nullptr);
            } else {
                address = SCR_CodeAddress(code);
            }
            if (kScriptCodeAddress + static_cast<uint32_t>(script->mStartPos) * 4 == address) return 1;
        }
    }
    return 0;
}

// 0x100da590 (276 bytes): a copy of a script block placed at x, y, z with the scale in variable 8, flagged as 2D (variable 16: bits
// 0x50000 on, 0x200 off); answers its handle (AssignSlot of the copy, also of a failed allocation).
int32_t ESP_Spawn2DCopyOfScriptAt(int32_t handle, float x, float y, float z, float scale) {
    if (gScriptInitialised == 0) return 0;
    tagSCRIPT* block = SCR_NewScriptBlock();
    if (block != nullptr) {
        tagSCRIPT* source = ScriptOfHandle(handle);
        std::memcpy(block, source, sizeof(tagSCRIPT));
        block->mVars[1] = FloatWord(x);
        block->mVars[2] = FloatWord(y);
        block->mVars[3] = FloatWord(z);
        block->mVars[8] = FloatWord(scale);
        block->mVars[16] = static_cast<int32_t>(static_cast<uint32_t>(block->mVars[16]) | 0x50000u);
        block->mVars[16] = static_cast<int32_t>(static_cast<uint32_t>(block->mVars[16]) & 0xfffffdffu);
    }
    return AssignSlot(block);
}

// 0x100da6d8 (220 bytes): copies the code base, the position, the sprite set, number and action, variables 8 to 9 and so on from the
// script `source` to `handle`, then flags the destination 2D as above. No system test, no NULL test.
void ESP_Update2DScript(int32_t handle, int32_t source) {
    tagSCRIPT* dest = ScriptOfHandle(handle);
    tagSCRIPT* from = ScriptOfHandle(source);
    dest->mCodeBase = from->mCodeBase;
    dest->mPos = from->mPos;
    dest->mVars[10] = from->mVars[10];
    dest->mVars[11] = from->mVars[11];
    dest->mVars[12] = from->mVars[12];
    dest->mVars[17] = from->mVars[17];
    dest->mVars[18] = from->mVars[18];
    dest->mVars[7] = from->mVars[7];
    dest->mVars[9] = from->mVars[9];
    dest->mVars[15] = from->mVars[15];
    dest->mHasSprite = from->mHasSprite;
    dest->mVars[13] = from->mVars[13];
    dest->mVars[14] = from->mVars[14];
    dest->mVars[16] = static_cast<int32_t>(static_cast<uint32_t>(dest->mVars[16]) | 0x50000u);
    dest->mVars[16] = static_cast<int32_t>(static_cast<uint32_t>(dest->mVars[16]) & 0xfffffdffu);
}

// 0x100da7e0 (112 bytes): the handle of the first script (from 1) that uses the sprite record at `sprite`, 0 when none (or the system is off).
int32_t SCR_WhichScriptHasThisSprite(void* sprite) {
    if (gScriptInitialised == 0) return 0;
    for (int32_t handle = 1; handle < gScriptTableSize; ++handle) {
        tagSCRIPT* script = Entry(handle);
        // HOST: the drawn sprite is the block's host record (src/ScriptHostSprites.cpp), not the embedded Mac-layout one.
        if (script != nullptr && script->mHasSprite != 0 && (script->mSprite == sprite || ESP_IsScriptHostSprite(script, sprite)))
            return handle;
    }
    return 0;
}

namespace {
// ESP_InitScript on a Mac code address (ESP_SpawnScript's custom start addresses are Mac addresses, not host pointers).
void ESPInitScriptAt(tagSCRIPT* script, uint32_t address, ESP_Sprites set, int32_t index, ACTS acts, float x, float y, float z, int32_t a,
                     int32_t b) {
    script->mCodeBase = kScriptCodeAddress;
    script->mStackTop = 0x14;
    script->mPos = PositionOf(address, script->mCodeBase);
    script->mState = 1;
    script->mIfDepth = 0;
    script->mGosubDepth = 0;
    script->mLoopDepth = 0;
    script->mStartPos = script->mPos;
    script->mVars[10] = static_cast<int32_t>(set);
    script->mVars[11] = index;
    script->mVars[12] = static_cast<int32_t>(acts);
    script->mVars[17] = a;
    script->mVars[18] = b;
    script->mVars[1] = FloatWord(x);
    script->mVars[2] = FloatWord(y);
    script->mVars[3] = FloatWord(z);
    script->mVars[4] = FloatWord(0.0f);
    script->mVars[5] = FloatWord(0.0f);
    script->mVars[6] = FloatWord(0.0f);
    script->mVars[7] = 0xff;
    script->mVars[8] = FloatWord(1.0f);
    script->mVars[9] = FloatWord(1.0f);
    script->mVars[15] = 0;
    script->mVars[16] = 0;
    std::memset(script->mSprite + 0x28, 0, 4);
    script->mHasSprite = 0;
    script->mPeriod = 0x3e;
    script->mNextTime = static_cast<int32_t>(static_cast<uint32_t>(script->mPeriod) + static_cast<uint32_t>(gClock.Read()));
    script->mVars[14] = static_cast<int32_t>(gESPSprites[index + gESPGroups[set].mFirst]->GetActionFrames(static_cast<int32_t>(acts)));
}
}  // namespace

// 0x100da008 (312 bytes): InitScript for a sprite's script: variables 10, 11 and 12 hold the sprite set, the sprite in it and the action,
// 14 the action's frame count (GetActionFrames of the sprite), 17 and 18 the last two integers. No trace line.
void ESP_InitScript(tagSCRIPT* script, void* code, ESP_Sprites set, int32_t index, ACTS acts, float x, float y, float z, int32_t a,
                    int32_t b) {
    ESPInitScriptAt(script, SCR_CodeAddress(code), set, index, acts, x, y, z, a, b);
}

// 0x100da18c (332 bytes): makes a script for a sprite and answers its handle (0 with the system off). A code argument of 0 to 3 (signed)
// is not an address but the number of the sprite's custom data block, whose first value picks the start address from
// gScriptStartAddresses (an index outside 0 to 3 answers 0 here: the Mac reads the descriptor list past the table); GetCustomData also
// overwrites `acts` and gives the last two integers. The group must hold a sprite (GAME_ASSERTMSG on the group's name).
int32_t ESP_SpawnScript(void* code, ESP_Sprites set, int32_t index, ACTS acts, float x, float y, float z) {
    int32_t first = 0;
    int32_t third = 0;    // the stack slot 0x48
    int32_t fourth = 0;   // the stack slot 0x44
    if (gScriptInitialised == 0) return 0;
    tagSCRIPT* script = SCR_NewScriptBlock();
    if (script != nullptr) {
        const uintptr_t codeValue = reinterpret_cast<uintptr_t>(code);
        uint32_t address;
        if (codeValue <= 3) {
            const int32_t block = static_cast<int32_t>(codeValue);
            const int32_t sprite = gESPGroups[set].mFirst + index;
            GAME_ASSERTMSG(gESPGroups[set].mCount > 0, gESPGroups[set].mName);
            gESPSprites[sprite]->GetCustomData(block, &first, &acts, &third, &fourth);
            address = static_cast<uint32_t>(first) < 4 ? gScriptStartAddresses[first] : 0;
        } else {
            address = SCR_CodeAddress(code);
        }
        ESPInitScriptAt(script, address, set, index, acts, x, y, z, third, fourth);
    }
    return AssignSlot(script);
}

void PK_GOTO();

void PK_NOP();

void PK_END();

void PK_WAIT();

void PK_SETTEXT();

void PK_SETVAR();

void PK_PUSHVAR();

void PK_POPVAR();

void PK_GOSUB();

void PK_RETURN();

// 0x100da884 (164 bytes): reduces every script's sprite number (variable 11) modulo the number of sprites in its set (variable 10).
void ESP_FixSpriteIDsFromDetail() {
    if (gScriptInitialised != 0) {
        for (int32_t i = 1; i < gScriptTableSize; ++i) {
            tagSCRIPT* script = Entry(i);
            if (script != nullptr) {
                const int32_t count = ESP_GetNumberOfSprites(static_cast<ESP_Sprites>(script->mVars[10]));
                const int32_t id = script->mVars[11];
                script->mVars[11] = static_cast<int32_t>(static_cast<uint32_t>(id) - static_cast<uint32_t>(PpcDivw(id, count)) * static_cast<uint32_t>(count));
            }
        }
    }
}
