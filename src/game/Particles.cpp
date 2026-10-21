// Particles.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Debug.h"
#include "FS_System.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "OptionsBalance.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "rsse.h"
#include "tb_particle.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

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

// The lobby's flying meshes (main app of SimTheme Park 2.04b3, 0x100aa5d4..0x100aba0c; the
// inventory files them under lobby_globe.cpp): CLBFlyingMeshInstance 0x100aa5d4 (constructor),
// 0x100aa9dc (destructor), 0x100aaa80 (Tick); CLBFlyingMeshGroup 0x100aae1c, 0x100ab008, 0x100ab1a8,
// 0x100ab2fc, 0x100ab3b4, 0x100ab464, 0x100ab4d8; CLBFlyingMeshSystem 0x100ab578, 0x100ab620,
// 0x100ab874, 0x100ab8ac, 0x100ab8f0, 0x100ab99c, 0x100aba0c (Init, Tick and Render are in
// src/lobby_islands_tick.cpp); and two odds and ends: CLBLobby::WindowMessagesFn 0x100a6484 and
// PAR_GetEmitterParticleCount 0x100ba170.
//
// The theme description's FLYINGMESH( lines make groups (ParseThemeDescriptor). A group
// registers with the system and then makes its instances a few at a time on its ticks, up to the
// share of its count that the LOBBYOBJECTS option allows. An instance is a mesh of the lobby's
// mesh system that flies from one random point of the group's box to another: each tick it turns
// a tenth of the way towards the goal, moves, and picks a new goal when it is near enough.

// ---- PAR_GetEmitterParticleCount

int32_t PAR_GetEmitterParticleCount(uint32_t emitter) {
    if (gParSystemRunning != 0) {
        const PAREmitter& record = gParEmitters[emitter & 0xffff];
        if (record.mInUse != 0) return record.mParticles;
    }
    return 0;
}

// The particle system's start-up (main app, rsse.cpp range of the inventory, 0x100baacc..0x100be040): the library
// loader, the system's initialisation and reset, and the small helpers they use. The state is in
// src/rsse_particles_state.cpp (see include/rsse.h). The rest of the particle system (PAR_Tick, the emitters) is
// untranslated.

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

// 0x100ba324: 256 sin(2 pi i / 512) for the 512 angles, pi as the game has it (3.14159), cut towards zero.
int32_t* PAR_SetupSin() {
    for (uint32_t i = 0; i < 0x200; ++i) {
        const double angle = 3.14159 * static_cast<double>(i << 1) * 0.001953125;
        gParSinTable[i] = static_cast<int32_t>(256.0 * std::sin(angle));
    }
    return gParSinTable;
}

// 0x100babf8
void* PAR_Malloc(int32_t size) {
    if (size == 0) {
        GAME_ASSERTMSG(0, kMallocNothing);
        return nullptr;
    }
    return LbMem_Alloc(static_cast<uint32_t>(size));
}

// The particle system's save and load (main app, rsse.cpp range of the inventory, 0x100bacac..0x100bbc2c): PAR_SaveStatus and
// PAR_LoadStatus with their swap helpers (the main app's own instances of the SamsUtilities templates for the system's records and
// pointers) and tagPARTICLE::Byteswap. The state, the library and the pool go to a file as the Mac writes them: little-endian, every
// field turned (the original swaps a copy on the way out and the live block on the way in).
//
// The host's state is not the Mac's image: its header holds native pointers (include/rsse.h, PARState), so the save builds the Mac's
// image field by field with the three pointer words written as 0, and the load reads the image into a scratch block of the Mac's
// layout and copies it into the state field by field. A little-endian host does not swap (the file is little-endian too); the
// swaps below do something only when PAR_SWAP_FILE_WORDS is defined, as in src/rsse_particles_init.cpp.

