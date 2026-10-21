// ScriptOps.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Clock.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "helpers/ScriptHelpers.h"
#include "helpers/ScriptOpsHelpers.h"
#include "HLSprite.h"
#include "LbFile.h"
#include "Ptr32.h"
#include "World.h"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

extern tagSCRIPT* gScriptCurrent;

extern uint32_t gScriptCodeBase;

extern int32_t* gScriptVars;

uint32_t SCR_CodeWord(uint32_t address);

void SCR_Error(const char*);

void SCR_Push(int32_t value);

int32_t SCR_Pop();

// 0x100da95c (4 bytes): does nothing.
void PK_NOP() {}

// 0x100da980 (56 bytes): jumps to the code address in the operand.
void PK_GOTO() {
    const uint32_t target = NextOperand();
    gScriptCurrent->mPos = PositionOf(target, gScriptCodeBase);
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

// divw. The PowerPC leaves the quotient of a division by zero and of INT_MIN / -1 undefined; the tests' Mac side (Unicorn) answers the
// dividend for both. The host never traps.
int32_t PpcDivw(int32_t a, int32_t b) {
    if (b == 0) return a;
    if (a == INT32_MIN && b == -1) return a;
    return a / b;
}

// The operand at the current script's mPos; mPos moves on by one (the commands' common first step).
int32_t Operand() {
    const int32_t pos = gScriptCurrent->mPos;
    gScriptCurrent->mPos = pos + 1;
    return static_cast<int32_t>(SCR_CodeWord(gScriptCodeBase + static_cast<uint32_t>(pos) * 4));
}

float FloatVar(int32_t index) {
    float value;
    std::memcpy(&value, &gScriptVars[index], sizeof value);
    return value;
}

void SetFloatVar(int32_t index, float value) { std::memcpy(&gScriptVars[index], &value, sizeof value); }

// The constant operand of a float command: lis 0x4330 / xoris 0x8000 / fsubs, which is (float)(int32_t)value.
float FloatOf(int32_t value) { return static_cast<float>(value); }

// Variables 1 to 6 are floats: the listings' "index - 1 < 6" unsigned test.
bool IsFloatVar(int32_t index) { return static_cast<uint32_t>(index - 1) < 6; }

}  // namespace

void SCR_FindNextEndif();

void SCR_FindNextElseOrEndif();

// 0x100da9d8 (980 bytes): IF a, test, b. The operands are read first, then the IF depth counts up (above 10: an error, and it goes on).
// a - 1 < 6 chooses the float table, else the integer table; test 1 to 4 compare vars[b] (2 variables), 5 to 8 compare vars[a] with the
// constant b (the float table converts it with (float)(int32_t)). A false test skips to the next ELSE or ENDIF. The tests are the
// listings': test 1 and 2 are "equal" and "not equal", 3 and 4 skip when vars[a] <= / >= the other side (so a NaN does not skip),
// and a test code of 0 or above 8 reports an error and skips nothing.
void PK_IF() {
    const int32_t a = Operand();
    const uint32_t test = static_cast<uint32_t>(Operand());
    const int32_t b = Operand();
    gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth + 1;
    if (gScriptCurrent->mIfDepth > 10) SCR_Error("Too many nested IF's");
    if (IsFloatVar(a)) {
        if (test > 8) {
            SCR_Error("Bad test code in IF statement");
            return;
        }
        switch (test) {
            case 1:
                if (!(FloatVar(b) == FloatVar(a))) SCR_FindNextElseOrEndif();
                break;
            case 2:
                if (!(FloatVar(b) != FloatVar(a))) SCR_FindNextElseOrEndif();
                break;
            case 3:
                if (FloatVar(a) <= FloatVar(b)) SCR_FindNextElseOrEndif();
                break;
            case 4:
                if (FloatVar(a) >= FloatVar(b)) SCR_FindNextElseOrEndif();
                break;
            case 5:
                if (!(FloatOf(b) == FloatVar(a))) SCR_FindNextElseOrEndif();
                break;
            case 6:
                if (!(FloatOf(b) != FloatVar(a))) SCR_FindNextElseOrEndif();
                break;
            case 7:
                if (FloatVar(a) <= FloatOf(b)) SCR_FindNextElseOrEndif();
                break;
            case 8:
                if (FloatVar(a) >= FloatOf(b)) SCR_FindNextElseOrEndif();
                break;
            default:
                SCR_Error("Bad test code in IF statement");
                break;
        }
        return;
    }
    if (test > 8) {
        SCR_Error("Bad test code in IF statement");
        return;
    }
    const int32_t* vars = gScriptVars;
    switch (test) {
        case 1:
            if (vars[b] != vars[a]) SCR_FindNextElseOrEndif();
            break;
        case 2:
            if (vars[b] == vars[a]) SCR_FindNextElseOrEndif();
            break;
        case 3:
            if (vars[a] <= vars[b]) SCR_FindNextElseOrEndif();
            break;
        case 4:
            if (vars[a] >= vars[b]) SCR_FindNextElseOrEndif();
            break;
        case 5:
            if (b != vars[a]) SCR_FindNextElseOrEndif();
            break;
        case 6:
            if (b == vars[a]) SCR_FindNextElseOrEndif();
            break;
        case 7:
            if (vars[a] <= b) SCR_FindNextElseOrEndif();
            break;
        case 8:
            if (vars[a] >= b) SCR_FindNextElseOrEndif();
            break;
        default:
            SCR_Error("Bad test code in IF statement");
            break;
    }
}

