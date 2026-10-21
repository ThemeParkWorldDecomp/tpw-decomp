// RideEffects.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Clock.h"
#include "Debug.h"
#include "helpers/rsseHelpers.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"
#include "ThingReceivers.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPSound.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// The script objects of the ride scripts (main app, rsse.cpp range of the inventory, 0x100cc778..0x100d6bd0; unit w150): the emitters and
// sounds a script starts on its ride's nodes (CreateObject, KillObject, KillLinkedObject, SetObjParam, StartEvent, InitObject,
// ProcessObjects), the program reader and the stack (ExtractData, ReadScript, Push, Pop), the small getters and setters and two
// position getters of a ride set. An object lives in the arena (its links are Ptr32), where the Mac used LbMem_Alloc and LbMem_Free.

namespace {

const float kZero = 0.0f;      // 0x10264fb4
const float kHalf = 0.5f;      // 0x10264fb8

}  // namespace

// 0x100cc778 (220 bytes): the centre of the ride's bounding box, the height from the node of GetRidePosition. Answers 0 when found.
int32_t GetRideCentrePosition(uint32_t& ride, float& x, float& y, float& z) {
    if (ride != 0) {
        sAlignedBox box;
        if (GetRideMasterBoundArea(ride, &box) != 0) {
            const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[ride]->mHeads->mHeader->mF78.get());
            y = ReadFloat(node, 0x44);
            x = (box.mMin[0] + box.mMax[0]) * kHalf;
            z = (box.mMin[2] + box.mMax[2]) * kHalf;
            return 0;
        }
    }
    z = kZero;
    y = z;
    x = y;
    return 1;
}

// The peep tables of a ride script (main app, rsse.cpp range of the inventory, 0x100cc888..0x100ced00; unit w149): the walk nodes
// (WALKNODE_Trigger, Release, Get, Float, Tick), the bounce table (BOUNCE_Put, Get, ForceGet, Tick), the limbo table (LIMBO_Put, Get,
// ForceGet) and GetNodePos. The three tables hang off the script record (RSSE_SCRIPT::mWalk, mBounce, mLimbo); their records are plain
// structs (include/rsse.h). A peep is held by its thing id (0: a free slot).
//
// The walk nodes move a peep from node to node of the ride (DrawPeep, untranslated, draws it); the states of a walk record are 0 free,
// 1 walking in, 2 waiting, 3 walking out, 4 done. The time of a leg is 100 ms per whole unit of distance (100 for none), the direction
// is one of eight octants, worked out in doubles with the constants 6.28318 (not 2 pi) and 1.570795, as the original does.

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte; the first two end in a newline).
const char kRowZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // +0x400
const char kInvalidNode[] = "RSSE: Invalid Node ID\n";                                            // +0x443
const char kZeroPeep[] = "RSSE: WALK: Very odd ! Just found a zero peepid in walk table.";        // +0x48a
const char kBadWalknode[] = "RSSE: Bad walknode in %s";                                           // +0x4c9
const char kBadHeadNode[] = "RSSE: Bad head node in %s";                                          // +0x4e2
const char kBadWalkNode[] = "RSSE: Bad walk node in %s";                                          // +0x4fc
const char kNoSetwalk[] = "RSSE: Walknodes need a 'setwalk' in the script";                       // +0x516
const char kListFull[] = "RSSE: WALK: Could not add peep to walk list";                           // +0x545
const char kNotHere[] = "RSSE: WALK: Tried to release a peep who is not here.";                   // +0x571

constexpr int32_t kNodeWalk = 0x800;   // the node mask of a walk node
constexpr int32_t kNodeHead = 0x80;    // the node mask of a head node

// The script's ride set (gRideAnimSets is indexed by mRideAnim).
tag_sRideAnimSet* RideSet(const RSSE_SCRIPT* script) {
    return gRideAnimSets[script->mRideAnim];
}

// The root node's height: row 3, y of the matrix of the header's node at +0x78.
float RootY(const RSSE_SCRIPT* script) {
    return RideSet(script)->mHeads->mHeader->mF78->mMatrix.m[3][1];
}

