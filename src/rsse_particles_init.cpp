// The particle system's start-up (main app, rsse.cpp range of the inventory, 0x100baacc..0x100be040): the library
// loader, the system's initialisation and reset, and the small helpers they use. The state is in
// src/rsse_particles_state.cpp (see include/rsse.h). The rest of the particle system (PAR_Tick, the emitters) is
// untranslated.
#include <cmath>
#include <cstring>

#include "Debug.h"
#include "FS_System.h"
#include "LbFile.h"
#include "platform/samsutils.h"
#include "rsse.h"
#include "tb_particle.h"

// A file's little-endian field in the host's order. The main app's own instances of the SamsUtilities swap templates:
// byte reversals on a big-endian host, nothing on a little-endian one. The unit's tests build with
// TPW_SWAP_FILE_WORDS to make them swap, so that the library's bytes can be compared with the original's.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define PAR_SWAP_FILE_WORDS 1
#endif

namespace SamsUtilities {

// 0x100babb4
void USwapInplace16(int16_t& value) {
#ifdef PAR_SWAP_FILE_WORDS
    value = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(value)));
#else
    (void)value;
#endif
}

// 0x100bab6c
void USwapInplace16(uint16_t& value) {
#ifdef PAR_SWAP_FILE_WORDS
    value = __builtin_bswap16(value);
#else
    (void)value;
#endif
}

// 0x100bab28
void USwapInplace32(int32_t& value) {
#ifdef PAR_SWAP_FILE_WORDS
    value = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(value)));
#else
    (void)value;
#endif
}

// 0x100baacc
void USwapBlock32(int32_t* words, int32_t count) { _USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

}  // namespace SamsUtilities

