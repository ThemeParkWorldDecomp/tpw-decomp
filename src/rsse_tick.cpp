// The particle system's per-tick work and its drawing list (main app of SimTheme Park 2.04b3, the rsse.cpp range of the
// inventory):
//
//   0x100b9dd4 PAR_BuildDrawingList (880 bytes)  0x100bc9f0 MoveEmitter (404)    0x100bcbb4 DoEmitters (1,108)
//   0x100bd02c MoveEffecter (404)                0x100bd1f0 DoEffecters (1,064)  0x100bd63c DoAParticle (1,424)
//
// PAR_Tick (src/rsse_particles.cpp) calls DoEmitters and DoEffecters, then DoAParticle for every particle of every emitter. The state
// they work on is gParState (include/rsse.h): emitters, effecters and particles are chained by 16-bit links with -1 at the ends,
// the used chains hanging from the state's heads and the free ones too. The header's seed (mF10) feeds the system's own random
// numbers, x * 0x343fd + 0x269ec3 (the C library's old Microsoft generator), taken from the upper half of the new seed.
//
// Everything here is integer work. The listing's divisions are divw, which leaves the dividend in the register for a zero divisor
// and for the minimum over -1 (the emulator's answer; the hardware's is undefined), and its products are mullw (wrapping), which
// Div and Mul below reproduce.
#include <cstdint>

#include "Debug.h"
#include "rsse.h"
#include "tb_particle.h"

namespace {

// divw: the quotient truncated towards zero; the dividend where the quotient is undefined.
inline int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// mullw: the low 32 bits of the product.
inline int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

// slwi: a left shift that wraps.
inline int32_t Shl(int32_t a, int32_t n) { return static_cast<int32_t>(static_cast<uint32_t>(a) << n); }

// neg: the minimum stays the minimum.
inline int32_t Neg(int32_t a) { return static_cast<int32_t>(0u - static_cast<uint32_t>(a)); }

// srawi followed by addze: a / 2^n truncated towards zero (a plain shift would round down).
inline int32_t Half(int32_t a) { return a / 2; }
inline int32_t Quarter(int32_t a) { return a / 4; }
inline int32_t Thousandth(int32_t a) { return a / 1024; }

// The next number of the system's generator (the seed is the state's word at +0x10), as the listing takes it from the new seed:
// the upper 15 bits (rlwinm 16, 0x11, 0x1f) or the upper halfword as a signed number (srawi 16).
inline void NextSeed() { gParState.mF10 = Mul(gParState.mF10, 0x343fd) + 0x269ec3; }
inline int32_t Random15() {
    NextSeed();
    return static_cast<int32_t>((static_cast<uint32_t>(gParState.mF10) >> 16) & 0x7fff);
}
inline int32_t RandomSigned16() {
    NextSeed();
    return gParState.mF10 >> 16;
}

// The ground height the state's callback answers for the point (x, z) in 1/1024 world units (the callback is called through the
// original's function descriptor; here it is the function itself, DummyHeightCallBack unless the owner gave another).
using HeightFunction = int32_t (*)(int32_t x, int32_t z);
inline int32_t GroundHeight(int32_t x, int32_t z) {
    return reinterpret_cast<HeightFunction>(gParState.mHeightCallback)(x, z);
}

// The text of the assert in DoAParticle (the main app's string table, 0x1023ed27).
constexpr char kIllegalSprite[] = "PTCL: Awooga Awooga! Illegal Sprite code";

}  // namespace

// ---- MoveEmitter, MoveEffecter