// The script's name for the "%s" assertions.
const char* NameOf(const RSSE_SCRIPT* script) {
    return static_cast<const char*>(script->mName);
}

// The squared distance in single precision, as the listing orders it: dy*dy by fmuls, then dx*dx and dz*dz fused.
// Then (int)(float)sqrt, times 100; 100 when that is 0.
int32_t LegTime(const JCPOINT3D& a, const JCPOINT3D& b) {
    const float dy = a.y - b.y;
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    float sum = dy * dy;
    sum = ppc_fmadds(dx, dx, sum);
    sum = ppc_fmadds(dz, dz, sum);
    const float root = static_cast<float>(std::sqrt(static_cast<double>(sum)));
    const int32_t whole = ppc_fctiwz(static_cast<double>(root));
    const int32_t time = static_cast<int32_t>(static_cast<uint32_t>(whole) * 100u);
    return time == 0 ? 100 : time;
}

// The direction 0 to 7 (a signed C remainder) of the heading atan2(dz, dx): the doubles are 0.5 + (8.0 * (6.28318 - (d - 1.570795))) / 6.28318.
int16_t Direction(float dz, float dx) {
    const double angle = static_cast<double>(static_cast<float>(std::atan2(static_cast<double>(dz), static_cast<double>(dx))));
    double value = angle - 1.570795;
    value = 6.28318 - value;
    value = 8.0 * value;
    value = value / 6.28318;
    value = 0.5 + value;
    return static_cast<int16_t>(ppc_fctiwz(value) % 8);
}

// The walk fraction in thousandths: an UNSIGNED divide of (now - start) * 1000 by (end - start). (A zero divisor is undefined for
// divwu; 0 is taken.)
uint32_t Fraction(const RSSE_WALK& walk, int32_t now) {
    const uint32_t numerator = static_cast<uint32_t>(now - walk.mStart) * 1000u;
    const uint32_t denominator = static_cast<uint32_t>(walk.mEnd - walk.mStart);
    return denominator == 0 ? 0 : numerator / denominator;
}

// The emitter (type 0x13) the walk nodes of kind 2 fire at a node: x, y, z truncated to whole units, then times 1024.
void FireEmitter(RSSE_SCRIPT* script, int32_t node) {
    JCPOINT3D at = {0.0f, 0.0f, 0.0f};
    GetNodePos(script, &at, nullptr, node, kNodeWalk);
    const int32_t x = ppc_fctiwz(static_cast<double>(at.x));
    const int32_t y = ppc_fctiwz(static_cast<double>(at.y));
    const int32_t z = ppc_fctiwz(static_cast<double>(at.z));
    PAR_InitEmitter(0x13, static_cast<int32_t>(static_cast<uint32_t>(x) << 10), static_cast<int32_t>(static_cast<uint32_t>(y) << 10),
                    static_cast<int32_t>(static_cast<uint32_t>(z) << 10));
}

// The elapsed time of a bounce or limbo slot is split as (elapsed % 1000) / 200 (C signed divides): a slot is "on the beat" when it is 0.
bool OnTheBeat(int32_t elapsed) {
    return (elapsed % 1000) / 200 == 0;
}

}  // namespace

