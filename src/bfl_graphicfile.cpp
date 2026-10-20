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
#include "TbGraphicFile.h"

#include <cstring>
#include <new>

#include "LbFile.h"
#include "TbRaConvert.h"
#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"

using bfl::FileSystem::CFile;
using bfl::FileSystem::CWadFile;
using bfl::FileSystem::_TbBufferedFile;

namespace {

// The test the library makes of a result (see the note at the top).
bool Succeeded(const TbResult& result) { return result.mCode >= 0; }

// The format's six words into the first 0x18 bytes of an area.
void SetFormat(TbRenderArea& area, const TbPixFormat& format) {
    std::memcpy(area.mF00 + 0, &format.mRedMask, 4);
    std::memcpy(area.mF00 + 4, &format.mGreenMask, 4);
    std::memcpy(area.mF00 + 8, &format.mBlueMask, 4);
    std::memcpy(area.mF00 + 12, &format.mAlphaMask, 4);
    area.mDepth = format.mBitsPerPixel;
    area.mPalettised = format.mPalettised;
}

// Bytes of a pixel of `depth` bits, as the library rounds them.
uint32_t BytesOfPixel(int32_t depth) { return (static_cast<uint32_t>(depth) + 7) >> 3; }

}  // namespace

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

// ---- LbPalette_FromCompact

void LbPalette_FromCompact(TbPalette* palette, const uint8_t* compact, uint32_t count) {
    uint8_t* entry = reinterpret_cast<uint8_t*>(palette);
    for (uint32_t i = 0; i < count; ++i) {
        entry[0] = compact[0];
        entry[1] = compact[1];
        entry[2] = compact[2];
        entry[3] = 0;
        entry += 4;
        compact += 3;
    }
}

// ---- TbGraphicFileLoader

TbResult TbGraphicFileLoader::ReadHeader(const char* path, const char* second, TbRenderArea* area) {
    CWadFile file;
    if (!file.Open(path, second).Succeeded()) return TbResult{-1};
    return ReadHeader(file, area);
}

TbResult TbGraphicFileLoader::ReadHeader(CFile& file, TbRenderArea* area) {
    if (file.Seek(0, 0) != 0) return TbResult{-1};
    TbTGALoader* loader = static_cast<TbTGALoader*>(this);
    if (!Succeeded(loader->ReadHeaderInfo(file, mArea))) return TbResult{-1};
    if (area != nullptr) *area = mArea;
    return TbResult{0};
}

TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, const char* path, const char* second, TbRenderArea* other) {
    CWadFile file;
    if (!file.Open(path, second).Succeeded()) return TbResult{-1};
    return Load(area, file, other);
}

TbResult TbGraphicFileLoader::Load(const TbRenderArea& area, CFile& file, TbRenderArea* other) {
    _TbBufferedFile buffered;
    buffered.Open(file, 0x400);
    if (buffered.Seek(0, 0) != 0) return TbResult{-1};
    if (area.mData == nullptr) return TbResult{-1};
    TbTGALoader* loader = static_cast<TbTGALoader*>(this);
    if (!Succeeded(loader->ReadData(area, buffered, mArea))) return TbResult{-1};
    if (other != nullptr) *other = mArea;
    return TbResult{0};
}

bool TbGraphicFileLoader::IsValid(CFile& file) {
    if (file.Seek(0, 0) != 0) return false;
    return static_cast<TbTGALoader*>(this)->ValidImage(file);
}

// ---- TbTGALoader

TbTGALoader::TbTGALoader() {}

TbTGALoader::~TbTGALoader() {}

TbResult TbTGALoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _TgaGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

TbResult TbTGALoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _TgaGLL reader;
    return reader.ReadData(area, file, this);
}

