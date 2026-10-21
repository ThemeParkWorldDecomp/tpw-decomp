// The rsse.cpp range of the inventory (main app 0x100b8...-0x100d6...): the mesh paths,
// the particle system (PAR_*), the ride animations, the geometry tables and the 3D sound
// environment (RSSE_*). The movie player of the same range is in TPMovie.h. PARTIAL:
// src/game/LMatrix.cpp translates BuildGeometryTables; the rest is untranslated.
#pragma once
#include <cstddef>
#include <cstdint>

#include "Ptr32.h"

namespace bfl {
namespace FileSystem {
class CFile;
}
}  // namespace bfl
struct PARPOINT3D;
struct PTCL;
struct tag_sRideAnimSet;

// 0x100b9dd4 (880 bytes, src/game/Particles.cpp, unit w26c): the drawing list of emitter `list`: one PTCL per particle of its chain (the
// emitter's own records: x, y, z in 1/1024 world unit, the colour word turned left by a byte, the sprite from ESP_GetParticleSprite
// or ParticleNoTexture(), the size, the turn and the flags), in the system's 100-record block (gParDrawingList) while it is free and
// there are 100 or fewer, else in a PAR_Malloc'd block. NULL while the system is not running, for an emitter not in use, without
// particles, or masked. The caller gives the list back with PAR_FreeDrawingList. (A PTCL is bigger on the host than on the Mac, so
// the blocks are sized by sizeof(PTCL); see kParDrawingListRecords.)
PTCL* PAR_BuildDrawingList(int32_t list);
// 0x100ba170 (src/game/Particles.cpp): the particles an emitter has now: 0 while the particle system is not
// running (gParSystemRunning) or the emitter is not in use. Its answer is a 16-bit count.
int32_t PAR_GetEmitterParticleCount(uint32_t emitter);
// ---- the particle system's state (src/game/rides/rsse.cpp)
// The main app keeps the particle system in one run of globals from 0x102e49f4 to 0x102f7bd0: a table of 512 sines, three
// words of the library, the running flag, the drawing list's busy flag and block, the LIBRARY (0x8b60 bytes: 105 emitter
// definitions of 0x140 bytes and 20 effecter definitions of 0x68, as Data:Particle:Tp2.plb gives them) and the STATE
// (0x9e68 bytes at 0x102edd68, which PAR_InitParticleSystem clears). The state holds a header of 0x2c bytes, 120 emitters
// of 0x140 bytes (a live emitter is a copy of a library emitter, the same layout), 20 effecters of 0x68, and the heads of
// the chains. The header's three pointers are 4 bytes on the Mac and native here, so the host's header is longer; the
// halfword fields after it keep the Mac's distances (checked below).
// (PAR_Tick walks the emitters from the head at +0x9e4c as state + index * 0x140 + 0xfc and tests the paused halfword at +0x2e; the
// in-use flag at +0x2c is PAR_GetEmitterParticleCount's: the same records, indexed from the start of the block instead of from +0x2c.)
// A particle of the pool (tagPARTICLE, 0x34 bytes; the pool is PAR_Malloc'd with the system's maximum). The fields DoAParticle
// (unit w26c, src/game/Particles.cpp) and PAR_BuildDrawingList use are named, and SpawnParticle (unit w26b, src/game/Particles.cpp) fills them;
// positions are in 1/64 world unit (the drawing list shifts them left by 4).
struct tagPARTICLE {
    int32_t mPos[3];                 // 0x00  x, y, z
    int32_t mVel[3];                 // 0x0c  the velocity, added to the position each tick
    int32_t mColour;                 // 0x18  one of the emitter's 16 colour words
    int32_t mLifeTotal;              // 0x1c  the life it started with (never 0: the fades divide by it)
    int32_t mLifeLeft;               // 0x20  the life left: starts as the emitter's life word plus a random part, counts down each tick; below 0 the particle ends (a DoEffecters sphere sets it to 0)
    int16_t mState;                  // 0x24  2: just made, DoAParticle does not move it yet (and makes it 1)
    int16_t mEmitter;                // 0x26  the number of the emitter that owns it
    int16_t mSize;                   // 0x28  the sprite's size (the emitter's mSizeStart at the start; DoAParticle fades it between the emitter's two sizes)
    int16_t mKind;                   // 0x2a  the emitter's byte at +0x04 (the group DoEffecters matches)
    uint16_t mFrame;                 // 0x2c  the sprite's animation frame (0 at the start; DoAParticle runs it through the emitter's frames)
    uint16_t mSpin;                  // 0x2e  the sprite's turn, in 1/65536 of a turn (a random 15-bit number at the start when the emitter spins its particles, else 0; DoAParticle adds the emitter's turn each tick)
    int16_t mNext;                   // 0x30  the next particle of the chain it is in (the free chain, or an emitter's), -1 at the end
    int16_t mPrev;                   // 0x32  the one before, -1 at the start
    void Byteswap();                 // 0x100bb110 (src/game/Particles.cpp): every field turned in place (a file's little-endian field)
};
static_assert(sizeof(tagPARTICLE) == 0x34 && __builtin_offsetof(tagPARTICLE, mVel) == 0x0c &&
                  __builtin_offsetof(tagPARTICLE, mLifeLeft) == 0x20 && __builtin_offsetof(tagPARTICLE, mSize) == 0x28 &&
                  __builtin_offsetof(tagPARTICLE, mSpin) == 0x2e && __builtin_offsetof(tagPARTICLE, mNext) == 0x30,
              "a particle is 0x34 bytes");
// The fields DoEmitters, MoveEmitter, DoAParticle and PAR_BuildDrawingList (unit w26c, src/game/Particles.cpp) and the spawning functions
// (unit w26b, src/game/Particles.cpp) use are named; the rest is padding. A live emitter is a copy of its library definition
// (PARLibraryEmitter), so the offsets are the same. Positions and the box are in 1/64 world unit.
struct PAREmitter {
    int16_t mInUse;                  // 0x00  nonzero while the emitter exists
    int16_t mPaused;                 // 0x02  DoEmitters and the particles' turn skip an emitter while this is not 0
    int8_t mParticleKind;            // 0x04  copied into each particle (mKind); DoEffecters with a group matches it
    int8_t m2D;                      // 0x05  nonzero for an interface (2D) emitter: no bounding box, the drawing list maps it to the screen
    int16_t mFirstParticle;          // 0x06  the first particle of the chain it owns (-1: none)
    uint16_t mParticles;             // 0x08  how many particles it has
    uint16_t mSerial;                // 0x0a  the upper half of its handle (PAR_GetEmitterStatus compares it; DoEmitters clears it when the emitter ends)
    int16_t mType;                   // 0x0c  the library emitter it was made from
    int16_t mSpecial;                // 0x0e  1: DoSpecialEmitter makes its particles (the expanding ring, SpecialEmitter1), not the rate below
    int16_t mEndType;                // 0x10  the library emitter (or effecter) to start when this one ends (-1: none; set to -1 after)
    int16_t mJitter;                 // 0x12  PAR_InitEmitter adds a random number below this (0: none) to each of mVelocity
    int32_t mPos[3];                 // 0x14  the position (PAR_SetEmitterPos, PAR_InitEmitter: the caller's x, y, z shifted right by 4)
    int32_t mLife;                   // 0x20  counts down (unless mNoExpire); below 0 the emitter is ending (-1 the first tick, -2 killed: PAR_KillEmitter)
    int16_t mDrag;                   // 0x24  MoveEmitter: the share (1/1024) of the velocity lost each tick
    int16_t mNoExpire;               // 0x26  nonzero: mLife does not count down
    int16_t mGravity;                // 0x28  MoveEmitter: taken off the vertical velocity each tick
    int16_t mMask;                   // 0x2a  PAR_SetEmitterMask (PAR_KillAndHideAll2DObjects sets 1); nonzero: PAR_BuildDrawingList makes no list for it
    int32_t mVelocity[3];            // 0x2c  the emitter's own velocity, added to mPos by MoveEmitter (and, with mAddVelocity, to each particle's velocity)
    int32_t mLaunch[3];              // 0x38  the velocity of the particles (PAR_SetEmitterLaunchVector, PAR_InitEmitterLaunch: the vector times mLaunchScale over 1024)
    int32_t mRandomPos[3];           // 0x44  PAR_SetEmitterRandomPos: the random spread, the setter's x, y, z shifted right by 4
    int16_t mVelSpread;              // 0x50  the random spread of the velocity (SpecialEmitter1: how far the ring grows a tick)
    int16_t mDepth;                  // 0x52  a 2D emitter's depth: the drawing list's z
    int32_t mOffsetAngle;            // 0x54  the angle (1/4096 turn) of the offset; DoEmitters turns it by mOffsetSpin each tick
    int32_t mColourRandom;           // 0x58  0: a particle's colour follows its life; bit 0: a random one of the 16; else it keeps its own
    int32_t mSphere;                 // 0x5c  nonzero: the velocity is a burst of up to this size in a random direction
    int32_t mStartCount;             // 0x60  the particles made when the emitter starts (ApplyParticleDensityAdjustment scales it, only while
                                     //       positive; never 0 after)
    int16_t mMaxParticles;           // 0x64  DoEmitters makes no more particles while the emitter has this many (ApplyParticleDensityAdjustment scales it too)
    int16_t mSpawnCountdown;         // 0x66  a negative rate counts this down and makes one particle at 0 or below
    int8_t mSpawnRate[4];            // 0x68  the particles made a tick (negative: one every -rate ticks) for the first and later quarters of the life
                                     //       (ApplyParticleDensityAdjustment scales each when nonzero: at most 0x7f, 1 where the scaled value came
                                     //       to 0; a negative result stays negative). SpecialEmitter1 reads the four as the particles per step of the radius, one per life
                                     //       stage (the same quarters)
    int8_t mAddVelocity;             // 0x6c  nonzero: mVelocity is added to each particle's velocity
    int8_t mRandomSpin;              // 0x6d  nonzero: a particle gets a random spin (its mSpin)
    uint8_t mF6E;                    // 0x6e
    int8_t mAccelerates;             // 0x6f  nonzero: DoAParticle adds a random part of mAccel to a particle's velocity each tick (PAR_InitEmitter sets it: 1 when mAccel is not all 0)
    uint32_t mParticleFlags;         // 0x70  the drawing list's flags word (PAR_BuildDrawingList adds 0x4000, or 0x54000 for a 2D emitter)
    int16_t mSizeStart;              // 0x74  a particle's size when it starts, copied into its mSize (DoAParticle fades it to mSizeEnd)
    int16_t mBounce;                 // 0x76  nonzero: MoveEmitter bounces the emitter off the ground the height callback answers
    int32_t mLifeBase;               // 0x78  the particles' life; above 0, their size and colour follow it, else they keep the first
    int32_t mParticleDrag;           // 0x7c  DoAParticle: the share (1/1024) of a particle's velocity lost each tick
    int32_t mParticleGravity;        // 0x80  DoAParticle: taken off a particle's vertical velocity each tick
    int32_t mAccel[3];               // 0x84  with mAccelerates: the range of the random velocity added each tick
    uint8_t mF90[4];                 // 0x90
    int16_t mSprite;                 // 0x94  the sprite the particles use (ESP_GetParticleSprite's first argument)
    int16_t mSpriteFrames;           // 0x96  its frames (0: untextured); a particle's frame runs through them as its life goes
    int32_t mDeathType;              // 0x98  the library emitter to start where a particle ends (-1: none)
    int32_t mNotAttracted;           // 0x9c  nonzero: a DoEffecters sphere without a group leaves its particles alone
    int16_t mUnsignedY;              // 0xa0  nonzero: the random y offset is 0 to 32767, else -32768 to 32767
    int16_t mScale;                  // 0xa2  PAR_SetEmitterScale (PAR_LoadLibrary gives a definition that has none 1000); PAR_BuildDrawingList: 1000 as it is, else the particles' distance from the emitter times this / 1000
    uint8_t mFA4[2];                 // 0xa4
    int16_t mSizeEnd;                // 0xa6  a particle's size at the end of its life (with mLifeBase above 0)
    int16_t mShell;                  // 0xa8  nonzero: the x offset is a cosine of a random angle (z the sine), a disc
    int8_t mDieWithEmitter;          // 0xaa  nonzero: once the emitter is ending its particles end too
    int8_t mLinkedChild;             // 0xab  nonzero: started as the linked child of another, DoEmitters does not move it (PAR_InitEmitter clears it at the start)
    int32_t mTurn;                   // 0xac  the angle (1/4096 turn) the offset is turned by about the vertical axis
    int32_t mLaunchScale;            // 0xb0  the factor PAR_SetEmitterLaunchVector applies
    int32_t mLinked;                 // 0xb4  the effecter or emitter that starts with it: the library number before the start, its handle after (-1: none)
    int16_t mLinkFollow;             // 0xb8  nonzero: the linked one starts at an offset (its own at +0x2c or +0x18) and is marked as linked, and DoEmitters moves it to this emitter's position each tick
    int8_t mLinkIsEffecter;          // 0xba  nonzero: mLinked is an effecter
    int8_t mEndIsEffecter;           // 0xbb  nonzero: mEndType is an effecter
    int16_t mKillLinked;             // 0xbc  nonzero: the linked one is ended when this one is
    int16_t mSpinMin;                // 0xbe  with mSpinMax: the turn a particle takes each tick (1/65536 turn; equal: exactly that)
    int8_t mKeepDensity;             // 0xc0  nonzero: ApplyParticleDensityAdjustment is not called for it
    int8_t mLowSpecSkip;             // 0xc1  nonzero: not made while the state is in low-spec mode (the library's byte is the one tested)
    int8_t mBox;                     // 0xc2  nonzero: the offsets are three random numbers (a box), else a disc or ring
    uint8_t mFC3;                    // 0xc3
    int16_t mOffsetRadius;           // 0xc4  an offset at mOffsetAngle added to the position
    int16_t mSpinMax;                // 0xc6
    int32_t mOffsetSpin;             // 0xc8  turned into mOffsetAngle each tick
    int32_t mRadius;                 // 0xcc  SpecialEmitter1: the ring's radius, grown by mVelSpread a tick
    int16_t mNext;                   // 0xd0  the next emitter of its chain (the free chain, or the emitters in use), -1 at the end
    int16_t mPrev;                   // 0xd2  the one before, -1 at the start
    int32_t mLifeAtStart;            // 0xd4  mLife as it was at the start (DoEmitters picks the rate by the quarter of it left; SpecialEmitter1 compares them)
    int32_t mColours[16];            // 0xd8  the 16 colour words
    uint8_t mF118[0x128 - 0x118];    // 0x118
    int32_t mBoxMin[3];              // 0x128 the low corner of its bounding box (DoEmitters starts it again each tick, particles widen it; PAR_GetEmitterBoundingBox
                                     //       hands it out shifted left by 4)
    int32_t mBoxMax[3];              // 0x134 the high corner
};
static_assert(sizeof(PAREmitter) == 0x140, "an emitter record is 0x140 bytes");
static_assert(__builtin_offsetof(PAREmitter, mPaused) == 0x02 && __builtin_offsetof(PAREmitter, mFirstParticle) == 0x06 &&
                  __builtin_offsetof(PAREmitter, mLaunch) == 0x38 && __builtin_offsetof(PAREmitter, mRandomPos) == 0x44 &&
                  __builtin_offsetof(PAREmitter, mStartCount) == 0x60 && __builtin_offsetof(PAREmitter, mLaunchScale) == 0xb0 &&
                  __builtin_offsetof(PAREmitter, mSerial) == 0x0a && __builtin_offsetof(PAREmitter, mEndType) == 0x10 &&
                  __builtin_offsetof(PAREmitter, mPos) == 0x14 && __builtin_offsetof(PAREmitter, mLife) == 0x20 &&
                  __builtin_offsetof(PAREmitter, mDrag) == 0x24 && __builtin_offsetof(PAREmitter, mVelocity) == 0x2c &&
                  __builtin_offsetof(PAREmitter, mOffsetAngle) == 0x54 && __builtin_offsetof(PAREmitter, mMaxParticles) == 0x64 &&
                  __builtin_offsetof(PAREmitter, mSpawnRate) == 0x68 && __builtin_offsetof(PAREmitter, mAccelerates) == 0x6f &&
                  __builtin_offsetof(PAREmitter, mParticleFlags) == 0x70 && __builtin_offsetof(PAREmitter, mSizeStart) == 0x74 &&
                  __builtin_offsetof(PAREmitter, mLifeBase) == 0x78 && __builtin_offsetof(PAREmitter, mAccel) == 0x84 &&
                  __builtin_offsetof(PAREmitter, mSprite) == 0x94 && __builtin_offsetof(PAREmitter, mDeathType) == 0x98 &&
                  __builtin_offsetof(PAREmitter, mNotAttracted) == 0x9c && __builtin_offsetof(PAREmitter, mScale) == 0xa2 &&
                  __builtin_offsetof(PAREmitter, mSizeEnd) == 0xa6 && __builtin_offsetof(PAREmitter, mDieWithEmitter) == 0xaa &&
                  __builtin_offsetof(PAREmitter, mLinked) == 0xb4 && __builtin_offsetof(PAREmitter, mKillLinked) == 0xbc &&
                  __builtin_offsetof(PAREmitter, mSpinMin) == 0xbe && __builtin_offsetof(PAREmitter, mSpinMax) == 0xc6 &&
                  __builtin_offsetof(PAREmitter, mOffsetSpin) == 0xc8 && __builtin_offsetof(PAREmitter, mNext) == 0xd0 &&
                  __builtin_offsetof(PAREmitter, mLifeAtStart) == 0xd4 && __builtin_offsetof(PAREmitter, mColours) == 0xd8 &&
                  __builtin_offsetof(PAREmitter, mBoxMin) == 0x128 && __builtin_offsetof(PAREmitter, mBoxMax) == 0x134,
              "the emitter's named fields are where the functions use them");