int32_t GetNodePos(RSSE_SCRIPT* script, JCPOINT3D* pos, JCPOINT3D* dir, int32_t node, int32_t mask) {
    if (node >= 0 && script->mRideAnim != 0) {
        uint32_t index = FindDataNodeIndexByID(RideSet(script), static_cast<uint32_t>(mask), static_cast<uint32_t>(node));
        if (index == 0xffffffffu) {
            GAME_ASSERTMSG(0, kInvalidNode);
            return 0;
        }
        const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(RideSet(script)->mHeads->mF28);
        const float* block = reinterpret_cast<const float*>(embedded->mRecords[index].mBlock);
        if (block != nullptr) {
            pos->x = block[12];
            pos->y = block[13];
            pos->z = block[14];
        } else {
            pos->x = 0.0f;
            pos->y = 0.0f;
            pos->z = 0.0f;
        }
        if (dir != nullptr) {
            uint32_t rideId = script->mRideAnim;
            tag_sRideAnimSet* ride = *GetRideFromID(rideId);
            const float* matrix = GetDataNodeWorldMatrix(ride->mHeads, index);
            s3DPoint facing;
            if (matrix != nullptr) {
                // The index is read again after the call (it travels by reference).
                const sEmbeddedSet* rideEmbedded = static_cast<const sEmbeddedSet*>(ride->mHeads->mF28);
                if ((rideEmbedded->mRecords[index].mFlags & 0x10) != 0) {
                    facing.x = -matrix[8];
                    facing.y = -matrix[9];
                    facing.z = -matrix[10];
                } else {
                    facing.x = matrix[8];
                    facing.y = matrix[9];
                    facing.z = matrix[10];
                }
                NormaliseVector(&facing);
            } else {
                facing.x = 0.0f;
                facing.y = 0.0f;
                facing.z = 1.0f;
            }
            dir->x = 1024.0f * facing.x;
            dir->y = 1024.0f * facing.y;
            dir->z = 1024.0f * facing.z;
        }
        return 1;
    }
    // No node asked for or no ride: the middle of the ride's bounding box, at the root node's height (zeros when there is none).
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (script->mRideAnim != 0) {
        sAlignedBox box = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        if (GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
            const float sumX = box.mMin[0] + box.mMax[0];
            const float sumZ = box.mMin[2] + box.mMax[2];
            x = sumX * 0.5f;
            z = sumZ * 0.5f;
            y = RootY(script);
        }
    }
    pos->x = x;
    pos->y = y;
    pos->z = z;
    return 1;
}

// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w171: a ride script's screams (SCREAM_Start 0x100ccb30, SCREAM_SetLevel
// 0x100cccc0, SCREAM_Move 0x100ccd3c, SCREAM_Stop 0x100ccd98, SCREAM_Single 0x100ccde8, SCREAM_SingleSad 0x100cd098), the repair particle
// effect (TriggerRepairParticleEffect 0x100cd190), the drawing of a walking peep (DrawPeep 0x100cd388) and two getters (RSSE_GetSoundEventCode
// 0x100d6d04, RSSE_GetCoasterHandle 0x100d6f34). The screams are sound events of the bank gSoundBanks[1]; the handle is the event's answer.

// The function-local static of TriggerRepairParticleEffect: the first call stores -1 in the word and 1 in the guard. Nothing reads the word.
uint8_t gRepairEffectGuard;   // 0x102f7bd4
int32_t gRepairEffectWord;    // 0x102f7bd8

