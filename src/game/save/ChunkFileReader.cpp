// ChunkFileReader.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of ChunkFileReader code, named after it.

#include "CellBase.h"
#include "ChunkFile.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "World.h"
#include <cstdint>
#include <cstring>

// The chunk file reader and the attribute map load (main app of SimTheme Park 2.04b3; the inventory files these under
// Entertainer.cpp, lobby.cpp, MapDelta.cpp and World.cpp, unreliable; unit w112c): CChunkFileReader, its handler classes, the list
// members it uses and CMap::LoadAttribMap, which reads the level's attribute map ("Base.map") into the cells' attribute bytes.

// The list instance the reader derives from. The Mac has its constructor, destructor, AddHead and RemoveAll (0x101412a8, 0x10141320,
// 0x100f9dd0, 0x101413e0); the six other members are only here because the instance is whole (the Mac has no copy of them).
template class TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*>;
// Unit w205: the pointer list's destructor, 0x1013e7f0 (104 bytes). The Mac copy sets the list's vtable word (0x102a471c), destroys the list part
// (RemoveAll) and deletes the object when its flag is positive; on the host it is the template's inline `~TbPtrList() override {}`, emitted here.
template class TbPtrList<CIndividualChunkHandler>;

namespace {
constexpr uint32_t kEndChunk = 0x20444e45;   // "END " in the file
}  // namespace

// 0x100f98c8 (36 bytes).
void CChunkFileReader::AddChunkHandler(CIndividualChunkHandler* handler) { AddHead(handler); }

// 0x100f9d20 (124 bytes). The file object is not deleted (ReadFile only closes it).
int32_t CChunkFileReader::ReadFile(const char* path) {
    bfl::FileSystem::CFile* const file = TPFile_Open(path, "rb");
    if (file == nullptr) {
        gExitError.SetError(CExitError::kType3, 0, path);
        return 0;
    }
    return ReadFile(*file, nullptr);
}

// 0x100f9940 (780 bytes). The three failures differ in their order: a size of -1 is SetError then Close, no memory is Close then
// SetError (type 8), a short read is SetError, Close, Free. Past the 0x24 byte header the chunks are walked with a 32 bit byte
// offset (the Mac keeps a pointer and wraps at 2^32): the loop runs while the offset, as a signed number, is at most size - 8; a
// chunk whose length added to the offset (32 bit sum) passes the size ends the walk after a message; every handler that claims a
// chunk parses it (newest first). The answer is 1 for any file of 0x2c bytes or more with the right magic.
int32_t CChunkFileReader::ReadFile(bfl::FileSystem::CFile& file, char* name) {
    const int32_t size = file.GetSize();
    if (size == -1) {
        gExitError.SetError(CExitError::kType3, 0, name);
        file.Close();
        return 0;
    }
    uint8_t* const buffer = static_cast<uint8_t*>(LbMem_Alloc(static_cast<uint32_t>(size)));
    if (buffer == nullptr) {
        file.Close();
        gExitError.SetError(CExitError::kType8, 0, name);
        return 0;
    }
    if (static_cast<uint32_t>(size) != static_cast<uint32_t>(file.Read(buffer, static_cast<uint32_t>(size)))) {
        gExitError.SetError(CExitError::kType3, 0, name);
        file.Close();
        LbMem_Free(buffer);
        return 0;
    }
    file.Close();
    if (size < 0x2c) {
        LbMem_Free(buffer);
        return 0;
    }
    uint32_t magic;
    std::memcpy(&magic, buffer, 4);
    if (magic != mMagic) {
        LbMem_Free(buffer);
        return 0;
    }
    uint32_t offset = 0x24;
    bool ended = false;
    while (!ended && static_cast<int32_t>(offset) <= size - 8) {
        uint32_t chunkId;
        uint32_t length;
        std::memcpy(&chunkId, buffer + static_cast<int32_t>(offset), 4);
        std::memcpy(&length, buffer + static_cast<int32_t>(offset) + 4, 4);
        offset += 8;
        uint8_t* const chunkData = buffer + static_cast<int32_t>(offset);
        if (length + offset > static_cast<uint32_t>(size)) {
            DEBUG_MESSAGE("Claimed chunk length runs past the end of the whole file", 0);
            break;
        }
        if (chunkId == kEndChunk && length == 0) {
            ended = true;
        } else {
            bool claimed = false;
            POSITION position = GetHeadPosition();
            while (position != nullptr) {
                CIndividualChunkHandler* const handler = GetNext(position);
                if (handler->IsThisMyChunk(chunkId)) {
                    claimed = true;
                    handler->ParseChunk(chunkData, length);
                }
            }
            if (!claimed) DEBUG_MESSAGE("Unrecognised chunk", 0);
        }
        offset += length;
    }
    if (!ended) DEBUG_MESSAGE("Missing End Chunk", 0);
    LbMem_Free(buffer);
    return 1;
}
// 0x100f9c98 (28 bytes).
bool CIndividualChunkHandler::IsThisMyChunk(uint32_t chunkId) { return mChunkId == chunkId; }
