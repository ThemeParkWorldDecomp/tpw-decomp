// The particle system's core, second half (main app, rsse.cpp range of the inventory, 0x100ba1e0..0x100bf1b0): closing the
// system, the emitter library's per-emitter loader, the tick, the density adjustment, the emitter setters, queries and
// killers. The start-up half is src/rsse_particles_init.cpp, the state src/rsse_particles_state.cpp, the layouts include/rsse.h.
//
// A handle to an emitter or effecter is a 32-bit number: the index in the low half and the serial in the high half (the
// record's own serial must match, which tells a handle to a dead and reused record from a live one). The setters check it
// with PAR_GetEmitterStatus and do nothing otherwise. The index of a live emitter is below 120.
#include "Debug.h"
#include "FS_System.h"
#include "LbFile.h"
#include "rsse.h"
#include "tb_particle.h"

int32_t gParAllocBalance = 0;   // 0x102e49f0

namespace {

constexpr char kNotFreed[] = "PTCL: Something malloced but not freed - Naughty !";   // string table 0x1023ed27 + 0x29

// The low 32 bits of a product (mullw).
int32_t Product(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

// A value shifted left by 4 (slwi), wrapping.
int32_t ShiftLeft4(int32_t value) { return static_cast<int32_t>(static_cast<uint32_t>(value) << 4); }

// One of the four signed bytes ApplyParticleDensityAdjustment scales: when it is not 0, its product with the density over 1024
// (rounded down), at most 0x7f, and 1 where that is 0. A negative result stays negative.
void ScaleDensityByte(int8_t& value, int32_t density) {
    if (value == 0) return;
    int32_t scaled = Product(value, density) >> 10;
    if (scaled > 0x7f) scaled = 0x7f;
    if (scaled == 0) scaled = 1;
    value = static_cast<int8_t>(scaled);
}

}  // namespace

// 0x100bac60
void PAR_Free(void* block) {
    if (block != nullptr) LbMem_Free(block);
}

// 0x100bdf84
void PAR_CloseParticleSystem() {
    if (gParSystemRunning == 0) return;
    PARState& state = gParState;
    state.mF00 = 0;
    PAR_Free(state.mParticles);
    gParSystemRunning = 0;
    PAR_Free(gParDrawingList);
    if (gParAllocBalance != 0) GAME_ASSERTMSG(0, kNotFreed);
}

// 0x100bbc54: the first library slot whose name is empty takes the next 0x140 bytes of the file.
int32_t PAR_LoadLibEmitter(bfl::FileSystem::CFile* file) {
    if (gParSystemRunning == 0) return -1;
    for (int32_t i = 0; i < kParLibraryEmitters; ++i) {
        PARLibraryEmitter& slot = gParLibrary.mEmitters[i];
        // (The original also tests the address of the name for NULL first: never true.)
        if (slot.mName[0] == 0) {
            TPFile_Read(&slot, sizeof(slot), 1, file);
            PAR_SwapLibraryEmitter(slot);
            return i;
        }
    }
    return -1;
}

// 0x100bbf78
void PAR_KillLibEmitter(uint32_t slot) {
    if (gParSystemRunning == 0) return;
    if (slot >= static_cast<uint32_t>(kParLibraryEmitters)) return;
    gParLibrary.mEmitters[slot].mName[0] = 0;
}

// 0x100be564
int32_t PAR_TickResult() {
    if (gParSystemRunning != 0) {
        PARState& state = gParState;
        state.mTicks += 1;
        // The first tick after a reset only counts.
        if (state.mTicks > 1) {
            DoEmitters();
            DoEffecters();
            for (int32_t index = state.mEmittersUsed; index >= 0;) {
                PAREmitter& emitter = state.mEmitters[index];
                if (emitter.mPaused == 0) {
                    for (int32_t particle = emitter.mFirstParticle; particle >= 0;) {
                        // DoAParticle may free the particle it is given, so its successor is read first.
                        const int32_t next = state.mParticles[particle].mNext;
                        DoAParticle(particle);
                        particle = next;
                    }
                }
                index = emitter.mNext;
            }
        }
    }
    return gParAllocBalance;
}

// 0x100be564, the callers' form (see include/rsse.h).
void PAR_Tick() { PAR_TickResult(); }

// 0x100be654: the emitter's counts, which the library gives for full density, scaled to the system's density (1024: full).
void ApplyParticleDensityAdjustment(int32_t emitter) {
    const int32_t density = gParState.mDensity;
    PAREmitter& record = gParEmitters[emitter];
    for (int8_t& count : record.mSpawnRate) ScaleDensityByte(count, density);
    if (record.mMaxParticles > 0) {
        record.mMaxParticles = static_cast<int16_t>(Product(record.mMaxParticles, density) >> 10);
        if (record.mMaxParticles == 0) record.mMaxParticles = 1;
    }
    if (record.mStartCount > 0) {
        record.mStartCount = Product(record.mStartCount, density) >> 10;
        if (record.mStartCount == 0) record.mStartCount = 1;
    }
}

// 0x100bf110: no serial check for the handle 0, which is never an emitter.
int32_t PAR_GetEmitterStatus(uint32_t handle) {
    if (handle == 0) return 0;
    const uint32_t index = handle & 0xffff;
    if (gParSystemRunning != 0 && index < static_cast<uint32_t>(kParEmitters) && gParState.mEmitters[index].mSerial == (handle >> 16)) return 1;
    return 0;
}

// 0x100bf1b0: as PAR_GetEmitterStatus for the effecters, but the handle 0 is looked up like any other (it is effecter 0 with
// serial 0).
int32_t PAR_GetEffecterStatus(uint32_t handle) {
    const uint32_t index = handle & 0xffff;
    if (gParSystemRunning != 0 && index < static_cast<uint32_t>(kParEffecters) && gParState.mEffecters[index].mSerial == (handle >> 16)) return 1;
    return 0;
}

// 0x100be1e8: DoEmitters ends the emitter at its next turn (life -2).
void PAR_KillEmitter(int32_t handle) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) != 0) gParState.mEmitters[handle & 0xffff].mLife = -2;
}

