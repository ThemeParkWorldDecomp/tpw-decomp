// rsse.cpp range of the main app (SimTheme Park 2.04b3), unit w166: RSSE_Save, the ride scripts' save, and rsse.cpp's own instances of the
// SamsUtilities swap templates it uses.
//
// The file holds "RSSE", 0x14, the state's five words and, when the system is initialised, "PAD_" five times, the script count, 0xf4 and then
// each script of the list: its 0xf4 byte record (a swapped copy), the tables as a byte count and the table (program, stack, variables, a raw
// byte block, limbo, bounce, walk, a word table, the name) and "OBJ ", the object count, 0x1c and the script's objects (0x1c byte records).
// The file's words are little-endian. The Mac swaps every word (the main app's swap templates swap unconditionally), a little-endian host
// writes the same bytes by not swapping: the swaps are real only where the host's order differs from the file's (or when a test asks).
//
// Host design: the record's pointer words are the Ptr32 words (arena offsets; 0 for NULL), written like the Mac's addresses (the file holds
// addresses, which nothing reads back). The state's fifth word, the list head, is a native pointer on the host: its Ptr32 encoding is written.
// The thing id at +0xac is swapped by tag_IDX_TYPE::Byteswap, which ALWAYS swaps (it is an exchange of two bytes): it is called only where
// the other swaps are real, else it would put the big-endian image at +0xac of a little-endian host's file.
//
// Unit w167 adds RSSE_Load and RSSE_WALK::Byteswap here, the save's mirror: the same swaps, applied to what was read.
#include <cstdint>
#include <cstring>
#include <vector>

#include "Debug.h"
#include "LbFile.h"
#include "Ptr32.h"
#include "analyser.h"
#include "platform/arena.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "m3d2.h"
#include "platform/samsutils.h"
#include "rsse.h"

struct IDXTYPEUniqueNameThingId;

#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define RSSE_SWAP_FILE_WORDS 1
#endif