// A TGA 2.0 footer ("TRUEVISION-XFILE." and a zero) at the end of the file is taken for a TGA file whatever the
// header says; failing that the header is looked at: a colour map type of 0 or 1, an image type of 0 to 3 or 9 to
// 11 and a depth of 8, 15, 16, 24 or 32.
bool TbTGALoader::ValidImage(CFile& file) {
    if (file.GetSize() < 0x12) return false;
    const int32_t headerStart = file.GetSize() - 0x12;
    if (file.Seek(static_cast<uint32_t>(-0x12), 2) != headerStart) return false;
    char footer[0x12];
    if (static_cast<uint32_t>(file.Read(footer, 0x12)) != 0x12) return false;
    if (std::strncmp(footer, "TRUEVISION-XFILE.", 0x12) == 0) return true;
    if (file.Seek(0, 0) != 0) return false;
    uint8_t header[0x12];
    if (static_cast<uint32_t>(file.Read(header, 0x12)) != 0x12) return false;
    const uint8_t mapType = header[1];
    const uint8_t imageType = header[2];
    const uint8_t depth = header[0x10];
    const bool mapOk = mapType == 0 || mapType == 1;
    const bool typeOk = imageType == 0 || imageType == 1 || imageType == 2 || imageType == 3 || imageType == 9 ||
                        imageType == 10 || imageType == 11;
    const bool depthOk = depth == 8 || depth == 15 || depth == 16 || depth == 24 || depth == 32;
    return mapOk && typeOk && depthOk;
}

// ---- _TbBufferedFile

// The buffered file's destructor (0x10023024, Lbgfsave.cpp: it sits among the loader's functions): the file base class'
// destructor, and the object itself freed when the flag is positive (the compiler's `delete`). Load makes one on its stack.
bfl::FileSystem::_TbBufferedFile::~_TbBufferedFile() {}

// ---- _AndyGLL

void _AndyGLL::WriteLine() {
    std::memcpy(mDest, mLine, mDestBytes);
    mDest += mDestStep;
}

TbResult _AndyGLL::DoRead(const TbRenderArea& area, CFile& file, TbGraphicFileLoader* loader) {
    TbRenderArea& source = loader->mArea;
    const uint32_t areaLine = static_cast<uint32_t>(source.mWidth) * BytesOfPixel(area.mDepth);
    const uint32_t fileLine = GetSourceLineLength();
    mLineSize = fileLine > areaLine ? fileLine : areaLine;
    mLine = static_cast<uint8_t*>(LbMem_Alloc(mLineSize));
    if (mLine == nullptr) return TbResult{-1};
    const uint32_t width = static_cast<uint32_t>(source.mWidth) < static_cast<uint32_t>(area.mWidth)
                               ? static_cast<uint32_t>(source.mWidth)
                               : static_cast<uint32_t>(area.mWidth);
    mDestBytes = width * BytesOfPixel(area.mDepth);
    mSourceBytes = BytesOfPixel(source.mDepth) * static_cast<uint32_t>(source.mWidth);
    if (ReadFirstLine(file).mCode != 0) {
        LbMem_Free(mLine);
        return TbResult{-1};
    }
    if (mBottomUp == 0) {
        mDest = area.mData;
        mDestStep = area.mStride;
    } else {
        // The file's first line is the bottom one: the lines go up from the last row the area has room for.
        const uint32_t rows = static_cast<uint32_t>(source.mPictureHeight) < static_cast<uint32_t>(area.mPictureHeight)
                                  ? static_cast<uint32_t>(source.mPictureHeight)
                                  : static_cast<uint32_t>(area.mPictureHeight);
        const uint32_t offset = static_cast<uint32_t>(area.mStride) * rows;
        mDest = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(area.mData) + offset) - area.mStride;
        mDestStep = -area.mStride;
        if (static_cast<uint32_t>(area.mPictureHeight) < static_cast<uint32_t>(source.mPictureHeight)) {
            // The lines past the area's top are read and dropped.
            uint32_t skip = static_cast<uint32_t>(source.mPictureHeight) - static_cast<uint32_t>(area.mPictureHeight);
            while (skip != 0) {
                --skip;
                if (!Succeeded(ReadNextLine(file))) {
                    LbMem_Free(mLine);
                    return TbResult{-1};
                }
            }
        }
    }
    // The same format, with the palettised flag compared as a flag.
    bool sameFormat = false;
    if (std::memcmp(area.mF00, source.mF00, sizeof area.mF00) == 0 && area.mDepth == source.mDepth &&
        (area.mPalettised == 0) == (source.mPalettised == 0))
        sameFormat = true;
    uint32_t line = 0;
    if (sameFormat) {
        do {
            WriteLine();
            ++line;
            if (!Succeeded(ReadNextLine(file))) break;
        } while (line < static_cast<uint32_t>(area.mPictureHeight));
    } else {
        CConverter converter;
        converter.Init(reinterpret_cast<const TbPixFormat*>(&area), reinterpret_cast<const TbPixFormat*>(&source),
                       reinterpret_cast<const TbPalette*>(loader->mPalette), 1);
        do {
            converter.ConvBuf(mLine, mLine, source.mWidth);
            WriteLine();
            ++line;
            if (!Succeeded(ReadNextLine(file))) break;
        } while (line < static_cast<uint32_t>(area.mPictureHeight));
        converter.Release();
    }
    LbMem_Free(mLine);
    return TbResult{0};
}

