// sprite_engine.h: the engine's sprite files (engine library; the original file is
// tb_sprite.cpp by its assertions, 0x1002da98..0x1002f398 for the file reader and the sprite
// data, with the RLE loader of tb_decode_rle.cpp at 0x10005104.. and the wavelet sprite
// reader of tb_decode_wavelet.cpp at 0x10006260; the inventory files all of it under
// tb_starfield.cpp and tb_depthoffield.cpp).
//
// A sprite file (".TPC" and ".FPC" in the game's data) is little-endian:
//   u16 kind        2: wavelet pictures, 3: RLE pictures with a palette; anything below 2
//                   fails an assertion, anything else loads nothing
//   u16             read and ignored
//   u32 count       the sprites in the file
//   kind 3 only:    the palette, 0x100 entries of 4 bytes (BGRA; the loader converts them to
//                   the texture format in place)
//   then, per sprite
//     kind 3 (SSpriteEngineData::Load_RLE):
//       u32 size    of the RLE data
//       u16 x4      width, height, used width, used height
//       u32, u32    the offset x and y (only the low 16 bits are kept)
//       size bytes  the RLE picture
//     kind 2 (SSpriteEngineData::Load_WCT, CWaveLoad::LoadSprite): see SSpriteWaveHeader.
// Every field of a kind 3 file is swapped on the Mac (a no-op here). The kind 2 header is NOT:
// the Mac reads it raw as big-endian words, so a kind 2 file is only right on a big-endian
// machine there; a little-endian host reads the file's own words as they are. (No kind 2 file
// is in the game's data: all 75 sprite files of the disc are kind 3.)
//
// A loaded file is a CSpriteFileIO: an array of SSpriteEngineData and an array of the texture
// entries they point into, made together and freed together.
#pragma once
#include <cstdint>

#include "tb_decode_wavelet.h"
#include "tb_starfield.h"   // SSpriteEngineData (the ESP loader indexes a file's array of them)

class CTextureEntry;

// What CWaveLoad::LoadSprite reads for one wavelet sprite, kept in engine data (0x10052ad4,
// not exported; the name is invented) because SSpriteEngineData::Load_WCT takes the sprite's
// sizes and offsets back out of it. 0x2c bytes, as in the file (see the top of this header
// for the byte order). The four floats are the decompression scales: the loader turns them
// into the integers CWaveLoad keeps (2q - 2, 4q - 4, 4q - 4 and q - 1).
struct SSpriteWaveHeader {
    uint32_t mF00;               // 0x00  read and not used
    uint16_t mWidth;             // 0x04
    uint16_t mHeight;            // 0x06
    uint16_t mUsedWidth;         // 0x08
    uint16_t mUsedHeight;        // 0x0a
    uint32_t mOffsetX;           // 0x0c  only the low 16 bits are kept
    uint32_t mOffsetY;           // 0x10
    float mQuality[4];           // 0x14
    uint32_t mDataSize;          // 0x24
    uint32_t mAlphaSize;         // 0x28
};
static_assert(sizeof(SSpriteWaveHeader) == 0x2c, "the sprite header is read straight into this");
extern SSpriteWaveHeader gSpriteWaveHeader;   // engine data 0x10052ad4 (not exported; name invented)

// The RLE picture of a sprite: a CCompBase a texture entry unpacks from, holding the file's
// RLE bytes (0x20 bytes on the Mac: +0 the pointer to the virtual base CCompBase at +0x18, +0x14
// the vtable; here a plain base, as for CWaveLoad). The constructor, SetSize, SetPalette and
// LoadSprite are in src/tb_spritefileio.cpp; the destructor, Close and the three unpacking members
// (Decompress, Blit, AverageColor) are in src/tb_rleload.cpp (unit w38).
class CRLELoad : public CCompBase {
public:
    CRLELoad();                                                          // 0x10005104
    ~CRLELoad() override;                                                // 0x10005244 (Close())
    // Frees the RLE bytes and forgets the size (the palette is not the loader's). Always 1.
    int32_t Close();                                                     // 0x10005440
    // Decompress: the destination's `flip` is not looked at.
    int32_t Blit(SUnpackDestination* dest, int32_t flip) override;       // 0x10005b90
    // Unpacks the picture into `dest` (the format of the texture settings), scaled to its size: see src/tb_rleload.cpp.
    int32_t Decompress(SUnpackDestination* dest) override;               // 0x100054c0
    uint32_t AverageColor() override;                                    // 0x10005b5c: 0xffffffff