namespace {

const char kStartedScreaming[] = "RSSE: Started screaming without stopping first.";            // +0x45a
const float kThousand = 1000.0f;   // 0x10264fa8
const float k1024 = 1024.0f;       // 0x10264fac
// The sound bank of the screams: the word +4 of the block at 0x102f7c94.
int32_t ScreamBank() { return gSoundBanks[1]; }

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

// 0x100cccc0 (84 bytes): the sound parameter 6 of the handle becomes (a + b) / 2 (the sum wraps), clamped to 0..100. Handle 0: nothing
// and 0 (the original returns with the handle in r3); else SOUND_SetParamValue's answer.
int32_t SCREAM_SetLevel(int32_t handle, int32_t a, int32_t b) {
    if (handle == 0) return 0;
    const int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
    int32_t level = sum / 2;
    if (level > 100) level = 100;
    if (level < 0) level = 0;
    return SOUND_SetParamValue(handle, 6, static_cast<uint32_t>(level));
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

void WALKNODE_Tick(RSSE_SCRIPT* script) {
    if (script->mWalkCount == 0) return;
    SetRideSpritePosnFocus(RideSet(script)->mHeads);
    for (int32_t i = 0; i < script->mWalkCount; ++i) {
        RSSE_WALK* walk = &script->mWalk[i];
        if (walk->mState == 0) continue;
        GAME_ASSERTMSG(static_cast<int32_t>(walk->mPeep), kZeroPeep);
        switch (walk->mState) {
        case 1: {
            const uint32_t fraction = Fraction(*walk, gClock.Read());
            DrawPeep(script, walk, walk->mFrom, walk->mTo, walk->mKind == 4 ? 2 : 0, static_cast<int32_t>(fraction));
            if (fraction < 1000) break;
            walk->mState = 2;
            walk->mStart = gClock.Read();
            if (walk->mKind == 4) {
                const uint32_t index = FindDataNodeIndexByID(RideSet(script), kNodeHead, static_cast<uint32_t>(static_cast<int32_t>(walk->mTo)));
                if (index != 0xffffffffu) EnableRideHead(RideSet(script)->mHeads, index, walk->mPeep);
            }
            if (walk->mKind == 2) FireEmitter(script, walk->mTo);
            break;
        }
        case 2:
            if (walk->mKind == 4 || walk->mKind == 1 || walk->mKind == 2) {
                DrawPeep(script, walk, walk->mFrom, walk->mTo, walk->mKind == 4 ? 2 : 0, 1);
            } else {
                // Any other kind turns to face the node's heading and stands on it.
                JCPOINT3D pos = {0.0f, 0.0f, 0.0f};
                JCPOINT3D dir = {0.0f, 0.0f, 0.0f};
                GetNodePos(script, &pos, &dir, walk->mTo, kNodeWalk);
                walk->mDirection = Direction(dir.z, dir.x);
                DrawPeep(script, walk, walk->mTo, walk->mTo, 0, 0);
            }
            break;
        case 3: {
            const uint32_t fraction = Fraction(*walk, gClock.Read());
            DrawPeep(script, walk, walk->mOutFrom, walk->mOutTo, walk->mKind == 4 ? 1 : 0, static_cast<int32_t>(fraction));
            if (fraction >= 1000) walk->mState = 4;
            break;
        }
        case 4:
            DrawPeep(script, walk, walk->mOutTo, walk->mOutTo, 0, 0);
            break;
        default:
            break;
        }
    }
    if (script->mFloatDuration != 0) {
        const int32_t now = gClock.Read();
        if (static_cast<uint32_t>(script->mFloatDuration) + static_cast<uint32_t>(script->mFloatStart) < static_cast<uint32_t>(now)) {
            script->mFloatDuration = 0;
        }
    }
}

void WALKNODE_Trigger(RSSE_SCRIPT* script, uint32_t peep, int32_t from, int32_t to, int32_t outFrom, int32_t outTo, int32_t kind, int32_t last) {
    JCPOINT3D start = {0.0f, 0.0f, 0.0f};
    JCPOINT3D end = {0.0f, 0.0f, 0.0f};
    GAME_ASSERTMSG(GetNodePos(script, &start, nullptr, from, kNodeWalk), kBadWalknode, NameOf(script));
    if (kind == 4) {
        GAME_ASSERTMSG(GetNodePos(script, &end, nullptr, to, kNodeHead), kBadHeadNode, NameOf(script));
    } else {
        GAME_ASSERTMSG(GetNodePos(script, &end, nullptr, to, kNodeWalk), kBadWalkNode, NameOf(script));
    }
    const int32_t time = LegTime(start, end);
    GAME_ASSERTMSG(script->mWalkCount, kNoSetwalk);
    for (int32_t i = 0; i < script->mWalkCount; ++i) {
        RSSE_WALK& walk = script->mWalk[i];
        if (walk.mState != 0) continue;
        walk.mFrom = static_cast<int16_t>(from);
        walk.mTo = static_cast<int16_t>(to);
        walk.mOutFrom = static_cast<int16_t>(outFrom);
        walk.mOutTo = static_cast<int16_t>(outTo);
        walk.mPeep = peep;
        walk.mKind = static_cast<int16_t>(kind);
        walk.mStart = gClock.Read();
        walk.mEnd = static_cast<int32_t>(static_cast<uint32_t>(time) + static_cast<uint32_t>(gClock.Read()));
        walk.mDirection = Direction(end.z - start.z, end.x - start.x);
        walk.mState = 1;
        walk.mLast = static_cast<int16_t>(last);
        return;
    }
    GAME_ASSERTMSG(0, kListFull);
}

uint32_t WALKNODE_Get(RSSE_SCRIPT* script) {
    for (int32_t i = 0; i < script->mWalkCount; ++i) {
        RSSE_WALK& walk = script->mWalk[i];
        if (walk.mState != 4) continue;
        const uint32_t peep = walk.mPeep;
        walk.mState = 0;
        walk.mPeep = 0;
        return peep;
    }
    return 0;
}

void WALKNODE_Float(RSSE_SCRIPT* script, int32_t duration, int32_t a, int32_t b) {
    script->mFloatStart = gClock.Read();
    script->mFloatDuration = duration;
    script->mFloatA = static_cast<int16_t>(a);
    script->mFloatB = static_cast<int16_t>(b);
}

void WALKNODE_Release(RSSE_SCRIPT* script, uint32_t peep) {
    for (int32_t i = 0; i < script->mWalkCount; ++i) {
        RSSE_WALK& walk = script->mWalk[i];
        if (walk.mState == 0 || static_cast<int32_t>(walk.mPeep) != static_cast<int32_t>(peep)) continue;
        JCPOINT3D from = {0.0f, 0.0f, 0.0f};
        JCPOINT3D to = {0.0f, 0.0f, 0.0f};
        GetNodePos(script, &from, nullptr, walk.mOutFrom, walk.mKind == 4 ? kNodeHead : kNodeWalk);
        GetNodePos(script, &to, nullptr, walk.mOutTo, kNodeWalk);
        const int32_t time = LegTime(from, to);
        walk.mDirection = Direction(to.z - from.z, to.x - from.x);
        walk.mEnd = static_cast<int32_t>(static_cast<uint32_t>(time) + static_cast<uint32_t>(gClock.Read()));
        walk.mStart = gClock.Read();
        if (walk.mKind == 2) FireEmitter(script, walk.mOutFrom);
        if (walk.mKind == 4 && walk.mState == 2) {
            const uint32_t index = FindDataNodeIndexByID(RideSet(script), kNodeHead, static_cast<uint32_t>(static_cast<int32_t>(walk.mTo)));
            if (index != 0xffffffffu) DisableRideHead(RideSet(script)->mHeads, index);
        }
        walk.mState = 3;
        return;
    }
    GAME_ASSERTMSG(0, kNotHere);
}

int32_t BOUNCE_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time) {
    for (int32_t i = 0; i < script->mBounceCount; ++i) {
        RSSE_BOUNCE& slot = script->mBounce[i];
        if (slot.mPeep != 0) continue;
        slot.mPeep = peep;
        slot.mNode = i + script->mBounceBase;
        slot.mRelease = static_cast<int32_t>(static_cast<uint32_t>(time) * 1000u + static_cast<uint32_t>(gClock.Read()));
        slot.mStart = gClock.Read();
        script->mBounceUsed = static_cast<int16_t>(script->mBounceUsed + 1);
        return 1;
    }
    return 0;
}

uint32_t BOUNCE_Get(RSSE_SCRIPT* script) {
    const int32_t now = gClock.Read();
    for (int32_t i = 0; i < script->mBounceCount; ++i) {
        RSSE_BOUNCE& slot = script->mBounce[i];
        if (slot.mPeep == 0) continue;
        const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(gClock.Read()) - static_cast<uint32_t>(slot.mStart));
        if (slot.mRelease >= now) continue;
        if (!OnTheBeat(elapsed)) continue;
        script->mBounceUsed = static_cast<int16_t>(script->mBounceUsed - 1);
        const uint32_t peep = slot.mPeep;
        slot.mPeep = 0;
        return peep;
    }
    return 0;
}

