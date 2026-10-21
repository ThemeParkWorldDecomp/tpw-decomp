// CBlockSave (BlockSave.cpp in the inventory; main app): the save block table, one {pointer, size} entry per saved block, indexed by
// the block number (unit w113). The only registration in the program is block 0, the world (CWorld::Allocate). The object at
// 0x1034c1ac has room for exactly two entries (the next global begins 0x10 bytes on). The Mac entry is 8 bytes; the host's is 16.
#pragma once
#include <cstdint>

class CBlockSave {
public:
    enum SaveBlock : int32_t { kWorld = 0 };   // the name is invented
    struct Entry {
        void* mData;
        int32_t mSize;
    };
    // 0x100f46ec (20 bytes, src/game/save/BlockSave.cpp): entry `block` = {data, size}. The name is never read.
    void RegisterSaveBlock(SaveBlock block, void* data, int32_t size, const char* name);

    Entry mBlocks[2];
};
extern CBlockSave gBlockSave;   // 0x1034c1ac
