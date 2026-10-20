// Unit v5127: LoadFileIntoMemory 0x1005e300, LoadRideHeightMap 0x1005e40c, GetRideHeightMap 0x1005e8e8.
#include "TPEngine.h"
#include "FS_System.h"
#include "m3d2.h"
#include "platform/samsutils.h"

// 0x1005e300: the whole file in a block; mode "rb" (the TOC string at 0x1023d2b0) when binary, else "rb" + 3 = "b".
char* LoadFileIntoMemory(char* path, int32_t binary) {
    void* block = nullptr;
    uint32_t read = 0;
    bfl::FileSystem::CFile* file;
    // The TOC string is "rb" followed by "r": the pool holds "rb\0r\0", so "rb" + 3 is the text "r".
    static const char kModes[] = "rb\0r";
    if (binary == 0)
        file = TPFile_Open(path, kModes + 3);
    else
        file = TPFile_Open(path, kModes);
    if (file != nullptr) {
        uint32_t size = TPFile_Size(file);
        block = MReleaseAllocF(size, 1);
        if (block != nullptr) read = TPFile_Read(block, size, 1, file);
        TPFile_Close(file);
    }
    if (read == 0) {
        MReleaseFreeF(block);
        block = nullptr;
    }
    return static_cast<char*>(block);
}

// 0x1005e40c: loads and byte-swaps a height map file, checks its magic and turns the offsets into pointers.
sRideHeightMap* LoadRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path) {
    sRideHeightMap* map = reinterpret_cast<sRideHeightMap*>(LoadFileIntoMemory(path, 1));
    if (map != nullptr) {
        uint8_t* bytes = reinterpret_cast<uint8_t*>(map);
        USwapInplace32ULong_1005e658(map->mF00);
        USwapInplace32ULong_1005e658(map->mF04);
        USwapInplace16UShort_1005e610(*reinterpret_cast<uint16_t*>(bytes + 8));
        USwapInplace16UShort_1005e610(*reinterpret_cast<uint16_t*>(bytes + 10));
        uint32_t* words = reinterpret_cast<uint32_t*>(bytes);
        words[3] = USwap32UCharPtr_1005e5b4(words[3]);
        words[4] = USwap32UCharPtr_1005e5b4(words[4]);
        words[5] = USwap32UCharPtr_1005e5b4(words[5]);
        reinterpret_cast<sAlignedBox*>(bytes + 0x18)->Byteswap();
        if (map->mF00 == 0xab1e0003 && map->mF04 == 0x640005) {
            // The Mac adds the block's address; here the three words become arena offsets (Ptr32's convention, as
            // GenerateRideHeightMap fills them), so the block's own arena offset is added.
            uint32_t base = tpw::ArenaOffset(map);
            words[3] += base;
            words[4] += base;
            words[5] += base;
        } else {
            MReleaseFreeF(map);
            map = nullptr;
        }
    }
    return map;
}

// 0x1005e8e8: the stored map when it matches (w, h), else a generated one that is saved; its box goes to the mesh header.
void GetRideHeightMap(tag_sRideAnimSet* set, uint32_t w, uint32_t h, char* path, void* a, int32_t b) {
    sRideHeightMap* map = LoadRideHeightMap(set, w, h, path);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(map);
    if (map == nullptr || w != *reinterpret_cast<const uint16_t*>(bytes + 8) ||
        h != *reinterpret_cast<const uint16_t*>(bytes + 10)) {
        map = GenerateRideHeightMap(set, w, h, static_cast<RideShape*>(a), b);
        if (map != nullptr) SaveRideHeightMap(map, path);
    }
    // No NULL check on map, as in the original.
    uint32_t* dest = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(set->mHeads->mHeader) + 0x80);
    const uint32_t* src = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(map) + 0x18);
    for (int i = 0; i < 6; ++i) dest[i] = src[i];
    set->mHeightMap = map;
}
