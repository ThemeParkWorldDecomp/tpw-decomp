// landscape.h: terrain texture generation (engine library, original landscape.cpp,
// functions at 0x1000017c..0x10002db4).
//
// A landscape file holds a control bitmap (4 bytes per texel; channels 1..3 steer the
// blending) and three tilesets (64, 32 and 16 pixel tiles); only the tileset matching the
// current tile size is loaded. generate_tile builds one ARGB tile (bytes A, R, G, B; A is
// always 0) from three texture layers chosen per 16-pixel run by two cover maps.
#pragma once
#include <cstdint>

#include "Bitmap.h"

struct sTileOverlay {
    void* mData;          // 0x00  overlay pixels: uint16_t RGBA4444 (format 2) or uint32_t RGBA8888 (4)
    int32_t mWidth;       // 0x04
    int32_t mHeight;      // 0x08
    int32_t mFormat;      // 0x0c  2 or 4; anything else writes nothing
    uint32_t mFlags;      // 0x10  orientation: kRotate, kFlip8, kFlip10, kFlip20 (see generate_tile)

    enum : uint32_t { kFlip8 = 0x08, kFlip10 = 0x10, kFlip20 = 0x20, kRotate = 0x40 };
};

// The 13 bitmaps of a tileset (index -> role, from generate_tile); 4 = bytes per pixel.
enum TilesetBitmap {
    kLayerC_TextureA = 0,   // 4
    kLayerC_TextureB = 1,   // 4
    kLayerC_Mask = 2,       // 1
    kLayerB_TextureA = 3,   // 4
    kLayerB_TextureB = 4,   // 4
    kLayerB_Mask = 5,       // 1
    kCover2_MapA = 6,       // 1  layer B against layer C
    kCover2_MapB = 7,       // 1
    kLayerA_TextureA = 8,   // 4
    kLayerA_TextureB = 9,   // 4
    kLayerA_Mask = 10,      // 1
    kCover1_MapA = 11,      // 1  layer A against the others
    kCover1_MapB = 12,      // 1
    kTilesetBitmaps = 13,
};

class Tileset {
public:
    Tileset();
    ~Tileset();
    void clear();
    int32_t load(void* file);

    int32_t mTileSize;                   // 0x00  64 >> level after Landscape::clear
    uint8_t mLevel;                      // 0x04
    Bitmap mBitmaps[kTilesetBitmaps];    // 0x08  (0x10 bytes each in the original)
};

class Landscape {
public:
    Landscape();
    ~Landscape();
    void clear();
    void set_tilesize(int32_t size);
    void precalc_tables();
    void generate_tile(uint8_t* out, uint32_t tileX, uint32_t tileY, sTileOverlay* overlay);
    int32_t load_landscape(char* name);

    uint8_t mTileSize;          // 0x00  64, 32 or 16
    uint8_t mTileset;           // 0x01  0, 1, 2 (the loaded tileset)
    uint8_t mRampC[2];          // 0x02  centre, width of layer C's ramp
    uint8_t mRampB[2];          // 0x04  layer B
    uint8_t mRampCover2[2];     // 0x06
    uint8_t mRampA[2];          // 0x08  layer A
    uint8_t mRampCover1[2];     // 0x0a
    int32_t mWidthTiles;        // 0x0c  (control width - 1) >> 2
    int32_t mHeightTiles;       // 0x10  (control height - 1) >> 2
    Bitmap mControl;            // 0x14
    Tileset mTilesets[3];       // 0x24  (0xd8 bytes each in the original)
};

// Bilinear 4x upsampling of one channel of a 4-byte control bitmap around tile (tx, ty)
// into a 1-byte bitmap whose width is 4 blocks; returns 1.
int32_t resample_control(const Bitmap& src, uint32_t tx, uint32_t ty, uint32_t channel, Bitmap& dst);
// 16 pixels of three-layer blending (control byte c[0] for all 16 pixels, as in the original).
void blend_3(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2, uint8_t* t2,
             uint8_t* a3, uint8_t* b3, uint8_t* m3, uint8_t* t3, uint8_t* c, uint8_t* w12, uint8_t* w23, uint8_t* out);
// 16 pixels of two-layer blending.
void blend_2(uint8_t* a1, uint8_t* b1, uint8_t* m1, uint8_t* t1, uint8_t* a2, uint8_t* b2, uint8_t* m2, uint8_t* t2,
             uint8_t* c, uint8_t* w, uint8_t* out);
// 16 pixels of one layer.
void single_layer(uint8_t* a, uint8_t* b, uint8_t* m, uint8_t* t, uint8_t* c, uint8_t* out);
// Cover weights of 16 pixels (max of two ramped maps) into out; returns 0xff when all are
// 0xff, 0 when all are 0, else 0xaa.
uint8_t calc_cover(uint8_t* mapA, uint8_t* mapB, uint8_t* ramp, uint8_t* control, uint8_t* out);

// Shared tile buffer (tile size squared, 4 bytes per pixel), allocated by set_tilesize.
extern uint8_t* gTileBuffer;        // 0x100524e8
extern uint8_t gRampB[256];         // 0x100524ec  from mRampB
extern uint8_t gRampA[256];         // 0x100525ec  from mRampA
extern uint8_t gRampC[256];         // 0x100526ec  from mRampC
extern uint8_t gRampCover2[256];    // 0x100527ec
extern uint8_t gRampCover1[256];    // 0x100528ec
// Control channels resampled to tile resolution (1 byte per pixel).
extern Bitmap gControl2;            // 0x100529ec  channel 2 (cover 2)
extern Bitmap gControl3;            // 0x10052a08  channel 3 (cover 1)
extern Bitmap gControl1;            // 0x10052a24  channel 1 (layer masks)
