// Test library for unit w26c (libtpw_unit_ParticleTick.dylib; see tests/test_ParticleTick.py): the particle system's per-tick
// functions (DoEmitters, DoEffecters, DoAParticle, MoveEmitter, MoveEffecter, PAR_BuildDrawingList; src/game/Particles.cpp) and the effect
// manager (src/game/HLSprite.cpp), over the particle system's real state block (src/game/rides/rsse.cpp).
//
// Fakes that record on both sides (name, then every argument): the spawning and core functions the tick calls (PAR_InitEmitter,
// PAR_InitEffecter, SpawnParticle, DoSpecialEmitter, PAR_GetEmitterStatus, PAR_GetEffecterStatus, PAR_KillEmitter,
// PAR_SetEmitterPos), PAR_Malloc (a count of drawing list records instead of bytes, since a record is bigger on the host),
// ESP_GetParticleSprite, GAME_ASSERTMSG, the state's height callback, and the effect manager's callees (RealTimePauseAdjusted, HOST_Sin,
// HOST_Cos, BMP_GenerateSinTab). The values they return come from queues of the case.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Debug.h"
#include "calllog.h"
#include "effect_manager.h"
#include "rsse.h"
#include "tb_particle.h"

using namespace calllog;

// ---- the fakes

uint32_t PAR_InitEmitter(int32_t type, int32_t x, int32_t y, int32_t z) {
    ev("PAR_InitEmitter", {type, x, y, z});
    return static_cast<uint32_t>(ret("PAR_InitEmitter"));
}
uint32_t PAR_InitEffecter(int32_t type, int32_t x, int32_t y, int32_t z) {
    ev("PAR_InitEffecter", {type, x, y, z});
    return static_cast<uint32_t>(ret("PAR_InitEffecter"));
}
int32_t SpawnParticle(int32_t emitter) {
    ev("SpawnParticle", {emitter});
    return 0;
}
void DoSpecialEmitter(int32_t emitter, int32_t start) { ev("DoSpecialEmitter", {emitter, start}); }
int32_t PAR_GetEmitterStatus(uint32_t handle) {
    ev("PAR_GetEmitterStatus", {static_cast<int64_t>(handle)});
    return static_cast<int32_t>(ret("PAR_GetEmitterStatus"));
}
int32_t PAR_GetEffecterStatus(uint32_t handle) {
    ev("PAR_GetEffecterStatus", {static_cast<int64_t>(handle)});
    return static_cast<int32_t>(ret("PAR_GetEffecterStatus"));
}
void PAR_KillEmitter(int32_t handle) { ev("PAR_KillEmitter", {handle}); }
void PAR_SetEmitterPos(int32_t handle, int32_t x, int32_t y, int32_t z) { ev("PAR_SetEmitterPos", {handle, x, y, z}); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

namespace {
constexpr int32_t kBlocks = 8;
void* gBlocks[kBlocks];
int32_t gBlockCount;
}  // namespace

// The drawing list's blocks: a count of records (the Mac's 0x1c bytes each, the host's sizeof(PTCL)) is what is recorded; the
// block is filled with 0xa5 and labelled.
void* PAR_Malloc(int32_t size) {
    void* block = std::malloc(static_cast<std::size_t>(size) + 64);
    std::memset(block, 0xa5, static_cast<std::size_t>(size) + 64);
    if (gBlockCount < kBlocks) gBlocks[gBlockCount++] = block;
    ev("PAR_Malloc", {size / static_cast<int32_t>(sizeof(PTCL)), newlab(block)});
    return block;
}

PTCLTexture* ESP_GetParticleSprite(int32_t sprite, int32_t frame, int32_t variant) {
    ev("ESP_GetParticleSprite", {sprite, frame, variant});
    return reinterpret_cast<PTCLTexture*>(static_cast<uintptr_t>(ret("ESP_GetParticleSprite")));
}

uint32_t RealTimePauseAdjusted() {
    ev("RealTimePauseAdjusted");
    return static_cast<uint32_t>(ret("RealTimePauseAdjusted"));
}

namespace {
// The table of sines the case gives (the main app's is at 0x102b4ec0; HOST_Sin reads entry (angle & 511) and shifts it left by 2).
int32_t gSine[512];
}  // namespace
int32_t HOST_Sin(int32_t angle) {
    ev("HOST_Sin", {angle});
    return static_cast<int32_t>(static_cast<uint32_t>(gSine[angle & 511]) << 2);
}
int32_t HOST_Cos(int32_t angle) {
    ev("HOST_Cos", {angle});
    return static_cast<int32_t>(static_cast<uint32_t>(gSine[(angle + 128) & 511]) << 2);
}
void BMP_GenerateSinTab() { ev("BMP_GenerateSinTab"); }

namespace {
// The state's height callback: records the point and answers the case's next value.
int32_t FakeHeight(int32_t x, int32_t z) {
    ev("Height", {x, z});
    return static_cast<int32_t>(ret("Height"));
}
}  // namespace

// ---- the state, as flat lists and regions

extern "C" {
// header[]: 0 mF00, 1 mTicks, 2 mLowSpec, 3 mMaxParticles, 4 mF10 (the seed), 5 mF14, 6 mF18, 7 mF1A, 8 mDensity.
void tpw_pt_set_header(const int32_t* words) {
    PARState& s = gParState;
    s.mF00 = words[0];
    s.mTicks = words[1];
    s.mLowSpec = words[2];
    s.mMaxParticles = words[3];
    s.mF10 = words[4];
    s.mF14 = words[5];
    s.mF18 = static_cast<int16_t>(words[6]);
    s.mF1A = static_cast<int16_t>(words[7]);
    s.mDensity = words[8];
}
void tpw_pt_get_header(int64_t* out) {
    const PARState& s = gParState;
    out[0] = s.mF00;
    out[1] = s.mTicks;
    out[2] = s.mLowSpec;
    out[3] = s.mMaxParticles;
    out[4] = s.mF10;
    out[5] = s.mF14;
    out[6] = s.mF18;
    out[7] = s.mF1A;
    out[8] = s.mDensity;
    out[9] = s.mHeightCallback == reinterpret_cast<const void*>(&FakeHeight) ? 1 : 0;
    out[10] = lab(s.mParticles);
}
// Points the state at the case's pool and the height fake, and sets the globals: running, the drawing list's busy flag and its
// block (labelled 7).
void tpw_pt_attach(void* pool, void* drawingList, int32_t running, int32_t busy) {
    gParState.mParticles = static_cast<tagPARTICLE*>(pool);
    gParState.mHeightCallback = reinterpret_cast<void*>(&FakeHeight);
    gParState.mSinTable = gParSinTable;
    gParDrawingList = static_cast<PTCL*>(drawingList);
    gParDrawingListBusy = busy;
    gParSystemRunning = running;
    tpw_cl_label(7, drawingList);
    tpw_cl_label(8, pool);
}
void tpw_pt_globals(int64_t* out) {
    out[0] = gParSystemRunning;
    out[1] = gParDrawingListBusy;
    out[2] = lab(gParDrawingList);
}
// Regions: 0 the emitters, 1 the effecters, 2 the chain heads (mEmittersUsed to the end of the state), 3 the pool.
void* tpw_pt_region(int32_t which, int32_t* size) {
    switch (which) {
    case 0: *size = sizeof(gParState.mEmitters); return gParState.mEmitters;
    case 1: *size = sizeof(gParState.mEffecters); return gParState.mEffecters;
    case 2: *size = 0x1c; return &gParState.mEmittersUsed;
    default: *size = gParState.mMaxParticles * static_cast<int32_t>(sizeof(tagPARTICLE)); return gParState.mParticles;
    }
}
int32_t tpw_pt_particle_size() { return static_cast<int32_t>(sizeof(PTCL)); }
void tpw_pt_set_sine(const int32_t* table) { std::memcpy(gSine, table, sizeof(gSine)); }
void tpw_pt_free_blocks() {
    for (int32_t i = 0; i < gBlockCount; ++i) {
        forget(gBlocks[i]);
        std::free(gBlocks[i]);
    }
    gBlockCount = 0;
}

// ---- the functions
void tpw_pt_do_emitters() { DoEmitters(); }
void tpw_pt_do_effecters() { DoEffecters(); }
void tpw_pt_do_particle(int32_t index) { DoAParticle(index); }
void tpw_pt_move_emitter(int32_t index) { MoveEmitter(&gParState.mEmitters[index]); }
void tpw_pt_move_effecter(int32_t index) { MoveEffecter(&gParState.mEffecters[index]); }
void* tpw_pt_build(int32_t emitter) { return PAR_BuildDrawingList(emitter); }

// ---- the effect manager (its table is read and written as bytes)
void* tpw_pt_effects() { return gEffects; }
void tpw_pt_em_init() { EM_Init(); }
int32_t tpw_pt_em_on(int32_t effect, int32_t count, int32_t emitter) { return EM_SetEmitersOn(effect, count, emitter); }
void tpw_pt_em_translation(int32_t effect, int32_t index, float x, float y, float z) {
    EM_SetEffectTranslation(effect, index, x, y, z);
}
void tpw_pt_em_orbit(int32_t effect, int32_t index, float a, float b, float c) { EM_SetEffectOrbit(effect, index, a, b, c); }
void tpw_pt_em_tick() { EM_Tick(); }
}
