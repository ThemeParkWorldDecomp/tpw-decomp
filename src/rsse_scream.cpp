// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w171: a ride script's screams (SCREAM_Start 0x100ccb30, SCREAM_SetLevel
// 0x100cccc0, SCREAM_Move 0x100ccd3c, SCREAM_Stop 0x100ccd98, SCREAM_Single 0x100ccde8, SCREAM_SingleSad 0x100cd098), the repair particle
// effect (TriggerRepairParticleEffect 0x100cd190), the drawing of a walking peep (DrawPeep 0x100cd388) and two getters (RSSE_GetSoundEventCode
// 0x100d6d04, RSSE_GetCoasterHandle 0x100d6f34). The screams are sound events of the bank gSoundBanks[1]; the handle is the event's answer.
#include <cstdint>
#include <cstring>

#include "Clock.h"
#include "Debug.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

// The function-local static of TriggerRepairParticleEffect: the first call stores -1 in the word and 1 in the guard. Nothing reads the word.
uint8_t gRepairEffectGuard;   // 0x102f7bd4
int32_t gRepairEffectWord;    // 0x102f7bd8

namespace {

// The messages (the original's string pool at 0x1023eeae + the offset in the comment, copied byte for byte).
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400
const char kStartedScreaming[] = "RSSE: Started screaming without stopping first.";            // +0x45a
const char kNotInitialised[] = "RSSE: System not initialised";                                  // +0x96c
const char kBadScriptId[] = "Bad ScriptID";                                                     // +0x989
const char kBadSoundVariable[] = "RSSE: Bad Sound Variable Script variable reference";          // +0x9b3
const char kNoSoundScript[] = "RSSE: no sound script is present";                               // +0x9e6

const float kThousand = 1000.0f;   // 0x10264fa8
const float k1024 = 1024.0f;       // 0x10264fac
const float kHalf = 0.5f;          // 0x10264fb8

// The sound bank of the screams: the word +4 of the block at 0x102f7c94.
int32_t ScreamBank() { return gSoundBanks[1]; }

// The first script of the list whose handle is `handle`: RSSE_GetScriptPointer inlined (the same code as src/rsse_ride_tick.cpp).
RSSE_SCRIPT* FindScript(int32_t handle) {
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

// sin(v) from the sine table of gTPGlobals: one multiply by the entries per radian, a truncation and the mask.
float TableSin(float v) {
    const float scaled = gTPGlobals.mSinTableScale * v;
    return gTPGlobals.mSinTable[static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(scaled))) & gTPGlobals.mSinTableMask];
}

// An integer as the original converts it to single precision: through the double 2^52 + x and a subtraction (signed: xoris 0x8000).
float SignedToFloat(int32_t value) { return static_cast<float>(value); }
float UnsignedToFloat(uint32_t value) { return static_cast<float>(value); }

}  // namespace

// 0x100ccb30 (356 bytes): starts the scream of `count` riders (1: event 0x47, below 4: 0x48, below 8: 0x49, else 0x4a; a negative count is
// below 4) and sets its level to the mean of a and b. `handle` must be 0 (the previous scream stopped), else it asserts and answers 0.
int32_t SCREAM_Start(int32_t handle, int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    if (handle != 0) {
        GAME_ASSERTMSG(0, kStartedScreaming);
        return 0;
    }
    if (count == 0) return 0;
    int32_t code;
    if (count == 1) code = 0x47;
    else if (count < 4) code = 0x48;
    else if (count < 8) code = 0x49;
    else code = 0x4a;
    const int32_t started = SOUND_SendEvent(0, ScreamBank(), code, p, q, r);
    SCREAM_SetLevel(started, a, b);
    return started;
}

// 0x100cccc0 (84 bytes): the sound parameter 6 of the handle becomes (a + b) / 2 (the sum wraps), clamped to 0..100. Handle 0: nothing.
void SCREAM_SetLevel(int32_t handle, int32_t a, int32_t b) {
    if (handle == 0) return;
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    int32_t level = sum / 2;
    if (level > 100) level = 100;
    if (level < 0) level = 0;
    SOUND_SetParamValue(handle, 6, static_cast<uint32_t>(level));
}

// 0x100ccd3c (52 bytes)
int32_t SCREAM_Move(int32_t handle, int32_t x, int32_t y, int32_t z) {
    if (handle == 0) return 0;
    return SOUND_MoveSoundEffect(handle, x, y, z);
}

// 0x100ccd98 (44 bytes)
void SCREAM_Stop(int32_t handle) {
    if (handle != 0) SOUND_FadeSoundEffect(handle);
}

