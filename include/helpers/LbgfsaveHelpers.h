// LbgfsaveHelpers.h: file-local helpers that src/bullfrog/Lbgfsave.cpp shares with AndyGLL.cpp and TbRenderArea.cpp.
// Before the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now
// lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "TbRaConvert.h"
#include <cstring>

// The test the library makes of a result (see the note at the top).
static inline bool Succeeded(const TbResult& result) { return result.mCode >= 0; }

// The format's six words into the first 0x18 bytes of an area.
static inline void SetFormat(TbRenderArea& area, const TbPixFormat& format) {
    std::memcpy(area.mF00 + 0, &format.mRedMask, 4);
    std::memcpy(area.mF00 + 4, &format.mGreenMask, 4);
    std::memcpy(area.mF00 + 8, &format.mBlueMask, 4);
    std::memcpy(area.mF00 + 12, &format.mAlphaMask, 4);
    area.mDepth = format.mBitsPerPixel;
    area.mPalettised = format.mPalettised;
}

// Bytes of a pixel of `depth` bits, as the library rounds them.
static inline uint32_t BytesOfPixel(int32_t depth) { return (static_cast<uint32_t>(depth) + 7) >> 3; }
