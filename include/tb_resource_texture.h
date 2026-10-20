// tb_resource_texture.h: engine texture settings (engine library, original
// tb_resource_texture.cpp, functions at 0x10029078..0x10029330).
#pragma once
#include <cstdint>

#include "platform/ddraw_types.h"

// The engine's global texture configuration (one instance, exported as
// gei_TextureSettings). Field names come from the engine code that reads them
// (tb_hfield.cpp, tb_init.cpp, Wavelet.cpp, tb_depthoffield.cpp); fields whose use
// is not known yet keep their offset in the name.
struct CTextureSettings {
    enum : uint32_t {
        kProcTexturesOn = 0x00000004,        // PT_Enable sets it, PT_Disable clears it
        kProcTexturesBlocked = 0x10000000,   // PT_Enable and PT_Load do nothing while set
    };

    uint32_t mFlags;               // 0x00  0x17a after construction
    int32_t mFormat;               // 0x04  texture pixel format index (4 or 5: 16-bit formats);
                                   //       indexes per-format tables in the engine
    float mFloat08;                // 0x08  1.5
    float mFloat0C;                // 0x0c  0.8
    int32_t mTextureMemoryLimit;   // 0x10  bytes; 0 = choose from the card (CTextureCache::AutoDetectPageCreation)
    int32_t mTextureMemory;        // 0x14  bytes in use for texture pages (not set by the constructor)
    float mFloat18;                // 0x18  0.5
    int32_t mProcTextureParam;     // 0x1c  2500, passed to CProcTextureHandler by CHField::LoadProcTextures
    float mUVFixup[4];             // 0x20  0x20/0x24 = -1.0 after construction; CalculateUVFixup zeroes all four
    DDPIXELFORMAT mPixelFormat;    // 0x30  RGBA 4444: 16 bits, masks f000/0f00/00f0/000f

    CTextureSettings();
    uint32_t GetAlphaMask();
    void CalculateUVFixup();
    void OutputDebugInfo();
};
static_assert(sizeof(CTextureSettings) == 0x50, "CTextureSettings layout");

extern CTextureSettings gei_TextureSettings;   // engine data 0x10165fc8

// Maps a square surface size in pixels (1..256, powers of two) to the engine's size
// flag: 256 -> 0, 128 -> 1, ... 2 -> 7, 1 -> 8; anything else -> -1.
int32_t ConvertSurfaceSizeToFlag(int32_t size);
