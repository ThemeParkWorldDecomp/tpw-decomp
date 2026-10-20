// Unit v5882: ProcessInstruction 0x100cfdf4 (12,340 bytes), the ride script interpreter of the original's rsse.cpp: one instruction of a
// script's program per call. The program's words come from ReadScript (the program counter is mState); an instruction is 0x80000000 + its
// number (0 to 0x69, a jump table), its operands follow. An operand word with the top byte 0x40 names a variable (mVars[low bits]), 0x20 a
// program address (a jump), 0x10 an offset into mSaveBlock (a text) or a value for mF74[0]; ExtractData turns an operand into a value.
// Most instructions leave their result in mCommandAnswer (0x48) and copy it into a variable when the destination word names one.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Clock.h"
#include "Debug.h"
#include "LevelInit.h"
#include "Light.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte).
const char kHeapError[] = "RSSE: Heap Error";                                                    // +0x6de
const char kRemoteVariable[] = "RSSE: Illegal variable reference to remote script\n";            // +0x6ef
const char kBadParticleNode[] = "RSSE: Bad particle node in %s";                                 // +0x722
const char kUnknownInstruction[] = "RSSE: Unknown instruction";                                  // +0x740
const char kMissingParameter[] = "RSSE: Bad instruction - missing parameter ?";                  // +0x75a

const int32_t kDead = -10000;   // 0xffffd8f0: the state of a killed script

// The fields inside RSSE_SCRIPT's byte arrays, by Mac offset (the word at 0xb4 in mFB4, 0xbc in mFB9, 0xc4 in mFC2; the halves of mFDC).
uint32_t Word(const RSSE_SCRIPT* script, size_t offset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(script) + offset, 4);
    return value;
}

void SetWord(RSSE_SCRIPT* script, size_t offset, uint32_t value) {
    std::memcpy(reinterpret_cast<uint8_t*>(script) + offset, &value, 4);
}

int16_t Half(const RSSE_SCRIPT* script, size_t offset) {
    int16_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(script) + offset, 2);
    return value;
}

void SetHalf(RSSE_SCRIPT* script, size_t offset, int16_t value) {
    std::memcpy(reinterpret_cast<uint8_t*>(script) + offset, &value, 2);
}

bool IsVariable(uint32_t word) { return (word & 0xff000000u) == 0x40000000u; }

// The answer copied into the variable the destination word names (any other word: nothing).
void StoreAnswer(RSSE_SCRIPT* script, uint32_t word) {
    if (IsVariable(word)) script->mVars[word ^ 0x40000000u] = script->mCommandAnswer;
}

// A jump: the word's low bits when its top byte is 0x20, else the script dies.
void Jump(RSSE_SCRIPT* script, uint32_t word) {
    if ((word & 0xff000000u) == 0x20000000u)
        script->mState = static_cast<int32_t>(word ^ 0x20000000u);
    else
        script->mState = kDead;
}

int32_t Operand(RSSE_SCRIPT* script) { return ExtractData(script, static_cast<int32_t>(ReadScript(script))); }

// SetCurRideAnim on the script's ride (a local copy of mRideAnim goes to GetRideFromID); no ride: 0.
uint32_t PlayAnim(RSSE_SCRIPT* script, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b) {
    uint32_t id = script->mRideAnim;
    if (id == 0) return 0;
    return SetCurRideAnimDuration(*GetRideFromID(id), a, anim, loop, speed, b);
}

// GetCurrentAnim on the script's ride into *kind; no ride: 0 and *kind untouched.
uint32_t CurrentAnim(RSSE_SCRIPT* script, uint32_t* kind, uint32_t control) {
    uint32_t id = script->mRideAnim;
    if (id == 0) return 0;
    return processinstruction_callee::GetCurrentAnim(*GetRideFromID(id), kind, nullptr, control);
}

// The animation's length less 300, at least 300 (signed).
void ClampAnswer(RSSE_SCRIPT* script, uint32_t length) {
    script->mCommandAnswer = static_cast<int32_t>(length - 300u);
    if (script->mCommandAnswer < 300) script->mCommandAnswer = 300;
}

// The answer in ticks at the script's speed.
int32_t Scaled(int32_t value, float speed) { return ppc_fctiwz(static_cast<float>(value) / speed); }