// 0x100ccde8 (644 bytes): one scream of `count` riders, louder with (a + b) / 50 (0 to 3; the level is limited above before the count is
// looked at, a negative level starts nothing). The groups: 1 rider 0x4b, below 4 0x4f, below 8 0x53, else 0x57 (a negative count is below 4).
int32_t SCREAM_Single(int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r) {
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    int32_t level = sum / 50;
    if (level > 3) level = 3;
    if (count == 0) return 0;
    int32_t base;
    if (count == 1) base = 0x4b;
    else if (count < 4) base = 0x4f;
    else if (count < 8) base = 0x53;
    else base = 0x57;
    if (level < 0) return 0;
    return SOUND_SendEvent(0, ScreamBank(), base + level, p, q, r);
}

// 0x100cd098 (204 bytes): the sad scream: 1 rider 0x69, below 4 0x6a, below 8 0x6c (0x6b is never used), else 0x6d.
int32_t SCREAM_SingleSad(int32_t count, int32_t p, int32_t q, int32_t r) {
    if (count == 0) return 0;
    int32_t code;
    if (count == 1) code = 0x69;
    else if (count < 4) code = 0x6a;
    else if (count < 8) code = 0x6c;
    else code = 0x6d;
    return SOUND_SendEvent(0, ScreamBank(), code, p, q, r);
}

// 0x100cd190 (440 bytes): the repair effect of the ride, an emitter whose handle the script keeps in its word +0xcc. Off: kills the emitter
// (life -2) and clears the word. On: starts the ride type's repair effect at the middle of the ride's bounding box (the height from the ride's
// node), unless the script has no thing or the type has no effect; the old handle is overwritten, not killed.
void TriggerRepairParticleEffect(RSSE_SCRIPT* script, int32_t on) {
    if (gRepairEffectGuard == 0) {
        gRepairEffectWord = -1;
        gRepairEffectGuard = 1;
    }
    if (on == 0) {
        if (script->mFCC[0] != 0) {
            PAR_SetEmitterLife(static_cast<int32_t>(script->mFCC[0]), -2);
            script->mFCC[0] = 0;
        }
        return;
    }
    // The original compares the thing id with the halfword at 0x1029ef70, which is zero in the data and only this function reads.
    if (script->mThingId == 0) return;
    const uint16_t row = script->mThingId;
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    const CObject* thing = static_cast<const CObject*>(gThingArray[row].mThing);
    const RideInfo* info = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{thing->mObjectType});
    const int32_t effect = info->Value(kRideSlot_RepairParticleEffect).mInteger;
    if (effect == 0) return;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    sAlignedBox box;
    if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
        x = (box.mMin[0] + box.mMax[0]) * kHalf;
        z = (box.mMin[2] + box.mMax[2]) * kHalf;
        // The height: the float +0x44 of the ride's node (GetRidePosition's chain).
        const tag_sRideAnimSet* set = gRideAnimSets[script->mRideAnim];
        const uint8_t* node = reinterpret_cast<const uint8_t*>(set->mHeads->mHeader->mF78.get());
        std::memcpy(&y, node + 0x44, sizeof(y));
    }
    const int32_t px = ppc_fctiwz(static_cast<double>(k1024 * x));
    const int32_t py = ppc_fctiwz(static_cast<double>(k1024 * y));
    const int32_t pz = ppc_fctiwz(static_cast<double>(k1024 * z));
    script->mFCC[0] = PAR_InitEmitter(effect, px, py, pz);
}