namespace {

// The texts of the main app's string table that this file uses (the table is at 0x1023ec9d).
constexpr char kReadMode[] = "rb";
constexpr char kLoadWhenNotRunning[] = "PTCL: Tried to load into an uninitialised system";
constexpr char kMallocNothing[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";

constexpr uint32_t kMaxLibraryEmitters = 0x69;
constexpr uint32_t kMaxLibraryEffecters = 0x14;

// Reads `length` bytes of the library file into `buffer`.
void ReadFile(bfl::FileSystem::CFile* file, void* buffer, uint32_t length) { LbFile_Read(file, buffer, length, nullptr); }

// Reads a word of the library file, in the host's order (the original swaps it with _USwapBlock32; the file's words are
// little-endian). It is left as it was when the file has no more.
void ReadWord(bfl::FileSystem::CFile* file, uint32_t& word) {
    ReadFile(file, &word, 4);
    SamsUtilities::_USwapBlock32(&word, 1);
}

// The same for one library effecter.
void SwapLibraryEffecter(PARLibraryEffecter& e) {
    using namespace SamsUtilities;
    USwapInplace32(e.mL00);
    USwapInplace32(e.mL04);
    USwapInplace16(e.mU08);
    USwapInplace32(e.mL0C);
    USwapInplace32(e.mL10);
    USwapInplace32(e.mL14);
    USwapInplace32(e.mL18);
    USwapInplace32(e.mL1C);
    USwapInplace32(e.mL20);
    USwapInplace32(e.mL24);
    USwapInplace32(e.mL28);
    USwapInplace32(e.mL2C);
    USwapInplace32(e.mL30);
    USwapInplace32(e.mL34);
    USwapInplace32(e.mL38);
    USwapInplace32(e.mL3C);
    USwapInplace32(e.mL40);
    USwapInplace16(e.mS46);
    USwapInplace16(e.mS4A);
    USwapInplace32(e.mL4C);
    USwapInplace16(e.mS60);
    USwapInplace16(e.mS62);
}

}  // namespace

// The loader's field-by-field swap of one library emitter (the original calls the main app's four helpers in this order; it is
// inline in PAR_LoadLibrary and in PAR_LoadLibEmitter, which shares this copy, src/rsse_particles.cpp).
void PAR_SwapLibraryEmitter(PARLibraryEmitter& e) {
    using namespace SamsUtilities;
    USwapInplace16(e.mS00);
    USwapInplace16(e.mS02);
    USwapInplace16(e.mS06);
    USwapInplace16(e.mU08);
    USwapInplace16(e.mU0A);
    USwapInplace16(e.mS0C);
    USwapInplace16(e.mS0E);
    USwapInplace16(e.mS10);
    USwapInplace16(e.mS12);
    USwapInplace32(e.mL14);
    USwapInplace32(e.mL18);
    USwapInplace32(e.mL1C);
    USwapInplace32(e.mL20);
    USwapInplace16(e.mS24);
    USwapInplace16(e.mS26);
    USwapInplace16(e.mS28);
    USwapInplace16(e.mS2A);
    USwapInplace32(e.mL2C);
    USwapInplace32(e.mL30);
    USwapInplace32(e.mL34);
    USwapInplace32(e.mL38);
    USwapInplace32(e.mL3C);
    USwapInplace32(e.mL40);
    USwapInplace32(e.mL44);
    USwapInplace32(e.mL48);
    USwapInplace32(e.mL4C);
    USwapInplace16(e.mS50);
    USwapInplace16(e.mS52);
    USwapInplace32(e.mL54);
    USwapInplace32(e.mL58);
    USwapInplace32(e.mL5C);
    USwapInplace32(e.mL60);
    USwapInplace16(e.mS64);
    USwapInplace16(e.mS66);
    USwapInplace32(e.mL70);
    USwapInplace16(e.mS74);
    USwapInplace16(e.mS76);
    USwapInplace32(e.mL78);
    USwapInplace32(e.mL7C);
    USwapInplace32(e.mL80);
    USwapInplace32(e.mL84);
    USwapInplace32(e.mL88);
    USwapInplace32(e.mL8C);
    USwapInplace32(e.mL90);
    USwapInplace16(e.mS94);
    USwapInplace16(e.mS96);
    USwapInplace32(e.mL98);
    USwapInplace32(e.mL9C);
    USwapInplace16(e.mSA0);
    USwapInplace16(e.mSA2);
    USwapInplace16(e.mSA4);
    USwapInplace16(e.mSA6);
    USwapInplace16(e.mSA8);
    USwapInplace32(e.mLAC);
    USwapInplace32(e.mLB0);
    USwapInplace32(e.mLB4);
    USwapInplace16(e.mSB8);
    USwapInplace16(e.mSBC);
    USwapInplace16(e.mSBE);
    USwapInplace16(e.mSC4);
    USwapInplace16(e.mSC6);
    USwapInplace32(e.mLC8);
    USwapInplace32(e.mLCC);
    USwapInplace16(e.mSD0);
    USwapInplace16(e.mSD2);
    USwapInplace32(e.mLD4);
    USwapBlock32(e.mLD8, 0x10);
    USwapInplace32(e.mL128);
    USwapInplace32(e.mL12C);
    USwapInplace32(e.mL130);
    USwapInplace32(e.mL134);
    USwapInplace32(e.mL138);
    USwapInplace32(e.mL13C);
}

// 0x100bdbf0: the ground height callback of a system that was given none.
int32_t DummyHeightCallBack(int32_t, int32_t) { return 0; }

// 0x100babf8
void* PAR_Malloc(int32_t size) {
    if (size == 0) {
        GAME_ASSERTMSG(0, kMallocNothing);
        return nullptr;
    }
    return LbMem_Alloc(static_cast<uint32_t>(size));
}

// 0x100ba324: 256 sin(2 pi i / 512) for the 512 angles, pi as the game has it (3.14159), cut towards zero.
int32_t* PAR_SetupSin() {
    for (uint32_t i = 0; i < 0x200; ++i) {
        const double angle = 3.14159 * static_cast<double>(i << 1) * 0.001953125;
        gParSinTable[i] = static_cast<int32_t>(256.0 * std::sin(angle));
    }
    return gParSinTable;
}

// 0x100bdd64
void PAR_ResetParticleSystem() {
    if (gParSystemRunning == 0) return;
    PARState& state = gParState;
    state.mF00 = 0;
    state.mTicks = 0;
    state.mF10 = 1;
    state.mF14 = 1;
    state.mF18 = 1;
    state.mF1A = 1;
    std::memset(state.mParticles, 0, static_cast<uint32_t>(state.mMaxParticles) * sizeof(tagPARTICLE));
    std::memset(state.mEmitters, 0, sizeof(state.mEmitters));
    std::memset(state.mEffecters, 0, sizeof(state.mEffecters));
    // The emitters: one chain of all 120, none in use.
    state.mEmittersUsed = -1;
    state.mEmittersFree = 0;
    state.mEmitters[0].mPrev = -1;
    state.mEmitters[0].mNext = 1;
    state.mEmitters[kParEmitters - 1].mNext = -1;
    state.mEmitters[kParEmitters - 1].mPrev = kParEmitters - 2;
    for (int32_t i = 1; i < kParEmitters - 1; ++i) {
        state.mEmitters[i].mPrev = static_cast<int16_t>(i - 1);
        state.mEmitters[i].mNext = static_cast<int16_t>(i + 1);
    }
    // The effecters.
    state.mEffectersUsed = -1;
    state.mEffectersFree = 0;
    state.mEffecters[0].mPrev = -1;
    state.mEffecters[0].mNext = 1;
    state.mEffecters[kParEffecters - 1].mNext = -1;
    state.mEffecters[kParEffecters - 1].mPrev = kParEffecters - 2;
    for (int32_t i = 1; i < kParEffecters - 1; ++i) {
        state.mEffecters[i].mPrev = static_cast<int16_t>(i - 1);
        state.mEffecters[i].mNext = static_cast<int16_t>(i + 1);
    }
    // The particles of the pool.
    state.mParticlesFree = 0;
    state.mParticles[0].mPrev = -1;
    state.mParticles[0].mNext = 1;
    state.mParticles[state.mMaxParticles - 1].mNext = -1;
    state.mParticles[state.mMaxParticles - 1].mPrev = static_cast<int16_t>(state.mMaxParticles - 2);
    for (int32_t i = 1; i < state.mMaxParticles - 1; ++i) {
        state.mParticles[i].mPrev = static_cast<int16_t>(i - 1);
        state.mParticles[i].mNext = static_cast<int16_t>(i + 1);
    }
    gParDrawingListBusy = 0;
}

// 0x100bdc24
void PAR_InitParticleSystem(void* owner, int32_t density, int32_t maxParticles) {
    if (gParSystemRunning != 0) return;
    if (maxParticles > 0x7fff) maxParticles = 0x7fff;
    if (maxParticles < 100) maxParticles = 100;
    PARState& state = gParState;
    std::memset(&state, 0, sizeof(state));
    state.mDensity = density;
    if (state.mDensity == 0) state.mDensity = 1;
    state.mF10 = 1;
    state.mSinTable = PAR_SetupSin();
    state.mMaxParticles = maxParticles;
    state.mLowSpec = 0;
    state.mHeightCallback = owner != nullptr ? owner : reinterpret_cast<void*>(&DummyHeightCallBack);
    state.mParticles = static_cast<tagPARTICLE*>(PAR_Malloc(state.mMaxParticles * static_cast<int32_t>(sizeof(tagPARTICLE))));
    gParSystemRunning = 1;
    PAR_ResetParticleSystem();
    // 0xaf0 bytes on the Mac: 100 records of 0x1c. The host's PTCL is bigger (a native pointer), the block holds the same 100.
    gParDrawingList = static_cast<PTCL*>(PAR_Malloc(kParDrawingListRecords * static_cast<int32_t>(sizeof(PTCL))));
    gParDrawingListBusy = 0;
}

// 0x100be03c
void PAR_SetLowSpec(int32_t on) { gParState.mLowSpec = on; }

// 0x100ba3e0
int32_t PAR_LoadLibraryResult(char* path) {
    if (path == nullptr) return 1;
    if (gParSystemRunning == 0) {
        GAME_ASSERTMSG(0, kLoadWhenNotRunning);
        return 1;
    }
    bfl::FileSystem::CFile* file = TPFile_Open(path, kReadMode);
    if (file == nullptr) return 1;
    // The original's two locals are not set when the file is too short for them.
    uint32_t count = 0;
    uint32_t size = 0;
    uint8_t* const library = reinterpret_cast<uint8_t*>(&gParLibrary);
    ReadWord(file, count);
    ReadWord(file, size);
    uint32_t shown = count < kMaxLibraryEmitters ? count : kMaxLibraryEmitters;
    ReadFile(file, library, size * shown);
    for (uint32_t i = 0; i < shown; ++i) PAR_SwapLibraryEmitter(gParLibrary.mEmitters[i]);
    // The skip over the emitters the list has beyond the 105 counts 20 of them, not 105 (a bug of the original).
    if (count > kMaxLibraryEmitters) LbFile_Seek(file, static_cast<int32_t>((count - kMaxLibraryEffecters) * size), 1);
    ReadWord(file, count);
    ReadWord(file, size);
    shown = count < kMaxLibraryEffecters ? count : kMaxLibraryEffecters;
    ReadFile(file, library + sizeof(gParLibrary.mEmitters), size * shown);
    for (uint32_t i = 0; i < shown; ++i) SwapLibraryEffecter(gParLibrary.mEffecters[i]);
    if (count > kMaxLibraryEffecters) LbFile_Seek(file, static_cast<int32_t>((count - kMaxLibraryEffecters) * size), 1);
    ReadWord(file, gParLibraryWordA);
    ReadWord(file, gParLibraryWordB);
    if (gParLibraryWordA < 10) gParLibraryWordA = 10;
    if (gParLibraryWordA > 500) gParLibraryWordA = 500;
    LbFile_Close(file);
    for (uint32_t i = 0; i < kMaxLibraryEmitters; ++i) {
        gParLibrary.mEmitters[i].mS00 = 0;
        if (gParLibrary.mEmitters[i].mSA2 == 0) gParLibrary.mEmitters[i].mSA2 = 1000;
    }
    for (uint32_t i = 0; i < kMaxLibraryEffecters; ++i) gParLibrary.mEffecters[i].mL00 = 0;
    PAR_ResetParticleSystem();
    return 0;
}

// 0x100ba3e0, the callers' form (see include/rsse.h).
void PAR_LoadLibrary(char* path) { PAR_LoadLibraryResult(path); }
