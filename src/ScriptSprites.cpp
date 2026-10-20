// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w165: the script sprites. The ESP_ functions that restart, compare, copy
// and repair the scripts of sprites, and the PK_ commands that set the sprite set, the sprite and its action, show and remove the sprite,
// and change the scale and the rotation (variables 8 and 9). The script block, its table and the code array are those of
// src/ScriptInterp.cpp (unit w163); the variable window gScriptVars is the current script's mVars.
//
// A variable is a 32-bit word; the float ones (1 to 6, 8, 9) are read and written with std::memcpy. The Mac functions do not guard their
// handles: a handle outside the table finds a NULL script, which the ones that dereference it would read at address 0 (not reproduced; the
// host faults the same way on the NULL pointer). A custom data block number comes in as a code argument of 0 to 3.
#include <cstdint>
#include <cstring>

#include "ESP_sprites.h"
#include "Ptr32.h"
#include "analyser.h"
#include "LbFile.h"

namespace {

// The table's entry for `index`, re-read from the global each time as the Mac does.
Ptr32<tagSCRIPT> Entry(int32_t index) { return static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index]; }

// The script of a handle: 1 to the table's size - 1 (signed), else none.
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