// The wait of ops 17, 24, 44 and 45: answers true when a wait runs (mFA0[0] set): done when the clock reached it, else the
// instruction runs again (the program counter goes back `back` words) and the tick ends.
bool Waiting(RSSE_SCRIPT* script, int32_t back) {
    if (script->mFA0[0] == 0) return false;
    if (static_cast<uint32_t>(gClock.Read()) >= script->mFA0[0]) {
        script->mFA0[0] = 0;
    } else {
        script->mState -= back;
        script->mCountdown = 0;
    }
    return true;
}

// The light of the ride's data node with the id `node` (FindDataNodeIndexByID's index, mask 0x20000).
sNodeLightParams* Light(RSSE_SCRIPT* script, int32_t node) {
    uint32_t id = script->mRideAnim;
    const uint32_t index = FindDataNodeIndexByID(*GetRideFromID(id), 0x20000, static_cast<uint32_t>(node));
    return GetLightsourceHandle(script->mRideAnim, index);
}

// The script name and the text at the save block's offset (ops 63 and 64): the path of a linked script.
void LinkedPath(RSSE_SCRIPT* script, uint32_t word, char* path) {
    processinstruction_callee::StrCpy(path, script->mName.get());
    processinstruction_callee::StrCat(path, reinterpret_cast<const char*>(script->mSaveBlock.get() + (word ^ 0x10000000u)));
}

// Ops 66 and 68: a variable of the script `handle` set (an index past its count: nothing; a negative one is written).
void SetRemote(RSSE_SCRIPT* script, int32_t handle, int32_t index, int32_t value) {
    if (handle == 0) return;
    RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);   // no NULL check in the original
    if (index >= other->mVarCount) return;
    other->mVars[index] = value;
    script->mCommandAnswer = value;
}

// Ops 67 and 69: a variable of the script `handle` read into the variable `word` names.
void GetRemote(RSSE_SCRIPT* script, int32_t handle, uint32_t word, int32_t index) {
    if (!IsVariable(word)) return;
    if (handle == 0) return;
    RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
    if (index >= other->mVarCount) return;
    script->mCommandAnswer = other->mVars[index];
    script->mVars[word ^ 0x40000000u] = script->mCommandAnswer;
}

// Ops 97 to 102: a word of the local time (struct tm, `field` longs in; op 98 adds 1 to the month).
void TimeField(RSSE_SCRIPT* script, int32_t field, int32_t add) {
    int32_t now;
    processinstruction_callee::Time(&now);
    const int32_t* tm = processinstruction_callee::LocalTime(&now);
    script->mCommandAnswer = tm[field] + add;
    const uint32_t word = ReadScript(script);
    StoreAnswer(script, word);
}

}  // namespace