// 0x100bc9f0. Drag takes drag / 1024 of each velocity off it, gravity comes off the vertical one, the velocity moves the position.
// With the bounce flag, a position below the ground is put right: when it is further below the ground than the vertical velocity
// took it this tick (it went in through the side) the x and y moves are undone (not z, as in the original) and the velocities
// halved, x and z turned back, else the emitter is put on the ground and the vertical velocity turned and halved.
void MoveEmitter(PAREmitter* e) {
    e->mVelocity[0] -= Thousandth(Mul(e->mVelocity[0], e->mDrag));
    e->mVelocity[1] -= e->mGravity + Thousandth(Mul(e->mVelocity[1], e->mDrag));
    e->mVelocity[2] -= Thousandth(Mul(e->mVelocity[2], e->mDrag));
    e->mPos[0] += e->mVelocity[0];
    e->mPos[1] += e->mVelocity[1];
    e->mPos[2] += e->mVelocity[2];
    if (e->mBounce == 0) return;
    const int32_t ground = GroundHeight(Shl(e->mPos[0], 4), Shl(e->mPos[2], 4)) >> 4;
    if (e->mPos[1] >= ground) return;
    if (e->mPos[1] - ground < e->mVelocity[1]) {
        e->mPos[0] -= e->mVelocity[0];
        e->mPos[1] -= e->mVelocity[1];
        e->mVelocity[0] = Half(Neg(e->mVelocity[0]));
        e->mVelocity[1] = Half(e->mVelocity[1]);
        e->mVelocity[2] = Half(Neg(e->mVelocity[2]));
    } else {
        e->mPos[1] = ground;
        e->mVelocity[1] = Half(Neg(e->mVelocity[1]));
        e->mVelocity[0] = Half(e->mVelocity[0]);
        e->mVelocity[2] = Half(e->mVelocity[2]);
    }
}

// 0x100bd02c. The same for an effecter, whose drag and gravity are words and whose bounce off the side keeps a quarter.
void MoveEffecter(PAREffecter* e) {
    e->mVel[0] -= Thousandth(Mul(e->mVel[0], e->mDrag));
    e->mVel[1] -= e->mGravity + Thousandth(Mul(e->mVel[1], e->mDrag));
    e->mVel[2] -= Thousandth(Mul(e->mVel[2], e->mDrag));
    e->mPos[0] += e->mVel[0];
    e->mPos[1] += e->mVel[1];
    e->mPos[2] += e->mVel[2];
    if (e->mBounce == 0) return;
    const int32_t ground = GroundHeight(Shl(e->mPos[0], 4), Shl(e->mPos[2], 4)) >> 4;
    if (e->mPos[1] >= ground) return;
    if (e->mPos[1] - ground < e->mVel[1]) {
        e->mPos[0] -= e->mVel[0];
        e->mPos[1] -= e->mVel[1];
        e->mVel[0] = Quarter(Neg(e->mVel[0]));
        e->mVel[1] = Quarter(e->mVel[1]);
        e->mVel[2] = Quarter(Neg(e->mVel[2]));
    } else {
        e->mPos[1] = ground;
        e->mVel[1] = Half(Neg(e->mVel[1]));
        e->mVel[0] = Half(e->mVel[0]);
        e->mVel[2] = Half(e->mVel[2]);
    }
}

// ---- DoEmitters