namespace RsseSwap {

namespace {
uint32_t Swap32(uint32_t v) {
#ifdef RSSE_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
uint16_t Swap16(uint16_t v) {
#ifdef RSSE_SWAP_FILE_WORDS
    return __builtin_bswap16(v);
#else
    return v;
#endif
}
}  // namespace

// 0x100d5324 (12 bytes)
void USwapInplace32Int(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }
// 0x100d5368 (12 bytes)
void USwapInplace32UInt(uint32_t& word) { word = Swap32(word); }
// 0x100d53b0 (40 bytes): a pointer field as its 32-bit word.
uint32_t USwap32ObjectPtr(uint32_t value) { return Swap32(value); }
// 0x100d5424 (12 bytes)
void USwapInplace16UShort(uint16_t& value) { value = Swap16(value); }
// 0x100d546c (12 bytes)
void USwapInplace32ULong(uint32_t& word) { word = Swap32(word); }
// 0x100d54b4 (12 bytes)
void USwapInplace16Short(int16_t& value) { value = static_cast<int16_t>(Swap16(static_cast<uint16_t>(value))); }
// 0x100d54f8 (40 bytes)
uint32_t USwap32CharPtr(uint32_t value) { return Swap32(value); }
// 0x100d5554 (40 bytes)
uint32_t USwap32WalkPtr(uint32_t value) { return Swap32(value); }
// 0x100d55c0 (40 bytes)
uint32_t USwap32LongPtr(uint32_t value) { return Swap32(value); }
// 0x100d561c (12 bytes)
void USwapInplace32Long(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }
// 0x100d5660 (40 bytes)
uint32_t USwap32VoidPtr(uint32_t value) { return Swap32(value); }

}  // namespace RsseSwap

namespace {

// The fields of a record held as bytes (the record and the walk and object copies are not read through their types), swapped in place.
template <class T, class Function>
void SwapAt(uint8_t* record, uint32_t offset, Function swap) {
    T value;
    std::memcpy(&value, record + offset, sizeof value);
    swap(value);
    std::memcpy(record + offset, &value, sizeof value);
}
void Long(uint8_t* r, uint32_t o) { SwapAt<int32_t>(r, o, RsseSwap::USwapInplace32Long); }
void ULong(uint8_t* r, uint32_t o) { SwapAt<uint32_t>(r, o, RsseSwap::USwapInplace32ULong); }
void UInt(uint8_t* r, uint32_t o) { SwapAt<uint32_t>(r, o, RsseSwap::USwapInplace32UInt); }
void Int(uint8_t* r, uint32_t o) { SwapAt<int32_t>(r, o, RsseSwap::USwapInplace32Int); }
void Short(uint8_t* r, uint32_t o) { SwapAt<int16_t>(r, o, RsseSwap::USwapInplace16Short); }
void UShort(uint8_t* r, uint32_t o) { SwapAt<uint16_t>(r, o, RsseSwap::USwapInplace16UShort); }
void Pointer(uint8_t* r, uint32_t o, uint32_t (*swap)(uint32_t)) {
    uint32_t value;
    std::memcpy(&value, r + o, sizeof value);
    value = swap(value);
    std::memcpy(r + o, &value, sizeof value);
}

// One swapped word to the file: a _USwapCopy32 copy of it, written (the count in the original's place for the bytes done), deleted.
void WriteWord(void* file, uint32_t word) {
    uint32_t transferred;
    uint32_t* copy = SamsUtilities::_USwapCopy32(&word, 1);
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

// A table of `words` words behind a byte count: the count, then the swapped copy of the table when the count is not 0.
void WriteTable(void* file, uint32_t bytes, const void* table, int32_t words) {
    WriteWord(file, bytes);
    if (bytes != 0) {
        uint32_t* copy = SamsUtilities::_USwapCopy32(static_cast<const uint32_t*>(table), words);
        LbFile_Write(file, copy, bytes, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// The walk table: records of 32 bytes, each copied and swapped field by field (the word at +0x1c is copied and not swapped).
void WriteWalkTable(void* file, const RSSE_SCRIPT* script) {
    WriteWord(file, static_cast<uint32_t>(script->mWalkCount));
    if (script->mWalkCount != 0) {
        uint8_t* copy = static_cast<uint8_t*>(SamsUtilities::_USwapAllocate(static_cast<uint32_t>(script->mWalkCount) << 5));
        for (int32_t i = 0; i < script->mWalkCount; ++i) {
            uint8_t* record = copy + i * 0x20;
            std::memcpy(record, reinterpret_cast<const uint8_t*>(script->mWalk.get()) + i * 0x20, 0x20);
            Short(record, 0);
            Short(record, 2);
            Short(record, 4);
            Short(record, 6);
            Long(record, 8);
            Long(record, 0xc);
            Long(record, 0x10);
            Short(record, 0x14);
            Short(record, 0x16);
            Short(record, 0x18);
            Short(record, 0x1a);
        }
        LbFile_Write(file, copy, static_cast<uint32_t>(script->mWalkCount) << 5, nullptr);
        SamsUtilities::_USwapDelete(copy);
    }
}

// The script record's fields swapped in place, in the original's order and widths (+0xb8 to +0xbb, +0xae and +0xe8 to +0xf3 stay). The save swaps
// a copy before it writes it, the load swaps the record after it read it (the swaps are their own inverses).
void SwapRecord(uint8_t* r) {
    using namespace RsseSwap;
    Pointer(r, 0x00, USwap32VoidPtr);
    Pointer(r, 0x04, USwap32VoidPtr);
    for (uint32_t o = 0x08; o <= 0x14; o += 4) Long(r, o);
    for (uint32_t o = 0x18; o <= 0x28; o += 4) Pointer(r, o, USwap32LongPtr);
    Pointer(r, 0x2c, USwap32WalkPtr);
    Pointer(r, 0x30, USwap32LongPtr);
    Pointer(r, 0x34, USwap32CharPtr);
    Pointer(r, 0x38, USwap32CharPtr);
    for (uint32_t o = 0x3c; o <= 0x68; o += 4) Long(r, o);
    Short(r, 0x6c);
    Short(r, 0x6e);
    for (uint32_t o = 0x70; o <= 0x7c; o += 4) Long(r, o);
    ULong(r, 0x80);
    ULong(r, 0x84);
    UShort(r, 0x88);
    UShort(r, 0x8a);
    for (uint32_t o = 0x8c; o <= 0x9c; o += 4) Long(r, o);
    ULong(r, 0xa0);
    ULong(r, 0xa4);
    ULong(r, 0xa8);
#ifdef RSSE_SWAP_FILE_WORDS
    SwapAt<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>>(r, 0xac, [](tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>& id) { id.Byteswap(); });
#endif
    Pointer(r, 0xb0, USwap32ObjectPtr);
    UInt(r, 0xb4);
    Int(r, 0xbc);
    Short(r, 0xc0);
    Short(r, 0xc2);
    ULong(r, 0xc4);
    ULong(r, 0xc8);
    for (uint32_t o = 0xcc; o <= 0xd8; o += 4) Long(r, o);
    Short(r, 0xdc);
    Short(r, 0xde);
    ULong(r, 0xe0);
    Short(r, 0xe4);
    Short(r, 0xe6);
}

// The script's record: a copy with its fields swapped, written.
void WriteRecord(void* file, const RSSE_SCRIPT* script) {
    RSSE_SCRIPT local;
    std::memcpy(static_cast<void*>(&local), script, sizeof local);
    SwapRecord(reinterpret_cast<uint8_t*>(&local));
    LbFile_Write(file, &local, 0xf4, nullptr);
}

// An object's fields swapped in place: its two links, its four words and its unsigned word.
void SwapObject(uint8_t* object) {
    using namespace RsseSwap;
    Pointer(object, 0x00, USwap32VoidPtr);
    Pointer(object, 0x04, USwap32VoidPtr);
    Long(object, 0x08);
    Long(object, 0x0c);
    Long(object, 0x10);
    ULong(object, 0x14);
    Long(object, 0x18);
}

// One object of the script's list: a swapped copy, written.
void WriteObject(void* file, const RSSE_OBJECT* object) {
    uint8_t local[0x1c];
    std::memcpy(local, object, sizeof local);
    SwapObject(local);
    LbFile_Write(file, local, 0x1c, nullptr);
}

}  // namespace

// 0x100d45b0 (3,408 bytes): writes the script system to a file; answers 0. The state's words are swapped by hand into a local and written in
// one piece (the list head as its Ptr32 word); nothing else is written while the system is off.
int32_t RSSE_Save(void* file) {
    WriteWord(file, 0x45535352);   // "RSSE"
    WriteWord(file, 0x14);         // the state's size
    // The state's words, swapped by hand (inline in the original) into a local; the fifth is the list head as its Ptr32 word.
    uint32_t state[5];
    state[0] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mInitialised));
    state[1] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mF04));
    state[2] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mF08));
    state[3] = RsseSwap::USwap32VoidPtr(static_cast<uint32_t>(gRSSEState.mScriptCount));
    state[4] = RsseSwap::USwap32VoidPtr(gRSSEState.mScripts != nullptr ? tpw::ArenaOffset(gRSSEState.mScripts) : 0);
    LbFile_Write(file, state, 0x14, nullptr);
    if (gRSSEState.mInitialised != 0) {
        for (int32_t i = 0; i < 5; ++i) WriteWord(file, 0x5f444150);   // "PAD_" five times
        uint32_t count = 0;
        for (const RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) ++count;
        WriteWord(file, count);
        WriteWord(file, 0xf4);   // the record size
        for (const RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr; script = script->mNext) {
            WriteRecord(file, script);
            WriteTable(file, static_cast<uint32_t>(script->mProgramWords) << 2, script->mCode.get(), script->mProgramWords);
            WriteTable(file, static_cast<uint32_t>(script->mStackSize) << 2, script->mStack.get(), script->mStackSize);
            WriteTable(file, static_cast<uint32_t>(script->mVarCount) << 2, script->mVars.get(), script->mVarCount);
            // The +0x34 block: its size is the word at +0x90 itself (written swapped), the block raw.
            WriteWord(file, static_cast<uint32_t>(script->mSaveBlockBytes));
            if (script->mSaveBlockBytes != 0) LbFile_Write(file, script->mSaveBlock.get(), static_cast<uint32_t>(script->mSaveBlockBytes), nullptr);
            WriteTable(file, static_cast<uint32_t>(script->mLimboCount) << 3, script->mLimbo.get(), script->mLimboCount << 1);
            WriteTable(file, static_cast<uint32_t>(script->mBounceCount) << 4, script->mBounce.get(), script->mBounceCount << 2);
            WriteWalkTable(file, script);
            WriteTable(file, static_cast<uint32_t>(script->mSaveWordCount) << 2, script->mSaveWords.get(), script->mSaveWordCount);
            // The name, with its terminator, raw.
            const uint32_t nameBytes = static_cast<uint32_t>(std::strlen(script->mName.get())) + 1;
            WriteWord(file, nameBytes);
            if (nameBytes != 0) LbFile_Write(file, script->mName.get(), nameBytes, nullptr);
            WriteWord(file, 0x204a424f);   // "OBJ "
            uint32_t objects = 0;
            for (const RSSE_OBJECT* object = script->mObjects; object != nullptr; object = object->mNext) ++objects;
            WriteWord(file, objects);
            WriteWord(file, 0x1c);   // the object size
            for (const RSSE_OBJECT* object = script->mObjects; object != nullptr; object = object->mNext) WriteObject(file, object);
        }
    }
    return 0;
}

