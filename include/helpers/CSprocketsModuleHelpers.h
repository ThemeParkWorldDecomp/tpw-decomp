// CSprocketsModuleHelpers.h: file-local helpers that src/sound/CSprocketsModule.cpp shares with SprocketsChannel.cpp.
// Before the regroup, the functions using them shared a source file (2 files in all), which had one copy of each; that
// copy now lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include <cstdint>
#include <cstring>

static inline uint32_t LoadWord(const void* base, uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, 4);
    return value;
}

static inline void StoreWord(void* base, uint32_t offset, uint32_t value) {
    std::memcpy(static_cast<uint8_t*>(base) + offset, &value, 4);
}

namespace SamsUtilities {
static inline void Put32(uint8_t* base, size_t offset, uint32_t value) { std::memcpy(base + offset, &value, 4); }

}  // namespace SamsUtilities
