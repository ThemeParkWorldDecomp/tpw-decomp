// CBlockSave (main app of SimTheme Park 2.04b3; unit w113): the save block table.
#include "BlockSave.h"

// 0x100f46ec (20 bytes): entry `block` (the Mac's table has 8-byte entries, index << 3) gets the pointer, then the size. The name is
// never read. The object has room for two entries; a larger block number would overwrite the next global on the Mac.
void CBlockSave::RegisterSaveBlock(SaveBlock block, void* data, int32_t size, const char* name) {
    mBlocks[block].mData = data;
    mBlocks[block].mSize = size;
}