// 0x100d65a8 (184 bytes): the walk record's fields swapped in place, always (the load calls it only where the host's order differs from the
// file's): the halfwords at +0, +2, +4, +6, the words at +8, +0xc, +0x10, the halfwords at +0x14, +0x16, +0x18, +0x1a. The word at +0x1c stays.
void RSSE_WALK::Byteswap() {
    mFrom = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mFrom)));
    mTo = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mTo)));
    mOutFrom = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mOutFrom)));
    mOutTo = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mOutTo)));
    mStart = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(mStart)));
    mEnd = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(mEnd)));
    mPeep = __builtin_bswap32(mPeep);
    mDirection = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mDirection)));
    mKind = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mKind)));
    mState = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mState)));
    mLast = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(mLast)));
}

namespace {

// The messages (the original's string pool at 0x1023eeae + offset, copied byte for byte).
const char kMalloc[] = "RSSE Malloc %ld\n";                                                              // +0x3ca
const char kMallocNothing[] = "RSSE: Tried to MALLOC 0 - Stoopid !\n";                                  // +0x3db
const char kNotFreed[] = "RSSE: Arrooga - Something malloced but not freed - bad boy - fix it !";      // +0x7dc
const char kNoObjects[] = "RSSE: Load Fail - Object list missing";                                      // +0x925
const char kNoHeader[] = "RSSE: Load Fail - Missing header";                                            // +0x94b

const float kHalf = 0.5f;   // 0x10264fb8

// The original reads a count or a tag: four bytes, the transferred count asked for, then _USwapBlock32 on the word (a no-op on a
// little-endian host, which the file's order is).
uint32_t ReadWord(void* file) {
    uint32_t word = 0;
    uint32_t transferred;
    LbFile_Read(file, &word, 4, &transferred);
    SamsUtilities::_USwapBlock32(&word, 1);
    return word;
}

// `bytes` bytes of the file into a destination of `capacity` bytes. The Mac reads them all into the destination (a larger size overflows it);
// the host keeps what fits.
void ReadInto(void* file, void* destination, uint32_t capacity, uint32_t bytes) {
    if (bytes <= capacity) {
        LbFile_Read(file, destination, bytes, nullptr);
    } else {
        std::vector<uint8_t> over(bytes);
        LbFile_Read(file, over.data(), bytes, nullptr);
        std::memcpy(destination, over.data(), capacity);
    }
}

// RSSE's malloc: the trace, then LbMem_Alloc (the arena here: the links and tables are Ptr32); 0 asserts and answers NULL.
void* Allocate(uint32_t size) {
    CTRACE(8, kMalloc, size);
    if (size == 0) {
        GAME_ASSERTMSG(0, kMallocNothing);
        return nullptr;
    }
    return tpw::ArenaAlloc(size);
}

// A table: its byte count was read; the block (when the count is not 0) is read raw. Answers the block.
void* ReadBlock(void* file, uint32_t bytes) {
    void* block = nullptr;
    if (bytes != 0) {
        block = Allocate(bytes);
        LbFile_Read(file, block, bytes, nullptr);
    }
    return block;
}

}  // namespace

