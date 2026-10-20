// tb_mapzoom.h: the map zoom (the engine's CMapZoom, tb_mapzoom.cpp by its assertions; the
// methods are at 0x10019884..0x1001a6dc, the construction is inline in
// __sinit_tb_mapzoom_cpp 0x1001a6dc). It shows a picture of the whole map (the minimap's
// source) as a grid of 64 by 64 texture entries that point into the picture's pixels, and
// draws the part of it that Use names as one sprite per tile into the rectangle Window names.
//
// Mac offsets in the comments. The object is 0x60 bytes there (gei_MathUnit follows it at
// 0x74); the entry array pointer makes the host copy larger.
#pragma once
#include <cstdint>

class CTextureEntry;

class CMapZoom {
public:
    enum : uint32_t {
        kUseSet = 0x00000002,           // Use ORs it in; nothing reads it
        kEntryFlags = 0x05180000,       // Init: the flags of each tile's entry (before SetSize)
    };

    CMapZoom();                         // the part of __sinit_tb_mapzoom_cpp (0x1001a6dc) that builds gei_MapZoom
    ~CMapZoom();                        // 0x10019884: Close
    void EnableFiltering();             // 0x10019904
    void DisableFiltering();            // 0x10019940
    // Makes the grid for a picture of width by height pixels, `pitch` bytes a row, at `data`:
    // one entry a 64 by 64 block, pointing at the block's first pixel. Answers 1. An array
    // already made is not freed.
    int32_t Init(void* data, int32_t width, int32_t height, int32_t pitch);   // 0x1001997c
    // Frees the entries (after clearing every data pointer: they belong to the picture). Answers 1.
    int32_t Close();                    // 0x10019afc
    // The rectangle of the picture to show (map pixels). QUIRK: the argument order is
    // x0, y0, x1, y1 but the original's assertion, whose text has a || where a && was meant,
    // and the stores keep the pairs apart: x0 -> mUseX0, y0 -> mUseY0, x1 -> mUseX1, y1 -> mUseY1.
    // A failed assertion is reported and the call goes on.
    void Use(float x0, float y0, float x1, float y1);      // 0x10019ba8
    // The screen rectangle it is drawn into.
    void Window(float x0, float y0, float x1, float y1);   // 0x10019da8
    // The part of the picture (map pixels, ints) that changed: the cache drops the tiles' pages.
    void Update(int32_t x0, int32_t y0, int32_t x1, int32_t y1);   // 0x10019de8
    // Asks the texture cache for every tile of the Use rectangle; 0 (and a failed assertion)
    // without entries.
    int32_t Prepare();                  // 0x10019f94
    // Draws the tiles of the Use rectangle, row by row, one sprite each; 0 without entries and
    // when a DrawSprite fails (the remaining tiles are not drawn). At most 32 tiles across or
    // down: the original's tables hold 32 pairs and a wider range overruns its stack.
    int32_t Execute();                  // 0x1001a0c0

    uint32_t mFlags;            // 0x00  0 (Use ORs kUseSet)
    int32_t mWidth;             // 0x04  the picture's size in pixels (Init)
    int32_t mHeight;            // 0x08
    int32_t mPitch;             // 0x0c  bytes a row of the picture
    int32_t mColumns;           // 0x10  tiles across: width / 64 rounded up
    int32_t mRows;              // 0x14
    int32_t mF18;               // 0x18  0 at start-up, not touched by the methods
    CTextureEntry* mEntries;    // 0x1c  mColumns * mRows entries (0x5c bytes each on the Mac)
    float mUseX0;               // 0x20  Use's x0
    float mUseY0;               // 0x24  Use's y0
    float mUseX1;               // 0x28  Use's x1
    float mUseY1;               // 0x2c  Use's y1
    float mWindowX0;            // 0x30  Window's x0 (screen)
    float mWindowY0;            // 0x34  Window's y0
    float mWindowX1;            // 0x38  Window's x1
    float mWindowY1;            // 0x3c  Window's y1
    uint32_t mColour;           // 0x40  -1 at start-up; only its top byte is read (0xff: opaque blending)
    float mZ;                   // 0x44  1.0 at start-up
    int32_t mFirstEntry;        // 0x48  0 after Init; added to every tile's index
    int32_t mF4C;               // 0x4c  not touched by the methods
    int32_t mF50;               // 0x50
    int32_t mF54;               // 0x54
    int32_t mEntryCount;        // 0x58  entries made (0 at start-up)
    int32_t mFiltering;         // 0x5c  1 at start-up; 0 adds blending bit 0x20000 to the sprites
};

extern CMapZoom gei_MapZoom;   // engine export (0x100afe20)
