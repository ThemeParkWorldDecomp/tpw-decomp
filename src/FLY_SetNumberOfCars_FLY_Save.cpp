#include <cstdint>
#include <cstring>

#include "HLSprite.h"
#include "rsse.h"
#include "SaveLoad.h"
#include "LbFile.h"
#include "TbError.h"
#include "analyser.h"
#include "platform/samsutils.h"

// 0x10079248 (1240 bytes): FLY_Save writes the fly file: the magic 'FLY_', the initialised flag, and when it is set the table of bases (400
// raw bytes: on the Mac 100 32-bit pointers; the original writes them as they are), the four zone limits, five 'DAP_' words, 0x122c, and for
// bases 1 to 99 that are set a swapped copy of the 0x122c-byte base. Every word goes through _USwapCopy32 and its own LbFile_Write.
// Always answers 0.

namespace {

constexpr uint32_t kBaseSize = 0x122c;

// _USwapCopy32 of one word, one LbFile_Write of 4 bytes, _USwapDelete.
void WriteWord(void* file, const uint32_t* word) {
    uint32_t* copy = SamsUtilities::_USwapCopy32(word, 1);
    uint32_t transferred;
    LbFile_Write(file, copy, 4, &transferred);
    SamsUtilities::_USwapDelete(copy);
}

}  // namespace

int32_t FLY_Save(void* file) {
    uint32_t word = 0x5f594c46;
    WriteWord(file, &word);
    WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyInitialised));
    if (gFlyInitialised != 0) {
        word = 0x190;
        WriteWord(file, &word);
        LbFile_Write(file, gFlyBases, word, nullptr);   // the table as it is in memory (the original's length is the word just written)
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneD));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneB));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneA));
        WriteWord(file, reinterpret_cast<const uint32_t*>(&gFlyZoneC));
        word = 0x5f444150;
        for (int32_t k = 0; k < 5; ++k) WriteWord(file, &word);
        word = kBaseSize;
        WriteWord(file, &word);
        for (int32_t i = 1; i < 100; ++i) {
            if (gFlyBases[i] == nullptr) continue;
            alignas(8) uint8_t buffer[kBaseSize];
            std::memcpy(buffer, gFlyBases[i], kBaseSize);
            for (uint32_t offset = 0; offset <= 0x54; offset += 4)
                SamsUtilities::USwapInplace32<int32_t>(*reinterpret_cast<int32_t*>(buffer + offset));
            reinterpret_cast<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>*>(buffer + 0x58)->Byteswap();
            SamsUtilities::USwapBlockStruct(reinterpret_cast<FLYBOY*>(buffer + 0x5c), 0x14);
            LbFile_Write(file, buffer, kBaseSize, nullptr);
        }
    }
    return 0;
}