// 0x100d56bc (3,784 bytes)
int32_t RSSE_Load(void* file) {
    if (gRSSEState.mInitialised != 0) {
        for (RSSE_SCRIPT* script = gRSSEState.mScripts; script != nullptr;) script = KillScript(script, 0);
        gRSSEState.mScripts = nullptr;
        gRSSEState.mInitialised = 0;
        if (gRSSEMallocCount != 0) GAME_ASSERTMSG(0, kNotFreed);
    }
    RSSE_Init();
    if (ReadWord(file) != 0x45535352) {   // "RSSE"
        GAME_ASSERTMSG(0, kNoHeader);
        return 1;
    }
    // The state: read straight over gRSSEState on the Mac, whose list head is a word there. Here the five words are a local image.
    const uint32_t stateSize = ReadWord(file);
    uint32_t image[5] = {static_cast<uint32_t>(gRSSEState.mInitialised), static_cast<uint32_t>(gRSSEState.mF04),
                         static_cast<uint32_t>(gRSSEState.mF08), static_cast<uint32_t>(gRSSEState.mScriptCount), 0};
    ReadInto(file, image, sizeof image, stateSize);
    gRSSEState.mInitialised = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[0]));
    gRSSEState.mF04 = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[1]));
    gRSSEState.mF08 = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[2]));
    gRSSEState.mScriptCount = static_cast<int32_t>(RsseSwap::USwap32VoidPtr(image[3]));
    gRSSEState.mScripts = nullptr;
    for (int32_t i = 0; i < 5; ++i) ReadWord(file);   // the five "PAD_" words
    const int32_t scriptCount = static_cast<int32_t>(ReadWord(file));
    const uint32_t recordSize = ReadWord(file);
    for (int32_t s = 0; s < scriptCount; ++s) {
        RSSE_SCRIPT* script = static_cast<RSSE_SCRIPT*>(Allocate(0xf4));
        std::memset(static_cast<void*>(script), 0, 0xf4);
        gRSSEState.mScriptCount += 1;
        script->mNext = gRSSEState.mScripts;
        script->mPrev = nullptr;
        if (gRSSEState.mScripts != nullptr) gRSSEState.mScripts->mPrev = script;
        gRSSEState.mScripts = script;
        // The record is read over the links; they are put back after the swaps.
        uint32_t links[2];
        std::memcpy(links, static_cast<void*>(script), 8);
        ReadInto(file, script, 0xf4, recordSize);
        SwapRecord(reinterpret_cast<uint8_t*>(script));
        std::memcpy(static_cast<void*>(script), links, 8);

        // The program, the stack and the variables: a byte count, the block, then the word count and the block swapped (always).
        script->mCode = nullptr;
        uint32_t bytes = ReadWord(file);
        script->mCode = static_cast<uint32_t*>(ReadBlock(file, bytes));
        script->mProgramWords = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(script->mCode.get(), script->mProgramWords);

        script->mStack = nullptr;
        bytes = ReadWord(file);
        script->mStack = static_cast<int32_t*>(ReadBlock(file, bytes));
        script->mStackSize = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mStack.get()), script->mStackSize);

        script->mVars = nullptr;
        bytes = ReadWord(file);
        script->mVars = static_cast<int32_t*>(ReadBlock(file, bytes));
        script->mVarCount = static_cast<int32_t>(bytes >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mVars.get()), script->mVarCount);

        // The +0x34 block: its size goes straight into the field at +0x90.
        script->mSaveBlockBytes = static_cast<int32_t>(ReadWord(file));
        script->mSaveBlock = nullptr;
        script->mSaveBlock = static_cast<uint8_t*>(ReadBlock(file, static_cast<uint32_t>(script->mSaveBlockBytes)));

        // The limbo table: the field holds the byte count, then the record count (8 bytes each; swapped as two words each).
        script->mLimboCount = static_cast<int32_t>(ReadWord(file));
        script->mLimbo = nullptr;
        if (script->mLimboCount != 0) {
            script->mLimbo = static_cast<RSSE_LIMBO*>(ReadBlock(file, static_cast<uint32_t>(script->mLimboCount)));
            script->mLimboCount = static_cast<int32_t>(static_cast<uint32_t>(script->mLimboCount) >> 3);
            SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mLimbo.get()), script->mLimboCount << 1);
        }

        // The bounce table: 16 bytes a record.
        script->mBounceCount = static_cast<int32_t>(ReadWord(file));
        if (script->mBounceCount != 0) {
            script->mBounce = static_cast<RSSE_BOUNCE*>(ReadBlock(file, static_cast<uint32_t>(script->mBounceCount)));
            script->mBounceCount = static_cast<int32_t>(static_cast<uint32_t>(script->mBounceCount) >> 4);
            SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mBounce.get()), script->mBounceCount << 2);
        } else {
            script->mBounce = nullptr;
        }

        // The walk table: 32 bytes a record, each swapped by RSSE_WALK::Byteswap where the host's order differs from the file's.
        script->mWalkCount = static_cast<int32_t>(ReadWord(file));
        script->mWalk = nullptr;
        if (script->mWalkCount != 0) {
            const uint32_t walkBytes = static_cast<uint32_t>(script->mWalkCount) << 5;
            script->mWalk = static_cast<RSSE_WALK*>(ReadBlock(file, walkBytes));
#ifdef RSSE_SWAP_FILE_WORDS
            for (int32_t i = 0; i < script->mWalkCount; ++i) script->mWalk.get()[i].Byteswap();
#endif
        }

        // The +0x30 words: the byte count in the field, then (outside the test) the word count and the swap.
        script->mSaveWordCount = static_cast<int32_t>(ReadWord(file));
        script->mSaveWords = nullptr;
        if (script->mSaveWordCount != 0) {
            script->mSaveWords = static_cast<int32_t*>(ReadBlock(file, static_cast<uint32_t>(script->mSaveWordCount)));
        }
        script->mSaveWordCount = static_cast<int32_t>(static_cast<uint32_t>(script->mSaveWordCount) >> 2);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(script->mSaveWords.get()), script->mSaveWordCount);

        // The name: its length with the terminator, then the characters (a length of 0 leaves the word the record held).
        const uint32_t nameBytes = ReadWord(file);
        if (nameBytes != 0) script->mName = static_cast<char*>(ReadBlock(file, nameBytes));

        // The sound: a handle in the record at +0xd4 asks for the sound again, at the middle of the ride's bound area.
        if (script->mFCC[2] != 0) {
            sAlignedBox box;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            bool found = false;
            if (script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                const uint8_t* node = reinterpret_cast<const uint8_t*>(gRideAnimSets[script->mRideAnim]->mHeads->mHeader->mF78.get());
                std::memcpy(&y, node + 0x44, sizeof y);
                x = (box.mMin[0] + box.mMax[0]) * kHalf;
                z = (box.mMin[2] + box.mMax[2]) * kHalf;
                found = true;
            }
            if (found && script->mRideAnim != 0 && GetRideMasterBoundArea(script->mRideAnim, &box) != 0) {
                const float dx = box.mMax[0] - box.mMin[0];
                const float dy = box.mMax[1] - box.mMin[1];
                const float dz = box.mMax[2] - box.mMin[2];
                script->mFCC[2] = static_cast<uint32_t>(SOUND_AddObject(x, y, z, (dx + dz) * kHalf, dy));
            }
        }

        // The objects.
        script->mFE6[0] = 0xff;
        script->mFE6[1] = 0xff;
        script->mObjects = nullptr;
        if (ReadWord(file) != 0x204a424f) {   // "OBJ "
            GAME_ASSERTMSG(0, kNoObjects);
            return 1;
        }
        const int32_t objectCount = static_cast<int32_t>(ReadWord(file));
        const uint32_t objectSize = ReadWord(file);
        for (int32_t o = 0; o < objectCount; ++o) {
            RSSE_OBJECT* object = static_cast<RSSE_OBJECT*>(Allocate(0x1c));
            object->mNext = script->mObjects;
            object->mPrev = nullptr;
            if (script->mObjects != nullptr) script->mObjects->mPrev = object;
            script->mObjects = object;
            gRSSEF7BE0 += 1;
            uint32_t objectLinks[2];
            std::memcpy(objectLinks, static_cast<void*>(object), 8);
            ReadInto(file, object, 0x1c, objectSize);
            SwapObject(reinterpret_cast<uint8_t*>(object));
            std::memcpy(static_cast<void*>(object), objectLinks, 8);
        }
    }
    return 0;
}
