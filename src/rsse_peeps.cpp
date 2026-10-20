// The peep tables of a ride script (main app, rsse.cpp range of the inventory, 0x100cc888..0x100ced00; unit w149): the walk nodes
// (WALKNODE_Trigger, Release, Get, Float, Tick), the bounce table (BOUNCE_Put, Get, ForceGet, Tick), the limbo table (LIMBO_Put, Get,
// ForceGet) and GetNodePos. The three tables hang off the script record (RSSE_SCRIPT::mWalk, mBounce, mLimbo); their records are plain
// structs (include/rsse.h). A peep is held by its thing id (0: a free slot).
//
// The walk nodes move a peep from node to node of the ride (DrawPeep, untranslated, draws it); the states of a walk record are 0 free,
// 1 walking in, 2 waiting, 3 walking out, 4 done. The time of a leg is 100 ms per whole unit of distance (100 for none), the direction
// is one of eight octants, worked out in doubles with the constants 6.28318 (not 2 pi) and 1.570795, as the original does.
#include <cmath>
#include <cstdint>

#include "Clock.h"
#include "Debug.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

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
