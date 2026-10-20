// The chunk file reader and the attribute map load (main app of SimTheme Park 2.04b3; the inventory files these under
// Entertainer.cpp, lobby.cpp, MapDelta.cpp and World.cpp, unreliable; unit w112c): CChunkFileReader, its handler classes, the list
// members it uses and CMap::LoadAttribMap, which reads the level's attribute map ("Base.map") into the cells' attribute bytes.
#include "ChunkFile.h"

#include <cstdint>
#include <cstring>

#include "CellBase.h"
#include "Debug.h"
#include "ExitErr.h"
#include "FS_System.h"
#include "LbFile.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "World.h"

// The list instance the reader derives from. The Mac has its constructor, destructor, AddHead and RemoveAll (0x101412a8, 0x10141320,
// 0x100f9dd0, 0x101413e0); the six other members are only here because the instance is whole (the Mac has no copy of them).
template class TbTList<CIndividualChunkHandler*, CIndividualChunkHandler*>;
// Unit w205: the pointer list's destructor, 0x1013e7f0 (104 bytes). The Mac copy sets the list's vtable word (0x102a471c), destroys the list part
// (RemoveAll) and deletes the object when its flag is positive; on the host it is the template's inline `~TbPtrList() override {}`, emitted here.
template class TbPtrList<CIndividualChunkHandler>;

namespace {
// The main balance's value slots of the height field's start (the Mac's byte offsets 0x544 and 0x548 less the table's 8 bytes of
// header, over four, equal Init's slot numbers 335 and 336 for the rows HeightfieldXStart and HeightfieldYStart).
constexpr int kBalanceHeightfieldXStart = 335;
constexpr int kBalanceHeightfieldYStart = 336;

constexpr uint32_t kEndChunk = 0x20444e45;   // "END " in the file
}  // namespace

// 0x100f9c98 (28 bytes).
bool CIndividualChunkHandler::IsThisMyChunk(uint32_t chunkId) { return mChunkId == chunkId; }

// 0x1010276c (420 bytes). The data is the chunk's: two extents at words 0 and 1 (file order, little endian), words 2 to 6 unused,
// the bytes from +0x1c. The outer loop runs to the SECOND word and subtracts the x start from its counter, the inner loop runs to
// the first and subtracts the y start; a byte is stored only for a cell inside 0 to 0x7f in both, and the source moves on for every
// byte. All compares are signed, the subtractions wrap.
void CMapChunkHandler::ParseChunk(void* data, uint32_t size) {
    const uint8_t* source = static_cast<const uint8_t*>(data) + 0x1c;
    uint32_t extent0;
    uint32_t extent1;
    std::memcpy(&extent0, data, 4);
    std::memcpy(&extent1, static_cast<const uint8_t*>(data) + 4, 4);
    const int32_t xStart = gMainBalance.mValues[kBalanceHeightfieldXStart].mInteger;
    const int32_t yStart = gMainBalance.mValues[kBalanceHeightfieldYStart].mInteger;
    CMapCell* const cells = reinterpret_cast<CMapCell*>(mMap);
    for (int32_t outer = 0; outer < static_cast<int32_t>(extent1); ++outer) {
        for (int32_t inner = 0; inner < static_cast<int32_t>(extent0); ++inner) {
            const int32_t cellX = static_cast<int32_t>(static_cast<uint32_t>(outer) - static_cast<uint32_t>(xStart));
            const int32_t cellY = static_cast<int32_t>(static_cast<uint32_t>(inner) - static_cast<uint32_t>(yStart));
            if (cellX >= 0 && cellX < 0x80 && cellY >= 0 && cellY < 0x80) {
                const uint16_t place = static_cast<uint16_t>(static_cast<uint32_t>(cellX) + (static_cast<uint32_t>(cellY) << 7) + 1u);
                cells[place - 1].mStatusFlags = *source;
            }
            ++source;
        }
    }
}

// 0x1013e754 (108 bytes): the Mac copy sets the list's vtable word, destroys the list part (RemoveAll) and deletes the object when
// its flag is positive. The host destructor is the base chain's.
CChunkFileReader::~CChunkFileReader() {}

// Unit w205. 0x1013e6b0 (112 bytes): the Mac copy tests `this` twice (the second test is redundant), sets the list's vtable word, destroys the
// list part (RemoveAll) and deletes the object when its flag is positive. It does not run ~CChunkFileReader (0x1013e754); the host destructor
// does (an empty body), then the base chain's. The handler member's destructor is the implicit, trivial one.
CMapAttribFileReader::~CMapAttribFileReader() {}

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

// 0x1013e5ac (216 bytes). The reader and its one handler are local objects: the magic is "TP2M" in the file, the chunk id "MAP ".
// The handler is added, the file read and the reader destroyed (its list nodes freed) before the answer is returned.
int32_t CMap::LoadAttribMap(const char* path) {
    CMapAttribFileReader reader(this);
    reader.AddChunkHandler(&reader.mHandler);
    return reader.ReadFile(path);
}