namespace {

constexpr uint32_t kMagic = 0x5054434c;          // "PTCL"
constexpr uint32_t kStateSize = 0x9e68;
constexpr uint32_t kLibrarySize = 0x8b60;

// The Mac's image of the state (0x9e68 bytes): the header with its pointers as 32-bit words, then the records as the state holds
// them (a live emitter or effecter has the library record's layout, include/rsse.h).
struct StateImage {
    int32_t mF00;                                   // 0x00
    int32_t mTicks;                                 // 0x04
    int32_t mLowSpec;                               // 0x08
    int32_t mMaxParticles;                          // 0x0c
    int32_t mF10;                                   // 0x10
    int32_t mF14;                                   // 0x14
    uint16_t mF18;                                  // 0x18
    uint16_t mF1A;                                  // 0x1a
    uint32_t mSinTable;                             // 0x1c  the Mac's pointer
    int32_t mDensity;                               // 0x20
    uint32_t mHeightCallback;                       // 0x24  the Mac's pointer
    uint32_t mParticles;                            // 0x28  the Mac's pointer
    PARLibraryEmitter mEmitters[kParEmitters];      // 0x2c
    PARLibraryEffecter mEffecters[kParEffecters];   // 0x962c
    int16_t mHeads[5];                              // 0x9e4c  emitters used, effecters used, emitters free, effecters free, particles free
    uint8_t mTail[0x12];                            // 0x9e56
};
static_assert(sizeof(StateImage) == kStateSize && __builtin_offsetof(StateImage, mEmitters) == 0x2c &&
                  __builtin_offsetof(StateImage, mEffecters) == 0x962c && __builtin_offsetof(StateImage, mHeads) == 0x9e4c,
              "the state's image is 0x9e68 bytes as on the Mac");
// What follows the header in the host's state is the Mac's image from +0x2c on (the static_asserts of include/rsse.h).
constexpr size_t kRecordsSize = kStateSize - 0x2c;

// The swaps of the state image (shared by the save and the load, in the original's order).
void SwapStateImage(StateImage* image) {
    using namespace SamsUtilities;
    USwapInplace32(image->mF00);
    USwapInplace32(image->mTicks);
    USwapInplace32(image->mLowSpec);
    USwapInplace32(image->mMaxParticles);
    USwapInplace32(image->mF10);
    USwapInplace32(image->mF14);
    USwapInplace16(image->mF18);
    USwapInplace16(image->mF1A);
    image->mSinTable = USwap32LongPtr(image->mSinTable);
    USwapInplace32(image->mDensity);
    image->mHeightCallback = USwap32VoidPtr(image->mHeightCallback);
    image->mParticles = USwap32ParticlePtr(image->mParticles);
    USwapBlockStruct(image->mEmitters, kParEmitters);
    USwapBlockStruct(image->mEffecters, kParEffecters);
    USwapInplace16(image->mHeads[0]);
    USwapInplace16(image->mHeads[1]);
    USwapInplace16(image->mHeads[2]);
    USwapInplace16(image->mHeads[3]);
    USwapInplace16(image->mHeads[4]);
}

// The swaps of the library (emitters, then effecters).
void SwapLibrary(PARLibrary* library) {
    SamsUtilities::USwapBlockStruct(library->mEmitters, kParLibraryEmitters);
    SamsUtilities::USwapBlockStruct(library->mEffecters, kParLibraryEffecters);
}

// One 4-byte word of the file: a byte-swapped copy of it, written only while the system runs, then the copy is deleted. (The original
// does this inline for each word, with the word on its stack or the running flag itself.)
void WriteWord(void* file, const uint32_t* word) {
    uint32_t* copy = SamsUtilities::_USwapCopy32(word, 1);
    if (gParSystemRunning != 0) LbFile_Write(file, copy, 4, nullptr);
    SamsUtilities::_USwapDelete(copy);
}

// One 4-byte word of the file, in the host's order (read only while the system runs; a short file leaves it as it was).
void ReadWord_2(void* file, uint32_t& word) {
    if (gParSystemRunning != 0) LbFile_Read(file, &word, 4, nullptr);
    SamsUtilities::_USwapBlock32(&word, 1);
}

// PAR_Malloc as the listing has it inline (0x100babf8): a zero size asserts and answers NULL.
void* MallocInline(uint32_t size) {
    if (size != 0) return LbMem_Alloc(size);
    GAME_ASSERTMSG(0, kMallocNothing);
    return nullptr;
}

}  // namespace