// 0x100bcbb4. The next emitter of the chain is read before the emitter is dealt with, since an emitter that has ended leaves the
// chain. An emitter that is paused is passed over altogether. The rate for the quarter of the life left (mLifeAtStart over
// mLife, by four) is read from the four signed bytes at +0x68 backwards, the first one when there is no start life.
void DoEmitters() {
    PARState& state = gParState;
    int32_t index = state.mEmittersUsed;
    while (index >= 0) {
        PAREmitter& e = state.mEmitters[index];
        const int32_t next = e.mNext;
        if (e.mPaused == 0) {
            if (e.mNoExpire == 0) e.mLife -= 1;
            if (e.m2D == 0) {
                e.mBoxMin[0] = e.mBoxMin[1] = e.mBoxMin[2] = INT32_MAX;
                e.mBoxMax[0] = e.mBoxMax[1] = e.mBoxMax[2] = INT32_MIN;
            }
            if (e.mLife < 0) {
                // Ending. The end emitter or effecter starts on the tick the life reached -1, once.
                const int32_t endType = e.mEndType;
                if (endType >= 0 && e.mLife == -1) {
                    if (e.mEndIsEffecter != 0) {
                        PAR_InitEffecter(endType, Shl(e.mPos[0], 4), Shl(e.mPos[1], 4), Shl(e.mPos[2], 4));
                    } else {
                        PAR_InitEmitter(endType, Shl(e.mPos[0], 4), Shl(e.mPos[1], 4), Shl(e.mPos[2], 4));
                    }
                    e.mEndType = -1;
                }
                // A linked emitter or effecter ends with this one.
                if (e.mKillLinked != 0 && e.mLinked >= 0) {
                    const uint32_t handle = static_cast<uint32_t>(e.mLinked);
                    if (e.mLinkIsEffecter != 0) {
                        if (PAR_GetEffecterStatus(handle) != 0) state.mEffecters[handle & 0xffff].mLife = -1;
                    } else {
                        if (PAR_GetEmitterStatus(handle) != 0) state.mEmitters[handle & 0xffff].mLife = -1;
                    }
                }
                // With no particles left it goes back to the free chain (the serial is cleared so that its handle is dead).
                if (e.mParticles == 0) {
                    const int32_t head = state.mEmittersUsed;
                    const int32_t previous = e.mPrev;
                    const int32_t following = e.mNext;
                    if (head == index) state.mEmittersUsed = static_cast<int16_t>(following);
                    if (previous >= 0) state.mEmitters[previous].mNext = e.mNext;
                    if (following >= 0) state.mEmitters[following].mPrev = e.mPrev;
                    e.mInUse = 0;
                    e.mNext = state.mEmittersFree;
                    e.mPrev = -1;
                    e.mSerial = 0;
                    state.mEmittersFree = static_cast<int16_t>(index);
                }
            } else {
                // Alive. A linked emitter or effecter is carried along.
                if (e.mLinkFollow != 0 && e.mLinked >= 0) {
                    const uint32_t handle = static_cast<uint32_t>(e.mLinked);
                    if (e.mLinkIsEffecter != 0) {
                        if (PAR_GetEffecterStatus(handle) != 0) {
                            PAREffecter& linked = state.mEffecters[handle & 0xffff];
                            linked.mPos[0] = e.mPos[0];
                            linked.mPos[1] = e.mPos[1];
                            linked.mPos[2] = e.mPos[2];
                        }
                    } else {
                        if (PAR_GetEmitterStatus(handle) != 0) {
                            PAREmitter& linked = state.mEmitters[handle & 0xffff];
                            linked.mPos[0] = e.mPos[0];
                            linked.mPos[1] = e.mPos[1];
                            linked.mPos[2] = e.mPos[2];
                        }
                    }
                }
                if (e.mOffsetSpin != 0) e.mOffsetAngle = (e.mOffsetAngle + e.mOffsetSpin) & 0xfff;
                if (e.mLinkedChild == 0 &&
                    (e.mVelocity[0] != 0 || e.mVelocity[1] != 0 || e.mVelocity[2] != 0 || e.mGravity != 0)) {
                    MoveEmitter(&e);
                }
                if (e.mSpecial != 0) {
                    DoSpecialEmitter(index, 0);
                } else {
                    int32_t rate;
                    if (e.mLifeAtStart == 0) {
                        rate = e.mSpawnRate[0];
                    } else {
                        const int32_t quarter = Div(Shl(e.mLife, 2), e.mLifeAtStart);
                        if (quarter == 0) rate = e.mSpawnRate[3];
                        else if (quarter == 1) rate = e.mSpawnRate[2];
                        else if (quarter == 2) rate = e.mSpawnRate[1];
                        else rate = e.mSpawnRate[0];
                    }
                    if (e.mMaxParticles > static_cast<int32_t>(e.mParticles)) {
                        if (rate < 0) {
                            // A negative rate makes one particle every -rate ticks.
                            const int16_t countdown = static_cast<int16_t>(e.mSpawnCountdown - 1);
                            e.mSpawnCountdown = countdown;
                            if (countdown <= 0) {
                                SpawnParticle(index);
                                e.mSpawnCountdown = static_cast<int16_t>(-rate);
                            }
                        } else {
                            for (; rate != 0; --rate) SpawnParticle(index);
                        }
                    }
                }
            }
        }
        index = next;
    }
}

// ---- DoEffecters