// 0x100dadc8 (80 bytes): counts the IF depth down; below 0 it reports an error.
void PK_ENDIF() {
    gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth - 1;
    if (gScriptCurrent->mIfDepth < 0) SCR_Error("ENDIF without IF");
}

// 0x100dae38 (72 bytes): reached by running into an ELSE (the true branch is done): skips to the ENDIF. Without an open IF it reports an
// error first and skips anyway.
void PK_ELSE() {
    if (gScriptCurrent->mIfDepth <= 0) SCR_Error("ELSE without IF");
    SCR_FindNextEndif();
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

// 0x100daff8 (72 bytes): pushes the position after the REPEAT and counts the loop depth up.
void PK_REPEAT() {
    SCR_Push(gScriptCurrent->mPos);
    gScriptCurrent->mLoopDepth = gScriptCurrent->mLoopDepth + 1;
}

// 0x100db060 (1036 bytes): UNTIL a, test, b, the twin of PK_IF with its own tables. The three operands are read first; without an open loop
// it reports an error and stops. The test says whether to go round again (listing order: test 1 "not equal", 2 "equal", 3 a <= b, 4 a >= b,
// 5 to 8 the same against the constant); when it does, the saved position is popped, pushed back and made the position; else it is
// popped and the loop depth counts down. A bad test code (0 or above 8) reports an error and ends the loop.
void PK_UNTIL() {
    const int32_t a = Operand();
    const uint32_t test = static_cast<uint32_t>(Operand());
    const int32_t b = Operand();
    if (gScriptCurrent->mLoopDepth <= 0) {
        SCR_Error("UNTIL without REPEAT");
        return;
    }
    bool again = false;
    if (IsFloatVar(a)) {
        switch (test) {
            case 1:
                again = !(FloatVar(b) == FloatVar(a));
                break;
            case 2:
                again = FloatVar(b) == FloatVar(a);
                break;
            case 3:
                again = FloatVar(a) <= FloatVar(b);
                break;
            case 4:
                again = FloatVar(a) >= FloatVar(b);
                break;
            case 5:
                again = !(FloatOf(b) == FloatVar(a));
                break;
            case 6:
                again = FloatOf(b) == FloatVar(a);
                break;
            case 7:
                again = FloatVar(a) <= FloatOf(b);
                break;
            case 8:
                again = FloatVar(a) >= FloatOf(b);
                break;
            default:
                SCR_Error("Bad test code in UNTIL statement");
                break;
        }
    } else {
        const int32_t* vars = gScriptVars;
        switch (test) {
            case 1:
                again = vars[a] != vars[b];
                break;
            case 2:
                again = vars[a] == vars[b];
                break;
            case 3:
                again = vars[a] <= vars[b];
                break;
            case 4:
                again = vars[a] >= vars[b];
                break;
            case 5:
                again = vars[a] != b;
                break;
            case 6:
                again = vars[a] == b;
                break;
            case 7:
                again = vars[a] <= b;
                break;
            case 8:
                again = vars[a] >= b;
                break;
            default:
                SCR_Error("Bad test code in UNTIL statement");
                break;
        }
    }
    if (again) {
        const int32_t position = SCR_Pop();
        SCR_Push(position);
        gScriptCurrent->mPos = position;
    } else {
        SCR_Pop();
        gScriptCurrent->mLoopDepth = gScriptCurrent->mLoopDepth - 1;
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

// 0x100db588 (124 bytes): DO count: pushes the position after the operand, then the count, and counts the loop depth up.
void PK_DO() {
    const int32_t count = Operand();
    SCR_Push(gScriptCurrent->mPos);
    SCR_Push(count);
    gScriptCurrent->mLoopDepth = gScriptCurrent->mLoopDepth + 1;
}

// 0x100db620 (176 bytes): LOOP: without an open loop it reports an error. Else it pops the count, then the position; a count of 2 or more
// pushes the position and count - 1 back and jumps to the position, otherwise the loop depth counts down.
void PK_LOOP() {
    if (gScriptCurrent->mLoopDepth <= 0) {
        SCR_Error("LOOP without DO");
        return;
    }
    const int32_t count = SCR_Pop();
    const int32_t position = SCR_Pop();
    const int32_t remaining = static_cast<int32_t>(static_cast<uint32_t>(count) - 1);
    if (remaining > 0) {
        SCR_Push(position);
        SCR_Push(remaining);
        gScriptCurrent->mPos = position;
    } else {
        gScriptCurrent->mLoopDepth = gScriptCurrent->mLoopDepth - 1;
    }
}

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

// 0x100db864 (184 bytes): ADD var, constant: a float variable (1 to 6) gets (float)constant added (fadds), else the integers add.
void PK_ADD() {
    const int32_t dest = Operand();
    if (IsFloatVar(dest)) {
        const float value = FloatVar(dest);
        const int32_t constant = Operand();
        SetFloatVar(dest, value + FloatOf(constant));
        return;
    }
    const uint32_t value = static_cast<uint32_t>(gScriptVars[dest]);
    gScriptVars[dest] = static_cast<int32_t>(value + static_cast<uint32_t>(Operand()));
}

// 0x100db93c (184 bytes): SUB var, constant.
void PK_SUB() {
    const int32_t dest = Operand();
    if (IsFloatVar(dest)) {
        const float value = FloatVar(dest);
        const int32_t constant = Operand();
        SetFloatVar(dest, value - FloatOf(constant));
        return;
    }
    const uint32_t value = static_cast<uint32_t>(gScriptVars[dest]);
    gScriptVars[dest] = static_cast<int32_t>(value - static_cast<uint32_t>(Operand()));
}

// 0x100dba14 (184 bytes): MUL var, constant.
void PK_MUL() {
    const int32_t dest = Operand();
    if (IsFloatVar(dest)) {
        const float value = FloatVar(dest);
        const int32_t constant = Operand();
        SetFloatVar(dest, value * FloatOf(constant));
        return;
    }
    const uint32_t value = static_cast<uint32_t>(gScriptVars[dest]);
    gScriptVars[dest] = static_cast<int32_t>(value * static_cast<uint32_t>(Operand()));
}

// 0x100dbaec (148 bytes): MULVAR var, var: multiplies FLOAT variables only: the destination is variable 1 if the operand is 1, else variable
// 2 (whatever the operand was; a quirk of the original). The listing has an integer block (a mullw into variable 2 at 0x100dbb64) that no
// branch reaches, so it is not translated.
void PK_MULVAR() {
    int32_t dest = Operand();
    const int32_t source = Operand();
    if (dest != 1) dest = 2;
    SetFloatVar(dest, FloatVar(dest) * FloatVar(source));
}

// 0x100dbba0 (192 bytes): DIV var, constant: a float variable is divided by (float)constant (fdivs: 0 gives an infinity or a NaN), else divw.
void PK_DIV() {
    const int32_t dest = Operand();
    if (IsFloatVar(dest)) {
        const float value = FloatVar(dest);
        const int32_t constant = Operand();
        SetFloatVar(dest, value / FloatOf(constant));
        return;
    }
    const int32_t value = gScriptVars[dest];
    gScriptVars[dest] = PpcDivw(value, Operand());
}

// 0x100dbc80 (152 bytes): DIVVAR var, var: vars[dest] / vars[source], float for the destinations 1 to 6, else divw.
void PK_DIVVAR() {
    const int32_t dest = Operand();
    const int32_t source = Operand();
    if (IsFloatVar(dest)) {
        SetFloatVar(dest, FloatVar(dest) / FloatVar(source));
        return;
    }
    gScriptVars[dest] = PpcDivw(gScriptVars[dest], gScriptVars[source]);
}

// 0x100dbd38 (104 bytes): MOD var, constant (integers only): a - (a / n) * n with divw.
void PK_MOD() {
    const int32_t dest = Operand();
    const int32_t value = gScriptVars[dest];
    const int32_t divisor = Operand();
    gScriptVars[dest] = static_cast<int32_t>(static_cast<uint32_t>(value) - static_cast<uint32_t>(PpcDivw(value, divisor)) * static_cast<uint32_t>(divisor));
}

// 0x100dbdc0 (124 bytes): AND var, var, constant: vars[dest] = vars[source] & constant.
void PK_AND() {
    const int32_t dest = Operand();
    const int32_t source = Operand();
    const int32_t constant = Operand();
    gScriptVars[dest] = gScriptVars[source] & constant;
}

// 0x100dbe5c (112 bytes): MODVAR var, var (integers only): a - (a / vars[source]) * vars[source].
void PK_MODVAR() {
    const int32_t dest = Operand();
    const int32_t source = Operand();
    const int32_t value = gScriptVars[dest];
    const int32_t divisor = gScriptVars[source];
    gScriptVars[dest] = static_cast<int32_t>(static_cast<uint32_t>(value) - static_cast<uint32_t>(PpcDivw(value, divisor)) * static_cast<uint32_t>(divisor));
}

// 0x100dbeec (144 bytes): ADDVAR var, var: vars[dest] + vars[source], float (fadds) for the destinations 1 to 6, else the integers add.
void PK_ADDVAR() {
    const int32_t dest = Operand();
    const int32_t source = Operand();
    if (IsFloatVar(dest)) {
        SetFloatVar(dest, FloatVar(dest) + FloatVar(source));
        return;
    }
    gScriptVars[dest] = static_cast<int32_t>(static_cast<uint32_t>(gScriptVars[dest]) + static_cast<uint32_t>(gScriptVars[source]));
}

// 0x100dbf9c (92 bytes): COPYVAR dest, source: the raw word.
void PK_COPYVAR() {
    const int32_t dest = Operand();
    const int32_t source = Operand();
    gScriptVars[dest] = gScriptVars[source];
}

// 0x100dc01c (164 bytes): RANDOM var, n: reads the destination, asks the world for a random number (before reading n), then stores
// r - (r / (n + 1)) * (n + 1) with r the answer as a signed number.
void PK_RANDOM() {
    const int32_t dest = Operand();
    const int32_t random = static_cast<int32_t>(gGameWorld->GetRand());
    const int32_t divisor = static_cast<int32_t>(static_cast<uint32_t>(Operand()) + 1u);
    gScriptVars[dest] = static_cast<int32_t>(static_cast<uint32_t>(random) - static_cast<uint32_t>(PpcDivw(random, divisor)) * static_cast<uint32_t>(divisor));
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

float WordFloat(int32_t word) {
    float value;
    std::memcpy(&value, &word, sizeof value);
    return value;
}

}  // namespace

// 0x100dc0e0 (96 bytes): starts a script at the code pointer in the operand, with the object id (variable 11) and the position (1 to 3).
void PK_SPAWN() {
    tagSCRIPT* current = gScriptCurrent;
    const int32_t pos = current->mPos;
    current->mPos = pos + 1;
    const uint32_t word = SCR_CodeWord(gScriptCodeBase + static_cast<uint32_t>(pos) * 4);
    const int32_t* vars = gScriptVars;
    // The operand is a Mac code address; SCR_SpawnScript takes a pointer, which the host finds in the code array (NULL outside it).
    const void* code = nullptr;
    if (word >= kScriptCodeAddress && word < kScriptCodeAddress + static_cast<uint32_t>(gScriptCodeWordsEnd - gScriptCodeWords) * 4)
        code = &gScriptCodeWords[(word - kScriptCodeAddress) / 4];
    SCR_SpawnScript(const_cast<void*>(code), vars[11], WordFloat(vars[1]), WordFloat(vars[2]), WordFloat(vars[3]));
}

// 0x100dc160 (52 bytes): the operand is the object id (variable 11).
void PK_SET_OBJ_ID() {
    const uint32_t operand = NextOperand();
    gScriptVars[11] = static_cast<int32_t>(operand);
}

// 0x100dc1b8 (92 bytes): the operand is the action (variable 12).
void PK_SET_OBJ_ACT() {
    const uint32_t operand = NextOperand();
    gScriptVars[12] = static_cast<int32_t>(operand);
}

// 0x100dc214 (148 bytes): variable 13 (the sprite) takes the variable the operand names; -1 removes the sprite flag, anything else draws
// the sprite. The script then waits (state 2).
void PK_PutSpriteVAR() {
    const int32_t index = static_cast<int32_t>(NextOperand());
    gScriptVars[13] = gScriptVars[index];
    if (gScriptVars[13] == -1) {
        gScriptCurrent->mHasSprite = 0;
    } else {
        DrawHLSprite(gScriptCurrent);
    }
    gScriptCurrent->mState = 2;
}

// 0x100dc2d0 (180 bytes): the same with the operand as the sprite number itself.
void PK_PutSpriteIM() {
    const int32_t operand = static_cast<int32_t>(NextOperand());
    gScriptVars[13] = operand;
    if (gScriptVars[13] == -1) {
        gScriptCurrent->mHasSprite = 0;
    } else {
        DrawHLSprite(gScriptCurrent);
    }
    gScriptCurrent->mState = 2;
}

// 0x100dc384 (148 bytes): the operand is the 3D sprite number; always drawn; the script waits.
void PK_Put3DSpriteIM() {
    const int32_t operand = static_cast<int32_t>(NextOperand());
    gScriptVars[13] = operand;
    DrawHLSprite(gScriptCurrent);
    gScriptCurrent->mState = 2;
}

// 0x100dc418 (28 bytes): clears the sprite record's word at +0x28 and the sprite flag.
void PK_RemoveSprite() {
    std::memset(gScriptCurrent->mSprite + 0x28, 0, 4);
    gScriptCurrent->mHasSprite = 0;
}

// The scale (variable 8) and rotation (variable 9) commands: 1.0 and the step 0.01 (0x3c23d70a) of the Mac's data.
void PK_ResetScale() { gScriptVars[8] = FloatWord(1.0f); }
void PK_ReduceScale() { gScriptVars[8] = FloatWord(WordFloat(gScriptVars[8]) - 0.01f); }
void PK_IncreaseScale() { gScriptVars[8] = FloatWord(WordFloat(gScriptVars[8]) + 0.01f); }
void PK_ResetRotation() { gScriptVars[9] = FloatWord(1.0f); }
void PK_ReduceRotation() { gScriptVars[9] = FloatWord(WordFloat(gScriptVars[9]) - 0.01f); }
void PK_IncreaseRotation() { gScriptVars[9] = FloatWord(WordFloat(gScriptVars[9]) + 0.01f); }