uint32_t BOUNCE_ForceGet(RSSE_SCRIPT* script) {
    gClock.Read();   // the first read's answer is not used
    for (int32_t i = 0; i < script->mBounceCount; ++i) {
        RSSE_BOUNCE& slot = script->mBounce[i];
        if (slot.mPeep == 0) continue;
        const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(gClock.Read()) - static_cast<uint32_t>(slot.mStart));
        if (!OnTheBeat(elapsed)) continue;
        script->mBounceUsed = static_cast<int16_t>(script->mBounceUsed - 1);
        const uint32_t peep = slot.mPeep;
        slot.mPeep = 0;
        return peep;
    }
    return 0;
}

void BOUNCE_Tick(RSSE_SCRIPT* script) {
    for (int32_t i = 0; i < script->mBounceCount; ++i) {
        RSSE_BOUNCE* slot = &script->mBounce[i];
        if (slot->mPeep == 0) continue;
        const int32_t elapsed = static_cast<int32_t>(static_cast<uint32_t>(gClock.Read()) - static_cast<uint32_t>(slot->mStart));
        const uint32_t thing = slot->mPeep & 0xffffu;
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
        CPeep* peep = static_cast<CPeep*>(gThingArray[thing].mThing);
        const uint32_t index = FindDataNodeIndexByID(RideSet(script), kNodeWalk, static_cast<uint32_t>(slot->mNode));
        if (static_cast<int32_t>(index) < 0) continue;
        const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(RideSet(script)->mHeads->mF28);
        const float* block = reinterpret_cast<const float*>(embedded->mRecords[index].mBlock);
        float nodeX = 0.0f;
        float nodeY = 0.0f;
        float nodeZ = 0.0f;
        if (block != nullptr) {
            nodeX = block[12];
            nodeY = block[13];
            nodeZ = block[14];
        }
        const int32_t id = static_cast<int32_t>(slot->mPeep);
        const float rootY = RootY(script);
        if (script->mVars[7] != 0) {
            // Standing still: the frame is the peep's id % 7.
            peep->DrawMySpriteAt(nodeX, nodeY, nodeZ, id % 7, static_cast<CDelayedScript::ScriptIdx>(3));
            continue;
        }
        // Bouncing: the beat runs at 0.8 + speed / 200 of the clock; the height is a sine of its position in the second.
        const float speed = static_cast<float>(script->mSpeed);
        const double rate = 0.8 + static_cast<double>(speed / 200.0f);
        const float elapsedF = static_cast<float>(elapsed);
        const int32_t beat = ppc_fctiwz(static_cast<double>(elapsedF) * rate);
        const float phase = static_cast<float>(beat % 1000) / 320.0f;
        const float scaled = phase * gTPGlobals.mSinTableScale;
        const int32_t sinIndex = ppc_fctiwz(static_cast<double>(scaled));
        const float sine = gTPGlobals.mSinTable[static_cast<uint32_t>(sinIndex) & gTPGlobals.mSinTableMask];
        const float amplitude = 12.0f + static_cast<float>(id % 7);
        const float lift = rootY + static_cast<float>(script->mBounceHeight);
        const float top = ppc_fmadds(sine, amplitude, lift);
        const float y = nodeY < top ? top : nodeY;   // bge is taken when unordered, so a NaN keeps nodeY
        peep->DrawMySpriteAt(nodeX, y, nodeZ, elapsed / 500, static_cast<CDelayedScript::ScriptIdx>(2));
    }
}

