// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w164: the script interpreter's flow and arithmetic. The commands IF, ENDIF,
// ELSE, REPEAT, UNTIL, DO and LOOP, the two finders that skip code, and the arithmetic commands ADD, SUB, MUL, MULVAR, DIV, DIVVAR, MOD,
// AND, MODVAR, ADDVAR, COPYVAR and RANDOM. The model is unit w163's (src/ScriptInterp.cpp): every command reads gScriptCurrent,
// gScriptCodeBase and gScriptVars afresh, and an operand is the code word at gScriptCodeBase + 4 * mPos followed by ++mPos.
//
// Tests: a variable index is only ever 0 to 0x12 (the Mac writes outside the block for a larger one). Variables 1 to 6 are floats for the
// arithmetic commands and for IF / UNTIL (their "destination - 1 < 6" test), all others are integers.
#include <climits>
#include <cstdint>
#include <cstring>

#include "World.h"
#include "analyser.h"

namespace {

// The descriptor words the finders look for (kScriptCommandAddress + 8 * index: IF 1, ENDIF 2, ELSE 3).
constexpr uint32_t kIfWord = 0x10266960;
constexpr uint32_t kEndifWord = 0x10266968;
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

// The code address of the word at mPos, counted from `base` (slwi 2; add), and the code end test (cmpw, signed).
uint32_t AddressAt(uint32_t base) { return base + static_cast<uint32_t>(gScriptCurrent->mPos) * 4; }
bool BeforeEnd(uint32_t address) { return static_cast<int32_t>(address) < static_cast<int32_t>(kScriptCodeEnd); }

// The compiler inlined two levels of SCR_FindNextEndif's own recursion into itself (432 bytes), and the three copies differ:
//   level 0 (the function): no tail at all;
//   level 1: leaves with --IF depth (done by the caller here);
//   level 2: tests "address >= end" and reports "End of script before ENDIF found", then --IF depth; its own nested IF is the real call.
// Every level steps over a nested IF with ++mPos, ++depth, the inner walk, --depth, ++mPos.
void WalkToEndif(int level) {
    const uint32_t base = gScriptCurrent->mCodeBase;
    for (;;) {
        const uint32_t address = AddressAt(base);
        if (!BeforeEnd(address)) break;
        const uint32_t word = SCR_CodeWord(address);
        if (word == kEndifWord) break;
        if (word == kIfWord) {
            gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth + 1;
            if (level < 2) {
                WalkToEndif(level + 1);
            } else {
                SCR_FindNextEndif();
            }
            gScriptCurrent->mIfDepth = gScriptCurrent->mIfDepth - 1;
        }
        gScriptCurrent->mPos = gScriptCurrent->mPos + 1;
    }
    if (level == 2) {
        if (!BeforeEnd(AddressAt(base))) SCR_Error("End of script before ENDIF found");
    }
}

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
