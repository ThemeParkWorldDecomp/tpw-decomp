// TbRenderArea.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TbRenderArea code, named after it.

#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "helpers/LbgfsaveHelpers.h"
#include "LbFile.h"
#include "TbGraphicFile.h"
#include "TbRaConvert.h"
#include <cstring>
#include <new>

// The Bullfrog library's picture loading (bullfrog symbols shared 2.04b3): TbRenderArea (0x10016560..0x10016ad0),
// the loader base class (0x10022870..0x100230cc), the TGA loader (0x10023a80..0x10023f30), the TGA line reader
// (0x10021228..0x10021d28) and the line reader's base with LbPalette_FromCompact (0x1001de44, 0x1001e2f4,
// 0x1001a2f8) and the buffered file's destructor (0x10023024), which the loader needs and which are not in the unit's first
// list. See include/TbGraphicFile.h.
//
// The library tests the results of the calls it makes against words in its data that nothing writes: each comparison is
// (result >= 0) == (word == 0), or its negation where the word is -1, so a negative result is a failure in every one. The
// words: 0x10072f94, 0x10072f98, 0x10072f9c and 0x10072fa0 in the loader base class (the two opens, the header read and the
// data read), 0x10072dfc in _TgaGLL::ReadData, 0x10072e64 (-1) in ReadNextLine and 0x10072d84, 0x10072d88 and 0x10072d8c in
// DoRead.

// ---- TbRenderArea

TbRenderArea::TbRenderArea() {
    mDepth = 0;
    mPalettised = 0;
    mData = nullptr;
    mWidth = 0;
    mPictureHeight = 0;
    mStride = 0;
}

TbRenderArea::TbRenderArea(const TbSurface* surface) {
    mDepth = 0;
    mPalettised = 0;
    mWidth = 0;
    mPictureHeight = 0;
    const TbRenderArea& source = surface->GetRenderArea();
    std::memcpy(mF00, source.mF00, sizeof mF00);
    mDepth = source.mDepth;
    mPalettised = source.mPalettised;
    mData = source.mData;
    mWidth = source.mWidth;
    mPictureHeight = source.mPictureHeight;
    mStride = source.mStride;
}

TbRenderArea::TbRenderArea(const TbSize& size, const TbPixFormat& format, uint8_t* data, uint32_t stride) {
    SetFormat(*this, format);
    mData = data;
    mWidth = size.width;
    mPictureHeight = size.height;
    if (stride == 0) stride = static_cast<uint32_t>(mWidth) * BytesOfPixel(mDepth);
    mStride = static_cast<int32_t>(stride);
}

TbResult TbRenderArea::AllocMemory() {
    if (mData != nullptr) return TbResult{-1};
    mStride = static_cast<int32_t>(static_cast<uint32_t>(mWidth) * BytesOfPixel(mDepth));
    mData = static_cast<uint8_t*>(LbMem_Alloc(GetMemSize()));
    return TbResult{mData != nullptr ? 0 : -1};
}

void TbRenderArea::FreeMemory() {
    if (mData != nullptr) {
        LbMem_Free(mData);
        mData = nullptr;
    }
}

TbResult TbRenderArea::SetupClientRA(TbRenderArea& client, const TbPoint& origin, const TbSize& size) const {
    TbRect rect;
    rect.left = origin.x;
    rect.top = origin.y;
    rect.right = origin.x + size.width;
    rect.bottom = origin.y + size.height;
    return SetupClientRA(client, rect);
}

TbResult TbRenderArea::SetupClientRA(TbRenderArea& client, const TbRect& rect) const {
    TbRect clip;
    clip.left = 0;
    clip.top = 0;
    clip.right = mWidth;
    clip.bottom = mPictureHeight;
    if (LbRect_Intersection(&clip, &rect) == 0) LbRect_Empty(&clip);
    std::memcpy(client.mF00, mF00, sizeof mF00);
    client.mDepth = mDepth;
    client.mPalettised = mPalettised;
    client.mData = mData;
    client.mWidth = mWidth;
    client.mPictureHeight = mPictureHeight;
    client.mStride = mStride;
    client.mWidth = clip.right - clip.left;
    client.mPictureHeight = clip.bottom - clip.top;
    // The first pixel: the clip's column in the new area's pixels, its row in this area's rows.
    const uint32_t column = static_cast<uint32_t>(clip.left) * BytesOfPixel(client.mDepth);
    const uint32_t row = static_cast<uint32_t>(clip.top) * static_cast<uint32_t>(mStride);
    client.mData = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(client.mData) + column + row);
    return TbResult{client.mWidth != 0 && client.mPictureHeight != 0 ? 0 : -1};
}

uint32_t TbRenderArea::GetMemSize() const { return static_cast<uint32_t>(mStride) * static_cast<uint32_t>(mPictureHeight); }
