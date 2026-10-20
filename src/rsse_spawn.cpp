// The particle system's spawning (main app, rsse.cpp range of the inventory, 0x100bbfd4..0x100bf790): SpawnParticle, the emitter
// starters PAR_InitEmitter and PAR_InitEmitterLaunch, the effecter starter PAR_InitEffecter and the expanding ring
// (DoSpecialEmitter, SpecialEmitter1). The state is in src/rsse_particles_state.cpp, its layout in include/rsse.h.
//
// All of it is integer arithmetic. The random numbers are drawn from the state's seed (gParState.mF10, 1 after a reset) with a
// linear congruential generator, the Microsoft C library's: seed = seed * 0x343fd + 0x269ec3, and a draw uses the high half of
// the new seed. The listing takes it three ways: signed (-32768 to 32767), 15 bits (0 to 32767) and 12 bits (0 to 4095); the
// "below n" forms are the C remainder and draw nothing when n is 0. The sines come from gParState.mSinTable (512 entries, 256
// times the sine, indexed by an angle in 1/512 turns), which the code multiplies by 4 to get 1/1024.
#include <cstdint>
#include <cstring>

#include "rsse.h"

namespace {

// Advances the seed; the new seed.
uint32_t Advance() {
    const uint32_t next = static_cast<uint32_t>(gParState.mF10) * 0x343fdu + 0x269ec3u;
    gParState.mF10 = static_cast<int32_t>(next);
    return next;
}
int32_t DrawSigned() { return static_cast<int32_t>(Advance()) >> 16; }
int32_t Draw15() { return static_cast<int32_t>((Advance() >> 16) & 0x7fff); }
int32_t Draw12() { return static_cast<int32_t>((Advance() >> 16) & 0xfff); }
int32_t SignedBelow(int32_t limit) { return limit == 0 ? 0 : DrawSigned() % limit; }
int32_t Below15(int32_t limit) { return limit == 0 ? 0 : Draw15() % limit; }

// The sine and cosine (1/1024) of an angle in 1/512 turns. The cosine is the table a quarter turn on (128).
int32_t Sine(int32_t angle) { return gParState.mSinTable[angle & 511] * 4; }
int32_t Cosine(int32_t angle) { return gParState.mSinTable[(angle + 128) & 511] * 4; }

// A shift left by 4 that wraps (the originals' slwi).
int32_t Times16(int32_t value) { return static_cast<int32_t>(static_cast<uint32_t>(value) << 4); }

// Takes the first particle of the free chain and makes it the first of the emitter's own chain (the old first becomes its next); -1 when
// the pool is empty and nothing changes. SpawnParticle and SpecialEmitter1 do this in line. The taken particle's own previous link is
// not written: it led the free chain, where that link is -1.
int32_t TakeParticle(PAREmitter& emitter) {
    PARState& state = gParState;
    const int16_t taken = state.mParticlesFree;
    if (taken < 0) return -1;
    const int16_t previousFirst = emitter.mFirstParticle;
    emitter.mFirstParticle = taken;
    state.mParticlesFree = state.mParticles[taken].mNext;
    if (state.mParticlesFree >= 0) state.mParticles[state.mParticlesFree].mPrev = -1;
    state.mParticles[emitter.mFirstParticle].mNext = previousFirst;
    if (previousFirst >= 0) state.mParticles[previousFirst].mPrev = emitter.mFirstParticle;
    return emitter.mFirstParticle;
}

// Turns (x, z) by the angle (1/512 turns), as SpawnParticle's three offsets do: x' = x cos - z sin, z' = x sin + z cos, in 1/1024.
void TurnAbout(tagPARTICLE& particle, int32_t angle) {
    const int32_t x = particle.mPos[0];
    const int32_t sine = Sine(angle);
    const int32_t cosine = Cosine(angle);
    const int32_t z = particle.mPos[2];
    particle.mPos[0] = (x * cosine - z * sine) >> 10;
    particle.mPos[2] = (x * sine + z * cosine) >> 10;
}

}  // namespace