static_assert(__builtin_offsetof(PAREmitter, mType) == 0x0c && __builtin_offsetof(PAREmitter, mJitter) == 0x12 &&
                  __builtin_offsetof(PAREmitter, mVelSpread) == 0x50 && __builtin_offsetof(PAREmitter, mSphere) == 0x5c &&
                  __builtin_offsetof(PAREmitter, mAddVelocity) == 0x6c && __builtin_offsetof(PAREmitter, mUnsignedY) == 0xa0 &&
                  __builtin_offsetof(PAREmitter, mShell) == 0xa8 && __builtin_offsetof(PAREmitter, mTurn) == 0xac &&
                  __builtin_offsetof(PAREmitter, mKeepDensity) == 0xc0 && __builtin_offsetof(PAREmitter, mOffsetRadius) == 0xc4 &&
                  __builtin_offsetof(PAREmitter, mRadius) == 0xcc,
              "the spawning functions' fields (unit w26b) are where they use them");
// The fields DoEffecters and MoveEffecter (unit w26c) use are named. An effecter pulls the particles of the emitters towards a point
// (a force field) and can end them.
struct PAREffecter {
    int32_t mInUse;                  // 0x00  cleared when the effecter ends
    int32_t mGroup;                  // 0x04  the particles it acts on: -1 all (leaving out those of an emitter with mNotAttracted),
                                     //       else the group (emitter byte +0x04 and particle mKind both match it)
    uint16_t mSerial;                // 0x08  the upper half of its handle (PAR_GetEffecterStatus compares it)
    uint8_t mLinked;                 // 0x0a  nonzero: started as the linked child of an emitter (the emitter that started it sets it when it has an offset link), DoEffecters does not move it
    int8_t mBounce;                  // 0x0b  nonzero: MoveEffecter bounces it off the ground the height callback answers
    int32_t mPos[3];                 // 0x0c  the position (PAR_InitEffecter: the caller's x, y, z shifted right by 4)
    int32_t mVel[3];                 // 0x18  the velocity, added to mPos by MoveEffecter
    int32_t mDrag;                   // 0x24  the share (1/1024) of the velocity lost each tick
    int32_t mGravity;                // 0x28  taken off the vertical velocity each tick
    int32_t mRadius;                 // 0x2c  the reach of mode 2 (DoEffecters makes it 1 when it is 0)
    int32_t mMode;                   // 0x30  2: a particle nearer than mRadius ends
    int32_t mSpread[3];              // 0x34  the range of the random offset of the point, drawn again each tick
    int32_t mStrength;               // 0x40  the pull
    uint8_t mF44;                    // 0x44
    int8_t m2D;                      // 0x45  nonzero for an interface effecter (PAR_KillAndHideAll2DObjects ends it)
    int16_t mEndType;                // 0x46  the library emitter (or effecter) to start when this one ends
    uint8_t mF48;                    // 0x48
    int8_t mEndIsEffecter;           // 0x49  nonzero: mEndType is an effecter (set to -1 when it has ended)
    int16_t mNoExpire;               // 0x4a  nonzero: mLife does not count down
    int32_t mLife;                   // 0x4c  counts down; below 0 the effecter ends (-2 asks DoEffecters to end it)
    uint8_t mF50[0x60 - 0x50];       // 0x50
    int16_t mNext;                   // 0x60  the next effecter of its chain, -1 at the end
    int16_t mPrev;                   // 0x62  the one before, -1 at the start
    uint8_t mF64[4];
};
static_assert(sizeof(PAREffecter) == 0x68, "an effecter record is 0x68 bytes");
static_assert(__builtin_offsetof(PAREffecter, mGroup) == 0x04 && __builtin_offsetof(PAREffecter, mPos) == 0x0c &&
                  __builtin_offsetof(PAREffecter, mVel) == 0x18 && __builtin_offsetof(PAREffecter, mRadius) == 0x2c &&
                  __builtin_offsetof(PAREffecter, mSpread) == 0x34 && __builtin_offsetof(PAREffecter, mStrength) == 0x40 &&
                  __builtin_offsetof(PAREffecter, mEndType) == 0x46 && __builtin_offsetof(PAREffecter, mEndIsEffecter) == 0x49 &&
                  __builtin_offsetof(PAREffecter, mSerial) == 0x08 && __builtin_offsetof(PAREffecter, mLinked) == 0x0a &&
                  __builtin_offsetof(PAREffecter, m2D) == 0x45 &&
                  __builtin_offsetof(PAREffecter, mLife) == 0x4c && __builtin_offsetof(PAREffecter, mNext) == 0x60,
              "the effecter's named fields are where the functions use them");
constexpr int32_t kParEmitters = 120;     // 0x78
constexpr int32_t kParEffecters = 20;     // 0x14
struct PARState {
    int32_t mF00;                    // 0x00  PAR_ResetParticleSystem and PAR_CloseParticleSystem clear it
    int32_t mTicks;                  // 0x04  PAR_Tick counts it up; PAR_ResetParticleSystem clears it
    int32_t mLowSpec;                // 0x08  PAR_SetLowSpec
    int32_t mMaxParticles;           // 0x0c  the pool's length (100 to 0x7fff)
    int32_t mF10;                    // 0x10  1; the particle system's random seed (x * 0x343fd + 0x269ec3, DoAParticle, DoEffecters)
    int32_t mF14;                    // 0x14  1
    int16_t mF18;                    // 0x18  1
    int16_t mF1A;                    // 0x1a  1
    const int32_t* mSinTable;        // 0x1c  gParSinTable
    int32_t mDensity;                // 0x20  the particle density (1024 = full; 0 became 1)
    void* mHeightCallback;           // 0x24  the function that answers the ground height (x, z): the owner argument of
                                     //       PAR_InitParticleSystem, or DummyHeightCallBack
    tagPARTICLE* mParticles;         // 0x28  the pool
    PAREmitter mEmitters[kParEmitters];     // 0x2c on the Mac
    PAREffecter mEffecters[kParEffecters];  // 0x962c on the Mac
    int16_t mEmittersUsed;           // 0x9e4c  the first emitter in use (-1: none)
    int16_t mEffectersUsed;          // 0x9e4e  the first effecter in use (-1: none)
    int16_t mEmittersFree;           // 0x9e50  the first free emitter
    int16_t mEffectersFree;          // 0x9e52  the first free effecter
    int16_t mParticlesFree;          // 0x9e54  the first free particle
    uint8_t mF9E56[0x12];            // 0x9e56
};
static_assert(__builtin_offsetof(PARState, mEmittersUsed) - __builtin_offsetof(PARState, mEmitters) == 0x9e4c - 0x2c,
              "the chain heads follow the emitters and effecters as on the Mac");
static_assert(__builtin_offsetof(PARState, mF9E56) + sizeof(PARState::mF9E56) - __builtin_offsetof(PARState, mEmitters) ==
                  0x9e68 - 0x2c,
              "the state ends 0x9e68 bytes in");
