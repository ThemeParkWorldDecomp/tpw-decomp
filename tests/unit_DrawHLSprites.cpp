// Test library for the script sprites' reset and draw (unit v5114; libtpw_unit_DrawHLSprites.dylib, tests/test_DrawHLSprites.py), main app:
//   src/ESP_sprites_DrawHLSprites.cpp   ResetHLSprites, DrawHLSprites and gHLSpritesEnabled
//
// The callees outside the unit (LLCC_SystemGetPointedPeepThing, CIntrface::SetCurrentThing, GetLMatrixRotationAroundYAxis) are recording
// fakes. The globals of other files are defined here, zero at start. The two CSpriteList arrays are plain zeroed storage under the
// program's symbol names (their constructors live in files this library does not compile); the test reads only their mCount.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPCamera.h"
#include "intrface.h"
#include "rsse.h"

using namespace calllog;

// ---- globals of other files
uint32_t gFirstPersonFlags;
sMatrix* gCurrentCameraMatrix;
float gCameraF5BF4;
CThing* gESPF20;
float gESPF88;
int32_t gESPGlobalCount;
int32_t gESPCellCounts[4][4];
CIntrface gIntrface;
alignas(16) uint8_t gHLStorage[16 * sizeof(CSpriteList)] __asm__("_gHLSpriteLists");
alignas(16) uint8_t gGlobalListStorage[sizeof(CSpriteList)] __asm__("_gESPGlobalList");

// ---- the fakes
CThing* LLCC_SystemGetPointedPeepThing() {
    ev("GetPointed", {1});
    return reinterpret_cast<CThing*>(static_cast<uintptr_t>(ret("GetPointed") & 0xffffffffu));
}

void CIntrface::SetCurrentThing(CThing* thing) {
    ev("SetCurrentThing", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(thing) & 0xffffffffu)});
}

float GetLMatrixRotationAroundYAxis(sMatrix* matrix) {
    ev("GetLMatrix", {matrix != nullptr ? 1 : 0});
    uint32_t bits = static_cast<uint32_t>(ret("GetLMatrix"));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

extern "C" {

uint64_t tpw_hl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"flags", &gFirstPersonFlags},
        {"cammat", &gCurrentCameraMatrix},
        {"yaw", &gCameraF5BF4},
        {"f20", &gESPF20},
        {"f88", &gESPF88},
        {"globalcount", &gESPGlobalCount},
        {"cellcounts", gESPCellCounts},
        {"enabled", &gHLSpritesEnabled},
        {"globallist", &gESPGlobalList.mCount},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    if (std::strncmp(name, "hl", 2) == 0) {
        int k = std::atoi(name + 2);
        return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gHLSpriteLists[k].mCount));
    }
    return 0;
}

uint64_t tpw_hl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_hl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(sMatrix));
    return n;
}

void tpw_hl_reset() { ResetHLSprites(); }
void tpw_hl_draw() { DrawHLSprites(); }

}  // extern "C"
