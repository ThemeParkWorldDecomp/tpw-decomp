// GAMESYSTEM_Save (0x1023678c) and GAMESYSTEM_Load (0x10236b30): the park file's clock words, big-endian.
#include <cstdint>

#include "GameSys.h"
#include "LbFile.h"
#include "platform/samsutils.h"

namespace {

inline uint32_t Swap(uint32_t v) { return __builtin_bswap32(v); }

// Writes one word big-endian and swaps it back (the original swaps the variable in place around the write).
bool WriteWord(void* file, uint32_t* word) {
    uint32_t transferred;
    *word = Swap(*word);
    LbFile_Write(file, word, 4, &transferred);
    *word = Swap(*word);
    return (transferred >> 2) == 1;
}

bool ReadWord(void* file, uint32_t* word) {
    uint32_t transferred;
    LbFile_Read(file, word, 4, &transferred);
    if ((transferred >> 2) != 1) return false;
    *word = Swap(*word);
    return true;
}

}  // namespace

// 0x1023678c
int32_t GAMESYSTEM_Save(void* file) {
    uint32_t* const words[5] = {
        reinterpret_cast<uint32_t*>(&gTickTime), reinterpret_cast<uint32_t*>(&gLastFlyTime),
        reinterpret_cast<uint32_t*>(&gLastTurnTime), reinterpret_cast<uint32_t*>(&gTickCount),
        reinterpret_cast<uint32_t*>(&gGameSys_103bc178)};
    for (uint32_t* w : words) {
        if (!WriteWord(file, w)) {
            SamsUtilities::UDebug("write failed");
            return 0;
        }
    }
    // The original writes an uninitialised stack word first (0x2cc); the port writes 0.
    uint32_t rel[4];
    rel[0] = 0;
    rel[1] = static_cast<uint32_t>(gTickTime) - static_cast<uint32_t>(gTickNow);
    rel[2] = static_cast<uint32_t>(gLastFlyTime) - static_cast<uint32_t>(gTickNow);
    rel[3] = static_cast<uint32_t>(gLastTurnTime) - static_cast<uint32_t>(gTickNow);
    for (uint32_t& w : rel) {
        if (!WriteWord(file, &w)) {
            SamsUtilities::UDebug("write failed");
            return 0;
        }
    }
    return 1;
}

// 0x10236b30
int32_t GAMESYSTEM_Load(void* file) {
    uint32_t* const words[5] = {
        reinterpret_cast<uint32_t*>(&gTickTime), reinterpret_cast<uint32_t*>(&gLastFlyTime),
        reinterpret_cast<uint32_t*>(&gLastTurnTime), reinterpret_cast<uint32_t*>(&gTickCount),
        reinterpret_cast<uint32_t*>(&gGameSys_103bc178)};
    for (uint32_t* w : words) {
        if (!ReadWord(file, w)) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }
    }
    uint32_t rel[4];   // the first is read and dropped
    for (uint32_t& w : rel) {
        if (!ReadWord(file, &w)) {
            SamsUtilities::UDebug("read failed");
            return 0;
        }
    }
    gTickTime = static_cast<int32_t>(static_cast<uint32_t>(gTickNow) + rel[1]);
    gLastFlyTime = static_cast<int32_t>(static_cast<uint32_t>(gTickNow) + rel[2]);
    gLastTurnTime = static_cast<int32_t>(static_cast<uint32_t>(gTickNow) + rel[3]);
    return 1;
}