// 0x100be494: ends the emitters and effecters in use that are interface objects; the emitters also get the mask 1.
void PAR_KillAndHideAll2DObjects() {
    if (gParSystemRunning == 0) return;
    PARState& state = gParState;
    for (int32_t index = state.mEmittersUsed; index >= 0;) {
        PAREmitter& emitter = state.mEmitters[index];
        if (emitter.m2D != 0) {
            emitter.mLife = -2;
            emitter.mMask = 1;
        }
        index = emitter.mNext;
    }
    for (int32_t index = state.mEffectersUsed; index >= 0;) {
        PAREffecter& effecter = state.mEffecters[index];
        if (effecter.m2D != 0) effecter.mLife = -2;
        index = effecter.mNext;
    }
}

// 0x100be070: the vector times the emitter's launch scale over 1024, cut towards zero.
void PAR_SetEmitterLaunchVector(int32_t handle, int32_t x, int32_t y, int32_t z) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) == 0) return;
    PAREmitter& emitter = gParState.mEmitters[handle & 0xffff];
    emitter.mLaunch[0] = Product(x, emitter.mLaunchScale) / 1024;
    emitter.mLaunch[1] = Product(y, emitter.mLaunchScale) / 1024;
    emitter.mLaunch[2] = Product(z, emitter.mLaunchScale) / 1024;
}

// 0x100be150
void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) == 0) return;
    PAREmitter& emitter = gParState.mEmitters[handle & 0xffff];
    emitter.mPos[0] = x >> 4;
    emitter.mPos[1] = y >> 4;
    emitter.mPos[2] = z >> 4;
}

// 0x100be260: the mask is kept as 16 bits.
void PAR_SetEmitterMask(int32_t handle, int32_t mask) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) != 0) gParState.mEmitters[handle & 0xffff].mMask = static_cast<int16_t>(mask);
}

// 0x100be2e8
void PAR_SetEmitterLife(int32_t handle, int32_t life) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) != 0) gParState.mEmitters[handle & 0xffff].mLife = life;
}

// 0x100be36c: the scale is kept as 16 bits.
void PAR_SetEmitterScale(int32_t handle, int32_t scale) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) != 0) gParState.mEmitters[handle & 0xffff].mScale = static_cast<int16_t>(scale);
}

// 0x100be3f4
void PAR_SetEmitterRandomPos(int32_t handle, int32_t x, int32_t y, int32_t z) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) == 0) return;
    PAREmitter& emitter = gParState.mEmitters[handle & 0xffff];
    emitter.mRandomPos[0] = x >> 4;
    emitter.mRandomPos[1] = y >> 4;
    emitter.mRandomPos[2] = z >> 4;
}

// 0x100ba1e0: the emitter's box times 16. There is no check of the index or of the emitter being in use.
void PAR_GetEmitterBoundingBox(uint32_t emitter, PARPOINT3D* min, PARPOINT3D* max) {
    if (gParSystemRunning == 0) return;
    const PAREmitter& record = gParEmitters[emitter];
    min->x = ShiftLeft4(record.mBoxMin[0]);
    min->y = ShiftLeft4(record.mBoxMin[1]);
    min->z = ShiftLeft4(record.mBoxMin[2]);
    max->x = ShiftLeft4(record.mBoxMax[0]);
    max->y = ShiftLeft4(record.mBoxMax[1]);
    max->z = ShiftLeft4(record.mBoxMax[2]);
}

// 0x100ba298: the system's own block is only marked free again, any other is freed.
void PAR_FreeDrawingList(PTCL* list) {
    if (gParSystemRunning == 0) return;
    if (list == nullptr) return;
    if (list == gParDrawingList) gParDrawingListBusy = 0;
    else PAR_Free(list);
}
