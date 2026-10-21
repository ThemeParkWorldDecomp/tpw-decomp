// Test library for unit w26a (libtpw_unit_Particles.dylib; see tests/test_Particles.py): the particle system's core, second half
// (src/game/Particles.cpp): PAR_CloseParticleSystem, PAR_Free, PAR_LoadLibEmitter, PAR_KillLibEmitter, PAR_Tick (PAR_TickResult),
// ApplyParticleDensityAdjustment, the status queries, the emitter setters and killers, PAR_GetEmitterBoundingBox and
// PAR_FreeDrawingList.
//
// Real: src/game/Particles.cpp with the start-up half (src/game/Particles.cpp: PAR_InitParticleSystem, PAR_LoadLibrary and the
// swap helpers, which the test uses to build a state from the game's own Tp2.plb) and the state's globals
// (src/game/rides/rsse.cpp). The library is built WITH TPW_SWAP_FILE_WORDS, like unit ParticlesInit's: the swap helpers
// really swap, so that PAR_LoadLibEmitter's records end up in the original's byte order and the test compares the library as it
// is. (The state's emitters, effecters, chain heads and pool stay in the host's order: the test turns them into the original's
// order field by field before it compares.)
// Fakes that record on both sides (name, then every argument): LbMem_Alloc and LbMem_Free (the library heap; the
// pool and the drawing list are labelled blocks), the file calls of the library loader and of PAR_LoadLibEmitter (TPFile_Open,
// TPFile_Read, LbFile_Read, LbFile_Seek, LbFile_Close, served from a file the case sets), the platform's _USwapBlock32,
// GAME_ASSERTMSG (src/game/advisor/Advisor.cpp, unit w22) and the three functions of unit w26c that PAR_Tick calls (DoEmitters, DoEffecters,
// DoAParticle). DoEmitters and DoAParticle can change the state while they run, from queues the case fills ("DoEmitters head":
// the emitter chain head it leaves; "DoAParticle next:<index>": the next link they give that particle), so that the order of PAR_Tick's
// reads shows.
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
#include "tb_particle.h"

using namespace calllog;

namespace {

// A queue value that means "leave the state alone".
constexpr int64_t kNoChange = 0x10000;

// The file the case sets: a byte buffer and a position; the handle is one static object.
std::uint8_t gFileData[0x20000];
int32_t gFileLength;
int32_t gFilePosition;
char gFileObject[16];
constexpr int kLabelFile = 5;

const std::uint8_t* LibraryStart() { return reinterpret_cast<const std::uint8_t*>(&gParLibrary); }

bool InLibrary(const void* p) {
    const auto* byte = static_cast<const std::uint8_t*>(p);
    return byte >= LibraryStart() && byte < LibraryStart() + sizeof(gParLibrary);
}

}  // namespace

// ---- the callees of the particle code

// The library's heap. A block of 2 MB or more is not filled (the PowerPC side does not make one either: only its label
// matters). The case's queue "LbMem_Alloc fails" refuses one.
void* LbMem_Alloc(uint32_t size) {
    const bool refuse = on() && (ret("LbMem_Alloc fails") != 0 || size >= 0x40000000u);
    void* p = refuse ? nullptr : std::malloc(size != 0 ? size : 1);
    if (p != nullptr && size < 0x200000u) std::memset(p, 0xa5, size != 0 ? size : 1);
    // The system's drawing list holds 100 records: 0xaf0 bytes of 0x1c on the Mac, more on the host (PTCL has a native pointer).
    const int64_t shown = size == kParDrawingListRecords * sizeof(PTCL) ? 0xaf0 : static_cast<int64_t>(size);
    ev("LbMem_Alloc", {shown, p != nullptr ? newlab(p) : 0});
    return p;
}

// Frees nothing: the blocks the case hands over belong to the test, and the label is what the log compares.
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }

void GAME_ASSERTMSG(int32_t condition, const char* format, ...) { ev("GAME_ASSERTMSG", {condition}, {format}); }

bfl::FileSystem::CFile* TPFile_Open(const char* name, const char* mode) {
    ev("TPFile_Open", {}, {name, mode});
    if (ret("TPFile_Open", 1) == 0) return nullptr;
    gFilePosition = 0;
    return reinterpret_cast<bfl::FileSystem::CFile*>(gFileObject);
}

