// The chunk file reader (the inventory files these under Entertainer.cpp and World.cpp, unreliable; main app, unit w112c): the
// reader that CMap::LoadAttribMap uses on the level's "Base.map". A file is a 0x24 byte header (the first word is the reader's magic,
// "TP2M" in the file), then chunks of {id, length, data} up to an END chunk; each chunk goes to the handlers that claim its id.
//
// Mac layouts (the host's differ where a class has virtuals: the vtable pointer comes first here):
//   handler:  +0 id (u32), +4 vtable (slot 8 IsThisMyChunk, slot 0xc ParseChunk), CMapChunkHandler adds +8 the map (0xc bytes);
//   reader:   TbPtrList<CIndividualChunkHandler> (vtable 0x102a471c at +0, head, tail, count) and +0x10 the magic: 0x14 bytes;
//   CMapAttribFileReader (the local object of LoadAttribMap): the reader and, at +0x14, a CMapChunkHandler: 0x20 bytes.
// The Mac reader has no virtual of its own (its object carries the list's vtable word); its constructor is inline in the users.
#pragma once
#include <cstdint>

#include "TbTList.h"
#include "bfl_FileSystem.h"

class CMap;

class CIndividualChunkHandler {
public:
    explicit CIndividualChunkHandler(uint32_t chunkId) : mChunkId(chunkId) {}
    // 0x100f9c98 (28 bytes, src/game/save/ChunkFileReader.cpp): whether the id is this handler's. Slot +8 of the Mac vtable. The parameter is a
    // tag_IDX_TYPE<IDXTYPEUniqueNameChunkId, unsigned long> there (one register); a plain number here.
    virtual bool IsThisMyChunk(uint32_t chunkId);
    // Slot +0xc of the Mac vtable (0 in the base's table, 0x102a46f0): the handler's work on a chunk's data.
    virtual void ParseChunk(void* data, uint32_t size) = 0;

    uint32_t mChunkId;      // Mac +0
};

// The handler for the "MAP " chunk (id 0x2050414d) of an attribute map: copies the file's bytes into the attribute byte of the cells.
class CMapChunkHandler : public CIndividualChunkHandler {
public:
    CMapChunkHandler(uint32_t chunkId, CMap* map) : CIndividualChunkHandler(chunkId), mMap(map) {}
    // 0x1010276c (420 bytes, src/game/park/Map.cpp): data is the chunk's data (the size is not read). Words 0 and 1 are the two extents
    // (little endian), the bytes start at +0x1c, x-major; only the cells inside the 128 by 128 map get their attribute byte.
    void ParseChunk(void* data, uint32_t size) override;

    CMap* mMap;             // Mac +8  the map (the address of its first cell)
};

class CChunkFileReader : public TbPtrList<CIndividualChunkHandler> {
public:
    explicit CChunkFileReader(uint32_t magic) : mMagic(magic) {}
    // 0x1013e754 (108 bytes, src/game/World.cpp): the destructor; the Mac copy takes a delete flag and frees the list nodes.
    ~CChunkFileReader() override;

    // 0x100f98c8 (36 bytes): puts the handler at the head of the list (the newest handler is asked first).
    void AddChunkHandler(CIndividualChunkHandler* handler);
    // 0x100f9d20 (124 bytes): opens the file ("rb") and reads it; answers 0 when it does not open (CExitError type 3 with the path).
    // The file object is never deleted.
    int32_t ReadFile(const char* path);
    // 0x100f9940 (780 bytes): reads the whole file and walks its chunks. 0 for a failed read, a file under 0x2c bytes or a wrong
    // magic; 1 for anything else, whatever the chunks do. `name` is only the text of the errors.
    int32_t ReadFile(bfl::FileSystem::CFile& file, char* name);

    uint32_t mMagic;        // Mac +0x10  the file's first word
};

// The local object of CMap::LoadAttribMap (0x1013e5ac): a reader with the magic 0x4d325054 and one "MAP " handler.
class CMapAttribFileReader : public CChunkFileReader {
public:
    explicit CMapAttribFileReader(CMap* map) : CChunkFileReader(0x4d325054), mHandler(0x2050414d, map) {}
    // Unit w205 (src/game/World.cpp). 0x1013e6b0 (112 bytes): the destructor of the local object; the Mac copy sets the list's vtable word, destroys
    // the list part (RemoveAll) and deletes the object when its flag is positive. The host destructor is the base chain's.
    ~CMapAttribFileReader() override;

    CMapChunkHandler mHandler;   // Mac +0x14
};