// ---- _TgaGLL

TbResult _TgaGLL::ReadData(const TbRenderArea& area, CFile& file, TbTGALoader* loader) {
    if (!Succeeded(ReadHeaderInfo(file, loader))) return TbResult{-1};
    return DoRead(area, file, loader);
}

// Reads the 18 bytes of the header into the loader and swaps its halfwords to host order (the file's order here).
// Colour-mapped files (types 1 and 9) must have a 24-bit map of at most 256 entries: the map is read into the
// loader's palette, as red, green, blue and a zero byte, from entry 0 whatever the header says its first entry is;
// any other file with a map has the map skipped, by a length that counts its bits instead of its bytes and wraps at
// 16 bits. The format of the loader's area is that of the depth (15 and 16 bits are the 16-bit format).
TbResult _TgaGLL::ReadHeaderInfo(CFile& file, TbTGALoader* loader) {
    TgaHeader& header = loader->mHeader;
    if (static_cast<uint32_t>(file.Read(&header, 0x12)) != 0x12) return TbResult{-1};
    mHeader = header;
    const uint8_t imageType = header.mImageType;
    if (imageType >= 9) {
        if (imageType >= 12) return TbResult{-1};
    } else if (imageType >= 4) {
        return TbResult{-1};
    }
    const uint8_t mapType = header.mMapType;
    if (mapType > 1) return TbResult{-1};
    if ((imageType == 1 || imageType == 9) && mapType == 0) return TbResult{-1};
    if (mapType == 1) {
        if (header.mMapLength > 0x100) return TbResult{-1};
        if (header.mMapEntryBits != 0x18) return TbResult{-1};
    }
    mRle = mHeader.mImageType >= 9 ? 1 : 0;
    mBottomUp = 0;
    if ((header.mDescriptor & 0x20) == 0) mBottomUp = 1;
    mRightToLeft = 0;
    if ((header.mDescriptor & 0x10) != 0) mRightToLeft = 1;
    file.Seek(header.mIdLength, 1);
    loader->mHasPalette = 0;
    if (imageType == 1 || imageType == 9) {
        const uint32_t count = header.mMapLength;
        uint8_t* compact = static_cast<uint8_t*>(LbMem_Alloc(count * 3));
        file.Read(compact, count * 3);
        std::memset(loader->mPalette, 0, sizeof loader->mPalette);
        LbPalette_FromCompact(reinterpret_cast<TbPalette*>(loader->mPalette), compact, count);
        loader->mHasPalette = 1;
        // The file's entries are blue, green, red: swap the first and the last of each.
        uint8_t* entry = loader->mPalette;
        for (uint32_t i = 0; i < header.mMapLength; ++i) {
            entry[0] = entry[0] ^ entry[2];
            entry[2] = entry[2] ^ entry[0];
            entry[0] = entry[0] ^ entry[2];
            entry += 4;
        }
        LbMem_Free(compact);
    } else {
        const uint32_t skip = (static_cast<uint32_t>(header.mMapLength) * header.mMapEntryBits) & 0xffff;
        if (skip != 0) file.Seek(skip, 1);
    }
    const TbPixFormat* format;
    switch (header.mDepth) {
    case 8: format = &TbPixFormat8; break;
    case 15:
    case 16: format = &TbPixFormat16; break;
    case 24: format = &TbPixFormat24; break;
    case 32: format = &TbPixFormat32; break;
    default: return TbResult{-1};
    }
    TbRenderArea& area = loader->mArea;
    SetFormat(area, *format);
    area.mWidth = header.mWidth;
    area.mStride = static_cast<int32_t>(static_cast<uint32_t>(header.mWidth) * BytesOfPixel(area.mDepth));
    area.mPictureHeight = header.mHeight;
    return TbResult{0};
}