// 0x100bbfd4
int32_t SpawnParticle(int32_t emitterNumber) {
    PARState& state = gParState;
    PAREmitter& emitter = state.mEmitters[emitterNumber];
    const int32_t number = TakeParticle(emitter);
    if (number < 0) return number;
    tagPARTICLE& particle = state.mParticles[number];
    ++state.mF00;
    ++emitter.mParticles;
    particle.mState = 2;
    particle.mEmitter = static_cast<uint16_t>(emitterNumber);
    particle.mSize = emitter.mSizeStart;
    particle.mKind = emitter.mParticleKind;
    // The life: the emitter's word plus a random part below a quarter of it; never 0.
    particle.mLifeLeft = emitter.mLifeBase + SignedBelow(emitter.mLifeBase >> 2);
    particle.mLifeTotal = particle.mLifeLeft;
    particle.mFrame = 0;
    if (particle.mLifeTotal == 0) particle.mLifeTotal = 1;
    particle.mSpin = emitter.mRandomSpin != 0 ? static_cast<uint16_t>(Draw15()) : 0;
    particle.mColour = emitter.mColourRandom != 0 ? emitter.mColours[Draw15() & 15] : emitter.mColours[15];

    // The position.
    const int32_t spreadX = emitter.mRandomPos[0];
    const int32_t spreadY = emitter.mRandomPos[1];
    const int32_t spreadZ = emitter.mRandomPos[2];
    if (spreadX == 0 && spreadY == 0 && spreadZ == 0) {
        particle.mPos[0] = emitter.mPos[0];
        particle.mPos[1] = emitter.mPos[1];
        particle.mPos[2] = emitter.mPos[2];
    } else {
        if (emitter.mBox != 0) {
            // A box: three random numbers, then turned.
            particle.mPos[0] = SignedBelow(spreadX);
            particle.mPos[1] = emitter.mUnsignedY != 0 ? Below15(spreadY) : SignedBelow(spreadY);
            particle.mPos[2] = SignedBelow(spreadZ);
            if (emitter.mTurn != 0) TurnAbout(particle, emitter.mTurn / 8);
        } else {
            // A disc (x a cosine, z a sine of a random angle, z negated) or, with mShell, a ring: the angle first, then the lengths.
            const int32_t angle = Draw12() / 8;
            if (emitter.mShell != 0) {
                particle.mPos[0] = (spreadX * Cosine(angle)) >> 10;
                particle.mPos[1] = emitter.mUnsignedY != 0 ? Below15(spreadY) : SignedBelow(spreadY);
                particle.mPos[2] = -((spreadZ * Sine(angle)) >> 10);
            } else {
                const int32_t lengthX = Below15(spreadX);
                particle.mPos[0] = (Cosine(angle) * lengthX) >> 10;
                particle.mPos[1] = emitter.mUnsignedY != 0 ? Below15(spreadY) : SignedBelow(spreadY);
                const int32_t lengthZ = Below15(spreadZ);
                particle.mPos[2] = -((Sine(angle) * lengthZ) >> 10);
            }
            if (emitter.mTurn != 0) TurnAbout(particle, emitter.mTurn / 8);
        }
        particle.mPos[0] += emitter.mPos[0];
        particle.mPos[1] += emitter.mPos[1];
        particle.mPos[2] += emitter.mPos[2];
    }
    if (emitter.mOffsetRadius != 0) {
        const int32_t angle = emitter.mOffsetAngle / 8;
        particle.mPos[0] += (emitter.mOffsetRadius * Sine(angle)) >> 10;
        particle.mPos[2] += (emitter.mOffsetRadius * Cosine(angle)) >> 10;
    }

    // The velocity.
    if (emitter.mSphere != 0) {
        // A burst: a random direction in the horizontal plane (the angle, then the length below mSphere), and a random height.
        const int32_t angle = Draw12() / 8;
        const int32_t length = Below15(emitter.mSphere);
        particle.mVel[0] = (length * Cosine(angle)) >> 10;
        particle.mVel[1] = SignedBelow(emitter.mSphere);
        particle.mVel[2] = (length * Sine(angle)) >> 10;
    } else {
        particle.mVel[0] = emitter.mLaunch[0] + SignedBelow(emitter.mVelSpread);
        particle.mVel[1] = emitter.mLaunch[1] + SignedBelow(emitter.mVelSpread);
        particle.mVel[2] = emitter.mLaunch[2] + SignedBelow(emitter.mVelSpread);
    }
    if (emitter.mAddVelocity != 0) {
        particle.mVel[0] += emitter.mVelocity[0];
        particle.mVel[1] += emitter.mVelocity[1];
        particle.mVel[2] += emitter.mVelocity[2];
    }
    return number;
}