// 0x100bd1f0. An effecter pulls every particle of the emitters it acts on (the used chain's emitters that are not paused and, with a
// group, have that group) towards a point: the velocity gains strength * the offset to the point / (distance squared over 256, 10 or
// more). The point is the effecter's position plus a random offset for each axis, drawn when the effecter has a spread for it.
// The random numbers are drawn x, y, z in that order whether or not there are particles.
void DoEffecters() {
    PARState& state = gParState;
    tagPARTICLE* const pool = state.mParticles;
    int32_t index = state.mEffectersUsed;
    while (index >= 0) {
        PAREffecter& e = state.mEffecters[index];
        const int32_t next = e.mNext;
        if (e.mNoExpire == 0) e.mLife -= 1;
        if (e.mRadius == 0) e.mRadius = 1;
        if (e.mLinked == 0 && (e.mVel[0] != 0 || e.mVel[1] != 0 || e.mVel[2] != 0 || e.mGravity != 0)) MoveEffecter(&e);
        if (e.mLife < 0) {
            if (e.mEndIsEffecter != 0) {
                PAR_InitEffecter(e.mEndType, Shl(e.mPos[0], 4), Shl(e.mPos[1], 4), Shl(e.mPos[2], 4));
            } else {
                PAR_InitEmitter(e.mEndType, Shl(e.mPos[0], 4), Shl(e.mPos[1], 4), Shl(e.mPos[2], 4));
            }
            e.mEndIsEffecter = -1;
            const int32_t head = state.mEffectersUsed;
            const int32_t previous = e.mPrev;
            const int32_t following = e.mNext;
            if (head == index) state.mEffectersUsed = static_cast<int16_t>(following);
            if (previous >= 0) state.mEffecters[previous].mNext = e.mNext;
            if (following >= 0) state.mEffecters[following].mPrev = e.mPrev;
            e.mInUse = 0;
            e.mNext = state.mEffectersFree;
            e.mPrev = -1;
            state.mEffectersFree = static_cast<int16_t>(index);
        } else {
            const int32_t x = e.mPos[0] + (e.mSpread[0] != 0 ? RandomSigned16() % e.mSpread[0] : 0);
            const int32_t y = e.mPos[1] + (e.mSpread[1] != 0 ? RandomSigned16() % e.mSpread[1] : 0);
            const int32_t z = e.mPos[2] + (e.mSpread[2] != 0 ? RandomSigned16() % e.mSpread[2] : 0);
            const int32_t group = e.mGroup;
            const int32_t strength = e.mStrength;
            int32_t emitterIndex = state.mEmittersUsed;
            while (emitterIndex >= 0) {
                const PAREmitter& em = state.mEmitters[emitterIndex];
                if (em.mPaused == 0 && (group == -1 || group == em.mParticleKind)) {
                    int32_t particleIndex = em.mFirstParticle;
                    while (particleIndex >= 0) {
                        tagPARTICLE& p = pool[particleIndex];
                        const bool skip = group >= 0 ? p.mKind != group : state.mEmitters[p.mEmitter].mNotAttracted != 0;
                        if (!skip) {
                            const int32_t dx = Quarter(x - p.mPos[0]);
                            const int32_t dy = Quarter(y - p.mPos[1]);
                            const int32_t dz = Quarter(z - p.mPos[2]);
                            const int32_t distance = ((Mul(dx, dx) + Mul(dy, dy) + Mul(dz, dz)) >> 8) + 10;
                            p.mVel[0] += Div(Mul(strength, dx), distance);
                            p.mVel[1] += Div(Mul(strength, dy), distance);
                            p.mVel[2] += Div(Mul(strength, dz), distance);
                            if (e.mMode == 2 && distance < (Mul(e.mRadius, e.mRadius) >> 8)) p.mLifeLeft = 0;
                        }
                        particleIndex = state.mParticles[particleIndex].mNext;
                    }
                }
                emitterIndex = state.mEmitters[emitterIndex].mNext;
            }
        }
        index = next;
    }
}

// ---- DoAParticle

