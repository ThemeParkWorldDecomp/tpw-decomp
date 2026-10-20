// tb_loaders.h: the engine's image loaders (engine library, original tb_loaders.cpp: its
// asserts name the file; STGAHeader, TGALoadAndConvert, CTextureLoadManager).
//
// PARTIAL: the startup and image loader range through 0x10016324.
#pragma once
#include <cstdint>

struct lARGBType;
class CTextureEntry;

struct STGAHeader {
    uint8_t mIdLength, mColourMapType, mImageCodeType;
    uint8_t mColourMap[5];
    int16_t mXOrigin, mYOrigin, mWidth, mHeight;
    uint8_t mPixelSize, mDescriptor;
    void* Load(char* name, int32_t extraPlanes);          // 0x10014fc4
    void* LoadBumpmap(char* name);                      // 0x10015194
    void ConvertToEmbossBumpmap(uint8_t* data);          // 0x10015328
};
static_assert(sizeof(STGAHeader) == 18, "TGA header layout");

struct STextureLoad {
    STextureLoad();                                    // 0x10015db8
    int32_t Close();                                   // 0x10016280
    char* mFolder;                                     // 0x00 (borrowed)
    char* mName;                                       // 0x04 (borrowed)
    CTextureEntry* mTexture;                            // 0x08 (array)
    int32_t mReferences;                               // 0x0c
};
class CTextureLoadManager {
public:
    CTextureLoadManager();                             // 0x10015d2c
    int32_t Init();                                    // 0x10015dfc
    int32_t Close();                                   // 0x10015ef4
    CTextureEntry* LoadFile(char* name);                // 0x10015f9c
    CTextureEntry* LoadTexture(char* folder, char* name);// 0x10016324
    int32_t RemoveTexture(CTextureEntry* texture);       // 0x1001658c (src/tb_loadmanager.cpp)
    void SetLoadFlags(int32_t flags) { mLoadFlags = static_cast<uint32_t>(flags); }   // 0x100166c8
    STextureLoad mLoads[4096];                          // 0x00000
    int32_t mCount;                                    // 0x10000
    int32_t mDefault;                                  // 0x10004
    uint32_t mLoadFlags;                                // 0x10008
};
extern CTextureLoadManager* gei_TLoadManager;            // 0x100af1c8
extern int32_t gTextureExtraPlanes;                     // 0x100af1cc
extern uint32_t gTextureAverageColour;                  // 0x100af1bc
extern uint32_t gTextureLoadFlags;                      // 0x100af1c0
extern void* gTextureWaveData;                          // 0x100af1c4
extern const int32_t gTextureBytesPerPixel[6];          // 0x10051bec
int32_t TextureLoadManager_Init();                     // 0x10014e68
bool TextureLoadManager_Close();                       // 0x10014f38
uint32_t UncompressedConvert(void* dest, void* source, int32_t width, int32_t height,
                             lARGBType* palette, int32_t destBytes, int32_t sourceBytes,
                             int32_t flipped);         // 0x10015b08

// Loads a TGA file and converts it to bytesPerPixel-byte pixels in a block from the
// engine allocator (gMalloc; the caller frees it with gFree); sets *width and *height.
// NULL when the file could not be loaded. (Arguments 4 and 5 are unused by the callers
// translated so far: 0 and NULL.)
void* TGALoadAndConvert(char* name, int32_t* width, int32_t* height, int32_t flags, lARGBType* palette,
                        int32_t bytesPerPixel);   // 0x100156d8
