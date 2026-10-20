// HLSprite.cpp: ResetHLSprites 0x1007af24 and DrawHLSprites 0x1007b02c.
#include <cmath>
#include <cstdint>

#include "ESP_sprites.h"
#include "HLSprite.h"
#include "TPCamera.h"
#include "intrface.h"
#include "rsse.h"

int32_t gHLSpritesEnabled;   // 0x102dbd38

void ResetHLSprites() {
    for (int32_t i = 0; i < 4; i++)
        for (int32_t j = 0; j < 4; j++) gESPCellCounts[i][j] = 0;
    gESPGlobalCount = 0;
    gHLSpritesEnabled = 1;
    if (gFirstPersonFlags & 0x16) {
        gESPF88 = GetLMatrixRotationAroundYAxis(gCurrentCameraMatrix);
    } else {
        double d = std::fmod(static_cast<double>(gCameraF5BF4), 6.2831854820251465);
        gESPF88 = static_cast<float>(d);
        float v = gESPF88;
        if (!(v >= 0.0f)) gESPF88 = 6.2831855f + v;
    }
}

void DrawHLSprites() {
    if (gHLSpritesEnabled != 0) {
        for (int32_t i = 0; i < 4; i++)
            for (int32_t j = 0; j < 4; j++) gHLSpriteLists[i * 4 + j].mCount = gESPCellCounts[i][j];
        gESPGlobalList.mCount = gESPGlobalCount;
    }
    CThing* thing = LLCC_SystemGetPointedPeepThing();
    // the Mac keeps the thing as a 32-bit word in gESPF20; here it is a native pointer
    gIntrface.SetCurrentThing(gESPF20);
    gESPF20 = thing;
}
