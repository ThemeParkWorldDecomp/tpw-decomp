// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w132: the ride script system's tick (RSSE_Tick, 0x100d33dc, 632 bytes), the
// render tick (RSSE_RenderTick, 0x100d3674, 108 bytes), the two list lookups (RSSE_Getvar 0x100d6c58, 124 bytes, and RSSE_GetScriptPointer
// 0x100d668c, 124 bytes) and the reverb (RSSE_SetReverbOnEax, 0x100d6e7c, 140 bytes). The scripts are a doubly linked list headed by
// gRSSEState.mScripts. The instruction interpreter, the object and walk-node ticks, the kill and the fly, coaster and animation setters
// are untranslated (rsse.h declares them; the launcher answers zero).
#include <cstdint>
#include <cstring>

#include "Debug.h"
#include "LevelInit.h"
#include "MainBalance.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "rsse.h"

// The texts sit in the program's string pool at 0x1023eeae + the offset in the comment.
namespace {
constexpr const char* kNeedInit = "RSSE: Need to initialise before you can tick\n";       // +0x7ae
constexpr const char* kNotInitialised = "RSSE: System not initialised";                    // +0x96c
constexpr const char* kBadScriptId = "Bad ScriptID";                                        // +0x989
constexpr const char* kBadVariable = "RSSE: Bad variable reference";                        // +0x996
constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400

uint32_t WordAt(const void* base, std::size_t offset) {
    uint32_t word;
    std::memcpy(&word, static_cast<const char*>(base) + offset, sizeof(word));
    return word;
}
}  // namespace

// Without the system on it asserts and does nothing. Else the tick count goes up by one and every script of the list is visited, in order:
// a script runs when its every-frame byte is set or its handle's low three bits equal the tick count's (so the others run every eighth tick).
// Running a script: instructions (the countdown is re-read from the record each time and only counts down when ProcessInstruction left
// gRSSEF7BE4 at 0), then, each when its handle is set, the ride's animation speed, the fly's time and speed, the coaster's speed, the ride
// object's performance and the linked script's speed copy. Every script then gets ProcessObjects, and a negative state kills it (the next
// link is read first). Answers gRSSEF7BE0 whatever happened.
int32_t RSSE_Tick() {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNeedInit);
    } else {
        gRSSEState.mF04 = gRSSEState.mF04 + 1;
        RSSE_SCRIPT* script = gRSSEState.mScripts;
        while (script != nullptr) {
            if (script->mEveryFrame != 0 || (script->mHandle & 7) == (gRSSEState.mF04 & 7)) {
                if (script->mCode != 0) {
                    gRSSEF7BE4 = 0;
                    script->mCountdown = script->mInstructions;
                    while (script->mCountdown > 0 && script->mState >= 0) {
                        ProcessInstruction(script);
                        if (gRSSEF7BE4 == 0) script->mCountdown = script->mCountdown - 1;
                    }
                }
                if (script->mRideAnim != 0) {
                    const float speed = static_cast<float>(script->mSpeed);
                    const float factor = static_cast<float>(script->mAnimFactor);
                    UpdateRideAnimSpeedAllChannels(gRideAnimSets[script->mRideAnim], ((0.5f + speed / 100.0f) * factor) / 1000.0f);
                }
                if (script->mFlyHandle != 0) {
                    if (script->mVarCount > 3) {
                        const int32_t time = static_cast<int32_t>(static_cast<uint32_t>(script->mVars[3]) * 1000u);
                        FLY_SetRunTime(script->mFlyHandle, time);
                    }
                    FLY_SetRunSpeed(script->mFlyHandle, script->mSpeed);
                }
                if (script->mCoaster != 0) CoasterSetSpeed(script->mCoaster, static_cast<uint32_t>(static_cast<int32_t>(script->mSpeed)));
                if (script->mThingId != 0) {
                    const uint16_t id = script->mThingId;
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                    void* thing = gThingArray[id].mThing;
                    if (thing != nullptr) {
                        const uint32_t ride = WordAt(thing, 0x24);
                        if (ride != 0) BMP_SetRidePerformance(static_cast<int32_t>(ride), script->mSpeed);
                    }
                }
                if (script->mLinked != 0) {
                    const int16_t speed = script->mSpeed;
                    RSSE_GetScriptPointer(script->mLinked)->mSpeed = speed;
                }
            }
            ProcessObjects(script);
            if (script->mState < 0) {
                RSSE_SCRIPT* next = script->mNext;
                RSSE_KillScript(script->mHandle, 0);
                script = next;
            } else {
                script = script->mNext;
            }
        }
    }
    return gRSSEF7BE0;
}

// Without the system on it asserts (the same text as the tick) and does nothing; else every script gets its bounce tick, then its walk-node tick.
void RSSE_RenderTick() {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNeedInit);
    } else {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            BOUNCE_Tick(script);
            WALKNODE_Tick(script);
        }
    }
}

// Variable `index` of the script. Without the system on it asserts and answers 0; a NULL script answers 0 quietly; an index outside 0 to the
// script's count (signed) asserts and answers 0.
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        return 0;
    }
    if (script == nullptr) return 0;
    if (index >= 0 && index < script->mVarCount) return script->mVars[index];
    GAME_ASSERTMSG(0, kBadVariable);
    return 0;
}

// The first script of the list whose handle is `handle`, NULL when there is none. Without the system on it asserts twice ("not initialised",
// then "Bad ScriptID") and answers NULL.
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t handle) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        GAME_ASSERTMSG(0, kBadScriptId);
        return nullptr;
    }
    for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
        if (script->mHandle == handle) return script;
    }
    return nullptr;
}

// The reverb level for the first person camera's ride: the first script of the list that belongs to that ride (its ride number is the id), has
// mF10 at 0 and a level of its own sets it (a negative level sets nothing and ends the search). No such script, or no ride: the balance
// sheet's default level.
void RSSE_SetReverbOnEax() {
    const uint32_t ride = GetFirstPersonRideID();
    if (ride != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            if (script->mRideAnim == ride && script->mF10 == 0 && script->mReverb != 0) {
                if (script->mReverb >= 0) SOUND_SetReverbLevel(script->mReverb);
                return;
            }
        }
    }
    SOUND_SetReverbLevel(gMainBalance.mValues[kMainBalanceDefaultReverb].mInteger);
}