    void SetSize(int32_t width, int32_t height);   // 0x10005300
    void SetPalette(void* palette);                // 0x10005334
    // Reads `size` bytes of RLE data from the open file into a block of the engine's allocator;
    // 1, or 0 without memory. A second call asserts (the block is not freed) and loses the first.
    int32_t LoadSprite(void* file, int32_t size);  // 0x10005368

    void* mData;         // 0x04  the RLE bytes (an engine allocation)
    void* mPalette;      // 0x08  not owned
    int32_t mWidth;      // 0x0c
    int32_t mHeight;     // 0x10
};

// A sprite file as loaded (0x10 bytes on the Mac; here with native pointers). The object is
// the caller's: Load fills it, Free empties it.
struct CSpriteFileIO {
    // The ESP loader's inline constructor (ESP_CSprite::LoadFromESPFile 0x1007b354, unit w33): it sets words 0 and 3 only. The
    // sprite array pointer and word 2 are left as the heap had them. (The host's pointers are wider, so word 3's whole pointer
    // is cleared.) Other users make the object as a global or a local and fill it with Load.
    CSpriteFileIO() : mCount(0), mPalette(nullptr) {}

    // Reads the open file `file` into `io` and closes the file (gFileClose) however it ends;
    // 1, or 0 (everything freed, an error pushed) when memory runs out. Calls the splash's
    // InterpolateSplash when the engine flag 1 is set. A file of some other kind (not 2 or 3)
    // loads nothing and still answers 1, leaving default sprites. Static in the original.
    static int32_t Load(CSpriteFileIO* io, void* file);   // 0x1002da98
    // Opens the file `name` ("rb"), reads it with the open-file Load above and closes it again (the original's
    // Load has closed it already: a second close; nothing calls this). Pushes error 0xb0754001 and answers 0
    // when the file is not there. The Mac's member has `this` first; the int is passed on and never read.
    static int32_t Load(CSpriteFileIO* io, char* name, int32_t unused);   // 0x1002dea0
    // Destroys the texture entries, then the sprites (their texture pointers cleared first, so
    // that they do not close entries that are gone), then frees the palette. Always 1.
    int32_t Free();                                        // 0x1002df78
    // Makes every sprite's texture blend as semitransparent (blending bits 0x200).
    void FlagAsSemitransparent();                          // 0x1002e068

    int32_t mCount;                  // 0x00  sprites in the file (and in each array)
    union {
        SSpriteEngineData* mSprites; // 0x04  array of mCount
        SSpriteEngineData* mRecords; //       (unit w33's name: the records the particle sprites index into)
    };
    union {
        CTextureEntry* mTextures;    // 0x08  array of mCount
        int32_t mF08;                //       (unit w33's name: a word it only stores, as the test's fakes do, and dumps)
    };
    union {
        void* mPalette;              // 0x0c  kind 3: 0x100 entries in the texture format's order (0x400 bytes)
        int32_t mF0C;                //       (unit w33's name: 0 after the ESP loader's constructor)
    };
};

// gei_Display + 0x93c and + 0x940 (see tb_resource_display.h: the Mac's display object goes on for 0x10 bytes after what the
// host's CDisplay holds). SSpriteGameData::SetupVertices (0x1002f688) compares a sprite's screen position with them;
// ESP_InitialiseAndLoadSprites sets both to 0.0 (unit w33). Defined in src/ESP_sprites.cpp for now: they belong next to
// gHotSprite (src/tb_sprites.cpp) once SetupVertices is translated.
extern float gSpriteHotX;   // gei_Display + 0x93c
extern float gSpriteHotY;   // gei_Display + 0x940
