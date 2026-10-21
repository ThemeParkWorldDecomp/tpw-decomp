// The advisor save's chunk reader: Read_OpenChunk 0x10021fac, Read_DataFromChunk 0x100220bc, Read_BumpFromChunk 0x100221d4.
// The file is little-endian: the Mac swaps every word after reading it; on the host the words are already in order, so the
// swaps (SamsUtilities::USwap32 of include/TbSoundBank.h) do nothing.
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "LbFile.h"
#include "SaveLoad.h"
#include "TbSoundBank.h"
#include "platform/macdoze.h"

// 0x100220bc: reads up to `size` bytes of the chunk's data into buffer (never past the chunk's end), zero fills what is missing,
// answers the bytes read, 0 when the read failed.
uint32_t Read_DataFromChunk(tabBMPCHUNK* chunk, void* buffer, uint32_t size) {
    uint32_t remaining = static_cast<uint32_t>(chunk->dataSize - static_cast<int32_t>(chunk->pad08));
    const uint32_t* count = remaining < size ? &remaining : &size;
    uint32_t transferred;
    TbError result = BMPFile_Read(chunk->file.get(), buffer, *count, &transferred);
    if (result.mValue < 0) {
        TRACE("#####Read_DataFromChunk - ReadFailed####\n");
        return 0;
    }
    chunk->pad08 += transferred;
    if (transferred < size) std::memset(static_cast<uint8_t*>(buffer) + transferred, 0, size - transferred);
    return transferred;
}

// 0x10021fac: makes a chunk at the file's position, reads its header (tag, data size, physical size) and answers the tag, 0 when
// the allocation fails.
uint32_t Read_OpenChunk(void* file, Ptr32<tabBMPCHUNK>* out) {
    tabBMPCHUNK* chunk = reinterpret_cast<tabBMPCHUNK*>(NS_MacDoze::GlobalAlloc(0x40, 0x18));
    if (chunk == nullptr) return 0;
    chunk->start = LbFile_GetPosition(file);
    chunk->file.set(static_cast<uint8_t*>(file));
    chunk->pad08 = 0;
    chunk->dataSize = 0xc;
    Read_DataFromChunk(chunk, &chunk->tag, 0xc);
    chunk->tag = SamsUtilities::USwap32(chunk->tag);
    chunk->dataSize = SamsUtilities::USwap32(chunk->dataSize);
    chunk->physicalSize = SamsUtilities::USwap32(chunk->physicalSize);
    out->set(chunk);
    return chunk->tag;
}

// 0x100221d4: reads the bump's 0xac bytes (43 words, the last one past tagBUMP), then swaps each word (a no-op here).
uint32_t Read_BumpFromChunk(tabBMPCHUNK* chunk, tagBUMP& bump) {
    uint32_t size = 0xac;
    uint32_t remaining = static_cast<uint32_t>(chunk->dataSize - static_cast<int32_t>(chunk->pad08));
    const uint32_t* count = remaining < 0xac ? &remaining : &size;
    uint32_t transferred;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(&bump);
    TbError result = BMPFile_Read(chunk->file.get(), bytes, *count, &transferred);
    uint32_t answer;
    if (result.mValue >= 0) {
        chunk->pad08 += transferred;
        if (transferred < size) std::memset(bytes + transferred, 0, size - transferred);
        answer = transferred;
    } else {
        TRACE("#####Read_DataFromChunk - ReadFailed####\n");
        answer = 0;
    }
    for (uint32_t offset = 0; offset <= 0xa8; offset += 4) {
        uint32_t word;
        std::memcpy(&word, bytes + offset, 4);
        word = SamsUtilities::USwap32(word);
        std::memcpy(bytes + offset, &word, 4);
    }
    return answer;
}