// Hands over what the file has and zeroes the rest of the buffer (the PowerPC side's fake does the same). The event says which
// library slot the destination is (-1 for any other address).
uint32_t TPFile_Read(void* dest, uint32_t size, uint32_t count, bfl::FileSystem::CFile* file) {
    const int64_t slot = InLibrary(dest) ? (static_cast<const std::uint8_t*>(dest) - LibraryStart()) / 0x140 : -1;
    ev("TPFile_Read", {slot, size, count, lab(file)});
    const uint32_t length = size * count;
    const uint32_t remaining = static_cast<uint32_t>(gFileLength - gFilePosition);
    const uint32_t done = std::min(length, remaining);
    std::memcpy(dest, gFileData + gFilePosition, done);
    std::memset(static_cast<std::uint8_t*>(dest) + done, 0, length - done);
    gFilePosition += static_cast<int32_t>(done);
    return size != 0 ? done / size : 0;
}

// A short read hands over what there is and zeroes the rest of the buffer (the loader's locals are not set when the file is
// too short).
TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {lab(file), length});
    const uint32_t remaining = static_cast<uint32_t>(gFileLength - gFilePosition);
    const uint32_t count = std::min(length, remaining);
    std::memcpy(buffer, gFileData + gFilePosition, count);
    std::memset(static_cast<std::uint8_t*>(buffer) + count, 0, length - count);
    gFilePosition += static_cast<int32_t>(count);
    if (transferred != nullptr) *transferred = count;
    return TbError(0);
}

int32_t LbFile_Seek(void* file, int32_t offset, uint32_t whence) {
    ev("LbFile_Seek", {lab(file), offset, whence});
    const int64_t position = static_cast<int64_t>(gFilePosition) + offset;
    gFilePosition = static_cast<int32_t>(std::max<int64_t>(0, std::min<int64_t>(gFileLength, position)));
    return 0;
}

TbError LbFile_Close(void* file) {
    ev("LbFile_Close", {lab(file)});
    return TbError(0);
}

// The platform's byte swap does nothing on a little-endian host, and the particle loader's counts and sizes must stay as the
// file has them; the library's records, though, are swapped here (as the original does) so that their bytes can be compared.
// The event says whether the buffer is in the library, as the PowerPC side's does.
void SamsUtilities::_USwapBlock32(uint32_t* words, int32_t count) {
    ev("SwapBlock32", {count, InLibrary(words) ? 1 : 0});
    if (!InLibrary(words)) return;
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap32(words[i]);
}

// Unit w26c's functions, which PAR_Tick calls. DoEmitters can leave the chain of emitters in use changed.
void DoEmitters() {
    ev("DoEmitters");
    const int64_t head = ret("DoEmitters head", kNoChange);
    if (head != kNoChange) gParState.mEmittersUsed = static_cast<int16_t>(head);
}

void DoEffecters() { ev("DoEffecters"); }

// DoAParticle can change the link of the particle it was called for (it frees particles); the queue is per particle
// ("DoAParticle next:<index>"), so that a case can keep every walk finite.
void DoAParticle(int32_t particle) {
    ev("DoAParticle", {particle});
    const int64_t next = ret("DoAParticle next", particle, kNoChange);
    if (next != kNoChange) gParState.mParticles[particle].mNext = static_cast<int16_t>(next);
}

// ---- the file, the state and its pokes