void ProcessInstruction(RSSE_SCRIPT* script) {
    using namespace processinstruction_callee;
    RSSE_SCRIPT* const s = script;
    const float speed = 0.5f + static_cast<float>(static_cast<int32_t>(s->mSpeed)) / 100.0f;
    const uint32_t op = ReadScript(s);
    if (s->mState < 0) return;
    if ((op & 0xff000000u) != 0x80000000u) {
        GAME_ASSERTMSG(0, kMissingParameter);
        s->mState = kDead;
        return;
    }
    const uint32_t n = op ^ 0x80000000u;
    if (n > 0x69) {
        GAME_ASSERTMSG(0, kUnknownInstruction);
        s->mState = kDead;
        return;
    }
    switch (n) {
    case 0:
        break;
    case 1:
        gRSSEF7BE4 = 1;
        break;
    case 2:
        gRSSEF7BE4 = 0;
        s->mCountdown = 0;
        break;
    case 3: {   // set a variable
        const uint32_t w = ReadScript(s);
        if (!IsVariable(w)) return;
        const int32_t v = Operand(s);
        s->mVars[w ^ 0x40000000u] = v;
        s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 4:
        s->mCommandAnswer = static_cast<int32_t>(ReadScript(s));
        break;
    case 5: {   // a difference
        const uint32_t w = ReadScript(s);
        const uint32_t b = ReadScript(s);
        const uint32_t c = ReadScript(s);
        const int32_t x = ExtractData(s, static_cast<int32_t>(b));
        s->mCommandAnswer = x - ExtractData(s, static_cast<int32_t>(c));
        StoreAnswer(s, w);
        break;
    }
    case 6:
        s->mCountdown = 0;
        break;
    case 7: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = gClock.Read();
        StoreAnswer(s, w);
        break;
    }
    case 8:
    case 9: {   // an object at a node (8: the second coordinate 1000)
        RSSE_OBJECT* object = CreateObject(s);
        object->mType = Operand(s);
        object->mNodeId = Operand(s);
        const int32_t x = Operand(s);
        const int32_t y = Operand(s);
        if (n == 8) {
            InitObject(s, object, x, 1000, y);
        } else {
            const int32_t z = Operand(s);
            InitObject(s, object, x, y, z);
        }
        break;
    }
    case 10:
    case 11: {   // the objects of a group killed (11: faded)
        const int32_t group = Operand(s);
        RSSE_OBJECT* object = s->mObjects.get();
        while (object != nullptr) {
            if (group == object->mF18) {
                KillLinkedObject(s, object, n == 11 ? 1 : 0);
                RSSE_OBJECT* next = object->mNext.get();
                KillObject(s, object);
                object = next;
            } else {
                object = object->mNext.get();
            }
        }
        break;
    }
    case 12: {
        const int32_t group = Operand(s);
        const int32_t param = Operand(s);
        const int32_t value = Operand(s);
        for (RSSE_OBJECT* object = s->mObjects.get(); object != nullptr; object = object->mNext.get())
            if (group == object->mF18) SetObjParam(s, object, param, value);
        break;
    }
    case 13: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        StartEvent(s, a, b, c, 1000);
        break;
    }
    case 14: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        const int32_t d = Operand(s);
        StartEvent(s, a, b, c, d);
        break;
    }
    case 15: {
        uint32_t id = s->mRideAnim;
        if (id == 0) return;
        FlushDeferredAnim(*GetRideFromID(id), 0);
        break;
    }
    case 16: {   // play an animation; the answer is its length
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        s->mFA0[2] = 0xffff;
        break;
    }
    case 17: {   // play an animation and wait for it
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        if (Waiting(s, 3)) return;
        s->mAnimFactor = 1000;
        s->mFA0[0] = PlayAnim(s, a, b, 0, speed, 0) - 300u;
        s->mFA0[0] = static_cast<uint32_t>(ppc_fctiwz(static_cast<float>(s->mFA0[0]) / speed));
        if (s->mFA0[0] < 300u) s->mFA0[0] = 300;
        s->mFA0[0] += static_cast<uint32_t>(gClock.Read());
        s->mState -= 3;
        s->mCountdown = 0;
        s->mFA0[1] = 0;
        s->mFA0[2] = 0xffff;
        break;
    }
    case 18: {   // loop an animation (once per key)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t key = static_cast<uint32_t>(a) + (static_cast<uint32_t>(b) << 16);
        if (s->mFA0[2] == key) return;
        s->mAnimFactor = 1000;
        PlayAnim(s, a, b, 1, speed, 0);
        s->mFA0[1] = 0;
        s->mFA0[2] = key;
        break;
    }
    case 19: {   // play an animation and wait until it is no longer the current one
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        uint32_t w = ReadScript(s);
        if (static_cast<int32_t>(Word(s, 0xbc)) != 0) {
            CurrentAnim(s, &w, 0);
            if (static_cast<int32_t>(w + 1) != static_cast<int32_t>(Word(s, 0xbc))) {
                s->mState -= 4;
                s->mCountdown = 0;
            } else {
                SetWord(s, 0xbc, 0);
            }
            return;
        }
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        s->mFA0[2] = 0xffff;
        SetWord(s, 0xbc, static_cast<uint32_t>(a + 1));
        s->mState -= 4;
        break;
    }
    case 20: {   // the current animation (-1 when it ended)
        const uint32_t w = ReadScript(s);
        if (CurrentAnim(s, reinterpret_cast<uint32_t*>(&s->mCommandAnswer), 0) & 4) s->mCommandAnswer = -1;
        StoreAnswer(s, w);
        break;
    }
    case 21: {   // play an animation at a factor (per thousand)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        const int32_t factor = Operand(s);
        s->mAnimFactor = static_cast<int16_t>(factor);
        ClampAnswer(s, PlayAnim(s, a, b, 0, (speed * static_cast<float>(factor)) / 1000.0f, 0));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>((s->mCommandAnswer * 1000) / factor + clock);
        s->mFA0[2] = 0xffff;
        break;
    }
    case 22: {
        const int32_t flag = Operand(s);
        uint32_t id = s->mRideAnim;
        if (id == 0) return;
        FlushDeferredAnim(*GetRideFromID(id), static_cast<uint32_t>(flag));
        break;
    }
    case 23: {   // 16 with a control word
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, c));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        break;
    }
    case 24: {   // 17 with a raw control word, unscaled
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t c = ReadScript(s);
        if (Waiting(s, 3)) return;
        s->mAnimFactor = 1000;
        s->mFA0[0] = PlayAnim(s, a, b, 0, speed, c) - 300u;
        if (s->mFA0[0] < 300u) s->mFA0[0] = 300;
        s->mFA0[0] += static_cast<uint32_t>(gClock.Read());
        s->mState -= 3;
        s->mCountdown = 0;
        s->mFA0[1] = 0;
        break;
    }
    case 25: {   // 18 with a raw control word, no key
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const uint32_t c = ReadScript(s);
        s->mAnimFactor = 1000;
        PlayAnim(s, a, b, 1, speed, c);
        s->mFA0[1] = 0;
        break;
    }
    case 26: {   // 19 with a control word
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        if (static_cast<int32_t>(Word(s, 0xbc)) != 0) {
            CurrentAnim(s, &w, static_cast<uint32_t>(c));
            if (static_cast<int32_t>(w + 1) != static_cast<int32_t>(Word(s, 0xbc))) {
                s->mState -= 4;
                s->mCountdown = 0;
            } else {
                SetWord(s, 0xbc, 0);
            }
            return;
        }
        s->mAnimFactor = 1000;
        ClampAnswer(s, PlayAnim(s, a, b, 0, speed, c));
        StoreAnswer(s, w);
        const int32_t clock = gClock.Read();
        s->mFA0[1] = static_cast<uint32_t>(Scaled(s->mCommandAnswer, speed) + clock);
        SetWord(s, 0xbc, static_cast<uint32_t>(a + 1));
        s->mState -= 4;
        break;
    }
    case 27: {   // 20 with a control word
        const uint32_t w = ReadScript(s);
        const int32_t c = Operand(s);
        if (CurrentAnim(s, reinterpret_cast<uint32_t*>(&s->mCommandAnswer), static_cast<uint32_t>(c)) & 4) s->mCommandAnswer = -1;
        StoreAnswer(s, w);
        break;
    }
    case 28: {   // a random number 0 to the operand (a raw halfword)
        const uint32_t w = ReadScript(s);
        const int32_t top = static_cast<int16_t>(ReadScript(s));
        const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
        const int32_t range = top + 1;
        s->mCommandAnswer = Labs(r - (r / range) * range);
        StoreAnswer(s, w);
        break;
    }
    case 29: {   // call
        const uint32_t w = ReadScript(s);
        Push(s, static_cast<int32_t>(static_cast<uint32_t>(s->mState) | 0x20000000u));
        if ((w & 0xff000000u) == 0x20000000u) s->mState = static_cast<int32_t>(w ^ 0x20000000u);
        break;
    }
    case 30: {   // return
        const uint32_t w = static_cast<uint32_t>(Pop(s));
        if ((w & 0xff000000u) == 0x20000000u) s->mState = static_cast<int32_t>(w ^ 0x20000000u);
        break;
    }
    case 31:
        Jump(s, ReadScript(s));
        break;
    case 32: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer != 0) return;
        Jump(s, w);
        break;
    }
    case 33: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer == 0) return;
        Jump(s, w);
        break;
    }
    case 34: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer >= 0) return;
        Jump(s, w);
        break;
    }
    case 35: {
        const uint32_t w = ReadScript(s);
        if (s->mCommandAnswer <= 0) return;
        Jump(s, w);
        break;
    }
    case 36:
        ReadScript(s);
        break;
    case 37: {
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) == 0x10000000u) s->mF74[0] = w ^ 0x10000000u;
        break;
    }
    case 38: {
        const uint32_t w = ReadScript(s);
        if (IsVariable(w)) s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 39: {
        const uint32_t w = ReadScript(s);
        const int32_t d = Operand(s);
        if (IsVariable(w)) s->mCommandAnswer = s->mVars[w ^ 0x40000000u] - d;
        break;
    }
    case 40: {
        const uint32_t w = ReadScript(s);
        const int32_t d = ExtractData(s, static_cast<int32_t>(w));
        Push(s, d);
        s->mCommandAnswer = d;
        break;
    }
    case 41: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = Pop(s);
        StoreAnswer(s, w);
        break;
    }
    case 42: {   // a push on the heap (mF44 counts up, unlike Push)
        const uint32_t w = ReadScript(s);
        if (s->mStack.get() != nullptr && static_cast<int32_t>(s->mF44) >= 0 && static_cast<int32_t>(s->mF44) < s->mStackSize) {
            const int32_t d = ExtractData(s, static_cast<int32_t>(w));
            s->mStack[s->mF44] = d;
            s->mCommandAnswer = d;
            s->mF44 += 1;
        } else {
            GAME_ASSERTMSG(0, kHeapError);
        }
        break;
    }
    case 43: {   // a pop from the heap (the bound tests mStackIndex, not mF44)
        const uint32_t w = ReadScript(s);
        if (s->mStack.get() != nullptr && static_cast<int32_t>(s->mF44) > 0 && s->mStackIndex <= s->mStackSize) {
            s->mF44 -= 1;
            s->mCommandAnswer = s->mStack[s->mF44];
            StoreAnswer(s, w);
        } else {
            GAME_ASSERTMSG(0, kHeapError);
        }
        break;
    }
    case 44: {   // wait, scaled by the speed
        if (s->mFA0[0] != 0) {
            ReadScript(s);
            Waiting(s, 2);
            return;
        }
        const int32_t d = Operand(s);
        s->mFA0[0] = static_cast<uint32_t>(Scaled(d, speed) + gClock.Read());
        s->mState -= 2;
        s->mCountdown = 0;
        break;
    }
    case 45: {   // wait, unscaled
        if (s->mFA0[0] != 0) {
            ReadScript(s);
            Waiting(s, 2);
            return;
        }
        const int32_t clock = gClock.Read();
        s->mFA0[0] = static_cast<uint32_t>(clock + Operand(s));
        s->mState -= 2;
        s->mCountdown = 0;
        break;
    }
    case 46: {   // wait for the animation's end (mFA0[1])
        if (s->mFA0[1] == 0) return;
        if (static_cast<uint32_t>(gClock.Read()) >= s->mFA0[1]) {
            s->mFA0[1] = 0;
        } else {
            s->mState -= 1;
            s->mCountdown = 0;
        }
        break;
    }
    case 47: {
        const uint32_t w = ReadScript(s);
        const uint32_t v = ReadScript(s);
        if (!IsVariable(w)) return;
        s->mVars[w ^ 0x40000000u] += ExtractData(s, static_cast<int32_t>(v));
        s->mCommandAnswer = s->mVars[w ^ 0x40000000u];
        break;
    }
    case 48:
    case 49:
    case 50: {   // product, quotient, remainder (a zero divisor answers 0)
        const uint32_t w = ReadScript(s);
        const int32_t x = Operand(s);
        const int32_t y = Operand(s);
        if (n == 48)
            s->mCommandAnswer = x * y;
        else if (n == 49)
            s->mCommandAnswer = y != 0 ? x / y : 0;
        else
            s->mCommandAnswer = y != 0 ? x % y : 0;
        StoreAnswer(s, w);
        break;
    }
    case 51:
        s->mEveryFrame = static_cast<uint8_t>(ReadScript(s));
        break;
    case 52:
        s->mState = kDead;
        break;
    case 53:
        ExecuteTourSpecificCommand(s);
        break;
    case 54:
        ExecuteBumperSpecificCommand(s);
        break;
    case 55:
        ExecuteCoasterSpecificCommand(s);
        break;
    case 56: {   // a peep on a free head of the ride (a random free slot of mSaveWords)
        const int32_t v = Operand(s);
        int32_t* slots = s->mSaveWords.get();
        if (slots == nullptr) return;
        bool free = false;
        for (int32_t i = 0; i < s->mSaveWordCount; ++i)
            if (slots[i] == 0) free = true;
        if (!free) return;
        int32_t i;
        do {
            const int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
            i = r - (r / s->mSaveWordCount) * s->mSaveWordCount;
        } while (s->mSaveWords[i] != 0);
        s->mSaveWords[i] = v;
        uint32_t id = s->mRideAnim;
        const uint32_t node = FindDataNodeIndexByID(*GetRideFromID(id), 0x80, static_cast<uint32_t>(i + 1));
        if (node == 0xffffffffu) return;
        uint32_t id2 = s->mRideAnim;
        EnableRideHead((*GetRideFromID(id2))->mHeads, node, static_cast<uint32_t>(v));
        break;
    }
    case 57: {   // the peep's heads taken off
        const int32_t v = Operand(s);
        if (s->mSaveWords.get() == nullptr) return;
        for (int32_t i = 0; i < s->mSaveWordCount; ++i) {
            if (v == s->mSaveWords[i]) {
                uint32_t id = s->mRideAnim;
                const uint32_t node = FindDataNodeIndexByID(*GetRideFromID(id), 0x80, static_cast<uint32_t>(i + 1));
                if (node != 0xffffffffu) {
                    uint32_t id2 = s->mRideAnim;
                    DisableRideHead((*GetRideFromID(id2))->mHeads, node);
                }
                s->mSaveWords[i] = 0;
            }
        }
        break;
    }
    case 58: {
        const int32_t peep = Operand(s);
        const int32_t time = Operand(s);
        s->mCommandAnswer = LIMBO_Put(s, static_cast<uint32_t>(peep), time);
        break;
    }
    case 59: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(LIMBO_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 60: {
        const uint32_t w = ReadScript(s);
        if (!IsVariable(w)) return;
        s->mCommandAnswer = static_cast<int32_t>(LIMBO_ForceGet(s));
        s->mVars[w ^ 0x40000000u] = s->mCommandAnswer;
        break;
    }
    case 61: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mLimboUsed;
        StoreAnswer(s, w);
        break;
    }
    case 62: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mLimboCount - s->mLimboUsed;
        StoreAnswer(s, w);
        break;
    }
    case 63: {   // a linked script started (it shares the thing, the fly ride and the ride)
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) != 0x10000000u) return;
        char path[256];
        LinkedPath(s, w, path);
        // The thing argument (a tag_IDX_TYPE word) is never set: its id half is 0.
        s->mLinked = RSSE_InitScript(path, 0, 0);
        if (s->mLinked == 0) return;
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(s->mLinked);   // no NULL check in the original
        other->mF10 = s->mHandle;
        other->mThingId = s->mThingId;
        other->mFlyHandle = s->mFlyHandle;
        SetWord(other, 0xb4, Word(s, 0xb4));
        other->mRideAnim = s->mRideAnim;
        break;
    }
    case 64: {   // a second script started
        const uint32_t w = ReadScript(s);
        if ((w & 0xff000000u) != 0x10000000u) return;
        char path[256];
        LinkedPath(s, w, path);
        s->mF14 = static_cast<uint32_t>(RSSE_InitScript(path, 0, 0));
        break;
    }
    case 65:
        if (s->mLinked != 0) RSSE_KillScript(s->mLinked, 0);
        s->mLinked = 0;
        break;
    case 66: {
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        SetRemote(s, s->mLinked, index, value);
        break;
    }
    case 67: {
        const uint32_t w = ReadScript(s);
        const int32_t index = Operand(s);
        GetRemote(s, s->mLinked, w, index);
        break;
    }
    case 68: {
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        SetRemote(s, s->mF10, index, value);
        break;
    }
    case 69: {
        const uint32_t w = ReadScript(s);
        const int32_t index = Operand(s);
        GetRemote(s, s->mF10, w, index);
        break;
    }
    case 70:
        s->mBounceBase = static_cast<int32_t>(ReadScript(s));
        break;
    case 71:
        s->mBounceHeight = static_cast<int16_t>(Operand(s));
        break;
    case 72: {
        const int32_t peep = Operand(s);
        const int32_t time = Operand(s);
        s->mCommandAnswer = BOUNCE_Put(s, static_cast<uint32_t>(peep), time);
        break;
    }
    case 73: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(BOUNCE_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 74: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(BOUNCE_ForceGet(s));
        StoreAnswer(s, w);
        break;
    }
    case 75: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mBounceUsed;
        StoreAnswer(s, w);
        break;
    }
    case 76: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        const int32_t d = Operand(s);
        const int32_t e = Operand(s);
        const int32_t f = Operand(s);
        const int32_t g = Operand(s);
        WALKNODE_Trigger(s, static_cast<uint32_t>(a), b, c, d, e, f, g);
        break;
    }
    case 77:
        WALKNODE_Release(s, static_cast<uint32_t>(Operand(s)));
        break;
    case 78: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(WALKNODE_Get(s));
        StoreAnswer(s, w);
        break;
    }
    case 79: {
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        const int32_t c = Operand(s);
        WALKNODE_Float(s, a * 1000, b, c);
        break;
    }
    case 80: {
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = s->mFloatDuration;
        StoreAnswer(s, w);
        break;
    }
    case 81: {   // the float's duration made 1000 with the same progress (unsigned)
        if (s->mFloatDuration == 0) return;
        const int32_t first = gClock.Read();
        const int32_t second = gClock.Read();
        s->mFloatStart = second - static_cast<int32_t>(static_cast<uint32_t>((first - s->mFloatStart) * 1000) /
                                                       static_cast<uint32_t>(s->mFloatDuration));
        s->mFloatDuration = 1000;
        break;
    }
    case 82:
        EnableLight(Light(s, Operand(s)));
        break;
    case 83:
        DisableLight(Light(s, Operand(s)));
        break;
    case 84: {
        const int32_t node = Operand(s);
        const int32_t level = Operand(s);
        SetLightColourRelative(Light(s, node), static_cast<float>(level) / 100.0f);
        break;
    }
    case 85: {
        const int32_t node = Operand(s);
        const int32_t red = Operand(s);
        const int32_t green = Operand(s);
        const int32_t blue = Operand(s);
        sNodeLightParams* light = Light(s, node);
        SetLightColourExplicit(light, static_cast<float>(red) / 100.0f, static_cast<float>(green) / 100.0f,
                               static_cast<float>(blue) / 100.0f);
        break;
    }
    case 86: {   // the screams started at the ride's centre
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        float x, y, z;
        GetRideCentrePosition(s->mRideAnim, x, y, z);
        s->mCommandAnswer = SCREAM_Start(static_cast<int32_t>(s->mFCC[1]), a, b, s->mSpeed, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        s->mFCC[1] = static_cast<uint32_t>(s->mCommandAnswer);
        break;
    }
    case 87:
        if (s->mFCC[1] != 0) SOUND_FadeSoundEffect(static_cast<int32_t>(s->mFCC[1]));
        s->mFCC[1] = 0;
        break;
    case 88: {   // one scream at the ride's centre (a negative kind: a sad one)
        const int32_t a = Operand(s);
        const int32_t b = Operand(s);
        float x, y, z;
        GetRideCentrePosition(s->mRideAnim, x, y, z);
        if (b < 0)
            SCREAM_SingleSad(a, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        else
            SCREAM_Single(a, b, s->mSpeed, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        break;
    }
    case 89: {   // the screams' level; the original keeps the callee's r3 as the handle
        const int32_t level = Operand(s);
        s->mFCC[1] = static_cast<uint32_t>(ScreamSetLevel(static_cast<int32_t>(s->mFCC[1]), level, s->mSpeed));
        break;
    }
    case 90: {   // a random script of the same kind (the same text at its mF74[0] offset): its handle
        const uint32_t w = ReadScript(s);
        const uint32_t v = ReadScript(s);
        int32_t count = 0;
        s->mCommandAnswer = 0;
        if ((w & 0xff000000u) != 0x10000000u) return;
        if (s->mSaveBlock.get() == nullptr) return;
        const char* name = reinterpret_cast<const char*>(s->mSaveBlock.get() + (w ^ 0x10000000u));
        for (RSSE_SCRIPT* other = gRSSEState.mScripts; other != nullptr; other = other->mNext.get())
            if (other->mSaveBlock.get() != nullptr && static_cast<int32_t>(other->mF74[0]) >= 0 &&
                StrCmp(name, reinterpret_cast<const char*>(other->mSaveBlock.get() + other->mF74[0])) == 0)
                ++count;
        if (count == 0) return;
        int32_t r = Labs(static_cast<int32_t>(gGameWorld->GetRand() >> 1));
        r = Labs(r - (r / count) * count);
        const int32_t pick = r + 1;
        count = 0;
        for (RSSE_SCRIPT* other = gRSSEState.mScripts; other != nullptr; other = other->mNext.get()) {
            if (other->mSaveBlock.get() != nullptr && static_cast<int32_t>(other->mF74[0]) >= 0 &&
                StrCmp(name, reinterpret_cast<const char*>(other->mSaveBlock.get() + other->mF74[0])) == 0) {
                ++count;
                if (count == pick) {
                    s->mCommandAnswer = other->mHandle;
                    if (v & 0x40000000u) s->mVars[v ^ 0x40000000u] = s->mCommandAnswer;   // only bit 30 is tested
                }
            }
        }
        break;
    }
    case 91: {   // a variable of any script (0 when there is none)
        const uint32_t w = ReadScript(s);
        const int32_t handle = Operand(s);
        const int32_t index = Operand(s);
        s->mCommandAnswer = 0;
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
        if (other != nullptr && index >= 0 && index < other->mVarCount) s->mCommandAnswer = other->mVars[index];
        StoreAnswer(s, w);
        break;
    }
    case 92: {   // a variable of any script set
        const int32_t handle = Operand(s);
        const int32_t index = Operand(s);
        const int32_t value = Operand(s);
        RSSE_SCRIPT* other = RSSE_GetScriptPointer(handle);
        if (other == nullptr) return;
        if (index >= 0 && index < other->mVarCount) {
            other->mVars[index] = value;
            s->mCommandAnswer = value;
        } else {
            GAME_ASSERTMSG(0, kRemoteVariable);
        }
        break;
    }
    case 93: {   // the repair effect and its sounds (off: the scream of variable 5)
        const int32_t on = Operand(s);
        TriggerRepairParticleEffect(s, on);
        float x, y, z;
        if (on != 0) {
            GetRidePosition(*GetRideFromID(s->mRideAnim), x, y, z);
            SOUND_SendEvent(0, gSoundBanks[2], 0x6f, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
            SOUND_SendEvent(0, gSoundBanks[2], 0xd3, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        } else {
            if (s->mVarCount <= 5) return;
            const int32_t kind = s->mVars[5];
            GetRidePosition(*GetRideFromID(s->mRideAnim), x, y, z);
            SCREAM_Single(kind, 1, 1, ppc_fctiwz(x), ppc_fctiwz(y), ppc_fctiwz(z));
        }
        break;
    }
    case 94: {
        const uint32_t w = ReadScript(s);
        Operand(s);
        s->mCommandAnswer = 0;
        StoreAnswer(s, w);
        break;
    }
    case 95: {   // a timer (the word at 0xc4) set
        const int32_t d = Operand(s);
        SetWord(s, 0xc4, static_cast<uint32_t>(d + gClock.Read()));
        break;
    }
    case 96: {   // what is left of it, at least 0
        const uint32_t w = ReadScript(s);
        s->mCommandAnswer = static_cast<int32_t>(Word(s, 0xc4)) - gClock.Read();
        if (s->mCommandAnswer < 0) s->mCommandAnswer = 0;
        StoreAnswer(s, w);
        break;
    }
    case 97:
        TimeField(s, 5, 0);   // tm_year
        break;
    case 98:
        TimeField(s, 4, 1);   // tm_mon + 1
        break;
    case 99:
        TimeField(s, 3, 0);   // tm_mday
        break;
    case 100:
        TimeField(s, 2, 0);   // tm_hour
        break;
    case 101:
        TimeField(s, 1, 0);   // tm_min
        break;
    case 102:
        TimeField(s, 0, 0);   // tm_sec
        break;
    case 103:
        s->mReverb = Operand(s);
        break;
    case 104: {
        const int32_t level = Operand(s);
        s->mFB9[0] = static_cast<uint8_t>(level);
        SND_DipMusic(level);
        break;
    }
    case 105: {   // two particle nodes (the halves at 0xdc and 0xde), checked; two more operands read and unused
        SetHalf(s, 0xdc, static_cast<int16_t>(Operand(s)));
        SetHalf(s, 0xde, static_cast<int16_t>(Operand(s)));
        Operand(s);
        Operand(s);
        JCPOINT3D position;
        GAME_ASSERTMSG(GetNodePos(s, &position, nullptr, Half(s, 0xdc), 0x100), kBadParticleNode, s->mName.get());
        GAME_ASSERTMSG(GetNodePos(s, &position, nullptr, Half(s, 0xde), 0x100), kBadParticleNode, s->mName.get());
        break;
    }
    }
}