// 0x100bd63c. The particle's life counts down; below 0 it ends. Otherwise (the emitter being over, with its flag, ends its particles
// at the next tick) the colour, frame, size and turn are set from the emitter and the particle's remaining life, the velocity gets
// a random part of the emitter's mAccel (the upper halfword of a new seed, mod the range, so it may be negative), loses drag, loses
// gravity, and moves the particle unless it is new (mState 2: it becomes 1). Unless the emitter is 2D the emitter's box widens to
// take in the particle and its size over 1024.
void DoAParticle(int32_t particleIndex) {
    PARState& state = gParState;
    tagPARTICLE& p = state.mParticles[particleIndex];
    PAREmitter& e = state.mEmitters[p.mEmitter];
    p.mLifeLeft -= 1;
    if (p.mLifeLeft < 0) {
        // Ended: the emitter and the system lose a particle, the death emitter starts there, the particle leaves the emitter's chain
        // (the emitter's first particle moves on if it was this one) and goes to the free chain.
        state.mF00 -= 1;
        e.mParticles = static_cast<uint16_t>(e.mParticles - 1);
        if (e.mDeathType >= 0) PAR_InitEmitter(e.mDeathType, Shl(p.mPos[0], 4), Shl(p.mPos[1], 4), Shl(p.mPos[2], 4));
        if (particleIndex >= 0) {
            PAREmitter& owner = state.mEmitters[p.mEmitter];
            const int32_t previous = p.mPrev;
            const int32_t following = p.mNext;
            if (owner.mFirstParticle == particleIndex) owner.mFirstParticle = static_cast<int16_t>(following);
            if (previous >= 0) state.mParticles[previous].mNext = p.mNext;
            if (following >= 0) state.mParticles[following].mPrev = p.mPrev;
            p.mState = 0;
            p.mNext = state.mParticlesFree;
            p.mPrev = -1;
            state.mParticlesFree = static_cast<int16_t>(particleIndex);
        }
        return;
    }

    if (e.mLife < 0 && e.mDieWithEmitter != 0) p.mLifeLeft = -1;

    // The colour: its place in the 16 follows the life left (15 at the start) while the emitter has a life for its particles, else
    // the first; with the emitter's random flag a random one.
    if (e.mColourRandom == 0) {
        const int32_t place = e.mLifeBase > 0 ? Div(Mul(p.mLifeLeft, 15), p.mLifeTotal) : 0;
        p.mColour = *(e.mColours + place);
    } else if ((e.mColourRandom & 1) != 0) {
        p.mColour = e.mColours[Random15() & 15];
    }

    // The frame runs backwards from the emitter's last frame as the life runs out (the last one for a particle that is about to end).
    if (e.mSpriteFrames != 0) {
        const int32_t last = e.mSpriteFrames - 1;
        if (p.mLifeLeft >= 0) {
            p.mFrame = static_cast<uint16_t>(last - Div(Mul(last, p.mLifeLeft), p.mLifeTotal));
        } else {
            p.mFrame = static_cast<uint16_t>(last);
        }
        if (static_cast<int32_t>(p.mFrame) >= static_cast<int32_t>(e.mSpriteFrames)) {
            p.mFrame = 0;
            GAME_ASSERTMSG(0, kIllegalSprite);
        }
    } else {
        p.mFrame = 0;
    }

    // The size.
    if (e.mLifeBase > 0) {
        const int32_t end = e.mSizeEnd;
        p.mSize = static_cast<int16_t>(end + Div(Mul(p.mLifeLeft, e.mSizeStart - end), p.mLifeTotal));
    } else {
        p.mSize = e.mSizeStart;
    }

    // The turn.
    if (e.mSpinMin == e.mSpinMax) {
        p.mSpin = static_cast<uint16_t>(p.mSpin + e.mSpinMin);
    } else {
        // (The original tests the range for 0 here, which cannot happen: the two are different.)
        const int32_t range = e.mSpinMax - e.mSpinMin;
        const int32_t part = Random15() % range;
        p.mSpin = static_cast<uint16_t>(e.mSpinMin + static_cast<int16_t>(part) + static_cast<int16_t>(p.mSpin));
    }

    // The random velocity.
    if (e.mAccelerates != 0) {
        p.mVel[0] += e.mAccel[0] != 0 ? RandomSigned16() % e.mAccel[0] : 0;
        p.mVel[1] += e.mAccel[1] != 0 ? RandomSigned16() % e.mAccel[1] : 0;
        p.mVel[2] += e.mAccel[2] != 0 ? RandomSigned16() % e.mAccel[2] : 0;
    }
    // Drag, gravity, the move.
    const int32_t drag = e.mParticleDrag;
    if (drag != 0) {
        p.mVel[0] -= Thousandth(Mul(p.mVel[0], drag));
        p.mVel[1] -= Thousandth(Mul(p.mVel[1], drag));
        p.mVel[2] -= Thousandth(Mul(p.mVel[2], drag));
    }
    p.mVel[1] -= e.mParticleGravity;
    if (p.mState == 2) {
        p.mState = 1;
    } else {
        p.mPos[0] += p.mVel[0];
        p.mPos[1] += p.mVel[1];
        p.mPos[2] += p.mVel[2];
    }

    // The emitter's box takes in the particle and a margin of its size over 1024.
    if (e.m2D == 0) {
        const int32_t margin = Thousandth(p.mSize);
        if (e.mBoxMin[0] > p.mPos[0] - margin) e.mBoxMin[0] = p.mPos[0] - margin;
        if (e.mBoxMin[1] > p.mPos[1] - margin) e.mBoxMin[1] = p.mPos[1] - margin;
        if (e.mBoxMin[2] > p.mPos[2] - margin) e.mBoxMin[2] = p.mPos[2] - margin;
        if (e.mBoxMax[0] < p.mPos[0] + margin) e.mBoxMax[0] = p.mPos[0] + margin;
        if (e.mBoxMax[1] < p.mPos[1] + margin) e.mBoxMax[1] = p.mPos[1] + margin;
        if (e.mBoxMax[2] < p.mPos[2] + margin) e.mBoxMax[2] = p.mPos[2] + margin;
    }
}