// 0x100bf248
uint32_t PAR_InitEffecter(int32_t type, int32_t x, int32_t y, int32_t z) {
    if (gParSystemRunning == 0) return 0xffffffffu;
    if (type < 0 || type >= kParEffecters) return 0xffffffffu;
    PARState& state = gParState;
    // In low-spec mode a definition marked with a byte at +0x44 is left out (and the answer is 0, not "none").
    if (state.mLowSpec != 0 && gParLibrary.mEffecters[type].mF44[0] != 0) return 0;
    // Takes the first effecter of the free chain and puts it first in the chain in use.
    int32_t number = -1;
    const int16_t taken = state.mEffectersFree;
    if (taken >= 0) {
        const int16_t previousFirst = state.mEffectersUsed;
        state.mEffectersUsed = taken;
        state.mEffectersFree = state.mEffecters[state.mEffectersFree].mNext;
        if (state.mEffectersFree >= 0) state.mEffecters[state.mEffectersFree].mPrev = -1;
        state.mEffecters[state.mEffectersUsed].mNext = previousFirst;
        if (previousFirst >= 0) state.mEffecters[previousFirst].mPrev = state.mEffectersUsed;
        number = state.mEffectersUsed;
    }
    if (number < 0) return 0xffffffffu;
    PAREffecter& effecter = state.mEffecters[number];
    // The definition is copied whole; the chain links the effecter had in the free chain are put back.
    const int16_t next = effecter.mNext;
    const int16_t previous = effecter.mPrev;
    std::memcpy(&effecter, &gParLibrary.mEffecters[type], sizeof(PAREffecter));
    effecter.mNext = next;
    effecter.mPrev = previous;
    effecter.mInUse = 1;
    effecter.mPos[0] = x >> 4;
    effecter.mPos[1] = y >> 4;
    effecter.mPos[2] = z >> 4;
    const uint16_t serial = static_cast<uint16_t>(state.mF1A);
    state.mF1A = static_cast<int16_t>(serial + 1);
    effecter.mSerial = serial;
    effecter.mLinked = 0;
    return static_cast<uint32_t>(number) | (static_cast<uint32_t>(effecter.mSerial) << 16);
}

