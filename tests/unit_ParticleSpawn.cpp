// Test library for unit w26b (libtpw_unit_ParticleSpawn.dylib; see tests/test_ParticleSpawn.py): the particle system's
// spawning (SpawnParticle, PAR_InitEmitter, PAR_InitEmitterLaunch, PAR_InitEffecter, DoSpecialEmitter, SpecialEmitter1).
//
// Real: src/rsse_spawn.cpp, and from unit w23 the system's set-up (src/rsse_particles_init.cpp: PAR_ResetParticleSystem builds the
// chains of a case, PAR_SetupSin the table of sines, PAR_LoadLibrary the library from the real Tp2.plb) with the state
// (src/rsse_particles_state.cpp). The library file is read at run time by the test; nothing of the game is copied here.
// Fakes that record on both sides (name, then every argument): ApplyParticleDensityAdjustment (unit w26a's function, called
// by the starters; it also writes the new emitter's start count and life from the case's queues "count" and "life", so
// that a case can steer them), and the marks the test puts between its steps. The loader's callees (the file calls, the
// byte swap, the assertion, the heap) are fakes that are used once, while the test loads the library.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "LbFile.h"
#include "calllog.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

// ---- the callee of unit w26a: records the emitter's number and lets the case set the start count and the life.
void ApplyParticleDensityAdjustment(int32_t emitter) {
    ev("ApplyParticleDensityAdjustment", {emitter});
    PAREmitter& record = gParState.mEmitters[emitter];
    record.mStartCount = static_cast<int32_t>(ret("count", record.mStartCount));
    record.mLife = static_cast<int32_t>(ret("life", record.mLife));
}

// ---- the loader's callees (used while the library is loaded, with the recording off)
void* LbMem_Alloc(uint32_t size) { return std::malloc(size != 0 ? size : 1); }
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

namespace {
// The library file: a byte buffer and a position; the handle is one static object.
std::uint8_t gFileData[0x20000];
int32_t gFileLength;
int32_t gFilePosition;
char gFileObject[16];
}  // namespace

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    gFilePosition = 0;
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject);
}

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    const uint32_t remaining = static_cast<uint32_t>(gFileLength - gFilePosition);
    const uint32_t count = std::min(length, remaining);
    std::memcpy(buffer, gFileData + gFilePosition, count);
    std::memset(static_cast<std::uint8_t*>(buffer) + count, 0, length - count);
    gFilePosition += static_cast<int32_t>(count);
    if (transferred != nullptr) *transferred = count;
    return TbError(0);
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    gFilePosition = std::max<int32_t>(0, std::min<int32_t>(gFileLength, gFilePosition + offset));
    return 0;
}

TbError LbFile_Close(void* file) { return TbError(0); }

// The file is little-endian and so is the host: the loader's swaps change nothing.
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) {}