// One emitter definition of the library, 0x140 bytes as the file has them: little-endian fields, which the Mac swaps one
// by one (the names say the width the loader swaps: S signed halfword, U unsigned halfword, L word; F is a run of bytes it
// leaves alone). The meaning of the fields is not known.
struct PARLibraryEmitter {
    int16_t mS00;                // 0x00  cleared after loading (the library entry is not an emitter in use)
    int16_t mS02;                // 0x02
    uint8_t mF04[2];             // 0x04
    int16_t mS06;                // 0x06
    uint16_t mU08;               // 0x08
    uint16_t mU0A;               // 0x0a
    int16_t mS0C;                // 0x0c
    int16_t mS0E;                // 0x0e
    int16_t mS10;                // 0x10
    int16_t mS12;                // 0x12
    int32_t mL14;                // 0x14
    int32_t mL18;                // 0x18
    int32_t mL1C;                // 0x1c
    int32_t mL20;                // 0x20
    int16_t mS24;                // 0x24
    int16_t mS26;                // 0x26
    int16_t mS28;                // 0x28
    int16_t mS2A;                // 0x2a
    int32_t mL2C;                // 0x2c
    int32_t mL30;                // 0x30
    int32_t mL34;                // 0x34
    int32_t mL38;                // 0x38
    int32_t mL3C;                // 0x3c
    int32_t mL40;                // 0x40
    int32_t mL44;                // 0x44
    int32_t mL48;                // 0x48
    int32_t mL4C;                // 0x4c
    int16_t mS50;                // 0x50
    int16_t mS52;                // 0x52
    int32_t mL54;                // 0x54
    int32_t mL58;                // 0x58
    int32_t mL5C;                // 0x5c
    int32_t mL60;                // 0x60
    int16_t mS64;                // 0x64
    int16_t mS66;                // 0x66
    uint8_t mF68[8];             // 0x68
    int32_t mL70;                // 0x70
    int16_t mS74;                // 0x74
    int16_t mS76;                // 0x76
    int32_t mL78;                // 0x78
    int32_t mL7C;                // 0x7c
    int32_t mL80;                // 0x80
    int32_t mL84;                // 0x84
    int32_t mL88;                // 0x88
    int32_t mL8C;                // 0x8c
    int32_t mL90;                // 0x90
    int16_t mS94;                // 0x94
    int16_t mS96;                // 0x96
    int32_t mL98;                // 0x98
    int32_t mL9C;                // 0x9c
    int16_t mSA0;                // 0xa0
    int16_t mSA2;                // 0xa2  1000 after loading when the file has 0
    int16_t mSA4;                // 0xa4
    int16_t mSA6;                // 0xa6
    int16_t mSA8;                // 0xa8
    uint8_t mFAA[2];             // 0xaa
    int32_t mLAC;                // 0xac
    int32_t mLB0;                // 0xb0
    int32_t mLB4;                // 0xb4
    int16_t mSB8;                // 0xb8
    uint8_t mFBA[2];             // 0xba
    int16_t mSBC;                // 0xbc
    int16_t mSBE;                // 0xbe
    uint8_t mFC0[4];             // 0xc0
    int16_t mSC4;                // 0xc4
    int16_t mSC6;                // 0xc6
    int32_t mLC8;                // 0xc8
    int32_t mLCC;                // 0xcc
    int16_t mSD0;                // 0xd0  the chain links of a live emitter (PAREmitter::mNext, mPrev)
    int16_t mSD2;                // 0xd2
    int32_t mLD4;                // 0xd4
    int32_t mLD8[16];            // 0xd8  swapped as a block
    union {
        uint8_t mF118[16];       // 0x118
        char mName[16];          // 0x118 the effect's name ("Sparks", "Key"); empty: the library slot is free (PAR_LoadLibEmitter
                                 //       takes the first one, PAR_KillLibEmitter empties one)
    };
    int32_t mL128;               // 0x128
    int32_t mL12C;               // 0x12c
    int32_t mL130;               // 0x130
    int32_t mL134;               // 0x134
    int32_t mL138;               // 0x138
    int32_t mL13C;               // 0x13c
};
static_assert(sizeof(PARLibraryEmitter) == 0x140, "a library emitter is 0x140 bytes");
static_assert(__builtin_offsetof(PARLibraryEmitter, mSA2) == 0xa2 && __builtin_offsetof(PARLibraryEmitter, mLD8) == 0xd8 &&
                  __builtin_offsetof(PARLibraryEmitter, mL13C) == 0x13c,
              "the library emitter's fields are where the loader swaps them");
// One effecter definition of the library, 0x68 bytes (same naming).
struct PARLibraryEffecter {
    int32_t mL00;                // 0x00  cleared after loading
    int32_t mL04;                // 0x04
    uint16_t mU08;               // 0x08
    uint8_t mF0A[2];             // 0x0a
    int32_t mL0C;                // 0x0c
    int32_t mL10;                // 0x10
    int32_t mL14;                // 0x14
    int32_t mL18;                // 0x18
    int32_t mL1C;                // 0x1c
    int32_t mL20;                // 0x20
    int32_t mL24;                // 0x24
    int32_t mL28;                // 0x28
    int32_t mL2C;                // 0x2c
    int32_t mL30;                // 0x30
    int32_t mL34;                // 0x34
    int32_t mL38;                // 0x38
    int32_t mL3C;                // 0x3c
    int32_t mL40;                // 0x40
    uint8_t mF44[2];             // 0x44
    int16_t mS46;                // 0x46
    uint8_t mF48[2];             // 0x48
    int16_t mS4A;                // 0x4a
    int32_t mL4C;                // 0x4c
    uint8_t mF50[16];            // 0x50
    int16_t mS60;                // 0x60  the chain links of a live effecter (PAREffecter::mNext, mPrev)
    int16_t mS62;                // 0x62
    uint8_t mF64[4];             // 0x64
};
static_assert(sizeof(PARLibraryEffecter) == 0x68, "a library effecter is 0x68 bytes");
constexpr int32_t kParLibraryEmitters = 0x69;    // 105
constexpr int32_t kParLibraryEffecters = 0x14;   // 20
struct PARLibrary {
    PARLibraryEmitter mEmitters[kParLibraryEmitters];     // 0x0000
    PARLibraryEffecter mEffecters[kParLibraryEffecters];  // 0x8340
};
static_assert(sizeof(PARLibrary) == 0x8b60, "the library is 0x8b60 bytes");
extern PARState gParState;                 // 0x102edd68
extern PARLibrary gParLibrary;             // 0x102e5208
extern int32_t gParSinTable[512];          // 0x102e49f4  256 * sin(2 pi i / 512), with pi as 3.14159 (PAR_SetupSin)
extern uint32_t gParLibraryWordB;          // 0x102e51f4  the library file's last word (1024 in Tp2.plb)
extern uint32_t gParLibraryWordA;          // 0x102e51f8  the one before, kept in 10 to 500 (33 in Tp2.plb)
extern int32_t gParSystemRunning;          // 0x102e51fc: 1 once PAR_InitParticleSystem has run
extern int32_t gParDrawingListBusy;        // 0x102e5200  PAR_BuildDrawingList sets it, PAR_FreeDrawingList clears it
extern PTCL* gParDrawingList;              // 0x102e5204  the drawing list's block: 100 records (0xaf0 bytes of 0x1c on the Mac; the host's
                                           //             PTCL has a native pointer, so PAR_InitParticleSystem sizes it by sizeof(PTCL))
