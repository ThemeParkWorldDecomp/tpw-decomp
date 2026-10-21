// Test library for unit w168 (libtpw_unit_ParticleSave.dylib and, built with TPW_SWAP_FILE_WORDS, libtpw_unit_ParticleSwap.dylib; see
// tests/test_ParticleSave.py and tests/test_ParticleSwap.py): the particle system's save and load and their swap helpers
// (src/game/Particles.cpp) over the real state and library (src/game/rides/rsse.cpp).
//
// Fakes that record on both sides (same event name and arguments as the PowerPC stubs): LbMem_Alloc (the queue "alloc" can make it
// fail; a block starts as 0xa5 bytes), LbMem_Free (records only), LbFile_Read (serves the case's stream, zero past its end),
// LbFile_Write (appends to the written stream), _USwapBlock32, _USwapCopy32, _USwapDelete and GAME_ASSERTMSG. The four swap helpers of
// the main app (src/game/Particles.cpp, not linked here) are copied below, with the same rule: they swap only when
// TPW_SWAP_FILE_WORDS is defined (the Swap library), so that the Save library, which does not swap, compares files byte for byte with
// the original's little-endian ones while the Swap library shows every field list.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Debug.h"
#include "LbFile.h"
#include "calllog.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "rsse.h"

using namespace calllog;

#if defined(TPW_SWAP_FILE_WORDS)
#define UNIT_SWAPS 1
#endif

namespace SamsUtilities {
void USwapInplace16(int16_t& value) {
#ifdef UNIT_SWAPS
    value = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(value)));
#else
    (void)value;
#endif
}
void USwapInplace16(uint16_t& value) {
#ifdef UNIT_SWAPS
    value = __builtin_bswap16(value);
#else
    (void)value;
#endif
}
void USwapInplace32(int32_t& value) {
#ifdef UNIT_SWAPS
    value = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(value)));
#else
    (void)value;
#endif
}
void USwapBlock32(int32_t* words, int32_t count) { _USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

void _USwapBlock32(uint32_t* words, int32_t count) {
    ev("USwapBlock32", {count});
#ifdef UNIT_SWAPS
    for (int32_t i = 0; i < count; ++i) words[i] = __builtin_bswap32(words[i]);
#else
    (void)words;
#endif
}
uint32_t* _USwapCopy32(const uint32_t* words, int32_t count) {
    uint32_t* copy = static_cast<uint32_t*>(std::malloc(4 * (count > 0 ? count : 1)));
    for (int32_t i = 0; i < count; ++i) {
#ifdef UNIT_SWAPS
        copy[i] = __builtin_bswap32(words[i]);
#else
        copy[i] = words[i];
#endif
    }
    ev("USwapCopy32", {count, count > 0 ? static_cast<int64_t>(words[0]) : 0, newlab(copy)});
    return copy;
}
void _USwapDelete(void* copy) {
    ev("USwapDelete", {lab(copy)});
    std::free(copy);
}
}  // namespace SamsUtilities

namespace {
std::vector<uint8_t> gIn;       // the stream LbFile_Read serves
size_t gInPos = 0;
std::vector<uint8_t> gOut;      // what LbFile_Write got, in order
}  // namespace

// ---- the fakes
void* LbMem_Alloc(uint32_t size) {
    if (ret("alloc", 1) == 0) {
        ev("LbMem_Alloc", {static_cast<int64_t>(size), 0});
        return nullptr;
    }
    void* block = tpw::ArenaAllocZeroed(size);
    std::memset(block, 0xA5, size);
    ev("LbMem_Alloc", {static_cast<int64_t>(size), newlab(block)});
    return block;
}
void LbMem_Free(void* block) { ev("LbMem_Free", {lab(block)}); }

TbError LbFile_Read(void* file, void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Read", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    std::memset(buffer, 0, length);
    const size_t left = gInPos < gIn.size() ? gIn.size() - gInPos : 0;
    const size_t n = length < left ? length : left;
    if (n) std::memcpy(buffer, gIn.data() + gInPos, n);
    gInPos += length;
    return TbError(0);
}
TbError LbFile_Write(void* file, const void* buffer, uint32_t length, uint32_t* transferred) {
    ev("LbFile_Write", {static_cast<int64_t>(reinterpret_cast<intptr_t>(file)), static_cast<int64_t>(length), transferred != nullptr});
    const uint8_t* bytes = static_cast<const uint8_t*>(buffer);
    gOut.insert(gOut.end(), bytes, bytes + length);
    return TbError(0);
}
void GAME_ASSERTMSG(int32_t condition, const char* format, ...) {
    (void)format;
    ev("GAME_ASSERTMSG", {condition});
}

