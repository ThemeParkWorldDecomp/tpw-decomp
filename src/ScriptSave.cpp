// analyser.cpp range of the main app (SimTheme Park 2.04b3), unit w165: the script system's save and load, and the file swaps they use.
//
// The file holds the tag 'SCPT' (0x53435054), the record size 0x118, the table size, the table (size words; the entries only matter as
// zero or not) and then one 0x118 byte record for each non-zero entry from 1 on. The file's words are little-endian: the Mac swaps them
// (the main app's own instances of the SamsUtilities templates, which swap unconditionally), a little-endian host does not. The save works
// on a copy of each block; the load fixes the code base to the code array's address and a position of 0x61 that was last at 0x5f to 0x5f.
//
// Host design: the table words the load reads are the file's (unused but for the zero test); each block is a SCR_NewScriptBlock (an arena
// block, so that the table's Ptr32 words reach it), the table itself a LbMem_Alloc block. The record constructor the Mac calls first
// (SSpriteGameData::SSpriteGameData, 0x1002f144) only builds a local record that the block copy then overwrites: not called.
#include <cstdint>
#include <cstring>

#include "LbFile.h"
#include "Ptr32.h"
#include "analyser.h"
#include "platform/samsutils.h"

// The swaps are real only where the host's order differs from the file's (or when a test asks).
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(TPW_SWAP_FILE_WORDS)
#define SCRIPT_SWAP_FILE_WORDS 1
#endif

namespace ScriptSaveSwap {

namespace {
uint32_t Swap32(uint32_t v) {
#ifdef SCRIPT_SWAP_FILE_WORDS
    return __builtin_bswap32(v);
#else
    return v;
#endif
}
}  // namespace

// 0x100d9920 (12 bytes)
void USwapInplace32Int(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }

// 0x100d9c2c (12 bytes)
void USwapInplace32Long(int32_t& word) { word = static_cast<int32_t>(Swap32(static_cast<uint32_t>(word))); }

// 0x100d9b00 (36 bytes)
void USwapBlock32SLONGfloat(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

// 0x100d9b74 (36 bytes)
void USwapBlock32Long(int32_t* words, int32_t count) { SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(words), count); }

// 0x100d9bd0 (40 bytes): a pointer field as its 32-bit word.
uint32_t USwap32LongPtr(uint32_t value) { return Swap32(value); }

// 0x100d9c70 (40 bytes)
uint32_t USwap32CharPtr(uint32_t value) { return Swap32(value); }

// 0x100d9964 (364 bytes): SSpriteGameData::Byteswap on the 0x44 byte sprite record: the words at 0 to 0x34 (14 of them), the half word at
// 0x38 and the word at 0x40 (the bytes from 0x3a to 0x3f stay).
void SpriteRecordByteswap(uint8_t* record) {
    for (int32_t offset = 0; offset <= 0x34; offset += 4) {
        uint32_t word;
        std::memcpy(&word, record + offset, 4);
        word = Swap32(word);
        std::memcpy(record + offset, &word, 4);
    }
#ifdef SCRIPT_SWAP_FILE_WORDS
    uint16_t half;
    std::memcpy(&half, record + 0x38, 2);
    half = static_cast<uint16_t>(half >> 8 | half << 8);
    std::memcpy(record + 0x38, &half, 2);
#endif
    uint32_t word;
    std::memcpy(&word, record + 0x40, 4);
    word = Swap32(word);
    std::memcpy(record + 0x40, &word, 4);
}

}  // namespace ScriptSaveSwap

namespace {

// The block's fields in the file's order, in the original's order (the save swaps a copy, the load the block read).
void SwapBlock(tagSCRIPT* block) {
    using namespace ScriptSaveSwap;
    block->mText = USwap32CharPtr(block->mText);
    USwapInplace32Long(block->mSlot);
    USwapInplace32Long(block->mPos);
    USwapInplace32Long(block->mStartPos);
    USwapInplace32Long(block->mLastPos);
    block->mCodeBase = USwap32LongPtr(block->mCodeBase);
    USwapInplace32Long(block->mState);
    USwapInplace32Long(block->mStackTop);
    USwapBlock32Long(block->mStack, 0x14);
    USwapInplace32Long(block->mIfDepth);
    USwapInplace32Long(block->mGosubDepth);
    USwapInplace32Long(block->mLoopDepth);
    USwapInplace32Long(block->mNextTime);
    USwapInplace32Long(block->mPeriod);
    USwapBlock32SLONGfloat(block->mVars, 0x13);
    SpriteRecordByteswap(block->mSprite);
    USwapInplace32Int(block->mHasSprite);
}

// The table's entry for `index` (a Ptr32 word the table is indexed by, re-read from the global each time as the Mac does).
Ptr32<tagSCRIPT>& EntryRef(int32_t index) { return static_cast<Ptr32<tagSCRIPT>*>(gScriptTable)[index]; }

}  // namespace