constexpr int32_t kParDrawingListRecords = 100;   // what the block holds; PAR_BuildDrawingList uses it for lists of up to this many
// The state's emitters as PAR_GetEmitterParticleCount reads them: the address arithmetic of the original takes any
// number below 126, which runs on past the 120 emitters into the effecters. A view of gParState.mEmitters
// (0x102edd94 on the Mac, the block's address plus 0x2c).
extern PAREmitter (&gParEmitters)[126];
// The helpers of the main app's particle loader (the main app's own instances of the SamsUtilities templates:
// 0x100babb4, 0x100bab6c, 0x100bab28 and 0x100baacc). They put a file's little-endian field in the host's order in place:
// nothing to do on a little-endian host.
namespace SamsUtilities {
void USwapInplace16(int16_t& value);                  // 0x100babb4 (USwapInplace16<short>)
void USwapInplace16(uint16_t& value);                 // 0x100bab6c (USwapInplace16<unsigned short>)
void USwapInplace32(int32_t& value);                  // 0x100bab28 (USwapInplace32<long>)
void USwapBlock32(int32_t* words, int32_t count);     // 0x100baacc (USwapBlock32<long>): _USwapBlock32 on `count` words
}  // namespace SamsUtilities
// ---- Unit w168 (src/game/Particles.cpp): the particle system's save and load and their swap helpers
namespace SamsUtilities {
void USwapBlockStruct(PARLibraryEmitter* records, uint32_t count);    // 0x100bb3b8 (USwapBlockStruct<tagEMITTER>)
void USwapBlockStruct(PARLibraryEffecter* records, uint32_t count);   // 0x100bb260 (USwapBlockStruct<tagEFFECTER>)
uint32_t USwap32ParticlePtr(uint32_t word);                           // 0x100bb6ac (USwap32<tagPARTICLE*>)
uint32_t USwap32VoidPtr(uint32_t word);                               // 0x100bb720 (USwap32<void*>)
uint32_t USwap32LongPtr(uint32_t word);                               // 0x100bb77c (USwap32<long*>)
}  // namespace SamsUtilities
// 0x100bacac (1084 bytes): writes "PTCL", the running flag and, while the system runs, the state, the library and (with `withParticles`) the pool,
// little-endian; answers 1 when a block cannot be allocated, else 0.
int32_t PAR_SaveStatus(void* file, int32_t withParticles);
// 0x100bb7d8 (1108 bytes): the reverse; answers 1 when the state's or the library's size word is not the one it knows, else 0.
int32_t PAR_LoadStatus(void* file);
// 0x100ba1e0 (108 bytes, src/game/Particles.cpp): while the system runs, writes the emitter's box times 16 (min.x, y, z, then max.x, y, z).
// There is no range or in-use check: any index below 126 reads a record of the state (a live or dead emitter, or the effecters after the
// 120th); the original indexes with all 32 bits.
void PAR_GetEmitterBoundingBox(uint32_t emitter, PARPOINT3D* min, PARPOINT3D* max);
// 0x100ba298 (92 bytes): while the system runs and `list` is not NULL: the system's own block (gParDrawingList) is marked free
// (gParDrawingListBusy 0), any other block is freed with PAR_Free.
void PAR_FreeDrawingList(PTCL* list);
// 0x100ba3e0 (1,732 bytes, src/game/Particles.cpp): loads the particle library `path` (Data:Particle:Tp2.plb): a count and a
// record size, that many records of 0x140 bytes (the first 105; a longer list is skipped over, by (count - 20) records of the
// size, where the original means (count - 105): its bug), a count and a size and the effecter records (the first 20, the
// same skip), then two words (the first kept in 10 to 500). The records are put in the host's order field by field. Then
// every emitter definition is marked not in use (and a zero 0xa2 halfword becomes 1000), every effecter definition's first
// word is cleared and the system is reset. The original answers 0 when it loaded and 1 when it could not (no path, the system
// is not running (and then it asserts), no such file); the declaration stays void because the callers' units fake it as
// void, and PAR_LoadLibraryResult is the translation.
void PAR_LoadLibrary(char* path);
int32_t PAR_LoadLibraryResult(char* path);                          // 0x100ba3e0
// 0x100bdc24 (272 bytes, src/game/Particles.cpp): once only (while gParSystemRunning is 0): clears the state, sets
// the density (0 becomes 1) and the maximum number of particles (kept in 100 to 0x7fff), makes the table of sines, takes
// `owner` (the function that answers the ground height; NULL: DummyHeightCallBack) and the pool of particles, marks the
// system running, resets it and makes the drawing list's block.
void PAR_InitParticleSystem(void* owner, int32_t density, int32_t maxParticles);
// 0x100bdd64 (496 bytes): when the system runs: starts the state's counters again, clears the pool, the emitters and the
// effecters and chains each as a free list.
void PAR_ResetParticleSystem();
int32_t* PAR_SetupSin();                                            // 0x100ba324 (152 bytes): fills gParSinTable, answers it
void* PAR_Malloc(int32_t size);                                     // 0x100babf8 (68 bytes): LbMem_Alloc; 0 asserts and answers NULL
int32_t DummyHeightCallBack(int32_t x, int32_t z);                  // 0x100bdbf0 (8 bytes): 0
// 0x100bdf84 (136 bytes, src/game/Particles.cpp): while the system runs: clears the state's first word, frees the pool, marks the
// system not running, frees the drawing list's block and asserts when gParAllocBalance is not 0. Nothing else is cleared: the state's
// pool and the drawing list's pointers stay as they were.
void PAR_CloseParticleSystem();
void PAR_SetLowSpec(int32_t on);                                    // 0x100be03c (12 bytes): the state's low-spec word
// ---- the rest of the particle system's core (src/game/Particles.cpp, unit w26a)
// The word before the table of sines (0x102e49f0), which only PAR_CloseParticleSystem (asserts when it is not 0) and PAR_Tick (answers
// it) read: its one table-of-contents slot (0x102610ec) is loaded by those two functions and nothing else, so it stays 0.
extern int32_t gParAllocBalance;
void PAR_Free(void* block);                                         // 0x100bac60 (44 bytes): LbMem_Free unless NULL
// 0x100bbc54 (736 bytes): reads one emitter definition (0x140 bytes) into the first library slot whose name is empty and puts it in
// the host's order (PAR_SwapLibraryEmitter); answers the slot's number, or -1 when the system is not running or no slot is free.
// Unlike PAR_LoadLibrary it does not clear the definition's first halfword or default a zero scale to 1000.
int32_t PAR_LoadLibEmitter(bfl::FileSystem::CFile* file);
// The loader's field-by-field swap of one library emitter (src/game/Particles.cpp). The original has it inline in PAR_LoadLibrary
// and in PAR_LoadLibEmitter, in the same order.
void PAR_SwapLibraryEmitter(PARLibraryEmitter& emitter);
void PAR_KillLibEmitter(uint32_t slot);                             // 0x100bbf78 (48 bytes): empties the name of a library slot below 105
void ApplyParticleDensityAdjustment(int32_t emitter);               // 0x100be654 (344 bytes): scales the emitter's counts by the density
// An emitter's or effecter's handle: the index in the low half and the serial (the emitter's mSerial) in the high half.
// 1 when the system runs, the index is in range and the serial matches; 0 otherwise. The emitter's answer is 0 for the handle 0.
int32_t PAR_GetEmitterStatus(uint32_t handle);                      // 0x100bf110 (116 bytes)
int32_t PAR_GetEffecterStatus(uint32_t handle);                     // 0x100bf1b0 (104 bytes)
// The setters do nothing unless the handle is a live emitter's (PAR_GetEmitterStatus).
void PAR_KillEmitter(int32_t handle);                               // 0x100be1e8 (80 bytes): life -2
void PAR_KillAndHideAll2DObjects();                                 // 0x100be494 (156 bytes)
void PAR_SetEmitterLaunchVector(int32_t handle, int32_t x, int32_t y, int32_t z);   // 0x100be070 (172 bytes)
void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z);            // 0x100be150 (108 bytes)
void PAR_SetEmitterMask(int32_t handle, int32_t mask);              // 0x100be260 (92 bytes)
void PAR_SetEmitterLife(int32_t handle, int32_t life);              // 0x100be2e8 (88 bytes)
void PAR_SetEmitterScale(int32_t handle, int32_t scale);            // 0x100be36c (92 bytes)
void PAR_SetEmitterRandomPos(int32_t handle, int32_t x, int32_t y, int32_t z);      // 0x100be3f4 (108 bytes)
// 0x100be564 (208 bytes, src/game/Particles.cpp): while the system runs, counts a tick; from the second one on it has DoEmitters and
// DoEffecters work and then DoAParticle on every particle of every emitter that is not paused (the next particle is read before
// the call, since DoAParticle may free the one it is given). The original leaves the word at 0x102e49f0 in r3 (gParAllocBalance);
// PAR_TickResult answers it and PAR_Tick, which the callers declare void, drops it.
int32_t PAR_TickResult();
void PAR_Tick();
// ---- the particle system's tick and drawing list, and what they call (src/game/Particles.cpp, unit w26c)
// The emitters' turn, the effecters' turn and one particle's: PAR_Tick calls them (see PAR_Tick).
// 0x100bcbb4 (1,108 bytes): for each emitter of the used chain that is not paused: counts its life down, restarts its bounding box
// (unless 2D), and while it is alive follows a linked emitter or effecter, turns its offset angle, moves it, and makes its
// particles (SpawnParticle at the rate for the quarter of the life left, or DoSpecialEmitter); when it is ending (life below 0) it
// starts its end emitter or effecter once, ends a linked one, and once it has no particles left it goes back to the free chain.
void DoEmitters();                                                  // 0x100bcbb4
// 0x100bd1f0 (1,064 bytes): for each effecter of the used chain: counts its life down, moves it, and while it is alive pulls the
// velocity of the particles it acts on towards its (randomly offset) point, and in mode 2 ends those inside its radius; when it is
// ending it starts its end emitter or effecter and goes back to the free chain.
void DoEffecters();                                                 // 0x100bd1f0
// 0x100bd63c (1,424 bytes): one particle's turn: its life counts down; when it ends the emitter loses a particle (and the system
// one), the emitter named by mDeathType starts where it was, and the particle goes back to the free chain. Otherwise it picks its
// colour, frame, size and turn, takes a random part of the emitter's mAccel, loses velocity to drag and gravity, moves (unless it is
// just made), and widens the emitter's bounding box.
void DoAParticle(int32_t particle);                                 // 0x100bd63c
// 0x100bc9f0 (404 bytes) and 0x100bd02c (404 bytes): move an emitter or an effecter by its velocity after drag and gravity, and
// with the bounce flag throw it back up off the ground the state's height callback answers (the callback is called with the
// position shifted left by 4).
void MoveEmitter(PAREmitter* emitter);                              // 0x100bc9f0
void MoveEffecter(PAREffecter* effecter);                           // 0x100bd02c
// What those call of the spawning and core units (w26a, w26b; the unit's fakes stand in for them) is declared with them; the sprite
// lookup below is a main-app function that is still a stand-in.
struct PTCLTexture;
// 0x1007cefc (HLSprite.cpp range, untranslated): the sprite record of particle sprite `sprite` at animation frame `frame`
// (`variant` is 0 here; the original masks it with 7 for sprites that vary).
PTCLTexture* ESP_GetParticleSprite(int32_t sprite, int32_t frame, int32_t variant);
// ---- the particle system's spawning (src/game/Particles.cpp, unit w26b)
// 0x100bec30 (1,204 bytes): starts an emitter of the particle library (type below 105) at (a, b, c) = (x, y, z) in 1/1024 world unit (the
// callers multiply by 1024.0; the emitter keeps them shifted right by 4, in 1/64). The answer is its handle (the emitter's serial in the
// high half, its number in the low half), 0xffffffff when the system is not running, the type is out of range or no emitter is free, and
// 0 in low-spec mode for a definition marked to be left out.
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c);       // 0x100bec30
// 0x100be7e4 (1,048 bytes): the same, with a launch vector (vx, vy, vz) that becomes the emitter's velocity (times its launch scale over 1024)
// and no random offset of the velocity.
uint32_t PAR_InitEmitterLaunch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz);   // 0x100be7e4
// 0x100bf248 (440 bytes): starts an effecter of the library (type below 20) at (x, y, z) (1/1024 world unit, kept shifted right by 4); the
// answer is its handle as for an emitter (0 in low-spec mode for a definition marked to be left out, 0xffffffff for none).
uint32_t PAR_InitEffecter(int32_t type, int32_t x, int32_t y, int32_t z);      // 0x100bf248
// 0x100bbfd4 (2,552 bytes): makes one particle for the emitter `emitter` (a number, 0 to 119) from the free chain and answers its number in
// the pool, or -1 when the pool is empty.
int32_t SpawnParticle(int32_t emitter);                                        // 0x100bbfd4
// 0x100bf42c (60 bytes): SpecialEmitter1 when the emitter's mSpecial is 1, else nothing.
void DoSpecialEmitter(int32_t emitter, int32_t start);                         // 0x100bf42c
// 0x100bf490 (780 bytes): the expanding ring. `start` nonzero: marks the emitter in use and clears its radius. Else grows the radius by the
// emitter's mVelSpread and, while it is in use, spawns a ring of particles at that radius.
void SpecialEmitter1(int32_t emitter, int32_t start);                          // 0x100bf490
void TickRideAnims();                                               // 0x100c6964
// 0x100c74d4 (600 bytes, src/game/rides/RideAnim.cpp): moves a set's animations on by the frame's time. The
// original returns 1 (0 for no set) and no caller reads it, so the declaration stays void.
// `a` goes to DoAnimateRideSet (nonzero: the nodes are animated); `flags` goes to SetCurRideAnim as
// the loop word, and with bit 3 (8) the embedded data waits for a header that animates (flag 4).
void AnimateRideSet(tag_sRideAnimSet* set, int32_t a, uint32_t flags);
struct sAnimTimeControl;
struct tag_sAnimHeader;
struct tag_sNode;
struct tag_sM3D2Header;
// What AnimateRideSet calls (TickRideAnimSet and DoAnimateRideSet are in src/game/rides/RideAnim.cpp, the
// others in src/game/rides/RideAnim.cpp or still untranslated; the unit's fakes stand in for them).
// 0x100c7a74 (src/game/rides/RideAnim.cpp): the animation queued behind a time control's playing one (kind 12: none; its
// place in the kind); either pointer may be NULL.
void GetDeferredAnim(tag_sRideAnimSet* set, uint32_t* kind, uint32_t* index, uint32_t control);   // 0x100c7a74
// 0x100c7bb4 (src/game/rides/RideAnim.cpp): the length in milliseconds (frames * 1000 / 30) of animation `index` of the
// group `kind`; 0 when the group has no entries (flags 1 or 2), has fewer, or is kind 12.
uint32_t GetAnimLength(tag_sRideAnimSet* set, uint32_t kind, uint32_t index);                  // 0x100c7bb4
int32_t TickRideAnimSet(tag_sRideAnimSet* set);                                       // 0x100c6a20
void DoAnimateRideSet(tag_sRideAnimSet* set, sAnimTimeControl* control, int32_t a);   // 0x100c6c2c
// 0x100c5054 (1,068 bytes, src/game/rides/RideAnim.cpp): takes the set's mesh out of the state `animation` (the one that
// has been playing) left it in, before `other` (the one that takes over, may be NULL) starts: the nodes it animated
// and disabled lose their animation flags and are shown again, and the nodes it animated that `other` does not get
// their matrices, vertices, boxes and texture coordinates back from the type's mesh (the set's mSelf).
void ResetRideAnimationSingleChannel(tag_sRideAnimSet* set, tag_sAnimHeader* animation, tag_sAnimHeader* other);   // 0x100c5054
// 0x100c54ec (1,372 bytes, src/game/rides/RideAnim.cpp): restores all channels in two ordered passes,
// then reapplies each current group animation's node flags.
void ResetRideAnimationAllChannels(tag_sRideAnimSet* set);
// 0x100c7224 (432 bytes, src/game/rides/RideAnim.cpp): gives each of the node's faces (mFaces, mF5C of them) its normal in mPoints
// (the face's index >> 1): the cross product of the edge from the vertex of the face's second slot to the vertex of its
// third and the edge from the same vertex to the first slot's (the vertices are found through the node's remap table
// mF94), made unit length; a face whose normal comes out as a NaN (a face with no area) gets (0, -1, 0). Clears flag
// 0x10000 of the node.
void RecalculateInvalidNodeFaceNormals(tag_sNode* node);                              // 0x100c7224
// 0x100c7418 (116 bytes, same file): RecalculateInvalidNodeFaceNormals for each of the header's nodes (mNodeCount, the
// full ones) that has flag 0x10000, then clears flag 0x40000 of the header.
void RecalculateInvalidFaceNormals(tag_sM3D2Header* header);                          // 0x100c7418
// 0x100c5d3c (60 bytes, src/game/rides/RideAnim.cpp): 1 when `kind` (12 is none) has entries (the group's flags 1 or 2)
// and `index` is below their count.
int32_t IsValidAnimation(tag_sRideAnimSet*& set, uint32_t a, uint32_t b);             // 0x100c5d3c
void SetWorldBoundingArea(tag_sM3D2Header* a, tag_sM3D2Header* b);                    // 0x100b9180
// 0x100c6670 (692 bytes, src/game/rides/RideAnim.cpp): starts animation `anim` of kind `a` on time control `b` of the set,
// or, when the control is playing an animation that has not run out and neither the control nor `loop` (bit 1: now)
// forbids it, queues it behind the one playing (kind 0xd and 0xe, the two special kinds, are never queued). `loop` is
// SetNewAnim's flags word. The original returns the time in milliseconds the control's animations will take (r3) and
// three untranslated callers read it, but three unit libraries fake this function as void, so the declaration stays
// void; SetCurRideAnimDuration (below) is the translation and this one only drops its answer.
void SetCurRideAnim(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b);   // 0x100c6670
// 0x100c7930 (136 bytes, src/game/rides/RideAnim.cpp): remaining milliseconds, truncated and saturated
// by CodeWarrior's unsigned conversion; each optional output describes the selected control.
uint32_t GetCurrentAnimStats(tag_sRideAnimSet* set, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t d);
// ---- the animation tick (src/game/rides/RideAnim.cpp, src/game/TPEmbed.cpp, src/game/TPMesh.cpp)
struct sMatrix;
struct sQuatKey;
struct sMorphSet;
struct tag_sShape;
struct tag_sNodeAnim;
struct tag_sAnimHeader;
struct s3DAxis;
struct s3DPoint;
// 0x100c34bc (372 bytes): finds the key of a key array that the control's frame (mF20) lies in.
// `keys` has `count` records whose first word is a 16-bit frame number; `type` gives their size:
// 1 four bytes, 2 0x14 (sQuatKey), 4 0x10 (sScaleKey), any other type answers 0. With a key at or
// before the frame it answers 1: *first is the last such key, *second the next one (the first
// key again when `wrap` is nonzero after the last, else the last again with a frame one past it)
// and *fraction how far the frame is from the one to the other. Answers 0 with nothing written
// when no key is at or before the frame.
int32_t GetAnimFrameData(sAnimTimeControl* control, void* keys, uint32_t count, uint32_t type, int32_t wrap,
                         uint32_t* first, uint32_t* second, float* fraction);
// 0x100c3678 (200 bytes): puts each animated texture of the animation on the frame it has reached.
void RAAnimateTextureFrames(tag_sM3D2Header* header, tag_sAnimHeader* animation, sAnimTimeControl* control);
// 0x100c45d4 (2,008 bytes): applies one node animation at the control's frame to its node: the
// flag that hides the node, the morph, the rotation, scale, position and heading, and the
// texture coordinates. Answers 0 when the frame is past the animation's length, else 1.
int32_t AnimateNode(sAnimTimeControl* control, tag_sM3D2Header* header, tag_sAnimHeader* animation, tag_sNode* node,
                    tag_sNodeAnim* nodeAnim);
// What those call (SetNewAnim and RADecompressMorph are in src/game/rides/RideAnim.cpp, the rest in src/game/rides/RideAnim.cpp, src/game/LMatrix.cpp;
// the unit's fakes stand in for them).
// 0x100c60fc (1,208 bytes): puts animation `index` of kind `kind` on the control (kind 0xc: none; 0xd: hold at frame 0;
// 0xe: hold at the end) and brings the control's clock words and the mesh's nodes into the matching state. `flags` bit 1
// is kept in the control (loop), bit 4 in the control's 0x20, bits 4 and 8 leave the mesh as it is
// (ResetRideAnimationSingleChannel is not called), bit 8 skips SetupAnimationFlags. The original returns the length of
// the control's animation in milliseconds (r3); DoAnimateRideSet ignores it, so the declaration stays void (the unit
// libraries of the callers fake it as void) and SetNewAnimDuration (below) is the translation.
void SetNewAnim(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control, uint32_t flags,
                float speed);                                                                // 0x100c60fc