namespace {

// Marks the effecter `handle` as linked, as PAR_InitEmitter and PAR_InitEmitterLaunch do for an emitter with an offset link. THE
// ORIGINAL INDEXES THE EFFECTERS WITH THE WHOLE HANDLE, not its low half, and a handle whose serial is not 0 puts the byte far from
// the effecters: at the state's address + 0x9636 + handle * 0x68 (32-bit arithmetic). No library definition has both an effecter
// link and an offset link, so the byte never lands anywhere in the game; the translation writes it when the address is inside the
// state's records (the Mac's offsets from 0x2c on, which the host's state keeps) and leaves it out otherwise (the host's memory
// there is not the state).
void MarkEffecterLinked(int32_t handle) {
    const uint32_t offset = 0x9636u + static_cast<uint32_t>(handle) * 0x68u;
    if (offset >= 0x2cu && offset < 0x9e68u) reinterpret_cast<uint8_t*>(&gParState.mEmitters[0])[offset - 0x2cu] = 1;
}

// What PAR_InitEmitter and PAR_InitEmitterLaunch share. The two listings differ in one block each: PAR_InitEmitter adds a random number
// below mJitter to each of the three words at +0x2c (after the emitter is marked in use), PAR_InitEmitterLaunch sets the three words at
// +0x38 from the launch vector (after the serial and the chain are set). Nothing in either reads what the other writes, so they run
// from the one body. `launch` is NULL for PAR_InitEmitter.
uint32_t StartEmitter(int32_t type, int32_t x, int32_t y, int32_t z, const int32_t* launch) {
    if (gParSystemRunning == 0) return 0xffffffffu;
    if (type < 0 || type >= kParLibraryEmitters) return 0xffffffffu;
    PARState& state = gParState;
    const PARLibraryEmitter& definition = gParLibrary.mEmitters[type];
    if (state.mLowSpec != 0 && definition.mFC0[1] != 0) return 0;
    // Takes the first emitter of the free chain and puts it first in the chain in use.
    int32_t number = -1;
    const int16_t taken = state.mEmittersFree;
    if (taken >= 0) {
        const int16_t previousFirst = state.mEmittersUsed;
        state.mEmittersUsed = taken;
        state.mEmittersFree = state.mEmitters[state.mEmittersFree].mNext;
        if (state.mEmittersFree >= 0) state.mEmitters[state.mEmittersFree].mPrev = -1;
        state.mEmitters[state.mEmittersUsed].mNext = previousFirst;
        if (previousFirst >= 0) state.mEmitters[previousFirst].mPrev = state.mEmittersUsed;
        number = state.mEmittersUsed;
    }
    if (number < 0) return 0xffffffffu;
    PAREmitter& emitter = state.mEmitters[number];
    // The definition is copied whole; the chain links the emitter had in the free chain are put back.
    const int16_t next = emitter.mNext;
    const int16_t previous = emitter.mPrev;
    std::memcpy(&emitter, &definition, sizeof(PAREmitter));
    emitter.mNext = next;
    emitter.mPrev = previous;
    if (emitter.mKeepDensity == 0) ApplyParticleDensityAdjustment(number);
    emitter.mLifeAtStart = emitter.mLife;
    emitter.mPos[0] = x >> 4;
    emitter.mPos[1] = y >> 4;
    emitter.mPos[2] = z >> 4;
    // The bounding box: nothing yet (an interface emitter has the point 0).
    if (emitter.m2D != 0) {
        for (int32_t i = 0; i < 3; ++i) {
            emitter.mBoxMin[i] = 0;
            emitter.mBoxMax[i] = 0;
        }
    } else {
        for (int32_t i = 0; i < 3; ++i) {
            emitter.mBoxMin[i] = INT32_MAX;
            emitter.mBoxMax[i] = INT32_MIN;
        }
    }
    emitter.mParticles = 0;
    emitter.mInUse = 1;
    emitter.mType = static_cast<int16_t>(type);
    if (launch == nullptr) {
        emitter.mVelocity[0] += SignedBelow(emitter.mJitter);
        emitter.mVelocity[1] += SignedBelow(emitter.mJitter);
        emitter.mVelocity[2] += SignedBelow(emitter.mJitter);
    }
    const uint16_t serial = static_cast<uint16_t>(state.mF18);
    state.mF18 = static_cast<int16_t>(serial + 1);
    emitter.mSerial = serial;
    emitter.mFirstParticle = -1;
    emitter.mLinkedChild = 0;
    if (launch != nullptr) {
        // The launch vector times the scale, over 1024 (rounded toward zero).
        for (int32_t i = 0; i < 3; ++i) emitter.mLaunch[i] = (launch[i] * emitter.mLaunchScale) / 1024;
    }
    emitter.mAccelerates = (emitter.mAccel[0] != 0 || emitter.mAccel[1] != 0 || emitter.mAccel[2] != 0) ? 1 : 0;
    if (emitter.mSpecial != 0) {
        DoSpecialEmitter(number, 1);
    } else if (emitter.mInUse != 0 && emitter.mStartCount != 0 && emitter.mInUse > 0) {
        // The tests of mInUse cannot fail: it was just set to 1. A negative count would make the loop run about 2^32 times.
        for (int32_t remaining = emitter.mStartCount; remaining != 0;) {
            --remaining;
            SpawnParticle(number);
        }
    }

    // The effecter or emitter that starts with it.
    if (emitter.mLinked >= 0) {
        if (emitter.mLinkIsEffecter != 0) {
            if (emitter.mLinkFollow != 0) {
                const PARLibraryEffecter& other = gParLibrary.mEffecters[emitter.mLinked];
                x += Times16(other.mL18);
                y += Times16(other.mL1C);
                z += Times16(other.mL20);
            }
            emitter.mLinked = static_cast<int32_t>(PAR_InitEffecter(emitter.mLinked, x, y, z));
            if (emitter.mLinked >= 0 && emitter.mLinkFollow != 0) MarkEffecterLinked(emitter.mLinked);
        } else {
            if (emitter.mLinkFollow != 0) {
                const PARLibraryEmitter& other = gParLibrary.mEmitters[emitter.mLinked];
                x += Times16(other.mL2C);
                y += Times16(other.mL30);
                z += Times16(other.mL34);
            }
            // A definition that names itself is not started again.
            if (type == definition.mLB4) {
                emitter.mLinked = -1;
            } else {
                emitter.mLinked = static_cast<int32_t>(PAR_InitEmitter(emitter.mLinked, x, y, z));
            }
            if (emitter.mLinked >= 0 && emitter.mLinkFollow != 0) state.mEmitters[emitter.mLinked & 0xffff].mLinkedChild = 1;
        }
    }
    return static_cast<uint32_t>(number) | (static_cast<uint32_t>(emitter.mSerial) << 16);
}

}  // namespace