// The particle system's core, second half (main app, rsse.cpp range of the inventory, 0x100ba1e0..0x100bf1b0): closing the
// system, the emitter library's per-emitter loader, the tick, the density adjustment, the emitter setters, queries and
// killers. The start-up half is src/rsse_particles_init.cpp, the state src/rsse_particles_state.cpp, the layouts include/rsse.h.
//
// A handle to an emitter or effecter is a 32-bit number: the index in the low half and the serial in the high half (the
// record's own serial must match, which tells a handle to a dead and reused record from a live one). The setters check it
// with PAR_GetEmitterStatus and do nothing otherwise. The index of a live emitter is below 120.

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

namespace SamsUtilities {

// 0x100bb6ac (USwap32<tagPARTICLE*>): a pointer's 32-bit word turned.
uint32_t USwap32ParticlePtr(uint32_t word) {
#ifdef PAR_SWAP_FILE_WORDS
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// 0x100bb720 (USwap32<void*>)
uint32_t USwap32VoidPtr(uint32_t word) {
#ifdef PAR_SWAP_FILE_WORDS
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// 0x100bb77c (USwap32<long*>)
uint32_t USwap32LongPtr(uint32_t word) {
#ifdef PAR_SWAP_FILE_WORDS
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// 0x100bb260 (USwapBlockStruct<tagEFFECTER>): the field list of the effecter record (the original's own copy of
// SwapLibraryEffecter in src/rsse_particles_init.cpp).
void USwapBlockStruct(PARLibraryEffecter* records, uint32_t count) {
    while (count-- != 0) {
        PARLibraryEffecter& e = *records++;
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
}

// 0x100bb3b8 (USwapBlockStruct<tagEMITTER>): the field list of the emitter record (PAR_SwapLibraryEmitter's, which the original
// has inline in PAR_LoadLibrary and again here).
void USwapBlockStruct(PARLibraryEmitter* records, uint32_t count) {
    while (count-- != 0) {
        PARLibraryEmitter& e = *records++;
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
}

}  // namespace SamsUtilities

// 0x100bacac (1084 bytes): the file gets "PTCL", the running flag and, while the system runs: the state's image (size word, then the
// image), the library's (the same) and, with `withParticles`, the pool (its size, then it). Every swap is of a copy. A block that
// cannot be allocated ends the save with 1 (what is written stays written); the answer is 0 otherwise.
int32_t PAR_SaveStatus(void* file, int32_t withParticles) {
    uint32_t word = kMagic;
    WriteWord(file, &word);
    WriteWord(file, reinterpret_cast<const uint32_t*>(&gParSystemRunning));
    if (gParSystemRunning == 0) return 0;

    // The state: the Mac's image of it, swapped, with the three pointer words 0.
    StateImage* image = static_cast<StateImage*>(LbMem_Alloc(kStateSize));
    if (image == nullptr) return 1;
    image->mF00 = gParState.mF00;
    image->mTicks = gParState.mTicks;
    image->mLowSpec = gParState.mLowSpec;
    image->mMaxParticles = gParState.mMaxParticles;
    image->mF10 = gParState.mF10;
    image->mF14 = gParState.mF14;
    image->mF18 = static_cast<uint16_t>(gParState.mF18);
    image->mF1A = static_cast<uint16_t>(gParState.mF1A);
    image->mSinTable = 0;
    image->mDensity = gParState.mDensity;
    image->mHeightCallback = 0;
    image->mParticles = 0;
    std::memcpy(&image->mEmitters, &gParState.mEmitters, kRecordsSize);
    SwapStateImage(image);
    word = kStateSize;
    WriteWord(file, &word);
    LbFile_Write(file, image, kStateSize, nullptr);
    LbMem_Free(image);

    // The library.
    PARLibrary* library = static_cast<PARLibrary*>(LbMem_Alloc(kLibrarySize));
    if (library == nullptr) return 1;
    std::memcpy(library, &gParLibrary, kLibrarySize);
    SwapLibrary(library);
    word = kLibrarySize;
    WriteWord(file, &word);
    LbFile_Write(file, library, kLibrarySize, nullptr);
    LbMem_Free(library);

    // The pool.
    uint32_t size = withParticles != 0 ? static_cast<uint32_t>(gParState.mMaxParticles) * sizeof(tagPARTICLE) : 0;
    word = size;
    WriteWord(file, &word);
    if (size == 0) return 0;
    tagPARTICLE* pool = static_cast<tagPARTICLE*>(MallocInline(size));
    if (pool == nullptr) return 1;
    std::memcpy(pool, gParState.mParticles, size);
    tagPARTICLE* p = pool;
    for (int32_t n = gParState.mMaxParticles; n-- != 0;) {
        p->Byteswap();
        p++;
    }
    LbFile_Write(file, pool, size, nullptr);
    LbMem_Free(pool);
    return 0;
}

// 0x100bb110 (292 bytes): the nine words of a particle, then its eight halfwords, each turned in place.
void tagPARTICLE::Byteswap() {
    using namespace SamsUtilities;
    USwapInplace32(mPos[0]);
    USwapInplace32(mPos[1]);
    USwapInplace32(mPos[2]);
    USwapInplace32(mVel[0]);
    USwapInplace32(mVel[1]);
    USwapInplace32(mVel[2]);
    USwapInplace32(mColour);
    USwapInplace32(mLifeTotal);
    USwapInplace32(mLifeLeft);
    USwapInplace16(mState);
    USwapInplace16(mEmitter);
    USwapInplace16(mSize);
    USwapInplace16(mKind);
    USwapInplace16(mFrame);
    USwapInplace16(mSpin);
    USwapInplace16(mNext);
    USwapInplace16(mPrev);
}

// 0x100bb7d8 (1108 bytes): the reverse. Answers 0 when it loaded or when it had nothing to load (not running, a different magic, no
// state saved); 1 when the state's or the library's size word is not the one it knows. A system that runs frees its pool before it
// reads the state (the pointer keeps its old value if a size check then fails).
int32_t PAR_LoadStatus(void* file) {
    if (gParSystemRunning == 0) return 0;
    uint32_t magic = 0;
    ReadWord_2(file, magic);
    if (magic != kMagic) return 0;
    uint32_t saved = 0;
    ReadWord_2(file, saved);
    if (saved == 0) return 0;
    if (gParState.mParticles != nullptr) LbMem_Free(gParState.mParticles);

    uint32_t size = 0;
    ReadWord_2(file, size);
    if (size != kStateSize) return 1;
    // The image is read into a block of its own and goes into the state field by field: the host keeps its pointers (the sine table,
    // the height callback), the file's are the Mac's.
    static StateImage image;
    LbFile_Read(file, &image, kStateSize, nullptr);
    SwapStateImage(&image);
    gParState.mF00 = image.mF00;
    gParState.mTicks = image.mTicks;
    gParState.mLowSpec = image.mLowSpec;
    gParState.mMaxParticles = image.mMaxParticles;
    gParState.mF10 = image.mF10;
    gParState.mF14 = image.mF14;
    gParState.mF18 = static_cast<int16_t>(image.mF18);
    gParState.mF1A = static_cast<int16_t>(image.mF1A);
    gParState.mDensity = image.mDensity;
    std::memcpy(&gParState.mEmitters, &image.mEmitters, kRecordsSize);

    ReadWord_2(file, size);
    if (size != kLibrarySize) return 1;
    LbFile_Read(file, &gParLibrary, kLibrarySize, nullptr);
    SwapLibrary(&gParLibrary);

    // The pool: allocated for the new maximum (a maximum of 0 asserts and leaves NULL, which does not end the load).
    uint32_t poolBytes = static_cast<uint32_t>(gParState.mMaxParticles) * sizeof(tagPARTICLE);
    gParState.mParticles = static_cast<tagPARTICLE*>(MallocInline(poolBytes));
    ReadWord_2(file, size);
    if (size != 0) {
        LbFile_Read(file, gParState.mParticles, size, nullptr);
        tagPARTICLE* p = gParState.mParticles;
        for (int32_t n = gParState.mMaxParticles; n-- != 0;) {
            p->Byteswap();
            p++;
        }
        return 0;
    }

    // No particles in the file: an empty pool with its free chain, and the emitters in use lose theirs.
    int32_t max = gParState.mMaxParticles;
    tagPARTICLE* pool = gParState.mParticles;
    std::memset(pool, 0, poolBytes);
    gParState.mF00 = 0;
    gParState.mParticlesFree = 0;
    pool[0].mPrev = -1;
    pool[0].mNext = 1;
    pool[max - 1].mNext = -1;
    pool[max - 1].mPrev = static_cast<int16_t>(max - 2);
    for (int32_t i = 1; i < max - 1; i++) {
        pool[i].mPrev = static_cast<int16_t>(i - 1);
        pool[i].mNext = static_cast<int16_t>(i + 1);
    }
    int32_t index = gParState.mEmittersUsed;
    while (index >= 0) {
        PAREmitter& e = gParState.mEmitters[index];
        e.mParticles = 0;
        e.mFirstParticle = -1;
        index = e.mNext;
    }
    return 0;
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

// The particle system's spawning (main app, rsse.cpp range of the inventory, 0x100bbfd4..0x100bf790): SpawnParticle, the emitter
// starters PAR_InitEmitter and PAR_InitEmitterLaunch, the effecter starter PAR_InitEffecter and the expanding ring
// (DoSpecialEmitter, SpecialEmitter1). The state is in src/rsse_particles_state.cpp, its layout in include/rsse.h.
//
// All of it is integer arithmetic. The random numbers are drawn from the state's seed (gParState.mF10, 1 after a reset) with a
// linear congruential generator, the Microsoft C library's: seed = seed * 0x343fd + 0x269ec3, and a draw uses the high half of
// the new seed. The listing takes it three ways: signed (-32768 to 32767), 15 bits (0 to 32767) and 12 bits (0 to 4095); the
// "below n" forms are the C remainder and draw nothing when n is 0. The sines come from gParState.mSinTable (512 entries, 256
// times the sine, indexed by an angle in 1/512 turns), which the code multiplies by 4 to get 1/1024.

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

// 0x100bdbf0: the ground height callback of a system that was given none.
int32_t DummyHeightCallBack(int32_t, int32_t) { return 0; }

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

// 0x100be1e8: DoEmitters ends the emitter at its next turn (life -2).
void PAR_KillEmitter(int32_t handle) {
    if (PAR_GetEmitterStatus(static_cast<uint32_t>(handle)) != 0) gParState.mEmitters[handle & 0xffff].mLife = -2;
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

// 0x100bf1b0: as PAR_GetEmitterStatus for the effecters, but the handle 0 is looked up like any other (it is effecter 0 with
// serial 0).
int32_t PAR_GetEffecterStatus(uint32_t handle) {
    const uint32_t index = handle & 0xffff;
    if (gParSystemRunning != 0 && index < static_cast<uint32_t>(kParEffecters) && gParState.mEffecters[index].mSerial == (handle >> 16)) return 1;
    return 0;
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

// 0x100be7e4
uint32_t PAR_InitEmitterLaunch(int32_t type, int32_t x, int32_t y, int32_t z, int32_t vx, int32_t vy, int32_t vz) {
    const int32_t launch[3] = {vx, vy, vz};
    return StartEmitter(type, x, y, z, launch);
}

// 0x100bec30
uint32_t PAR_InitEmitter(int32_t type, int32_t a, int32_t b, int32_t c) { return StartEmitter(type, a, b, c, nullptr); }

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

// Unit v51114: the status panel's small members (EnableNameClick, EditNamePanelGone, GetPreviousObject, ObjectGone, CloseObject,
// ZoomTo, CloseWindow, GetName, the empty base versions and the constructor), the SamsUtilities byte swaps next to them and the
// rank text object's destructor.