// 0x100c40dc (1,196 bytes, src/game/rides/RideAnim.cpp): decompresses the node's morph at `frame`. `morph` is the node
// animation's compressed morph (the mangled name says sCompressedMorph; sMorphSet is that record). `a` is not used
// (the original compares it with the frame's integer part and then does nothing with the result); `b` nonzero adds
// the points to what the node holds instead of replacing it.
void RADecompressMorph(tag_sNode* node, sMorphSet* morph, float frame, uint32_t a, int32_t b);   // 0x100c40dc
// ---- the node animation's interpolators (src/game/rides/RideAnim.cpp)
struct sQuaternion;
// 0x100c8054 (248 bytes): the orientation between key `index` and the next one (or the same key after
// the last: TPGlobals' word 0x401c holds the track's key count) at `fraction`. TPGlobals' engine flags
// (0x400c) bit 2 picks a plain blend of the two quaternions, else RASphericalLinearInterpolate; the
// result goes through RAQuaternionToLAxis into `out`.
void RACalcOrientationBetweenControlPoints(sQuatKey* keys, uint32_t index, float fraction, s3DAxis* out);
// 0x100c7ca0 (208 bytes): the three axes (rows of a rotation matrix) of a quaternion, which need not
// be a unit one: it works with 2 / (x*x + y*y + z*z + w*w).
void RAQuaternionToLAxis(sQuaternion* quaternion, s3DAxis* out);
// 0x100c7db0 (580 bytes): the quaternion between a and b at t, by the sine table of TPGlobals (an angle
// of acos(dot) turns into a table index with the table's scale and mask). Close quaternions are blended
// linearly; nearly opposite ones go through a quarter turn of a, of which only x, y and z are blended
// (out->w stays a->z, the original's bug).
void RASphericalLinearInterpolate(sQuaternion* a, sQuaternion* b, float t, sQuaternion* out);
// 0x100c37a4 (292 bytes): destination = source * destination for the 3 x 3 part, the source being
// three axes of three floats (the 4 x 4 form at 0x100c28fc is below).
void CombineMatricesNoWDest2nd(s3DAxis* source, sMatrix* destination);
// 0x100c1eac (100 bytes): copies each of the three given axes that is not null into the matrix's rows 0 to 2.
void SetLMatrixOrientation(sMatrix* matrix, s3DPoint* x, s3DPoint* y, s3DPoint* z);
// 0x100c1ab8 (328 bytes): makes each of the matrix's first three rows as long as the scale it is given.
void SetLMatrixScaleNoTrans(sMatrix* matrix, float& x, float& y, float& z);
// 0x100c2ee8 (384 bytes): row 2 becomes the direction, row 0 the unit vector perpendicular to the
// matrix's old row 1 and the direction, and row 1 the direction crossed with that.
void SetLMatrixXZOrientation(sMatrix* matrix, s3DPoint* direction);
// 0x100c8ca8 (356 bytes): the value of a track of `count` values (percentages that wrap round at 100) at
// `frame`, between its two neighbours; `wrap` makes the last neighbour the first value.
void RAInterpPosnAtFrame(float* values, uint32_t count, float frame, float* out, int32_t wrap);
// The five shape interpolators (0x100c84ec 428 bytes, 0x100c86e8 420, 0x100c88e0 152, 0x100c89c8 112,
// 0x100c8a8c 460): the point (Inter) or the direction (Tangent) of a bezier, linear or Catmull-Rom
// shape at `index` and `fraction`. `a` is the array of points (null: the shape's own). The bezier and
// Catmull-Rom ones wrap their point numbers round TPGlobals' word 0x4020 (the shape's point count,
// which the caller sets); the linear ones do not.
void RABezierInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out);
void RABezierTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out);
void RALinearInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out);
void RALinearTangentFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out);
void RACatmulInterFromShape(tag_sShape* shape, s3DPoint* a, uint32_t index, float fraction, s3DPoint* out);
// 0x100c81a0 (232 bytes): the key of a texture coordinate track that the control's frame has reached:
// `frames` are the `count` frame numbers of the track, *index the key before the first one above the
// frame (or count - 2 past the last) and *fraction how far the frame is towards that one.
void RAGetUVFrameData(sAnimTimeControl*& control, uint16_t*& frames, uint32_t count, uint32_t& index, float& fraction);
// 0x100c82d0 (460 bytes): puts each texture coordinate track of the node animation on the control's
// frame in the node's coordinate sets (track k is the set k / 4's lane k % 4: u there, v four lanes on),
// replacing them, or with `additive` nonzero adding to them.
void RAAnimateTextureCoords(sAnimTimeControl* control, tag_sNodeAnim* nodeAnim, int32_t additive);
struct sMatrix;
// The index into the table of axis orders at 0x1029ebdc (src/game/LMatrix.cpp): the string at that index lists the axes to
// turn about, in order, one angle each ("FUK", the entry for 0, has none: every character that is not X, Y or Z is skipped).
enum eRotateParams : int32_t {
    kRotateNone = 0,   // "FUK"
    kRotateX = 1,      // "X"
    kRotateY = 2,      // "Y"
    kRotateZ = 3,      // "Z"
    kRotateXY = 4,
    kRotateYX = 5,
    kRotateYZ = 6,
    kRotateZY = 7,
    kRotateXZ = 8,
    kRotateZX = 9,
    kRotateXYZ = 10,
    kRotateXZY = 11,
    kRotateYXZ = 12,
    kRotateYZX = 13,
    kRotateZXY = 14,
    kRotateZYX = 15,
};
void SetMatrixIdentity(sMatrix* matrix);                                      // 0x100c0e28
// 0x100c0fc8 (52 bytes, src/game/LMatrix.cpp): atan2(-m[1][0], m[1][1]) of the matrix, in double precision rounded to single: the turn about
// the z axis that the matrix's second row makes with the y axis.
float GetLMatrixRotationAroundZAxisYOnly(sMatrix* matrix);
float GetVectorRotationAroundYAxisFromZ(s3DPoint& point);   // 0x100c10b8 (48 bytes, src/game/LMatrix.cpp, unit w170); the pointer form is 0x100c121c below
void LUDecomp(sMatrix& matrix, int32_t* indices, float* parity);                // 0x100c2230
void LUBackSubst(sMatrix& matrix, int32_t* indices, float* values);             // 0x100c210c
void InvertMatrix(sMatrix* destination, const sMatrix* source);                // 0x100c249c (r4 was missed by Ghidra)
struct s3DPoint;
// 0x100c1f64: the point through the matrix's rotation and its translation (row 3), in place.
void VectorThroughLMatrixNoW(sMatrix* matrix, s3DPoint* point);
// 0x100c28fc: destination = source * destination for the 3x3 part and row 3, which also adds
// the destination's own translation; the fourth column of the destination is left alone.
void CombineMatricesNoWDest2nd(sMatrix* source, sMatrix* destination);
// 0x100c2694 (src/game/LMatrix.cpp, 548 bytes): the same product the other way round, destination = destination * source for the 3 x 3 part and
// row 3 (which adds the source's own translation); the fourth column of the destination is left alone. The elements are made row by row
// and each one reads the source again, so a source that is the destination changes under it.
void CombineMatricesNoWDest1st(sMatrix* destination, sMatrix* source);
// 0x100c13bc, 0x100c1698, 0x100c15f0 (src/game/LMatrix.cpp): turn the three rows of the matrix's 3 x 3 part (row 3, the
// translation, is left alone) about an axis, by the angle whose cosine and sine are `c` and `s`; the products are rounded
// to single and each sum is fused with the product before it, as the listings have them.
void XRotateLMatrixNoTrans(sMatrix* matrix, float c, float s);                      // 0x100c13bc
void YRotateLMatrixNoTrans(sMatrix* matrix, float c, float s);                      // 0x100c1698
void ZRotateLMatrixNoTrans(sMatrix* matrix, float c, float s);                      // 0x100c15f0
// 0x100c174c (432 bytes, src/game/LMatrix.cpp): for each X, Y or Z of the order's string (an eRotateParams) takes the next of
// the angles (radians) and turns the matrix about that axis. The sine and cosine come from gTPGlobals' table (entries per
// radian mSinTableScale; the cosine is the entry of angle + pi / 2), the index truncated as fctiwz does and masked.
void RotateLMatrixXYZNoTrans(sMatrix* matrix, eRotateParams axis, float* angle);   // 0x100c174c
// 0x100c1944 (296 bytes, same file): the same walk with the C library's cos and sin of each angle (as doubles, rounded to
// single).
void RotateLMatrixXYZNoTransHi(sMatrix* matrix, eRotateParams axis, float* angle);   // 0x100c1944
// 0x100c1c3c (src/game/LMatrix.cpp): the three rows of the 3 x 3 part times the point's x, y and z; row 3 is left alone.
void ScaleLMatrixNoTrans(sMatrix* matrix, s3DPoint* scale);                    // 0x100c1c3c
// 0x100c1db4 (192 bytes, src/game/LMatrix.cpp, unit w135): the 3 x 3 part from a pitch about X (the C library's sin and cos of the pitch) and
// then a yaw about Y (YRotateLMatrixNoTrans with the cos and sin of the yaw); row 3 is left alone.
void SetLMatrixOrientationHi(sMatrix* matrix, float pitch, float yaw);   // 0x100c1db4
// Unit w170 (src/game/LMatrix.cpp): the matrix and vector helpers of rsse.cpp that had no translation.
void SetLMatrixIdentityNoTrans(sMatrix* matrix);                                   // 0x100c0eac (64 bytes): 1.0 on the diagonal, 0.0 in the rest of the 3 x 4 block; m30..m32 untouched
float GetLMatrixRotationAroundYAxis(sMatrix* matrix);                              // 0x100c0f24 (104 bytes): atan2 by m[2][1] against +-0.9
float GetVectorRotationAroundXAxisFromZ(s3DPoint& point);                          // 0x100c1040 (52 bytes): atan2(-y, z)
float GetVectorRotationAroundZAxisFromY(s3DPoint& point);                          // 0x100c112c (52 bytes): atan2(-x, y)
float GetVectorRotationAroundXAxisFromZ(s3DPoint* point);                          // 0x100c11a4 (52 bytes): the pointer form
float GetVectorRotationAroundYAxisFromZ(s3DPoint* point);                          // 0x100c121c (48 bytes): the pointer form (what the original's callers use)
float GetVectorRotationAroundZAxisFromY(s3DPoint* point);                          // 0x100c1290 (52 bytes): the pointer form
void XRotateLMatrix(sMatrix* matrix, float c, float s);                            // 0x100c1308 (132 bytes): all four rows
void YRotateLMatrix(sMatrix* matrix, float c, float s);                            // 0x100c1458 (164 bytes): all four rows
void ZRotateLMatrix(sMatrix* matrix, float c, float s);                            // 0x100c152c (148 bytes): all four rows
void ScaleLMatrixNoTrans(sMatrix* matrix, float x, float y, float z);              // 0x100c1d0c (112 bytes): the three scales as floats (0x100c1c3c takes a point)
void CombineMatricesNoWNoTransDest2nd(sMatrix* first, sMatrix* second);            // 0x100c2b5c (428 bytes): second = first * second, 3 x 3 part
// 0x100c2d50 (360 bytes): merge sort of `count` object pointers by the float at byte `offset` of each object; the result is in `scratch`
// at an even depth and in `source` at an odd one (callers pass depth 0).
void FloatSortByArray(int32_t count, void** source, void** scratch, int32_t depth, int32_t offset);
// 0x100b7fa0 (156 bytes, src/game/MeshLoad.cpp, unit w114): walks the height field's quads (gHeightfieldHeader->mHField->mQuads): procedural
// textures on (nonzero) clears the orientation bits 0x78 of each quad whose flags have neither 0x81 bit; off (0) clears 0xf8 and puts back
// the same bits of gHeightfieldCopy's quad. Both pointers are read in both modes.
void CorrectOrientationFlagsForDiffProceduralMode(int32_t procedural);
void RSSE_Init();                                                   // 0x100d32e8
// 0x100d33dc (632 bytes, src/game/rides/rsse.cpp, unit w132): one tick of every script; answers gRSSEF7BE0 (also when the system is not on).
int32_t RSSE_Tick();
void RSSE_RenderTick();   // 0x100d3674 (108 bytes, src/game/rides/rsse.cpp, unit w132): BOUNCE_Tick and WALKNODE_Tick of every script
// 0x100d3708 (src/game/rides/rsse.cpp): kills the live scripts, marks the system closed and asserts that the scripts freed what they
// malloced.
void RSSE_Close();
struct RSSE_SCRIPT;
RSSE_SCRIPT* RSSE_GetScriptPointer(int32_t script);   // 0x100d668c (124 bytes, src/game/rides/rsse.cpp, unit w132)
int32_t RSSE_Getvar(RSSE_SCRIPT* script, int32_t index);   // 0x100d6c58 (124 bytes, src/game/rides/rsse.cpp, unit w132)
void RSSE_Setvar(RSSE_SCRIPT* script, int32_t index, int32_t value);   // 0x100d6734 (src/game/rides/rsse.cpp, unit w150; unit w126: CObject::SetState sets variable 6)
// 0x100614a0 (untranslated; unit w126): the ride's hoardings (the ride anim set of gRideAnimSets); CObject::SetState calls it with 1.
void ActivateHoardings(tag_sRideAnimSet* set, uint32_t flag);
// 0x100617cc (untranslated; unit w199): the hoardings of the anim set off again (RepairFully and RequestMaintenance open the ride with it).
void DeActivateHoardings(tag_sRideAnimSet* set);
// 0x100d67e0 (untranslated; unit w199): the upgrade effect of the ride script (RepairFully).
void RSSE_UpgradeRideEffect(int32_t handle);
extern uint16_t gRSSEUpgradeSkipThing;   // 0x1029f152: the thing id whose scripts RSSE_UpgradeRideEffect skips (src/game/rides/rsse.cpp)
// 0x100d2ecc (1,004 bytes, src/game/rides/rsse.cpp, unit w169): kills one script (bit 4 of `flag` also turns its ride heads off) and answers the next live one, or NULL.
RSSE_SCRIPT* KillScript(RSSE_SCRIPT* script, int32_t flag);
// The script system's state: five words at 0x102f7bec that RSSE_Init sets (the original names are unknown).
struct RSSEState {
    int32_t mInitialised;      // 0x00  1 after RSSE_Init, 0 after RSSE_Close
    int32_t mF04;              // 0x04  RSSE_Init sets it to 0; RSSE_Tick adds 1 first: the tick count, whose low three bits pick the scripts that run
    int32_t mF08;              // 0x08  RSSE_Init sets it to 1; the next script handle (RSSE_InitScript hands it out and adds 1)
    int32_t mScriptCount;      // 0x0c
    RSSE_SCRIPT* mScripts;     // 0x10  the head of the live script list (a native pointer here, the Mac word on the Mac)
};
extern RSSEState gRSSEState;           // 0x102f7bec
extern int32_t gRSSEMallocCount;       // 0x102f7bdc  blocks the scripts malloced and did not free yet
void RSSE_SetReverbOnEax();   // 0x100d6e7c (140 bytes, src/game/rides/rsse.cpp, unit w132)

