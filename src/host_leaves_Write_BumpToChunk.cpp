// Unit v5247: Write_BumpToChunk 0x10021960 and Write_CloseChunk 0x10021e44 (the advisor save's chunk writer), main app.
#include <cstdint>
#include <cstring>

#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "platform/macdoze.h"

namespace {
// The file is little-endian: the Mac swaps each big-endian word before the write; on the host the word is already in file
// order, so the swap does nothing (as the reader's USwap32 calls, src/host_leaves_Write_BumpToChunk_BMP_Save_Read_BumpFromChunk.cpp).
uint32_t Swap(uint32_t v) { return v; }
}  // namespace

// The original copies 0xac bytes of the bump (the struct is 0xa8: the last word is read past it), swaps every word
// (the USwap calls) and writes them.
uint32_t Write_BumpToChunk(tabBMPCHUNK* chunk, tagBUMP& bump) {
    uint32_t words[0x2b];
    std::memcpy(words, &bump, 0xac);
    for (uint32_t& word : words) word = Swap(word);
    uint32_t transferred;   // uninitialised in the original too
    TbError result = LbFile_Write(chunk->file.get(), words, 0xac, &transferred);
    if (result.mValue < 0) return 0;
    chunk->dataSize += 0xac;
    return transferred;
}

int32_t Write_CloseChunk(tabBMPCHUNK* chunk) {
    int32_t position = LbFile_GetPosition(chunk->file.get());
    chunk->physicalSize = position - chunk->start;
    TRACE("CloseChunk: DataSize %d  PhysicalSize %d\n", chunk->dataSize, chunk->physicalSize);
    LbFile_Seek(chunk->file.get(), chunk->start, 0);
    uint32_t header[3];
    header[0] = Swap(chunk->tag);
    header[1] = Swap(static_cast<uint32_t>(chunk->dataSize));
    header[2] = Swap(static_cast<uint32_t>(chunk->physicalSize));
    uint32_t transferred;
    TbError result = LbFile_Write(chunk->file.get(), header, 0xc, &transferred);
    if (result.mValue >= 0) chunk->dataSize += 0xc;
    LbFile_Seek(chunk->file.get(), position, 0);
    int32_t answer = chunk->physicalSize;
    NS_MacDoze::GlobalFree(reinterpret_cast<NS_MacDoze::InternalHandle*>(chunk));
    return answer;
}
