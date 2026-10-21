// DirectDraw data types used by game-side structures (DirectX 6 layouts, 32-bit fields).
// Only types the game code embeds in its own data live here; the DirectDraw objects and
// their methods belong to the platform layer (analysis/platform-layer-design.md).
#pragma once
#include <cstdint>

// DDPIXELFORMAT (ddraw.h): 32 bytes. The masks are unions in ddraw.h; the game only uses
// the RGB/alpha interpretation.
struct DDPIXELFORMAT {
    uint32_t dwSize;             // 0x00  sizeof(DDPIXELFORMAT) == 32
    uint32_t dwFlags;            // 0x04  DDPF_*
    uint32_t dwFourCC;           // 0x08
    uint32_t dwRGBBitCount;      // 0x0c
    uint32_t dwRBitMask;         // 0x10
    uint32_t dwGBitMask;         // 0x14
    uint32_t dwBBitMask;         // 0x18
    uint32_t dwRGBAlphaBitMask;  // 0x1c
};
static_assert(sizeof(DDPIXELFORMAT) == 32, "DDPIXELFORMAT layout");

enum : uint32_t {
    DDPF_ALPHAPIXELS = 0x00000001,
    DDPF_ALPHA = 0x00000002,
    DDPF_FOURCC = 0x00000004,
    DDPF_PALETTEINDEXED8 = 0x00000020,
    DDPF_RGB = 0x00000040,
};

// DDCOLORKEY and DDSCAPS2 (ddraw.h), parts of DDSURFACEDESC2.
struct DDCOLORKEY {
    uint32_t dwColorSpaceLowValue;
    uint32_t dwColorSpaceHighValue;
};

// Unit w231 (src/bullfrog/LbSurface.cpp): original descriptor, 0x6c Mac bytes.
// Native pointers change host offsets; this is distinct from DDSURFACEDESC2.
struct DDSURFACEDESC {
    uint32_t dwSize, dwFlags, dwHeight, dwWidth; // 0x00..0x0c
    int32_t lPitch;                            // 0x10
    uint32_t dwBackBufferCount, dwMipMapCount;  // 0x14, 0x18
    uint32_t dwAlphaBitDepth, dwReserved;       // 0x1c, 0x20
    void* lpSurface;                           // 0x24
    DDCOLORKEY ddckCKDestOverlay, ddckCKDestBlt; // 0x28, 0x30
    DDCOLORKEY ddckCKSrcOverlay, ddckCKSrcBlt;   // 0x38, 0x40
    DDPIXELFORMAT ddpfPixelFormat;             // 0x48
    uint32_t dwCaps;                           // 0x68
};

struct DDSCAPS2 {
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
};

// DDSURFACEDESC2 (ddraw.h, DirectX 6): 0x7c bytes on the Mac. lpSurface is a pointer, so
// a 64-bit host's copy is larger and the fields after it move; game code fills dwSize
// with sizeof (the Mac code's 0x7c) and the platform layer checks it against its own.
struct DDSURFACEDESC2 {
    uint32_t dwSize;                  // 0x00
    uint32_t dwFlags;                 // 0x04
    uint32_t dwHeight;                // 0x08
    uint32_t dwWidth;                 // 0x0c
    int32_t lPitch;                   // 0x10  (union with dwLinearSize)
    uint32_t dwBackBufferCount;       // 0x14
    uint32_t dwMipMapCount;           // 0x18  (union with dwRefreshRate)
    uint32_t dwAlphaBitDepth;         // 0x1c
    uint32_t dwReserved;              // 0x20
    void* lpSurface;                  // 0x24
    DDCOLORKEY ddckCKDestOverlay;     // 0x28 on the Mac
    DDCOLORKEY ddckCKDestBlt;         // 0x30
    DDCOLORKEY ddckCKSrcOverlay;      // 0x38
    DDCOLORKEY ddckCKSrcBlt;          // 0x40
    DDPIXELFORMAT ddpfPixelFormat;    // 0x48
    DDSCAPS2 ddsCaps;                 // 0x68
    uint32_t dwTextureStage;          // 0x78
};

enum : uint32_t {
    DDLOCK_WAIT = 0x00000001,
};