// ---- Unit w132 (src/game/rides/rsse.cpp): the ride scripts' list, their tick and the reverb.
// One script record of the list gRSSEState.mScripts heads (0xf4 bytes on the Mac; the names are mine, fields RSSE_Tick and its neighbours do not
// read are fillers). Its three links and the variable array are 32-bit fields, as the Mac's: a Ptr32 (arena offsets, platform/arena.h), so a record
// and its variable array must come from the arena; the list head in gRSSEState stays a native pointer. The program is only tested for "has one".
struct RSSE_LIMBO;    // unit w149, defined after RSSE_InitScript
struct RSSE_BOUNCE;
struct RSSE_WALK;
struct RSSE_OBJECT;   // unit w150, defined at the end of the file
struct RSSE_SCRIPT {
    Ptr32<RSSE_SCRIPT> mNext;      // 0x00  the next script of the list, NULL at the end
    Ptr32<RSSE_SCRIPT> mPrev;      // 0x04
    int32_t mHandle;               // 0x08  the script's number (RSSE_GetScriptPointer's key); its low three bits stagger the ticks
    int32_t mLinked;               // 0x0c  the handle of the script whose speed this one's copies (0: none)
    int32_t mF10;                  // 0x10  RSSE_SetReverbOnEax wants 0
    uint32_t mF14;                 // 0x14  the handle of a second linked script (RSSE_KillScript kills it too)
    Ptr32<uint32_t> mCode;         // 0x18  the program's words (NULL: the script runs no instructions); ReadScript reads mCode[mState]
    Ptr32<int32_t> mVars;          // 0x1c  the variables, mVarCount words
    Ptr32<int32_t> mStack;         // 0x20  the script's stack (unit w149 names; Push and Pop of w150 use it; NULL: none)
    Ptr32<RSSE_LIMBO> mLimbo;      // 0x24  the limbo table, mLimboCount records of 8 bytes
    Ptr32<RSSE_BOUNCE> mBounce;    // 0x28  the bounce table, mBounceCount records of 16 bytes
    Ptr32<RSSE_WALK> mWalk;        // 0x2c  the walk table, mWalkCount records of 32 bytes
    Ptr32<int32_t> mSaveWords;     // 0x30  a word table of mSaveWordCount words (unit w166: RSSE_Save writes it; nothing else reads it yet)
    Ptr32<uint8_t> mSaveBlock;     // 0x34  a byte block of mSaveBlockBytes bytes (unit w166: RSSE_Save writes it raw)
    Ptr32<char> mName;             // 0x38  the script's name (RSSE_InitScript mallocs and copies it; the "%s" asserts print it)
    int32_t mState;                // 0x3c  negative: the script is dead (RSSE_Tick kills it); also the program counter (ReadScript reads
                                   //       and advances it; ReadScript, Push and Pop store -10000 = 0xffffd8f0 on an error)
    int32_t mStackIndex;           // 0x40  the stack index (Push stores, then decrements it)
    uint32_t mF44;                 // 0x44
    int32_t mCommandAnswer;        // 0x48  the answer of the last ride command (unit w172; the name is mine; ProcessInstruction reads it)
    int32_t mSaveWordCount;        // 0x4c  words of mSaveWords (unit w166)
    int32_t mProgramWords;         // 0x50  the program's word count (ReadScript's bound)
    int32_t mStackSize;            // 0x54
    int32_t mLimboCount;           // 0x58  slots of mLimbo
    uint32_t mF5C;                 // 0x5c
    int32_t mLimboUsed;            // 0x60
    int32_t mBounceCount;          // 0x64  slots of mBounce
    uint32_t mF68;                 // 0x68
    int16_t mBounceUsed;           // 0x6c
    int16_t mBounceHeight;         // 0x6e  BOUNCE_Tick adds it to the root node's height
    int32_t mBounceBase;           // 0x70  a bounce slot's node id is the slot plus this
    uint32_t mF74[2];              // 0x74
    int32_t mWalkCount;            // 0x7c  slots of mWalk
    int32_t mFloatStart;           // 0x80  the clock when WALKNODE_Float was called
    int32_t mFloatDuration;        // 0x84  0: none
    int16_t mFloatA;               // 0x88
    int16_t mFloatB;               // 0x8a
    int32_t mVarCount;             // 0x8c
    int32_t mSaveBlockBytes;       // 0x90  bytes of mSaveBlock (unit w166)
    int32_t mInstructions;         // 0x94  instructions to run per tick
    int32_t mCountdown;            // 0x98  what is left of them (RSSE_Tick counts it down)
    int32_t mFlyHandle;            // 0x9c  the fly ride the script drives (0: none); RSSE_GetTourBaseID answers it
    uint32_t mFA0[3];              // 0xa0
    uint16_t mThingId;             // 0xac  the thing (ride object) the script belongs to
    uint8_t mFAE[2];               // 0xae
    Ptr32<RSSE_OBJECT> mObjects;   // 0xb0  the head of the list of the script's objects (emitters and sounds; unit w150)
    uint8_t mFB4[4];               // 0xb4
    uint8_t mEveryFrame;           // 0xb8  nonzero: runs on every tick, not only its own slot's
    uint8_t mFB9[7];               // 0xb9  [0]: nonzero, KillScript calls SND_DipMusic(0)
    int16_t mSpeed;                // 0xc0  percent
    uint8_t mFC2[6];               // 0xc2
    uint32_t mRideAnim;            // 0xc8  the ride's number in gRideAnimSets (0: none)
    uint32_t mFCC[3];              // 0xcc  [0] 0xcc an emitter handle, [1] 0xd0 a sound to fade, [2] 0xd4 the sound object RSSE_InitScript adds
    int32_t mReverb;               // 0xd8  the reverb level of the ride's camera (0: none, negative: do not set)
    uint32_t mFDC;                 // 0xdc
    uint32_t mCoaster;             // 0xe0  the coaster handle (0: none)
    int16_t mAnimFactor;           // 0xe4
    uint8_t mFE6[2];               // 0xe6
    uint32_t mFE8[3];              // 0xe8
};
static_assert(sizeof(RSSE_SCRIPT) == 0xf4, "RSSE_SCRIPT is 0xf4 bytes");
static_assert(offsetof(RSSE_SCRIPT, mVars) == 0x1c && offsetof(RSSE_SCRIPT, mState) == 0x3c && offsetof(RSSE_SCRIPT, mVarCount) == 0x8c, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mCountdown) == 0x98 && offsetof(RSSE_SCRIPT, mThingId) == 0xac && offsetof(RSSE_SCRIPT, mEveryFrame) == 0xb8, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mSpeed) == 0xc0 && offsetof(RSSE_SCRIPT, mRideAnim) == 0xc8 && offsetof(RSSE_SCRIPT, mReverb) == 0xd8, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mCoaster) == 0xe0 && offsetof(RSSE_SCRIPT, mAnimFactor) == 0xe4, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mStack) == 0x20 && offsetof(RSSE_SCRIPT, mLimbo) == 0x24 && offsetof(RSSE_SCRIPT, mBounce) == 0x28 && offsetof(RSSE_SCRIPT, mWalk) == 0x2c && offsetof(RSSE_SCRIPT, mName) == 0x38, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mSaveWords) == 0x30 && offsetof(RSSE_SCRIPT, mSaveBlock) == 0x34 && offsetof(RSSE_SCRIPT, mSaveWordCount) == 0x4c && offsetof(RSSE_SCRIPT, mSaveBlockBytes) == 0x90, "RSSE_SCRIPT layout (unit w166)");
static_assert(offsetof(RSSE_SCRIPT, mStackIndex) == 0x40 && offsetof(RSSE_SCRIPT, mProgramWords) == 0x50 && offsetof(RSSE_SCRIPT, mStackSize) == 0x54 && offsetof(RSSE_SCRIPT, mLimboCount) == 0x58, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mLimboUsed) == 0x60 && offsetof(RSSE_SCRIPT, mBounceCount) == 0x64 && offsetof(RSSE_SCRIPT, mBounceUsed) == 0x6c && offsetof(RSSE_SCRIPT, mBounceHeight) == 0x6e, "RSSE_SCRIPT layout");
static_assert(offsetof(RSSE_SCRIPT, mBounceBase) == 0x70 && offsetof(RSSE_SCRIPT, mWalkCount) == 0x7c && offsetof(RSSE_SCRIPT, mFloatStart) == 0x80 && offsetof(RSSE_SCRIPT, mFloatDuration) == 0x84 && offsetof(RSSE_SCRIPT, mFloatB) == 0x8a, "RSSE_SCRIPT layout");
// 0x102f7be0: the count of live RSSE_OBJECTs (CreateObject adds 1; KillObject and InitObject's failure take 1 off); RSSE_Tick answers it
// (the original's name is unknown).
extern int32_t gRSSEF7BE0;
// The callees of the tick and the reverb, untranslated (each answers 0 in the launcher).
void ProcessInstruction(RSSE_SCRIPT* script);   // 0x100cfdf4 (12,340 bytes): runs one instruction of the script (gRSSEF7BE4 nonzero keeps the countdown)
void ProcessObjects(RSSE_SCRIPT* script);       // 0x100cf884 (src/game/rides/rsse.cpp, unit w150)
void BOUNCE_Tick(RSSE_SCRIPT* script);          // 0x100ce740 (src/game/rides/RideEffects.cpp, unit w149)
void WALKNODE_Tick(RSSE_SCRIPT* script);        // 0x100cd928 (src/game/rides/RideEffects.cpp, unit w149)
void RSSE_KillScript(int32_t handle, int32_t flag);   // 0x100d42d0 (696 bytes, src/game/rides/rsse.cpp, unit w169)
void UpdateRideAnimSpeedAllChannels(tag_sRideAnimSet* set, float speed);   // 0x100c7ae4
void FLY_SetRunTime(int32_t fly, int32_t time);   // 0x10078958
void FLY_SetRunSpeed(int32_t fly, int32_t speed); // 0x100789ec
void CoasterSetSpeed(uint32_t coaster, uint32_t speed);   // 0x10049630
void BMP_SetRidePerformance(int32_t ride, int32_t speed); // 0x10026c98
uint32_t GetFirstPersonRideID();                // 0x10032b64
// 0x100d37b4 (2,756 bytes, src/game/rides/rsse.cpp, unit w169): creates a script from the file `path`; the thing id travels in the HIGH halfword of the second word (a tag_IDX_TYPE by value),
// the third argument is 1 when the action recorder is recording. Answers the new script's handle (0: none).
int32_t RSSE_InitScript(const char* path, uint32_t thing, int32_t recording);