// 0x100cd388 (1,380 bytes): draws the peep of a walk record between the nodes `from` and `to`, `fraction` thousandths of the way (limited
// to 1000). mode 1 takes the first point from the head's node mask (0x80), mode 2 the second; any other the walk node's (0x800). The height is
// interpolated when the record's last argument has bit 0, else the ground's. State 2 (waiting): a floating peep when the script has a float
// duration (a sway with the sine table), else the sprite by the record's kind or hidden; other states draw the sprite 1.
void DrawPeep(RSSE_SCRIPT* script, RSSE_WALK* walk, int32_t from, int32_t to, int32_t mode, int32_t fraction) {
    JCPOINT3D p1;
    JCPOINT3D p2;
    GetNodePos(script, &p1, nullptr, from, mode == 1 ? 0x80 : 0x800);
    GetNodePos(script, &p2, nullptr, to, mode == 2 ? 0x80 : 0x800);
    if (fraction > 1000) fraction = 1000;
    const float f = SignedToFloat(fraction);
    float x = ppc_fmadds(kThousand, p1.x, f * (p2.x - p1.x)) / kThousand;
    float z = ppc_fmadds(kThousand, p1.z, f * (p2.z - p1.z)) / kThousand;
    float y;
    if ((walk->mLast & 1) != 0) {
        y = ppc_fmadds(kThousand, p1.y, f * (p2.y - p1.y)) / kThousand;
    } else {
        GetRideHeightAtPoint(x, z, y);
    }
    const uint16_t row = static_cast<uint16_t>(walk->mPeep);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
    CPeep* peep = static_cast<CPeep*>(gThingArray[row].mThing);
    const auto scriptIdx = [](int32_t n) { return static_cast<CDelayedScript::ScriptIdx>(n); };
    if (walk->mState != 2) {
        peep->DrawMySpriteAt(x, y, z, walk->mDirection, scriptIdx(1));
        return;
    }
    if (script->mFloatDuration == 0) {
        if (walk->mKind == 3) peep->DrawMySpriteAt(x, y, z, walk->mDirection, scriptIdx(3));
        if (walk->mKind == 5) peep->DrawMySpriteAt(x, y, z, walk->mDirection, scriptIdx(8));
        if (walk->mKind == 6) peep->DrawMySpriteAt(x, y, z, walk->mDirection, scriptIdx(5));
        if (walk->mKind == 4 || walk->mKind == 1 || walk->mKind == 2) peep->HideMySprite();
        return;
    }
    // The floating peep: A and Q (by the peep's id) set the sway of this peep, S the script's speed, FA and FB the amounts.
    const float A = 0.4f + SignedToFloat(static_cast<int32_t>(walk->mPeep & 7)) / 10.0f;
    const float Q = ppc_fmadds(SignedToFloat(static_cast<int32_t>(walk->mPeep & 3)), 0.25f, 0.4f);
    const float S = kHalf + SignedToFloat(script->mSpeed) / 100.0f;
    const float FA = UnsignedToFloat(static_cast<uint16_t>(script->mFloatA)) / kThousand;
    const float FB = UnsignedToFloat(static_cast<uint16_t>(script->mFloatB)) / kThousand;
    const float T = UnsignedToFloat(static_cast<uint32_t>(gClock.Read()));
    const uint32_t sinceStart = static_cast<uint32_t>(gClock.Read()) - static_cast<uint32_t>(script->mFloatStart);
    const float el = SignedToFloat(static_cast<int32_t>(sinceStart));
    const float D = UnsignedToFloat(static_cast<uint32_t>(script->mFloatDuration));
    const float P = T * (S * A);
    const float R = T * (S * Q);
    const float E = TableSin((3.14159f * el) / D);
    const float s0 = TableSin(P / 250.0f);
    const float s3 = TableSin(P / 300.0f);
    const float s2 = TableSin(1.5707964f + R / 300.0f);
    y = ppc_fmadds(FB, ppc_fmadds(12.0f, E, (3.0f * E) * s0), y);
    x = ppc_fmadds(FA, s3 * (3.0f * (E * Q)), x);
    z = ppc_fmadds(FA, s2 * (3.0f * (E * A)), z);
    const uint32_t divisor = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(300.0f * Q)));
    const uint32_t quotient = static_cast<uint32_t>(gClock.Read()) / divisor;
    const int32_t direction = walk->mDirection + static_cast<int16_t>(quotient);
    walk->mDirection = static_cast<int16_t>(direction % 8);
    peep->DrawMySpriteAt(x, y, z, walk->mDirection, scriptIdx(2));
}

// 0x100d6d04 (328 bytes): variable `index` of the sound script that the script `handle` points to (its word +0x14 is that script's handle).
// Asserts and answers 0 for a system that is not on, for an index outside the variables; no script of that handle answers 0 quietly for the
// first lookup and with the assertion "no sound script is present" for the second.
int32_t RSSE_GetSoundEventCode(int32_t handle, int32_t index) {
    if (gRSSEState.mInitialised == 0) {
        GAME_ASSERTMSG(0, kNotInitialised);
        return 0;
    }
    RSSE_SCRIPT* script = nullptr;
    for (RSSE_SCRIPT* s = gRSSEState.mScripts; s != nullptr; s = s->mNext) {
        if (s->mHandle == handle) {
            script = s;
            break;
        }
    }
    if (script == nullptr) return 0;
    const int32_t soundHandle = static_cast<int32_t>(script->mF14);
    RSSE_SCRIPT* sound = nullptr;
    for (RSSE_SCRIPT* s = gRSSEState.mScripts; s != nullptr; s = s->mNext) {
        if (s->mHandle == soundHandle) {
            sound = s;
            break;
        }
    }
    if (sound == nullptr) {
        GAME_ASSERTMSG(0, kNoSoundScript);
        return 0;
    }
    if (index >= 0 && index < sound->mVarCount) return sound->mVars[index];
    GAME_ASSERTMSG(0, kBadSoundVariable);
    return 0;
}

// 0x100d6f34 (140 bytes): the coaster handle of the script, 0 when there is none.
uint32_t RSSE_GetCoasterHandle(int32_t handle) {
    RSSE_SCRIPT* script = FindScript(handle);
    return script != nullptr ? script->mCoaster : 0;
}