// 0x100d96b0 (580 bytes): writes the script system to a file (nothing with the system off): the tag, the record size and the table size
// as swapped words (each from a _USwapCopy32 copy that is deleted again), the table as it is, and a swapped copy of each block from 1 on.
void SCR_SaveAllScripts(void* file) {
    if (gScriptInitialised != 0) {
        uint32_t word = 0x53435054;
        uint32_t transferred;
        uint32_t* copy = SamsUtilities::_USwapCopy32(&word, 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        word = 0x118;
        copy = SamsUtilities::_USwapCopy32(&word, 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        copy = SamsUtilities::_USwapCopy32(reinterpret_cast<const uint32_t*>(&gScriptTableSize), 1);
        LbFile_Write(file, copy, 4, &transferred);
        SamsUtilities::_USwapDelete(copy);
        LbFile_Write(file, gScriptTable, static_cast<uint32_t>(gScriptTableSize) << 2, nullptr);
        for (int32_t i = 1; i < gScriptTableSize; ++i) {
            tagSCRIPT* block = EntryRef(i);
            if (block != nullptr) {
                tagSCRIPT local;
                std::memcpy(&local, block, sizeof local);
                SwapBlock(&local);
                LbFile_Write(file, &local, 0x118, nullptr);
            }
        }
    }
}

// 0x100d9ccc (784 bytes): reads the script system back; answers 0 when it is loaded, 1 when not (system off, a bad file header, a failed
// allocation). Frees the old blocks and the old table first; a bad header leaves a fresh empty table of 100 entries. A block whose
// position is 0x61 and whose last position 0x5f gets position 0x5f.
int32_t SCR_LoadAllScripts(void* file) {
    if (gScriptInitialised == 0) return 1;
    for (int32_t i = 0; i < gScriptTableSize; ++i) {
        if (gScriptInitialised != 0 && i > 0 && i < gScriptTableSize) {
            tagSCRIPT* old = EntryRef(i);
            if (old != nullptr) {
                EntryRef(i) = nullptr;
                SCR_FreeScriptBlock(old);
            }
        }
    }
    LbMem_Free(gScriptTable);
    uint32_t tag;
    uint32_t size;
    uint32_t transferred;
    LbFile_Read(file, &tag, 4, &transferred);
    SamsUtilities::_USwapBlock32(&tag, 1);
    LbFile_Read(file, &size, 4, &transferred);
    SamsUtilities::_USwapBlock32(&size, 1);
    if (tag == 0x53435054 && size == 0x118) {
        LbFile_Read(file, &gScriptTableSize, 4, &transferred);
        SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&gScriptTableSize), 1);
        gScriptTable = LbMem_Alloc(static_cast<uint32_t>(gScriptTableSize) << 2);
        if (gScriptTable == nullptr) return 1;
        LbFile_Read(file, gScriptTable, static_cast<uint32_t>(gScriptTableSize) << 2, nullptr);
        EntryRef(0).mOffset = 0;
        for (int32_t i = 1; i < gScriptTableSize; ++i) {
            if (EntryRef(i).mOffset != 0) {
                tagSCRIPT* block = SCR_NewScriptBlock();
                if (block == nullptr) return 1;
                EntryRef(i) = block;
                LbFile_Read(file, block, 0x118, nullptr);
                SwapBlock(block);
                block->mCodeBase = kScriptCodeAddress;
                if (block->mPos == 0x61 && block->mLastPos == 0x5f) block->mPos = 0x5f;
            }
        }
        return 0;
    }
    gScriptTableSize = 100;
    gScriptTable = LbMem_Alloc(400);
    std::memset(gScriptTable, 0, static_cast<uint32_t>(gScriptTableSize) << 2);
    return 1;
}