// ---- Unit w149 (src/game/rides/RideEffects.cpp): the peep tables of a ride script (the walk nodes, the bounce and limbo tables, GetNodePos).
// Plain records (no pointers) in the arena tables of RSSE_SCRIPT; the struct and field names are mine.
struct JCPOINT3D {   // the Mac's name (a mangled argument type); three floats
    float x, y, z;
};
static_assert(sizeof(JCPOINT3D) == 12, "JCPOINT3D layout");
struct RSSE_LIMBO {      // 8 bytes
    uint32_t mPeep;      // 0x00  the peep's thing id (0: free)
    int32_t mRelease;    // 0x04  time * 1000 + the clock when put
};
static_assert(sizeof(RSSE_LIMBO) == 8, "RSSE_LIMBO layout");
struct RSSE_BOUNCE {     // 16 bytes
    uint32_t mPeep;      // 0x00  the peep's thing id (0: free); its low halfword indexes gThingArray
    int32_t mNode;       // 0x04  the node id: the slot plus RSSE_SCRIPT::mBounceBase
    int32_t mRelease;    // 0x08  time * 1000 + the clock when put
    int32_t mStart;      // 0x0c  the clock (a second read) when put
};
static_assert(sizeof(RSSE_BOUNCE) == 16, "RSSE_BOUNCE layout");
struct RSSE_WALK {       // 32 bytes
    int16_t mFrom;       // 0x00  from-node
    int16_t mTo;         // 0x02  to-node
    int16_t mOutFrom;    // 0x04  Trigger's fifth argument (DrawPeep's nodes in state 3)
    int16_t mOutTo;      // 0x06  Trigger's sixth argument (and state 4)
    int32_t mStart;      // 0x08  the clock when this state began
    int32_t mEnd;        // 0x0c  the end time of the state
    uint32_t mPeep;      // 0x10  the peep's thing id
    int16_t mDirection;  // 0x14  0 to 7
    int16_t mKind;       // 0x16  4 a head (node mask 0x80), 2 an emitter, else a plain walk node (mask 0x800)
    int16_t mState;      // 0x18  0 free, 1 walking in, 2 waiting, 3 walking out, 4 done
    int16_t mLast;       // 0x1a  Trigger's last argument
    uint32_t mF1C;       // 0x1c
    void Byteswap();     // 0x100d65a8 (unit w167, src/game/rides/rsse.cpp): reverses the bytes of every field but +0x1c, with no condition
};
static_assert(sizeof(RSSE_WALK) == 32, "RSSE_WALK layout");
struct sAlignedBox;
struct tag_sMesh;
struct tag_sRideAnimSet;
// 0x100cc888 (608 bytes, src/game/rides/RideEffects.cpp): the position (and, when `dir` is not NULL, the facing) of node `node` of the script's ride;
// answers 1, or 0 (after the assertion "RSSE: Invalid Node ID") when the ride has no such node.
int32_t GetNodePos(RSSE_SCRIPT* script, JCPOINT3D* pos, JCPOINT3D* dir, int32_t node, int32_t mask);
void WALKNODE_Trigger(RSSE_SCRIPT* script, uint32_t peep, int32_t from, int32_t to, int32_t outFrom, int32_t outTo, int32_t kind, int32_t last);   // 0x100cdd4c
uint32_t WALKNODE_Get(RSSE_SCRIPT* script);                           // 0x100cdfc8
void WALKNODE_Float(RSSE_SCRIPT* script, int32_t duration, int32_t a, int32_t b);   // 0x100ce054
void WALKNODE_Release(RSSE_SCRIPT* script, uint32_t peep);            // 0x100ce0f4
int32_t BOUNCE_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time); // 0x100ce3d4
uint32_t BOUNCE_Get(RSSE_SCRIPT* script);                             // 0x100ce4ec
uint32_t BOUNCE_ForceGet(RSSE_SCRIPT* script);                        // 0x100ce610
int32_t LIMBO_Put(RSSE_SCRIPT* script, uint32_t peep, int32_t time);  // 0x100ceab0
uint32_t LIMBO_Get(RSSE_SCRIPT* script);                              // 0x100ceb90
uint32_t LIMBO_ForceGet(RSSE_SCRIPT* script);                         // 0x100cec58
// Untranslated callees of this unit (each answers 0 or does nothing in the launcher).
// 0x100cd388 (1,380 bytes, src/game/rides/RideEffects.cpp, unit w171): draws a peep between two nodes; `mode` 0 walk, 1 and 2 head states; `fraction` 0 to 1000.
void DrawPeep(RSSE_SCRIPT* script, RSSE_WALK* walk, int32_t from, int32_t to, int32_t mode, int32_t fraction);
void SetRideSpritePosnFocus(tag_sMesh* mesh);                         // 0x1005cc10 (TPRide.cpp, untranslated)
// 0x101ed058 (ui_ErrorDisplay.cpp, untranslated): the ride's bounding box; the Mac answers nonzero when it filled `box` (return type chosen: int32_t).
int32_t GetRideMasterBoundArea(uint32_t ride, sAlignedBox* box);
// 0x1002addc (TPAdvisor.cpp, untranslated): the world matrix (16 floats) of the mesh's embedded entry `index`, or NULL (return type chosen: float*).
float* GetDataNodeWorldMatrix(tag_sMesh*& mesh, uint32_t& index);
tag_sRideAnimSet** GetRideFromID(uint32_t& id);                       // 0x1002ae38 (TPAdvisor.cpp, untranslated; the answer is dereferenced once)
void NormaliseVector(s3DPoint* vector);                               // 0x1002ad34 (TPAdvisor.cpp, untranslated)

// ---- src/game/LMatrix.cpp
// 0x100c30a8: fills gTPGlobals' sine table with `size` entries (a power of two).
void BuildGeometryTables(uint32_t size);

// ---- loading meshes and the mesh paths (src/game/MeshLoad.cpp)
struct tag_sM3D2Header;
struct tag_sMesh;
// 0x100b7d04: loads a mesh file (with `oneFrame` its animated textures are cut to one
// frame), gives its textures their folders and, unless `noEngineMesh`, makes its engine
// mesh object. NULL when the file does not load. (The second argument is not used.)
tag_sMesh* LoadMesh(char* name, void* unused, int32_t noEngineMesh, int32_t pathArgument, int32_t oneFrame);
void CreateMeshTypeEngineMesh(tag_sMesh* mesh);   // 0x100b7dd4
char* GetMeshPath();                        // 0x100b88a0: the folder meshes load from, or NULL
int32_t GetFileAccessIndex();               // 0x100b88f0: the search path the meshes load from
void SetFileAccessIndex(int32_t index);     // 0x100b8928
void BuildSharedTexturePath(char* out);     // 0x100b8960: "<theme path>:<shared texture folder>"
// 0x100b89dc: where the meshes, their textures and the shared textures load from (any of
// the three may be NULL). Each becomes "<root><folder>:" in a table of at most three.
void SetNewMeshPaths(char* root, char* folder, char* textureFolder, char* sharedTextureFolder);
void DestroyPathAllocs();                   // 0x100b8df4: frees every table and forgets the folders
// 0x100b8ea8: points every texture of a mesh at the folder it loads from: the texture
// folder when the file is there (as .wct, then .tga; with quickloading on and `checkFiles`
// 0 the index is asked instead), else the shared folder, else the table's first folder.
void OverrideTexturePaths(tag_sM3D2Header* header, int32_t checkFiles, int32_t unused);
struct sDTextureSet;
struct tag_sDMeshSet;
// 0x100b8710 (324 bytes, src/game/MeshLoad.cpp; rsse.cpp on the Mac): looks for the dynamic textures' first file in the texture set's folder (then
// with its extension replaced by ".wct"), takes the search path it is found in (or the last name tried) as the root of the mesh paths
// (SetNewMeshPaths with the set's two folders) and points the textures of the set's mesh at them (OverrideTexturePaths).
void CorrectDTexturePaths(sDTextureSet* textures, tag_sDMeshSet* set);
extern void* gMeshPathAllocs;            // 0x102e49dc  the tables made so far, newest first
extern int32_t gFileAccessIndex;     // 0x102e49e0
extern int32_t gMeshPathCount;           // 0x102e49e4
extern char** gMeshPaths;                // 0x102e49e8  the current table
extern int32_t gMeshPathIndex;       // 0x1029eb6c  each folder's place in the table, or -1
extern int32_t gTexturePathIndex;    // 0x1029eb70
extern int32_t gSharedPathIndex;     // 0x1029eb74
int32_t CheckFileExists(char* name);        // 0x100b98a4: 1 when TPFile_Open opens it

// ---- the animation changes and the morph decoder (src/game/rides/RideAnim.cpp)
struct sCMKeyData;
struct sAlignedBox;
struct sVertexPosnSet;
// 0x100c5e18 (372 bytes): flags the nodes an animation touches so that the draw code redoes them: every node of a node
// animation gets 0x40000 (and 0x200000, 0x400000 and 0x100000 for the node animation's flags 0x1000, 0x10000 and
// 0x289), and the nodes the animation disables (its list of 16-bit node numbers) get 0x10 unless they have the top bit.
// With the mesh's flag 4 a node number is matched against the nodes' own number (word 0x50) instead of its place.
void SetupAnimationFlags(tag_sAnimHeader* animation, tag_sMesh* mesh);
// The two returning forms (see SetCurRideAnim and SetNewAnim above).
uint32_t SetCurRideAnimDuration(tag_sRideAnimSet* set, uint32_t a, uint32_t anim, uint32_t loop, float speed, uint32_t b);   // 0x100c6670
uint32_t SetNewAnimDuration(tag_sRideAnimSet* set, uint32_t kind, uint32_t index, sAnimTimeControl* control,
                            uint32_t flags, float speed);                                    // 0x100c60fc
// 0x100c3cac (976 bytes): the node's bounding box at the fraction `t` between the key data's current row and the next
// (the row's two words are the box's corners): the corners widened by the morph's scale and a quarter. `add` nonzero
// adds the box to what `box` holds.
void InterpolateCMorphBoundData(sMorphSet* morph, sAlignedBox* box, sCMKeyData* key, float t, int32_t add);   // 0x100c3cac
// 0x100c390c (832 bytes): the vertices of a key data (one 10-bit field per axis in a word, the vertex's place in the
// node's vertex sets in its table) at the fraction `t` between the current row and the next, set or (`add`) added.
void InterpolateCMorphData(sMorphSet* morph, sVertexPosnSet* positions, sCMKeyData* key, float t, int32_t add);   // 0x100c390c

// ---- the ride sign's image filters (src/game/rides/RideSign.cpp, unit w84; the shaders that use them are in PlasticShader.h)
class Bitmap;
// 0x100c8e40 (596 bytes): one round of erosion of `work` (two bytes per pixel: the current value, then the next): each pixel's next
// byte is the smallest current byte among itself and the neighbours the edge tests let it see (the right edge tests `col < width - 2`
// and the bottom `row < height - 2`, so the last two columns and rows are not eroded by their lower and right neighbours); a pixel
// whose value changed stamps `value` into `out` (one byte per pixel); then the next bytes become the current ones. `mask` is not read.
void erodebuffer_erode(Bitmap& work, Bitmap& mask, uint8_t value, Bitmap& out);
// 0x100c90d8 (288 bytes): a size x size gaussian (centre (size - 1) / 2, sigma `sigma`), normalised to sum 1.
void make_gaussian_filter_2d(double* filter, int32_t size, double sigma);
// 0x100c922c (412 bytes): dest = the first byte of each of src's pixels, moved by (dx, dy), one byte per pixel (dest is reallocated
// to src's size, zeroed). Answers 1. A move of a whole width or more writes past the row (the original's bug).
int32_t copy_offset_alpha(Bitmap& dest, const Bitmap& src, int32_t dx, int32_t dy);
// 0x100c9404 (548 bytes): each byte becomes the largest of the bytes `radius` either side, in the row and then in the column
// (only the first `width` bytes of a row of a pixel wider than one byte are visited, as the original does). Answers 1, 0 when new[]
// fails.
int32_t dilate(Bitmap& bitmap, int32_t radius);
// 0x100c9650 (624 bytes): the same with the smallest, and every byte within `radius` of the edge of its row (then column) is 0.
int32_t erode(Bitmap& bitmap, int32_t radius);
// 0x100c98e4 (904 bytes): a one-dimensional gaussian of sigma `sigma` ((int)(2.5 * sigma) + 3 taps) along the rows and then the
// columns. Answers 1, or 0 when the line buffer's new[] fails (the kernel leaks then).
int32_t blur(Bitmap& bitmap, double sigma);

// ---- Unit w150 (src/game/rides/rsse.cpp): a script's emitter and sound objects, its stack and the small getters.
// One object of the list RSSE_SCRIPT::mObjects heads: 0x1c bytes on the Mac (LbMem_Alloc; here the arena, since the links are Ptr32).
// The names are mine.
struct RSSE_OBJECT {
    Ptr32<RSSE_OBJECT> mNext;   // 0x00
    Ptr32<RSSE_OBJECT> mPrev;   // 0x04
    int32_t mType;              // 0x08  1 an emitter, 2 an emitter with a launch vector, 3 to 10 a sound
    int32_t mHandle;            // 0x0c  the emitter's or the sound's handle (0: none)
    int32_t mNodeId;            // 0x10  the ride node's id (ProcessObjects skips a negative one)
    uint32_t mNodeIndex;        // 0x14  FindDataNodeIndexByID's answer for the node id
    int32_t mF18;               // 0x18  InitObject's last argument
};
static_assert(sizeof(RSSE_OBJECT) == 0x1c, "RSSE_OBJECT is 0x1c bytes");
static_assert(offsetof(RSSE_OBJECT, mPrev) == 4 && offsetof(RSSE_OBJECT, mType) == 8 && offsetof(RSSE_OBJECT, mHandle) == 0xc, "RSSE_OBJECT layout");
static_assert(offsetof(RSSE_OBJECT, mNodeId) == 0x10 && offsetof(RSSE_OBJECT, mNodeIndex) == 0x14 && offsetof(RSSE_OBJECT, mF18) == 0x18, "RSSE_OBJECT layout");
// 0x100cece4 (48 bytes): a word with the top byte 0x40 is a variable's index (the variable's value), else the low halfword, sign extended.
int32_t ExtractData(RSSE_SCRIPT* script, int32_t word);
// 0x100cee2c (128 bytes): makes an object at the head of the script's list (its fields are not set) and counts it; answers it.
RSSE_OBJECT* CreateObject(RSSE_SCRIPT* script);
// 0x100ceedc (152 bytes): takes the object out of the list and frees it; answers its old next object (NULL for a NULL object).
RSSE_OBJECT* KillObject(RSSE_SCRIPT* script, RSSE_OBJECT* object);
// 0x100ced44 (164 bytes): ends the object's emitter (life -2) or sound (faded when `fade` is nonzero, else stopped).
void KillLinkedObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t fade);
// 0x100cefb0 (116 bytes, void): sets a parameter of a sound object (the handle is replaced by the answer); an emitter does nothing.
void SetObjParam(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t param, int32_t value);
// 0x100cf064 (1,584 bytes): starts the emitter or the sound of `type` at the ride node `node`; answers its handle (0: none).
int32_t StartEvent(RSSE_SCRIPT* script, int32_t type, int32_t node, int32_t a, int32_t b);
// 0x100cf6c8 (380 bytes): starts the object's event and finds its node index; an event with no handle kills the object again.
void InitObject(RSSE_SCRIPT* script, RSSE_OBJECT* object, int32_t a, int32_t b, int32_t e);
// 0x100cfbf8 (116 bytes): the next word of the program (the program counter is mState); outside the program it asserts, kills the script
// (mState -10000) and answers 0.
uint32_t ReadScript(RSSE_SCRIPT* script);
// 0x100cfc9c (124 bytes) and 0x100cfd44 (136 bytes): the stack grows down; a bad index or no stack asserts and kills the script.
void Push(RSSE_SCRIPT* script, int32_t value);
int32_t Pop(RSSE_SCRIPT* script);
void RSSE_SetRideSpeed(RSSE_SCRIPT* script, int32_t speed);   // 0x100d6ac0 (80 bytes)
int32_t RSSE_GetRideSpeed(RSSE_SCRIPT* script);               // 0x100d6b48 (80 bytes)
int32_t RSSE_GetTourBaseID(RSSE_SCRIPT* script);              // 0x100d6bd0 (80 bytes)
// 0x100d2e60 (44 bytes): the position of the ride's node mF78 (its floats +0x40, +0x44, +0x48).
void GetRidePosition(tag_sRideAnimSet* set, float& x, float& y, float& z);
// 0x100cc778 (220 bytes): the centre of the ride's bounding box (y from the ride's node); answers 0 when it found it, else 1 with all zeros.
int32_t GetRideCentrePosition(uint32_t& ride, float& x, float& y, float& z);