// ---- PAR_BuildDrawingList

// 0x100b9dd4. The list is as long as the emitter's count of particles says; the walk down the chain does not look at the count
// (and reads pool[mFirstParticle] even for -1, which the original does too). An untextured emitter leaves each record's turn as it
// was in the block. In the original each record is 0x1c bytes; here sizeof(PTCL).
PTCL* PAR_BuildDrawingList(int32_t emitter) {
    if (gParSystemRunning == 0) return nullptr;
    PARState& state = gParState;
    const PAREmitter& e = state.mEmitters[emitter];
    const int32_t scale = e.mScale;
    const bool is2D = e.m2D != 0;
    if (e.mInUse == 0) return nullptr;
    const int32_t count = e.mParticles;
    if (count == 0) return nullptr;
    if (e.mMask != 0) return nullptr;
    const int32_t frames = e.mSpriteFrames;
    const int32_t sprite = e.mSprite;
    PTCL* list;
    if (count <= kParDrawingListRecords && gParDrawingListBusy == 0) {
        list = gParDrawingList;
        gParDrawingListBusy = 1;
    } else {
        list = static_cast<PTCL*>(PAR_Malloc(count * static_cast<int32_t>(sizeof(PTCL))));
    }
    PTCL* record = list;
    int32_t index = e.mFirstParticle;
    do {
        const tagPARTICLE& p = state.mParticles[index];
        // The position, the particle's distance from the emitter scaled when the emitter's scale is not 1000.
        int32_t x;
        int32_t y;
        int32_t z;
        if (!is2D) {
            if (scale == 1000) {
                x = Shl(p.mPos[0], 4);
                y = Shl(p.mPos[1], 4);
                z = Shl(p.mPos[2], 4);
            } else {
                x = Shl(e.mPos[0] + Mul(scale, p.mPos[0] - e.mPos[0]) / 1000, 4);
                y = Shl(e.mPos[1] + Mul(scale, p.mPos[1] - e.mPos[1]) / 1000, 4);
                z = Shl(e.mPos[2] + Mul(scale, p.mPos[2] - e.mPos[2]) / 1000, 4);
            }
            record->mFlags = e.mParticleFlags | 0x4000;
            record->mSize = static_cast<uint16_t>(Shl(p.mSize, 1));
        } else {
            // A 2D emitter's world x and z become the screen's x and y (the 100000 by 75000 of EM_Tick's units over 49 and 37 steps),
            // its depth the emitter's.
            int32_t wx;
            int32_t wz;
            if (scale == 1000) {
                wx = Shl(p.mPos[0], 4);
                wz = Shl(p.mPos[2], 4);
            } else {
                wx = Shl(e.mPos[0] + Mul(scale, p.mPos[0] - e.mPos[0]) / 1000, 4);
                wz = Shl(e.mPos[2] + Mul(scale, p.mPos[2] - e.mPos[2]) / 1000, 4);
            }
            x = (wx - 50000) / 49;
            y = (wz - 37500) / 37;
            z = e.mDepth;
            record->mFlags = e.mParticleFlags | 0x54000;
            record->mSize = static_cast<uint16_t>(p.mSize / 64);
        }
        record->mX = x;
        record->mY = y;
        record->mZ = z;
        record->mColour = (static_cast<uint32_t>(p.mColour) << 8) | (static_cast<uint32_t>(p.mColour) >> 24);
        if (frames != 0) {
            record->mTexture = ESP_GetParticleSprite(sprite, p.mFrame, 0);
            record->mAngle = p.mSpin;
        } else {
            record->mTexture = ParticleNoTexture();
        }
        index = p.mNext;
        ++record;
    } while (index >= 0);
    return list;
}