// 0x100bec30
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) { return StartEmitter(type, a, b, c, nullptr); }

// 0x100be7e4
uint32_t PAR_InitEmitterLaunch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz) {
    const int32_t launch[3] = {vx, vy, vz};
    return StartEmitter(type, x, y, z, launch);
}

// 0x100bf42c
void DoSpecialEmitter(int32_t emitterNumber, int32_t start) {
    if (gParState.mEmitters[emitterNumber].mSpecial == 1) SpecialEmitter1(emitterNumber, start);
}

// 0x100bf490
void SpecialEmitter1(int32_t emitterNumber, int32_t start) {
    PARState& state = gParState;
    PAREmitter& emitter = state.mEmitters[emitterNumber];
    int32_t radius = emitter.mRadius;
    if (start != 0) {
        emitter.mInUse = 1;
        radius = 0;
    } else {
        radius += emitter.mVelSpread;
        if (emitter.mInUse > 0) {
            // One step of the ring: `((radius >> 7) + 32)` times the count of the life stage (the ratio of the life to the life at the
            // start, in quarters: 0 to 3, anything above 3 the same as 3), a signed byte of mSpawnRate.
            for (int32_t done = 0;; ++done) {
                int32_t count = emitter.mSpawnRate[0];
                if (emitter.mLifeAtStart != 0) {
                    const int32_t stage =
                        static_cast<int32_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(emitter.mLife) << 2)) /
                                             emitter.mLifeAtStart);
                    if (stage == 0) count = emitter.mSpawnRate[3];
                    else if (stage == 1) count = emitter.mSpawnRate[2];
                    else if (stage == 2) count = emitter.mSpawnRate[1];
                }
                if (!(done < ((radius >> 7) + 0x20) * count)) break;
                const int32_t number = TakeParticle(emitter);
                if (number < 0) continue;
                tagPARTICLE& particle = state.mParticles[number];
                ++state.mF00;
                ++emitter.mParticles;
                particle.mEmitter = static_cast<uint16_t>(emitterNumber);
                particle.mState = 2;
                particle.mSize = emitter.mSizeStart;
                particle.mKind = emitter.mParticleKind;
                particle.mColour = emitter.mColours[15];     // (stored a second time at the end in the listing)
                particle.mLifeLeft = emitter.mLifeBase + SignedBelow(emitter.mLifeBase >> 1);
                particle.mLifeTotal = particle.mLifeLeft;
                if (particle.mLifeTotal == 0) particle.mLifeTotal = 1;
                const int32_t angle = Draw12() / 8;
                // On the circle of the radius, moving outward at mVelSpread, level (height mLaunch[1] + 20).
                particle.mPos[0] = emitter.mPos[0] + ((radius * Cosine(angle)) >> 10);
                particle.mPos[1] = emitter.mPos[1];
                particle.mPos[2] = emitter.mPos[2] - ((radius * Sine(angle)) >> 10);
                particle.mVel[0] = (emitter.mVelSpread * Cosine(angle)) >> 10;
                particle.mVel[1] = emitter.mLaunch[1] + 0x14;
                particle.mVel[2] = -((emitter.mVelSpread * Sine(angle)) >> 10);
            }
        }
    }
    emitter.mRadius = radius;
}
