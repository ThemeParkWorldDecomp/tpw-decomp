// HFieldTextures.cpp (main app): the level height field's texture table and flatness.
//
// The level's terrain mesh (Terrain/base.md2) names its ground textures itself. A text file
// next to it, <theme path>:Terrain:<theme>.tct (the "texture correspondence table"), lists
// further textures by category: the path pieces ("PathTex") and the queue pieces
// ("QueueTex"). BuildHFieldTextureTable reads that file into the one table below, then puts
// the textures the file names into the mesh's own texture arrays, so the terrain draws them
// with the rest of its textures.
//
// Where the functions lie in the inventory: BuildHFieldTextureTable, ParseLevelTCT and
// FreeHFieldTextureTable (0x100319c0, 0x10031220, 0x10031b10) in TPAdvisor.cpp's range,
// DetermineHFieldFlatness (0x1005b9d0) in TPRide.cpp's, InterceptAndInsertM3D2Textures and
// ReplaceTextures (0x100b8350, 0x100b7e78) in rsse.cpp's. All six are here, with the only
// table they work on. BuildHFieldTextureTable, FreeHFieldTextureTable and
// DetermineHFieldFlatness are declared in TPEngine.h.
#pragma once
#include <cstdint>

#include "Ptr32.h"
#include "m3d2.h"

// One texture file name of a .tct text, with the number in front of it (12 bytes on the Mac,
// kept: the table counts the size of their block in the Mac's units). The names lie in the
// same block as the records, so mText is a Ptr32.
struct sExtraTexture {
    int32_t mNumber;           // 0x00  the number on the line (not used by the code translated so far)
    Ptr32<char> mText;         // 0x04  the file name, after the records in the table's block
    uint16_t mTextureIndex;    // 0x08  its place among the mesh's textures (InterceptAndInsertM3D2Textures)
    uint16_t mF0A;             // 0x0a  not written by anything
};
static_assert(sizeof(sExtraTexture) == 0xc, "sExtraTexture layout");

// A category of the table: the text file's line that names it, then the lines of its
// textures. The categories are fixed (gExtraTextureEntries); mValues points into the
// table's block.
struct sExtraTextureEntry {
    const char* mName;         // 0x00  "PathTex" or "QueueTex" (compared without regard to case)
    int32_t mCount;            // 0x04  textures read for it
    sExtraTexture* mValues;    // 0x08
};

// 0x30 bytes on the Mac, one instance (gExtraTextureTable, 0x1029cc2c). Two stages share it:
// ParseLevelTCT fills the first six fields, InterceptAndInsertM3D2Textures the rest.
struct sExtraTextureTable {
    int32_t mCount;                  // 0x00  categories
    sExtraTextureEntry* mEntries;    // 0x04
    int32_t mValueCount;             // 0x08  textures counted by ParseLevelTCT's first pass
    int32_t mBytes;                  // 0x0c  size of the block for them, in Mac units; never reset,
                                     //       so loading a second level asks for more than it needs
    void* mBlock;                    // 0x10  the records, then the names
    int32_t mStored;                 // 0x14  textures stored by the second pass
    sTextureInfo* mInfos;            // 0x18  the mesh's new texture infos; one block with the new
                                     //       textures and the folder names (FreeHFieldTextureTable)
    sTextureInfo* mOldInfos;         // 0x1c  the mesh's own, before
    sTexture* mTextures;             // 0x20  the mesh's new textures, inside the block of mInfos
    sTexture* mOldTextures;          // 0x24  the mesh's own, before
    int32_t mAdded;                  // 0x28  textures of the mesh that the file does not name (kept)
    char* mNames;                    // 0x2c  0x14 bytes for each texture the file names
};

extern sExtraTextureEntry gExtraTextureEntries[2];   // 0x1029cc14, original name unknown
extern sExtraTextureTable gExtraTextureTable;        // 0x1029cc2c, original name unknown

// 0x10031220: reads the .tct text `file` into the table in two passes (the first counts the
// textures and their names' bytes, the second stores them); always 1.
int32_t ParseLevelTCT(char* file, sExtraTextureTable* table);
// 0x100b8350: gives `header` the textures the table names, after the textures of its own
// that the table does not name; the name of each texture's folder is `folder` + the category.
// Always 1.
int32_t InterceptAndInsertM3D2Textures(tag_sM3D2Header* header, sExtraTextureTable* table, char* folder);
// 0x100b7e78: turns the face sets' and height field quads' texture numbers into places in
// the new texture array. `map` holds one number for each of the old textures: more than 0 is
// a place among the table's textures (after the `kept` ones), 0 or less minus a place among
// the kept ones. `total` is not used.
void ReplaceTextures(tag_sM3D2Header* header, int32_t* map, sTexture* textures, int32_t kept, uint32_t total);

// Unit v5906, 0x100b8080 (src/Postcard_Postcard_save.cpp): sets the texture and flags of the height field quad (x, y) of `header`:
// category 0 with texture 8 makes it plain, category 0 with texture 0x37 takes the saved copy's words (gHeightfieldCopy), otherwise
// the texture (wrapped into the table) is one of the category's; `rotation` 0x5a / 0xb4 / 0x10e sets bit 0x20 / 0x10 / 8.
void SetHeightfieldTexture(tag_sM3D2Header* header, sExtraTextureTable* table, int32_t x, int32_t y, int32_t category, int32_t texture,
                           int32_t rotation);