extern "C" {

uint64_t tpw_px_addr(const char* name) {
    if (std::strcmp(name, "library") == 0) return reinterpret_cast<uintptr_t>(&gParLibrary);
    if (std::strcmp(name, "running") == 0) return reinterpret_cast<uintptr_t>(&gParSystemRunning);
    return 0;
}
uint64_t tpw_px_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The host's offsets and sizes the module relies on.
int32_t tpw_px_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(__builtin_offsetof(PARState, mEmitters));
    out[n++] = static_cast<int64_t>(__builtin_offsetof(PARState, mEmittersUsed));
    out[n++] = static_cast<int64_t>(sizeof(PARState));
    out[n++] = static_cast<int64_t>(sizeof(PAREmitter));
    out[n++] = static_cast<int64_t>(sizeof(PAREffecter));
    out[n++] = static_cast<int64_t>(sizeof(PARLibrary));
    out[n++] = static_cast<int64_t>(sizeof(tagPARTICLE));
    return n;
}

// The state as the Mac's image of it (0x9e68 bytes, fields in this machine's order; the three pointer words are not part of it).
void tpw_px_state_set(const uint8_t* image) {
    int32_t w[6];
    std::memcpy(w, image, 24);
    gParState.mF00 = w[0];
    gParState.mTicks = w[1];
    gParState.mLowSpec = w[2];
    gParState.mMaxParticles = w[3];
    gParState.mF10 = w[4];
    gParState.mF14 = w[5];
    std::memcpy(&gParState.mF18, image + 0x18, 2);
    std::memcpy(&gParState.mF1A, image + 0x1a, 2);
    std::memcpy(&gParState.mDensity, image + 0x20, 4);
    std::memcpy(&gParState.mEmitters, image + 0x2c, 0x9e68 - 0x2c);
}
void tpw_px_state_get(uint8_t* image) {
    std::memset(image, 0, 0x9e68);
    const int32_t w[6] = {gParState.mF00, gParState.mTicks, gParState.mLowSpec, gParState.mMaxParticles, gParState.mF10, gParState.mF14};
    std::memcpy(image, w, 24);
    std::memcpy(image + 0x18, &gParState.mF18, 2);
    std::memcpy(image + 0x1a, &gParState.mF1A, 2);
    std::memcpy(image + 0x20, &gParState.mDensity, 4);
    std::memcpy(image + 0x2c, &gParState.mEmitters, 0x9e68 - 0x2c);
}
void tpw_px_set_ptrs(uint64_t pool, uint64_t sin, uint64_t callback) {
    gParState.mParticles = reinterpret_cast<tagPARTICLE*>(static_cast<uintptr_t>(pool));
    gParState.mSinTable = reinterpret_cast<const int32_t*>(static_cast<uintptr_t>(sin));
    gParState.mHeightCallback = reinterpret_cast<void*>(static_cast<uintptr_t>(callback));
}
void tpw_px_get_ptrs(uint64_t* out) {
    out[0] = reinterpret_cast<uintptr_t>(gParState.mParticles);
    out[1] = reinterpret_cast<uintptr_t>(gParState.mSinTable);
    out[2] = reinterpret_cast<uintptr_t>(gParState.mHeightCallback);
}

void tpw_px_file_in(const uint8_t* data, int32_t length) {
    gIn.assign(data, data + length);
    gInPos = 0;
    gOut.clear();
}
int32_t tpw_px_out_length() { return static_cast<int32_t>(gOut.size()); }
void tpw_px_out_data(uint8_t* out) {
    if (!gOut.empty()) std::memcpy(out, gOut.data(), gOut.size());
}

int32_t tpw_px_save(uint64_t file, int32_t with_particles) {
    return PAR_SaveStatus(reinterpret_cast<void*>(static_cast<uintptr_t>(file)), with_particles);
}
int32_t tpw_px_load(uint64_t file) { return PAR_LoadStatus(reinterpret_cast<void*>(static_cast<uintptr_t>(file))); }

void tpw_px_byteswap(uint64_t particle) { reinterpret_cast<tagPARTICLE*>(static_cast<uintptr_t>(particle))->Byteswap(); }
void tpw_px_swap_emitters(uint64_t records, uint32_t count) {
    SamsUtilities::USwapBlockStruct(reinterpret_cast<PARLibraryEmitter*>(static_cast<uintptr_t>(records)), count);
}
void tpw_px_swap_effecters(uint64_t records, uint32_t count) {
    SamsUtilities::USwapBlockStruct(reinterpret_cast<PARLibraryEffecter*>(static_cast<uintptr_t>(records)), count);
}
uint32_t tpw_px_swap_particle_ptr(uint32_t word) { return SamsUtilities::USwap32ParticlePtr(word); }
uint32_t tpw_px_swap_void_ptr(uint32_t word) { return SamsUtilities::USwap32VoidPtr(word); }
uint32_t tpw_px_swap_long_ptr(uint32_t word) { return SamsUtilities::USwap32LongPtr(word); }

}  // extern "C"