int32_t LIMBO_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time) {
    for (int32_t i = 0; i < script->mLimboCount; ++i) {
        RSSE_LIMBO& slot = script->mLimbo[i];
        if (slot.mPeep != 0) continue;
        slot.mPeep = peep;
        slot.mRelease = static_cast<int32_t>(static_cast<uint32_t>(time) * 1000u + static_cast<uint32_t>(gClock.Read()));
        script->mLimboUsed += 1;
        return 1;
    }
    return 0;
}

uint32_t LIMBO_Get(RSSE_SCRIPT* script) {
    const int32_t now = gClock.Read();
    for (int32_t i = 0; i < script->mLimboCount; ++i) {
        RSSE_LIMBO& slot = script->mLimbo[i];
        if (slot.mPeep == 0 || slot.mRelease >= now) continue;
        script->mLimboUsed -= 1;
        const uint32_t peep = slot.mPeep;
        slot.mPeep = 0;
        return peep;
    }
    return 0;
}

uint32_t LIMBO_ForceGet(RSSE_SCRIPT* script) {
    for (int32_t i = 0; i < script->mLimboCount; ++i) {
        RSSE_LIMBO& slot = script->mLimbo[i];
        if (slot.mPeep == 0) continue;
        script->mLimboUsed -= 1;
        const uint32_t peep = slot.mPeep;
        slot.mPeep = 0;
        return peep;
    }
    return 0;
}