TbResult _TgaGLL::ReadFirstLine(CFile& file) {
    mLinesLeft = mHeader.mHeight;
    mLineBytes = static_cast<uint32_t>(mHeader.mWidth) * (mHeader.mDepth >> 3);
    return ReadNextLine(file);
}

// A line of an uncompressed file is one read of mLineBytes; a run-length encoded one is TGALoadRLE. Either is then
// reversed when the descriptor says the lines run right to left. The line counter wraps below zero: a read after the
// last line fails (and the counter is left at -1).
TbResult _TgaGLL::ReadNextLine(CFile& file) {
    if (mRle == 1) {
        const uint32_t left = mLinesLeft;
        mLinesLeft = left - 1;
        if (left == 0) return TbResult{-1};
        if (!Succeeded(TGALoadRLE(file))) return TbResult{-1};
        if (mRightToLeft != 0) return ReverseScanLine(mLine);
        return TbResult{0};
    }
    if (mRle < 0 || mRle > 1) return TbResult{-1};
    const uint32_t left = mLinesLeft;
    mLinesLeft = left - 1;
    if (left == 0) return TbResult{-1};
    if (mLineBytes != static_cast<uint32_t>(file.Read(mLine, mLineBytes))) return TbResult{-1};
    if (mRightToLeft != 0) return ReverseScanLine(mLine);
    return TbResult{0};
}

// The pixels of the line swapped end for end: the whole bytes of a pixel (the depth divided by 8: a 15-bit file
// counts one byte), half the width of them. The swap is an exclusive-or swap, as the original's.
TbResult _TgaGLL::ReverseScanLine(uint8_t* line) {
    const uint32_t pixelBytes = static_cast<uint8_t>(mHeader.mDepth >> 3);
    uint8_t* left = line;
    uint8_t* right = line + mLineBytes - pixelBytes;
    const uint16_t pairs = static_cast<uint16_t>(mHeader.mWidth >> 1);
    for (uint32_t pair = 0; static_cast<uint16_t>(pair) < pairs; ++pair) {
        uint8_t* a = left;
        uint8_t* b = right;
        for (uint32_t byte = 0; byte < pixelBytes; ++byte) {
            *a = *a ^ *b;
            *b = *b ^ *a;
            *a = *a ^ *b;
            ++a;
            ++b;
        }
        left += pixelBytes;
        right -= pixelBytes;
    }
    return TbResult{0};
}

// Packets of a count byte and pixels: bit 7 set repeats the one pixel that follows (count & 0x7f) + 1 times, clear
// reads count + 1 pixels. They go on until the line has its width in pixels; a packet that ends past the width is
// written whole, past the end of the line buffer.
TbResult _TgaGLL::TGALoadRLE(CFile& file) {
    uint8_t* out = mLine;
    const uint32_t pixelBytes = static_cast<uint32_t>(mHeader.mDepth >> 3);
    uint32_t done = 0;
    uint8_t pixel[4];
    uint8_t packet;
    while (done < mHeader.mWidth) {
        if (static_cast<uint32_t>(file.Read(&packet, 1)) != 1) return TbResult{-1};
        const int32_t count = static_cast<int8_t>(packet);
        if (count < 0) {
            if (static_cast<uint32_t>(file.Read(pixel, pixelBytes)) != pixelBytes) return TbResult{-1};
            const uint32_t repeats = static_cast<uint32_t>(static_cast<int8_t>(packet) + 0x81);
            done += repeats;
            for (uint32_t repeat = repeats; repeat != 0; --repeat) {
                for (uint32_t byte = 0; byte < pixelBytes; ++byte) *out++ = pixel[byte];
            }
        } else {
            const uint32_t bytes = static_cast<uint32_t>(count + 1) * pixelBytes;
            if (static_cast<uint32_t>(file.Read(out, bytes)) != bytes) return TbResult{-1};
            out += bytes;
            done += static_cast<uint32_t>(static_cast<int8_t>(packet) + 1);
        }
    }
    return TbResult{0};
}

uint32_t _TgaGLL::GetSourceLineLength() { return static_cast<uint32_t>(mHeader.mWidth) * (mHeader.mDepth >> 3); }
