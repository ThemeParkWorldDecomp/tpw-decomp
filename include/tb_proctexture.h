// Procedural texture cache, Mac engine 0x1001275c..0x10013238.
#pragma once
#include <cstdint>
#include "landscape.h"
class CTextureEntry;
struct SProcTexture {
    SProcTexture* mPrevious;   // 0x00
    SProcTexture* mNext;       // 0x04
    SProcTexture** mSlot;      // 0x08, pointer into the tile lookup table
    CTextureEntry* mTexture;   // 0x0c, one-element array
};
union PrepareEntry {
    uint32_t mTile;
    CTextureEntry* mTexture;
};
class CProcTextureHandler {
public:
    CProcTextureHandler();
    ~CProcTextureHandler();
    int32_t Init(int32_t count, int32_t size);
    int32_t Close();
    bool Load(char* name);
    int32_t PrepareList(PrepareEntry* entries, sTileOverlay* overlays, int32_t count);
    int32_t CreateProcTexture(CTextureEntry* entry, int32_t x, int32_t y, sTileOverlay* overlay);

    Landscape* mLandscape;             // 0x00
    int32_t mSize;                     // 0x04
    SProcTexture* mTiles[16384];        // 0x08, index = (tile >> 8) * 128 + (tile & 255)
    SProcTexture* mCyclic;             // 0x10008
    SProcTexture* mCurrent;            // 0x1000c, not set by the constructor
    int32_t mCount;                    // 0x10010
};
extern uint8_t* gProcTextureBuffer;    // engine data 0x10052b00
void ConvertSquareFrom32(void* dest, void* source, int32_t size);
