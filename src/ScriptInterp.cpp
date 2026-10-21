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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Clock.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "Ptr32.h"
#include "analyser.h"

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

int32_t FloatWord(float value) {
    int32_t word;
    std::memcpy(&word, &value, sizeof word);
    return word;
}

// The operand at the current script's mPos; mPos moves on by one (the PK_ functions' common first step).
uint32_t NextOperand() {
    const int32_t pos = gScriptCurrent->mPos;
    gScriptCurrent->mPos = pos + 1;
    return SCR_CodeWord(gScriptCodeBase + static_cast<uint32_t>(pos) * 4);
}

// The next word's index for a code address (an arithmetic shift, no rounding toward zero).
int32_t PositionOf(uint32_t address, uint32_t base) { return static_cast<int32_t>(address - base) >> 2; }

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

// 0x100d8740 (4 bytes): an empty function in this build (the game's script error report was compiled out).
void SCR_Error(const char*) {}

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

// 0x100da980 (56 bytes): jumps to the code address in the operand.
void PK_GOTO() {
    const uint32_t target = NextOperand();
    gScriptCurrent->mPos = PositionOf(target, gScriptCodeBase);
}

// 0x100da95c (4 bytes): does nothing.
void PK_NOP() {}

// 0x100db6f0 (4 bytes): does nothing itself; DoScript recognises its descriptor and finishes the script.
void PK_END() {}

// 0x100db714 (20 bytes): the script waits (state 2) until SCR_ScriptTick runs it again.
void PK_WAIT() { gScriptCurrent->mState = 2; }

// 0x100db748 (48 bytes): the operand is the script's text word.
void PK_SETTEXT() {
    const uint32_t operand = NextOperand();
    gScriptCurrent->mText = operand;
}

// 0x100db79c (168 bytes): variable operand 1 = operand 2. Variables 1 to 6 are floats, so for them the integer operand is converted
// (to double, then to single precision); the others take the word as it is.
void PK_SETVAR() {
    const int32_t index = static_cast<int32_t>(NextOperand());
    if (static_cast<uint32_t>(index - 1) <= 5) {
        const int32_t value = static_cast<int32_t>(NextOperand());
        gScriptVars[index] = FloatWord(static_cast<float>(static_cast<double>(value)));
    } else {
        const int32_t value = static_cast<int32_t>(NextOperand());
        gScriptVars[index] = value;
    }
}

// 0x100db48c (88 bytes): pushes a variable (operand 1 is its index).
void PK_PUSHVAR() {
    const int32_t index = static_cast<int32_t>(NextOperand());
    SCR_Push(gScriptVars[index]);
}

// 0x100db508 (96 bytes): pops into a variable (operand 1 is its index).
void PK_POPVAR() {
    const int32_t index = static_cast<int32_t>(NextOperand());
    gScriptVars[index] = SCR_Pop();
}

// 0x100daea0 (172 bytes): calls the subroutine at the operand: pushes the position after the operand and jumps. Nesting deeper than 10
// reports an error (the call still happens).
void PK_GOSUB() {
    const uint32_t target = NextOperand();
    SCR_Push(gScriptCurrent->mPos);
    gScriptCurrent->mPos = PositionOf(target, gScriptCodeBase);
    gScriptCurrent->mGosubDepth = gScriptCurrent->mGosubDepth + 1;
    if (gScriptCurrent->mGosubDepth > 10) SCR_Error("Too many nested GOSUBS");
}

// 0x100daf6c (108 bytes): returns from a subroutine: counts the depth down; below 0 it reports an error (and leaves the position), else
// the position is popped.
void PK_RETURN() {
    gScriptCurrent->mGosubDepth = gScriptCurrent->mGosubDepth - 1;
    if (gScriptCurrent->mGosubDepth < 0) {
        SCR_Error("RETURN without GOSUB");
    } else {
        gScriptCurrent->mPos = SCR_Pop();
    }
}