// ---- Unit w166 (src/game/rides/rsse.cpp): the ride scripts' save and rsse.cpp's swap helpers
// 0x100d45b0 (3,408 bytes): writes the script system to a file: the tag "RSSE", 0x14, the state's five words (the list head as its Ptr32 word) and,
// when the system is initialised, "PAD_" five times, the script count, 0xf4 and each script: its record, its tables, its objects. Answers 0.
int32_t RSSE_Save(void* file);   // 0x100d45b0
// rsse.cpp's own instances of the SamsUtilities swap templates (the Mac has one per source file): the file is little-endian, so these swap on
// a big-endian host (or with TPW_SWAP_FILE_WORDS) and do nothing on a little-endian one. A pointer instance takes and answers the 32-bit word.
namespace RsseSwap {
void USwapInplace32Int(int32_t& word);          // 0x100d5324  USwapInplace32<int>
void USwapInplace32UInt(uint32_t& word);        // 0x100d5368  USwapInplace32<unsigned int>
uint32_t USwap32ObjectPtr(uint32_t value);      // 0x100d53b0  USwap32<RSSE_OBJECT*>
void USwapInplace16UShort(uint16_t& value);     // 0x100d5424  USwapInplace16<unsigned short>
void USwapInplace32ULong(uint32_t& word);       // 0x100d546c  USwapInplace32<unsigned long>
void USwapInplace16Short(int16_t& value);       // 0x100d54b4  USwapInplace16<short>
uint32_t USwap32CharPtr(uint32_t value);        // 0x100d54f8  USwap32<char*>
uint32_t USwap32WalkPtr(uint32_t value);        // 0x100d5554  USwap32<RSSE_WALK*>
uint32_t USwap32LongPtr(uint32_t value);        // 0x100d55c0  USwap32<long*>
void USwapInplace32Long(int32_t& word);         // 0x100d561c  USwapInplace32<long>
uint32_t USwap32VoidPtr(uint32_t value);        // 0x100d5660  USwap32<void*>
}  // namespace RsseSwap

// ---- Unit w167 (src/game/rides/rsse.cpp): the ride scripts' load
// 0x100d56bc (3,784 bytes): reads what RSSE_Save wrote. Kills the live scripts and restarts the system (RSSE_Init) first; answers 1 on a wrong
// tag ("RSSE", or "OBJ " after a script's tables: the script stays in the list, half loaded), else 0. The scripts are linked at the head of the
// list as they are read, so the list ends up in the reverse of the saved order.
int32_t RSSE_Load(void* file);   // 0x100d56bc

// ---- Unit w171 (src/game/rides/RideEffects.cpp): a ride script's screams, the repair particle effect, DrawPeep and two getters of rsse.cpp.
// The screams: the sound bank is gSoundBanks[1]; the handle is SOUND_SendEvent's answer. A negative count takes the group of 2 and 3.
int32_t SCREAM_Start(int32_t handle, int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r);   // 0x100ccb30 (356 bytes)
// 0x100cccc0 (84 bytes): the level (a + b) / 2 clamped to 0..100. Answers 0 for handle 0 (r3 is the handle), else
// SOUND_SetParamValue's answer (ProcessInstruction's op 89 keeps it as the handle).
int32_t SCREAM_SetLevel(int32_t handle, int32_t a, int32_t b);
int32_t SCREAM_Move(int32_t handle, int32_t x, int32_t y, int32_t z);   // 0x100ccd3c (52 bytes)
void SCREAM_Stop(int32_t handle);                                    // 0x100ccd98 (44 bytes)
int32_t SCREAM_Single(int32_t count, int32_t a, int32_t b, int32_t p, int32_t q, int32_t r);   // 0x100ccde8 (644 bytes)
int32_t SCREAM_SingleSad(int32_t count, int32_t p, int32_t q, int32_t r);   // 0x100cd098 (204 bytes)
// 0x100cd190 (440 bytes): starts (on nonzero) or kills (on 0) the ride's repair emitter, kept in the script's word +0xcc.
void TriggerRepairParticleEffect(RSSE_SCRIPT* script, int32_t on);
// The function-local static of TriggerRepairParticleEffect: the guard byte 0x102f7bd4 and the word 0x102f7bd8 (-1 after the first call; never read).
extern uint8_t gRepairEffectGuard;     // 0x102f7bd4
extern int32_t gRepairEffectWord;      // 0x102f7bd8
uint32_t RSSE_GetCoasterHandle(int32_t handle);   // 0x100d6f34 (140 bytes): the script's coaster handle, 0 when there is no such script
// 0x1005d198 (316 bytes, TPRide.cpp, untranslated): the height of the ride's ground at (x, z), written to y.
void GetRideHeightAtPoint(float x, float z, float& y);

// ---- Unit w172 (src/game/rides/rsse.cpp): a ride script's ride-specific commands (the original's rsse.cpp).
static_assert(offsetof(RSSE_SCRIPT, mCommandAnswer) == 0x48, "RSSE_SCRIPT layout (unit w172)");
// Each reads the command word from the program (ReadScript), switches on it (unsigned; 0 and anything above the bound assert) and calls one ride function.
void ExecuteTourSpecificCommand(RSSE_SCRIPT* script);      // 0x100d6fec (2,240 bytes): the tour (fly) ride's commands 1 to 18
void ExecuteBumperSpecificCommand(RSSE_SCRIPT* script);    // 0x100d78ec (2,008 bytes): the bumper ride's commands 1 to 17
void ExecuteCoasterSpecificCommand(RSSE_SCRIPT* script);   // 0x100d8104 (1,192 bytes): the coaster's commands 1 to 8 (its unknown-command text is the bumper's)
// 0x100d85f0 (28 bytes, __sinit_rsse_cpp): two words of rsse.cpp's BSS set at start-up; their readers are not known (the names are mine).
extern uint32_t gRsseWord102F7C00;   // 0x102f7c00  0x10000
extern uint32_t gRsseWord102F7C04;   // 0x102f7c04  0
// The callees of the three commands, all untranslated stand-ins (argument types from the mangled names; `long` is int32_t).
int32_t BMP_AddJoiningListToBump(int32_t ride);                    // 0x1002cc70
int32_t BMP_AddPeepToRideJoiningList(int32_t ride, uint32_t peep); // 0x1002e070
int32_t BMP_BeginRide(int32_t ride);                               // 0x10026924
int32_t BMP_BreakRide(int32_t ride);                               // 0x100264c0
int32_t BMP_CloseRide(int32_t ride);                               // 0x10026118
int32_t BMP_FixRide(int32_t ride);                                 // 0x1002672c
int32_t BMP_GetCarsOnRide(int32_t ride);                           // 0x10026e74
int32_t BMP_HaltRide(int32_t ride);                                // 0x100262cc
int32_t BMP_LAUNCH(int32_t ride, int32_t a);                       // 0x1002cff4
int32_t BMP_OpenRide(int32_t ride);                                // 0x10025ee4
int32_t BMP_PollForPeeps(int32_t ride);                            // 0x1002e198
int32_t BMP_REMOVEBUMP(int32_t ride, int32_t a);                   // 0x1002e614
int32_t BMP_RideWorn(int32_t ride);                                // 0x100263f4
int32_t BMP_SetRideDuration(int32_t ride, int32_t duration);       // 0x10026b14
// Unit w202 (untranslated callees of CObject::GoFirstPerson): the camera meshes of a ride and of a fly tour (types from the call sites: one word in, one word out).
int32_t BMP_GetCameraMesh(int32_t ride);                           // 0x1002cf30 (156 bytes)
int32_t FLY_GetCameraMesh(int32_t tour);                           // 0x10075278 (52 bytes)
uint32_t CoasterAddPeep(uint32_t coaster, uint32_t peep);          // 0x10049288
uint32_t CoasterGetPeep(uint32_t coaster);                         // 0x100494c0
uint32_t CoasterGetQueueSpace(uint32_t coaster);                   // 0x1004944c
uint32_t CoasterSetCapacity(uint32_t coaster, int32_t capacity);   // 0x10049758
uint32_t CoasterSetOpenClosedState(uint32_t coaster, int32_t state);   // 0x100496c0
uint32_t CoasterSetRepairState(uint32_t coaster, uint32_t state);  // 0x10049588
int32_t GetCoasterInstanceID(int32_t handle);                      // 0x10049120
// FLY_CreateBase 0x100753e8: the first parameter is the 32-bit word at the script's +0xac (a tag_IDX_TYPE by value: the thing id in the high halfword).
int32_t FLY_CreateBase(uint32_t thingWord, int32_t handle, int32_t x, int32_t y, int32_t z, int32_t angle, int32_t a);
int32_t FLY_AddPeep(int32_t fly, int32_t peep);                    // 0x100787c8
int32_t FLY_ExtractPeep(int32_t fly);                              // 0x1007848c
int32_t FLY_GetBrokenStat(int32_t fly);                            // 0x10078fb0
int32_t FLY_GetCarInBase(int32_t fly);                             // 0x10078f54
int32_t FLY_LaunchCar(int32_t fly);                                // 0x10079024
int32_t FLY_RideRunning(int32_t fly);                              // 0x100791c0
int32_t FLY_SetCarSize(int32_t fly, int32_t a);                    // 0x10079114
int32_t FLY_SetMotionScale(int32_t fly, int32_t a);                // 0x10079168
int32_t FLY_SetNumberOfCars(int32_t fly, int32_t a);               // 0x10078ee4
int32_t FLY_SetOpenClosed(int32_t fly, int32_t a);                 // 0x10078a58
int32_t FLY_SetRepairState(int32_t fly, int32_t a);                // 0x10078b40
// Unit v5980: 0x1007861c (384 bytes, src/game/rides/FlyingRide.cpp): takes the peep of the first slot of the fly base that
// has a positive count (active or not); answers it, or 0.
int32_t FLY_ForceExtractPeep(int32_t fly);
int32_t FLY_SetRunway(int32_t fly, int32_t a);                     // 0x100790c0
class CObject;
void LLCC_SetTrackAnim(CObject* object, bool flag);                // 0x1008c7b0
void LLCC_SetTrackAnimReverse(CObject* object, bool flag);         // 0x1008cad8

// ---- Unit v5308 (src/game/rides/RideAnim.cpp)
// 0x100c4e20 (172 bytes): runs AnimateNode for each node animation of `animation` (mesh->mHeader), then RAAnimateTextureFrames
// when the animation has flag 2 and the engine flags lack 8. Answers 1; NULL animation: nothing.
int32_t RATick(sAnimTimeControl* control, tag_sMesh* mesh, tag_sAnimHeader* animation);
// 0x100c4f18 (240 bytes): copies the vertices and bounding box of `from`'s nodes into `to`'s nodes that have vertices and flag 0x200000 (or any when `force`).
void QuickCopyUnmaskedMeshVertices(tag_sMesh* from, tag_sMesh* to, int32_t force);

// ---- Unit v5882 (src/game/rides/rsse.cpp): the script interpreter's callees that have no declaration or no returning form.
struct sNodeLightParams;
// 0x100647ec (56 bytes, untranslated): the light of the ride `ride`'s data node `index` (FindDataNodeIndexByID's answer for mask 0x20000).
sNodeLightParams* GetLightsourceHandle(uint32_t ride, uint32_t index);
// ProcessInstruction (and FLY_ForceExtractPeep's Labs) reach the C library through glue imports: these are the imports by another
// name, so that the test libraries can record them. The program's definitions are platform/rsse_dispatch.cpp.
namespace processinstruction_callee {
int32_t Labs(int32_t value);                             // labs, glue 0x10239928
char* StrCpy(char* to, const char* from);                // strcpy, glue 0x10238410
char* StrCat(char* to, const char* from);                // strcat, glue 0x102385c0
int32_t StrCmp(const char* a, const char* b);            // strcmp, glue 0x102385a8
int32_t Time(int32_t* out);                              // time, glue 0x1023a198 (time_t is a long)
const int32_t* LocalTime(const int32_t* when);           // localtime, glue 0x1023a1b0 (a struct tm: nine longs, tm_sec first)
}  // namespace processinstruction_callee
