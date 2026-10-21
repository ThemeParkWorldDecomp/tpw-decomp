// SaveLoad.cpp (main app 0x10157fec..): the park file reader. PARTIAL: only CSaveLoad::Load
// (0x10157fec) so far; LoadHeader, LoadModules and the Pre and PostLoadOperations are declared in
// SaveLoad.h and still stand-ins.
#include "SaveLoad.h"

#include "Debug.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

#include <cstdio>
#include <cstring>

uint32_t gSaveLoadOpenTestWord;   // 0x102a6cb8, 0 in the binary

namespace {

// The temporary allocator, made on first use (operator new(8) and its constructor, stored in the import gMemoryTemporary).
// The original tests the pointer at every use.
SamsUtilities::UAllocatorTemporary& TemporaryAllocator() {
    if (SamsUtilities::gMemoryTemporary == nullptr) SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return *SamsUtilities::gMemoryTemporary;
}

// The file's little endian version word as the host has it (the original, on a big endian Mac, swaps the bytes it read).
uint32_t FileWord(uint32_t word) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return __builtin_bswap32(word);
#else
    return word;
#endif
}

// The newest version this game loads for a park of the player's folder.
constexpr int32_t kNewestVersion = 500;
// The trace class of the save file messages.
constexpr uint32_t kTraceSaveLoad = 0x499602d2;

}  // namespace


// Reads a park file: the version word, the header, then the rest of the file, which is a zlib block of the modules. The block is
// unpacked into a memory file and LoadModules reads that. Type 0 stops after the header; type 1 (a downloaded park) skips the
// Pre and PostLoadOperations. On every failure the reason goes to mError and the answer is 0; the file objects are destroyed on
// every path (the disc file's destructor closes it).
//
// Quirks kept: the open test is the debug word's (see SaveLoad.h); a block that is not a zlib block is released with delete[]
// instead of the allocator's Free; the CTRACE of a short read is made before the packed buffer is freed.
int32_t CSaveLoad::Load(const char* path, CSaveLoadHeader* header, LoadType type) {
    GAME_ASSERTMSG(type != kLoadHeaderOnly || header != nullptr,
                   "Cannot have a header-only load but supply no header pointer\n");
    bfl::FileSystem::CDiscFile file;
    const bool opened = file.Open(path, 0x80000000).Succeeded();
    if (opened != (gSaveLoadOpenTestWord == 0)) {
        CTRACE(0, "Couldn't open file %s", path);
        mError = kErrorOpenOrRead;
        return 0;
    }

    uint32_t word = 0;
    if (file.Read(&word, 4) != 4) {
        DEBUG_MESSAGE("File read failed", 0);
        file.Close();
        mError = kErrorOpenOrRead;
        return 0;
    }
    const int32_t version = static_cast<int32_t>(FileWord(word));
    if (type == kLoadPark && version > kNewestVersion) {
        mError = kErrorFutureVersion;
        DEBUG_MESSAGE("Trying to load future version savegame into earlier game - get a patch\n", 0);
        file.Close();
        return 0;
    }

    if (LoadHeader(file, type, header) == 0) {
        CTRACE(kTraceSaveLoad, "Could not load saved game header!");
        file.Close();
        mError = kErrorHeader;
        return 0;
    }
    if (type == kLoadHeaderOnly) return 1;

    // what is left of the file
    const int32_t fileSize = file.GetSize();
    const int32_t position = file.Seek(0, 1);
    const int32_t remaining = static_cast<int32_t>(static_cast<uint32_t>(fileSize) - static_cast<uint32_t>(position));
    if (remaining == 0) {
        mError = kErrorEmptyOrBad;
        return 0;
    }
    uint8_t* packed = TemporaryAllocator().Allocate(static_cast<uint32_t>(remaining));
    if (file.Read(packed, static_cast<uint32_t>(remaining)) != remaining) {
        CTRACE(kTraceSaveLoad, "Could not read %d bytes out of compressed save file", remaining);
        TemporaryAllocator().Free(packed);
        mError = kErrorPackedRead;
        return 0;
    }
    file.Close();
    const uint32_t unpackedSize = LbZLib_MemDecompressedSize(packed);
    if (unpackedSize == 0) {
        delete[] packed;
        mError = kErrorEmptyOrBad;
        return 0;
    }
    uint8_t* unpacked = TemporaryAllocator().Allocate(unpackedSize);
    if (unpacked == nullptr) {
        TemporaryAllocator().Free(packed);
        mError = kErrorNoMemory;
        return 0;
    }
    LbZLib_MemDecompress(packed, unpacked, nullptr);
    TemporaryAllocator().Free(packed);

    // In the arena: BMP_Load's chunks keep the file's address in a 32-bit field (the Mac's file object was on the stack).
    tpw::ArenaHeld<bfl::FileSystem::CMemFile> held;
    bfl::FileSystem::CMemFile& memory = *held;
    if (!memory.Open(unpackedSize).Succeeded()) {
        TemporaryAllocator().Free(unpacked);
        mError = kErrorNoMemory;
        return 0;
    }
    if (memory.Write(unpacked, unpackedSize) < 0) {
        TemporaryAllocator().Free(unpacked);
        memory.Close();
        mError = kErrorMemoryWrite;
        return 0;
    }
    memory.Seek(0, 0);
    TemporaryAllocator().Free(unpacked);
    if (type != kLoadOnline) PreLoadOperations();
    const int32_t loaded = LoadModules(memory, type);
    file.Close();
    memory.Close();
    if (type != kLoadOnline) PostLoadOperations();
    if (loaded == 0) {
        GAME_ASSERTMSG(0, "Loading failed.  System state now (possibly) inconsistent.");
        mError = kErrorEmptyOrBad;
        return 0;
    }
    CTRACE(kTraceSaveLoad, "Loaded savegame: %s", path);
    return 1;
}
