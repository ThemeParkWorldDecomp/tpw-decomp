#include "DrawRenderClasses.h"
#include "TbRaConvert.h"
#include "LbPalette.h"
#include "LbFile.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// 0x100474cc. The three comparisons with a constant TbError (data words 0x10073 7d4, 7d8, 7dc, never
// written) reduce to Succeeded().
TbError LbPalette_GenerateGhostTable(uint8_t** table, const TbPalette* palette, uint32_t percent,
                                     const char* name) {
    TbError error(0);
    uint8_t* buffer = *table;
    if (buffer == nullptr) {
        buffer = static_cast<uint8_t*>(LbMem_Alloc(0x10000));
        if (buffer == nullptr) return TbError(-1);
    }
    error = LbFile_Load(name, buffer, 0x10000, nullptr);
    if (!error.Succeeded()) {
        const uint8_t* colours = reinterpret_cast<const uint8_t*>(palette);
        const int32_t scale = static_cast<int32_t>(percent);
        uint8_t* out = buffer;
        for (int i = 0; i < 256; i++) {
            const uint8_t* from = colours + 4 * i;
            for (int j = 0; j < 256; j++) {
                const uint8_t* to = colours + 4 * j;
                int32_t r = from[0] + (scale * (to[0] - from[0])) / 100;
                int32_t g = from[1] + (scale * (to[1] - from[1])) / 100;
                int32_t b = from[2] + (scale * (to[2] - from[2])) / 100;
                *out++ = LbPalette_FindColour(palette, static_cast<uint8_t>(r), static_cast<uint8_t>(g),
                                              static_cast<uint8_t>(b));
            }
        }
        error = LbFile_Save(name, buffer, 0x10000);
        if (!error.Succeeded() && *table == nullptr) LbMem_Free(buffer);
    }
    if (error.Succeeded() && *table == nullptr) *table = buffer;
    return error;
}
