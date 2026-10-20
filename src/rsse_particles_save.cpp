// The particle system's save and load (main app, rsse.cpp range of the inventory, 0x100bacac..0x100bbc2c): PAR_SaveStatus and
// PAR_LoadStatus with their swap helpers (the main app's own instances of the SamsUtilities templates for the system's records and
// pointers) and tagPARTICLE::Byteswap. The state, the library and the pool go to a file as the Mac writes them: little-endian, every
// field turned (the original swaps a copy on the way out and the live block on the way in).
//
// The host's state is not the Mac's image: its header holds native pointers (include/rsse.h, PARState), so the save builds the Mac's
// image field by field with the three pointer words written as 0, and the load reads the image into a scratch block of the Mac's
// layout and copies it into the state field by field. A little-endian host does not swap (the file is little-endian too); the
// swaps below do something only when PAR_SWAP_FILE_WORDS is defined, as in src/rsse_particles_init.cpp.
#include <cstring>
#include <memory>

#include "Debug.h"
#include "LbFile.h"
#include "platform/samsutils.h"
#include "rsse.h"

// A file's little-endian field in the host's order (see src/rsse_particles_init.cpp).
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define PAR_SWAP_FILE_WORDS 1
#endif

namespace {

// The texts of the main app's string table that this file uses (the table is at 0x1023ec9d).
constexpr char kMallocNothing[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";   // 0x1023ed02

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
void ReadWord(void* file, uint32_t& word) {
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

// 0x100bb7d8 (1108 bytes): the reverse. Answers 0 when it loaded or when it had nothing to load (not running, a different magic, no
// state saved); 1 when the state's or the library's size word is not the one it knows. A system that runs frees its pool before it
// reads the state (the pointer keeps its old value if a size check then fails).
int32_t PAR_LoadStatus(void* file) {
    if (gParSystemRunning == 0) return 0;
    uint32_t magic = 0;
    ReadWord(file, magic);
    if (magic != kMagic) return 0;
    uint32_t saved = 0;
    ReadWord(file, saved);
    if (saved == 0) return 0;
    if (gParState.mParticles != nullptr) LbMem_Free(gParState.mParticles);

    uint32_t size = 0;
    ReadWord(file, size);
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

    ReadWord(file, size);
    if (size != kLibrarySize) return 1;
    LbFile_Read(file, &gParLibrary, kLibrarySize, nullptr);
    SwapLibrary(&gParLibrary);

    // The pool: allocated for the new maximum (a maximum of 0 asserts and leaves NULL, which does not end the load).
    uint32_t poolBytes = static_cast<uint32_t>(gParState.mMaxParticles) * sizeof(tagPARTICLE);
    gParState.mParticles = static_cast<tagPARTICLE*>(MallocInline(poolBytes));
    ReadWord(file, size);
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