extern "C" {

// ---- the library and the table of sines, made once by the real loader and PAR_SetupSin

// Loads the library file `data` (`length` bytes) the way the game does, with the system marked running and a pool of one particle for
// the reset the loader ends with; answers the loader's result (0: loaded).
int32_t tpw_ps_load(const std::uint8_t* data, int32_t length, void* scratch) {
    std::memcpy(gFileData, data, static_cast<std::size_t>(length));
    gFileLength = length;
    gFilePosition = 0;
    std::memset(&gParState, 0, sizeof(gParState));
    gParState.mMaxParticles = 1;
    gParState.mParticles = static_cast<tagPARTICLE*>(scratch);
    gParSystemRunning = 1;
    char path[] = "Data:Particle:Tp2.plb";
    return PAR_LoadLibraryResult(path);
}
void tpw_ps_sin() { PAR_SetupSin(); }

// ---- the state of a case

// Regions: 0 the emitters, 1 the effecters, 2 the chain heads (mEmittersUsed to the end), 3 the library, 4 the table of sines,
// 5 the pool (its length is the state's maximum).
void* tpw_ps_region(int32_t which, int32_t* size) {
    switch (which) {
    case 0: *size = sizeof(gParState.mEmitters); return gParState.mEmitters;
    case 1: *size = sizeof(gParState.mEffecters); return gParState.mEffecters;
    case 2: *size = 0x1c; return &gParState.mEmittersUsed;
    case 3: *size = sizeof(gParLibrary); return &gParLibrary;
    case 4: *size = sizeof(gParSinTable); return gParSinTable;
    default: *size = gParState.mMaxParticles * static_cast<int32_t>(sizeof(tagPARTICLE)); return gParState.mParticles;
    }
}

// Starts a case: the state cleared, the pool given, the library and the sines put in, the system marked as `running`, then the
// real reset builds the chains (it needs the system running; a case that is not running keeps all-zero chains).
void tpw_ps_begin(int32_t running, int32_t maximum, void* pool, const std::uint8_t* library, const std::uint8_t* sines) {
    std::memset(&gParState, 0, sizeof(gParState));
    gParState.mMaxParticles = maximum;
    gParState.mParticles = static_cast<tagPARTICLE*>(pool);
    gParState.mSinTable = gParSinTable;
    gParSystemRunning = running;
    std::memcpy(&gParLibrary, library, sizeof(gParLibrary));
    std::memcpy(gParSinTable, sines, sizeof(gParSinTable));
    PAR_ResetParticleSystem();
}

// The header's words as a flat list: 0 mF00 (the particles made), 1 mTicks, 2 mLowSpec, 3 mMaxParticles, 4 mF10 (the seed), 5 mF14,
// 6 mF18 (the emitters' serial), 7 mF1A (the effecters' serial), 8 the table of sines is gParSinTable, 9 mDensity, 10 the running
// flag, 11 the pool's label.
void tpw_ps_header(int64_t* out) {
    const PARState& s = gParState;
    out[0] = s.mF00;
    out[1] = s.mTicks;
    out[2] = s.mLowSpec;
    out[3] = s.mMaxParticles;
    out[4] = s.mF10;
    out[5] = s.mF14;
    out[6] = s.mF18;
    out[7] = s.mF1A;
    out[8] = s.mSinTable == gParSinTable ? 1 : (s.mSinTable == nullptr ? 0 : 2);
    out[9] = s.mDensity;
    out[10] = gParSystemRunning;
    out[11] = lab(s.mParticles);
}
// A word of the header as a case wants it (the indices of tpw_ps_header; the serials are halfwords).
void tpw_ps_set_header(int32_t index, int64_t value) {
    PARState& s = gParState;
    switch (index) {
    case 0: s.mF00 = static_cast<int32_t>(value); break;
    case 1: s.mTicks = static_cast<int32_t>(value); break;
    case 2: s.mLowSpec = static_cast<int32_t>(value); break;
    case 4: s.mF10 = static_cast<int32_t>(value); break;
    case 5: s.mF14 = static_cast<int32_t>(value); break;
    case 6: s.mF18 = static_cast<int16_t>(value); break;
    case 7: s.mF1A = static_cast<int16_t>(value); break;
    case 9: s.mDensity = static_cast<int32_t>(value); break;
    case 10: gParSystemRunning = static_cast<int32_t>(value); break;
    }
}

// ---- the functions under test
void tpw_ps_mark(int32_t number) { ev("step", {number}); }
uint32_t tpw_ps_init(int32_t type, int32_t x, int32_t y, int32_t z) { return PAR_InitEmitter(type, x, y, z); }
uint32_t tpw_ps_launch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz) {
    return PAR_InitEmitterLaunch(type, x, y, z, vx, vy, vz);
}
uint32_t tpw_ps_effecter(int32_t type, int32_t x, int32_t y, int32_t z) { return PAR_InitEffecter(type, x, y, z); }
int32_t tpw_ps_spawn(int32_t emitter) { return SpawnParticle(emitter); }
void tpw_ps_dospecial(int32_t emitter, int32_t start) { DoSpecialEmitter(emitter, start); }
void tpw_ps_special(int32_t emitter, int32_t start) { SpecialEmitter1(emitter, start); }
}
