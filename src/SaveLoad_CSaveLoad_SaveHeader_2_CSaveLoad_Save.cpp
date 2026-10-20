#include "SaveLoad.h"
#include "Debug.h"
#include "bfl_FileSystem.h"
#include "bfl_ZLib.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include <cstring>
#include "Advisor.h"
#include "Clock.h"
#include "GameSys.h"
#include "HLSprite.h"
#include "LLCellSystem.h"
#include "LbFile.h"
#include "RideDB.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TbError.h"
#include "Text.h"
#include "UIInterface.h"
#include "World.h"
#include "analyser.h"
#include "rsse.h"
#include "ui_main.h"

namespace {
constexpr uint32_t kTrace = 0x499602d2;   // the trace level of the progress messages

// The temporary allocator, made on first use (operator new(8) and its constructor, stored in the import gMemoryTemporary).
SamsUtilities::UAllocatorTemporary& SaveTemporaryAllocator() {
    if (SamsUtilities::gMemoryTemporary == nullptr) SamsUtilities::gMemoryTemporary = new SamsUtilities::UAllocatorTemporary;
    return *SamsUtilities::gMemoryTemporary;
}
}  // namespace

// Writes the park: the version word (500, little endian), the header (SaveHeader), then the modules (SaveModules) into a memory
// file, which is packed with zlib and appended to the file. 1 on success; else 0 with mError set (1 open, 2 no memory, 3 write,
// 5 SaveModules failed). The original's file is closed and deleted on most failures.
int32_t CSaveLoad::Save(const char* path, CSaveLoadHeader* header, LoadType type) {
    GAME_ASSERTMSG(type != kLoadHeaderOnly, "Cannot have header-only save\n");
    void* handle;
    if (!LbFile_Open(handle, path, 0x40000010).Succeeded()) {
        CTRACE(0, "Failed to open %s", path);
        mError = kErrorOpenOrRead;
        return 0;
    }

    uint32_t version = 500;   // little endian in the file: the host's own order
    uint32_t written;
    if (!LbFile_Write(handle, &version, 4, &written).Succeeded()) {
        GAME_ASSERTMSG(0, "Write to %s failed", path);
        LbFile_Close(handle);
        LbFile_Delete(path);
        mError = kErrorMemoryWrite;
        return 0;
    }
    if (SaveHeader(handle, header) == 0) {
        GAME_ASSERTMSG(0, "Save Header FAILED!");
        LbFile_Close(handle);
        LbFile_Delete(path);
        mError = kErrorMemoryWrite;
        return 0;
    }
    CTRACE(kTrace, "Saved header: %s", path);
    LbFile_Close(handle);

    CTRACE(kTrace, "Opening memory file for saving,\n");
    // In the arena: BMP_Save's chunks keep the file's address in a 32-bit field (the Mac's file object was on the stack).
    tpw::ArenaHeld<bfl::FileSystem::CMemFile> held;
    bfl::FileSystem::CMemFile& file = *held;
    if (!file.Open(0x200).Succeeded()) {
        CTRACE(kTrace, "ERROR: Couldn't not open memory file for saving!");
        mError = kErrorNoMemory;
        return 0;
    }
    PreSaveOperations();
    if (SaveModules(&file, type) == 0) {
        GAME_ASSERTMSG(0, "Save FAILED!");
        file.Close();
        LbFile_Delete(path);
        mError = 5;
        return 0;
    }
    CTRACE(kTrace, "Saved: %s", path);

    const int32_t size = file.GetSize();
    uint8_t* raw = SaveTemporaryAllocator().Allocate(static_cast<uint32_t>(size));
    if (raw == nullptr) {
        file.Close();
        LbFile_Delete(path);
        mError = kErrorNoMemory;
        return 0;
    }
    file.Seek(0, 0);
    file.Read(raw, static_cast<uint32_t>(size));   // (nothing when the file has no access object)
    file.Close();

    uint8_t* packed = SaveTemporaryAllocator().Allocate(static_cast<uint32_t>(size));
    if (packed == nullptr) {
        LbFile_Delete(path);
        SaveTemporaryAllocator().Free(raw);
        mError = kErrorNoMemory;
        return 0;
    }
    const uint32_t packedSize = LbZLib_MemCompress(raw, packed, static_cast<uint32_t>(size), nullptr);
    GAME_ASSERTMSG(static_cast<uint32_t>(size) >= packedSize, "Need to allocate a bigger buffer during save game!");
    GAME_ASSERTMSG(packedSize != 0, "Save game is too small to be saved - increase size of Layout save padding action!");
    SaveTemporaryAllocator().Free(raw);

    bfl::FileSystem::CDiscFile disc;
    if (!disc.Open(path, 0xC0000030).Succeeded()) {
        CTRACE(0, "Failed to open %s", path);
        SaveTemporaryAllocator().Free(packed);
        LbFile_Delete(path);
        mError = kErrorOpenOrRead;
        return 0;
    }
    disc.Seek(0, 2);
    const uint32_t count = static_cast<uint32_t>(disc.Write(packed, packedSize));   // -1 without an access object
    if (packedSize != count) {
        SaveTemporaryAllocator().Free(packed);
        disc.Close();
        mError = kErrorMemoryWrite;
        return 0;
    }
    SaveTemporaryAllocator().Free(packed);
    disc.Close();
    return 1;
}
