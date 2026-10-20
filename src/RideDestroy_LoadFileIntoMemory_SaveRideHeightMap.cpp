#include "TPEngine.h"
#include "FS_System.h"
#include "m3d2.h"
#include "platform/samsutils.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "Debug.h"
#include "platform/arena.h"

namespace {
// The stored pointer words are arena offsets on the host (Ptr32); 0 is null.
inline void* PtrOf(uint32_t offset) { return offset != 0 ? tpw::gArenaBase + offset : nullptr; }
}  // namespace

// 0x1005e6a0: writes the height map to `path` as one block: the 0x30-byte header, then the cells (25 bytes per
// cell), then the two byte planes (1 byte per cell each); the three pointers are stored as offsets from the block start.
// Answers 1 when fwrite wrote its one item, else 0 (also when the file cannot be opened).
void SaveRideHeightMap(sRideHeightMap* map, char* path) {
    const uint8_t* src = reinterpret_cast<const uint8_t*>(map);
    uint32_t count = static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(src + 8)) *
                     static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(src + 10));
    uint32_t cellBytes = count * 0x19;
    uint32_t total = cellBytes + count * 2 + 0x30;
    uint8_t* block = static_cast<uint8_t*>(MReleaseAllocF(total, 0));
    uint32_t* head = reinterpret_cast<uint32_t*>(block);
    const uint32_t* from = reinterpret_cast<const uint32_t*>(src);
    for (int i = 0; i < 12; ++i) head[i] = from[i];

    uint8_t* cursor = block + 0x30;
    head[3] = tpw::ArenaOffset(cursor);
    cursor += cellBytes;
    if (PtrOf(head[3]) != nullptr && PtrOf(from[3]) != nullptr && cellBytes != 0)
        std::memcpy(PtrOf(head[3]), PtrOf(from[3]), cellBytes);
    head[4] = tpw::ArenaOffset(cursor);
    cursor += count;
    if (PtrOf(head[4]) != nullptr && PtrOf(from[4]) != nullptr && count != 0)
        std::memcpy(PtrOf(head[4]), PtrOf(from[4]), count);
    head[5] = tpw::ArenaOffset(cursor);
    if (PtrOf(head[5]) != nullptr && PtrOf(from[5]) != nullptr && count != 0)
        std::memcpy(PtrOf(head[5]), PtrOf(from[5]), count);

    uint32_t base = tpw::ArenaOffset(block);
    head[3] -= base;
    head[4] -= base;
    head[5] -= base;

    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        CTRACE(0, "Could not open file '%s' for writing\n", path);
        MReleaseFreeF(block);
        return;
    }
    std::fwrite(block, total, 1, file);
    std::fclose(file);
    MReleaseFreeF(block);
}