extern "C" {

void tpw_pa_set_file(const std::uint8_t* data, int32_t length) {
    std::memcpy(gFileData, data, static_cast<std::size_t>(length));
    gFileLength = length;
    gFilePosition = 0;
}
void tpw_pa_file_label() { tpw_cl_label(kLabelFile, gFileObject); }
void* tpw_pa_file_object() { return gFileObject; }

// Puts the state, the library and the sines into a known condition: every byte of them `fill`, then the running flag, the
// maximum and the pool as given; the drawing list and the allocation count are cleared.
void tpw_pa_prepare(int32_t fill, int32_t running, int32_t maximum, void* pool) {
    std::memset(&gParState, fill, sizeof(gParState));
    std::memset(&gParLibrary, fill, sizeof(gParLibrary));
    std::memset(gParSinTable, fill, sizeof(gParSinTable));
    gParSystemRunning = running;
    gParDrawingListBusy = fill;
    gParDrawingList = nullptr;
    gParLibraryWordA = 0xa5a5a5a5u;
    gParLibraryWordB = 0xa5a5a5a5u;
    gParAllocBalance = 0;
    gParState.mMaxParticles = maximum;
    gParState.mParticles = static_cast<tagPARTICLE*>(pool);
}

// The first byte of a region of the state (the numbers are those of tpw_pa_poke below, 1 to 5).
static std::uint8_t* RegionBase(int32_t region) {
    PARState& s = gParState;
    switch (region) {
    case 1: return reinterpret_cast<std::uint8_t*>(s.mEmitters);
    case 2: return reinterpret_cast<std::uint8_t*>(s.mEffecters);
    case 3: return reinterpret_cast<std::uint8_t*>(&s.mEmittersUsed);
    case 4: return reinterpret_cast<std::uint8_t*>(&gParLibrary);
    default: return reinterpret_cast<std::uint8_t*>(s.mParticles);
    }
}

// A field of the state as the case wants it, in the host's order. Regions: 0 the header (offset = the Mac's offset below 0x2c),
// 1 the emitters, 2 the effecters, 3 the chain heads and what follows (from mEmittersUsed), 4 the library, 5 the pool, 6 a global
// (offset: 0 running, 1 the drawing list's busy flag, 2 gParAllocBalance, 3 the library's word A, 4 its word B, 5 the drawing
// list's pointer, which is `value` as a label-less address).
void tpw_pa_poke(int32_t region, int32_t offset, int32_t width, int64_t value) {
    PARState& s = gParState;
    std::uint8_t* base = nullptr;
    switch (region) {
    case 0:
        switch (offset) {
        case 0x00: std::memcpy(&s.mF00, &value, 4); break;
        case 0x04: std::memcpy(&s.mTicks, &value, 4); break;
        case 0x08: std::memcpy(&s.mLowSpec, &value, 4); break;
        case 0x0c: std::memcpy(&s.mMaxParticles, &value, 4); break;
        case 0x10: std::memcpy(&s.mF10, &value, 4); break;
        case 0x14: std::memcpy(&s.mF14, &value, 4); break;
        case 0x18: std::memcpy(&s.mF18, &value, 2); break;
        case 0x1a: std::memcpy(&s.mF1A, &value, 2); break;
        case 0x20: std::memcpy(&s.mDensity, &value, 4); break;
        }
        return;
    case 6:
        switch (offset) {
        case 0: gParSystemRunning = static_cast<int32_t>(value); break;
        case 1: gParDrawingListBusy = static_cast<int32_t>(value); break;
        case 2: gParAllocBalance = static_cast<int32_t>(value); break;
        case 3: gParLibraryWordA = static_cast<uint32_t>(value); break;
        case 4: gParLibraryWordB = static_cast<uint32_t>(value); break;
        case 5: gParDrawingList = reinterpret_cast<PTCL*>(static_cast<std::uintptr_t>(value)); break;
        }
        return;
    default: base = RegionBase(region); break;
    }
    std::memcpy(base + offset, &value, static_cast<std::size_t>(width));
}

// A run of bytes into a region (the emitters, effecters, chain heads, library or pool), as the test hands them over: a live
// emitter made from a record of the game's library, in the host's order.
void tpw_pa_poke_bytes(int32_t region, int32_t offset, const std::uint8_t* data, int32_t length) {
    std::memcpy(RegionBase(region) + offset, data, static_cast<std::size_t>(length));
}

// The set-up through the real start-up half, as the game does it (a pool and a drawing list made, the library read).
void tpw_pa_init(void* owner, int32_t density, int32_t maxParticles) { PAR_InitParticleSystem(owner, density, maxParticles); }
int32_t tpw_pa_load(char* path) { return PAR_LoadLibraryResult(path); }

// ---- the functions under test
int32_t tpw_pa_tick() { return PAR_TickResult(); }
void tpw_pa_tick_void() { PAR_Tick(); }
void tpw_pa_close() { PAR_CloseParticleSystem(); }
void tpw_pa_free(void* block) { PAR_Free(block); }
int32_t tpw_pa_load_emitter(void* file) { return PAR_LoadLibEmitter(reinterpret_cast<bfl::FileSystem::CFile*>(file)); }
void tpw_pa_kill_lib(uint32_t slot) { PAR_KillLibEmitter(slot); }
void tpw_pa_density(int32_t emitter) { ApplyParticleDensityAdjustment(emitter); }
int32_t tpw_pa_emitter_status(uint32_t handle) { return PAR_GetEmitterStatus(handle); }
int32_t tpw_pa_effecter_status(uint32_t handle) { return PAR_GetEffecterStatus(handle); }
void tpw_pa_kill_emitter(int32_t handle) { PAR_KillEmitter(handle); }
void tpw_pa_kill_2d() { PAR_KillAndHideAll2DObjects(); }
void tpw_pa_launch(int32_t handle, int32_t x, int32_t y, int32_t z) { PAR_SetEmitterLaunchVector(handle, x, y, z); }
void tpw_pa_pos(int32_t handle, int32_t x, int32_t y, int32_t z) { PAR_SetEmitterPos(handle, x, y, z); }
void tpw_pa_mask(int32_t handle, int32_t mask) { PAR_SetEmitterMask(handle, mask); }
void tpw_pa_life(int32_t handle, int32_t life) { PAR_SetEmitterLife(handle, life); }
void tpw_pa_scale(int32_t handle, int32_t scale) { PAR_SetEmitterScale(handle, scale); }
void tpw_pa_random_pos(int32_t handle, int32_t x, int32_t y, int32_t z) { PAR_SetEmitterRandomPos(handle, x, y, z); }
void tpw_pa_bbox(uint32_t emitter, void* min, void* max) {
    PAR_GetEmitterBoundingBox(emitter, static_cast<PARPOINT3D*>(min), static_cast<PARPOINT3D*>(max));
}
void tpw_pa_free_list(void* list) { PAR_FreeDrawingList(static_cast<PTCL*>(list)); }

// ---- what the case compares

// header[]: 0 mF00, 1 mTicks, 2 mLowSpec, 3 mMaxParticles, 4 mF10, 5 mF14, 6 mF18, 7 mF1A, 8 sin table is gParSinTable, 9 mDensity,
// 10 height callback (0 none, 1 DummyHeightCallBack, 2 other), 11 the pool's label.
void tpw_pa_header(int64_t* out) {
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
    out[10] = s.mHeightCallback == nullptr ? 0 : s.mHeightCallback == reinterpret_cast<const void*>(&DummyHeightCallBack) ? 1 : 2;
    out[11] = lab(s.mParticles);
}
// globals[]: 0 running, 1 the drawing list's busy flag, 2 the drawing list's label, 3 word A, 4 word B, 5 gParAllocBalance.
void tpw_pa_globals(int64_t* out) {
    out[0] = gParSystemRunning;
    out[1] = gParDrawingListBusy;
    out[2] = lab(gParDrawingList);
    out[3] = gParLibraryWordA;
    out[4] = gParLibraryWordB;
    out[5] = gParAllocBalance;
}
void* tpw_pa_drawing_list() { return gParDrawingList; }
// Regions: 0 emitters, 1 effecters, 2 the chain heads (mEmittersUsed to the end), 3 the library, 4 the pool, 5 the sines.
void* tpw_pa_region(int32_t which, int32_t* size) {
    switch (which) {
    case 0: *size = sizeof(gParState.mEmitters); return gParState.mEmitters;
    case 1: *size = sizeof(gParState.mEffecters); return gParState.mEffecters;
    case 2: *size = 0x1c; return &gParState.mEmittersUsed;
    case 3: *size = sizeof(gParLibrary); return &gParLibrary;
    case 4: *size = gParState.mMaxParticles > 0 ? gParState.mMaxParticles * static_cast<int32_t>(sizeof(tagPARTICLE)) : 0;
            return gParState.mParticles;
    default: *size = sizeof(gParSinTable); return gParSinTable;
    }
}

}
