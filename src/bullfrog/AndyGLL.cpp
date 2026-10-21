// AndyGLL.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of AndyGLL code, named after it.

#include "bfl_FileSystem.h"
#include "bfl_WadFile.h"
#include "helpers/AndyGLLHelpers.h"
#include "helpers/LbgfsaveHelpers.h"
#include "LbFile.h"
#include "LbSCBack.h"
#include "LbSemaphore.h"
#include "lobby.h"
#include "platform/samsutils.h"
#include "TbGraphicFile.h"
#include "TbPixFmt.h"
#include "TbRaConvert.h"
#include "WinScrn.h"
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

using bfl::FileSystem::CFile;
void LbPalette_FromCompact(TbPalette* palette, const uint8_t* compact, uint32_t count);

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

// ---- _AndyGLL

void _AndyGLL::WriteLine() {
    std::memcpy(mDest, mLine, mDestBytes);
    mDest += mDestStep;
}

// The BMP and PCX picture loaders of Lbgfsave.cpp (bullfrog symbols shared), unit w220.
// Succeeded, SetFormat and BytesOfPixel copy the local helpers of bfl_graphicfile.cpp, lines 26..44.

uint8_t _AndyBMPGLL::sFileHeader[0xe];

// 0x1001e36c
TbResult _AndyBMPGLL::ReadData(const TbRenderArea& area, CFile& file, TbBMPLoader* loader) {
    if (!Succeeded(ReadHeaderInfo(file, loader))) return TbResult{-1};
    return DoRead(area, file, loader);
}

// 0x1001e478
TbResult _AndyBMPGLL::ReadHeaderInfo(CFile& file, TbBMPLoader* loader) {
    if (static_cast<uint32_t>(file.Read(sFileHeader, 0xe)) != 0xe) return TbResult{-1};
    if (Be16(sFileHeader) != 0x4d42) return TbResult{-1};
    if (static_cast<uint32_t>(file.GetSize()) != Be32(sFileHeader + 2)) return TbResult{-1};
    uint8_t* info = loader->mInfo;
    if (static_cast<uint32_t>(file.Read(info, 0x28)) != 0x28) return TbResult{-1};
    std::memcpy(mInfo, info, 0x28);
    if (Be32(info) != 0x28) return TbResult{-1};
    if (static_cast<int32_t>(Be32(info + 4)) <= 0 || static_cast<int32_t>(Be32(info + 8)) <= 0)
        return TbResult{-1};
    if (Be16(info + 0xc) != 1) return TbResult{-1};
    const uint32_t bpp = Be16(info + 0xe);
    if (bpp != 8 && bpp != 24 && bpp != 4) return TbResult{-1};
    loader->mHasPalette = 0;
    const uint32_t used = Be32(info + 0x20);
    if (bpp != 24 && used != 0) {
        if (used < 0x100) std::memset(loader->mPalette, 0, 0x400);
        const uint32_t bytes = used << 2;
        if (static_cast<uint32_t>(file.Read(loader->mPalette, bytes)) != bytes) return TbResult{-1};
        loader->mHasPalette = 1;
        uint8_t* entry = loader->mPalette;
        for (uint32_t i = 0; i < used; ++i) {
            entry[0] ^= entry[2];
            entry[2] ^= entry[0];
            entry[0] ^= entry[2];
            entry += 4;
        }
    }
    TbRenderArea& area = loader->mArea;
    SetFormat(area, bpp == 24 ? TbPixFormat24 : TbPixFormat8);
    area.mWidth = static_cast<int32_t>(Be32(info + 4));
    area.mStride = static_cast<int32_t>(Be32(info + 4) * BytesOfPixel(area.mDepth));
    area.mPictureHeight = static_cast<int32_t>(Be32(info + 8));
    const uint32_t off = Be32(sFileHeader + 0xa);
    if (static_cast<uint32_t>(file.Seek(off, 0)) != off) return TbResult{-1};
    return TbResult{0};
}

// 0x1001e9fc
TbResult _AndyBMPGLL::ReadFirstLine(CFile& file) {
    const int32_t compression = static_cast<int32_t>(Be32(mInfo + 0x10));
    const uint32_t bpp = Be16(mInfo + 0xe);
    if (compression == 1) {
        if (bpp != 8) return TbResult{-1};
    } else if (compression != 0) {
        return TbResult{-1};
    }
    mBottomUp = static_cast<int32_t>(Be32(mInfo + 8)) > 0 ? 1 : 0;
    if (compression == 1) {
        mRleData = static_cast<uint8_t*>(LbMem_Alloc(Be32(mInfo + 0x14)));
        if (mRleData == nullptr) return TbResult{-1};
        if (static_cast<uint32_t>(file.Read(mRleData, Be32(mInfo + 0x14))) != Be32(mInfo + 0x14)) {
            LbMem_Free(mRleData);
            return TbResult{-1};
        }
        mRleCursor = mRleData;
        mSkipLines = 0;
        mNextX = 0;
        mRleEnd = 0;
    } else {
        uint32_t width = Be32(mInfo + 4);
        if (bpp == 4) ++width;
        const int32_t bytes = static_cast<int32_t>(width * bpp) >> 3;
        mLineBytes = (static_cast<uint32_t>(bytes) + 3) & ~3u;
        mLinesLeft = Be32(mInfo + 8);
    }
    return ReadNextLine(file);
}

// 0x1001ec08
TbResult _AndyBMPGLL::ReadNextLine(CFile& file) {
    const int32_t compression = static_cast<int32_t>(Be32(mInfo + 0x10));
    if (compression == 1) return BMPLoadRLE(file);
    if (compression != 0) return TbResult{-1};
    const uint32_t old = mLinesLeft;
    mLinesLeft = old - 1;
    if (old == 0) return TbResult{-1};
    if (static_cast<uint32_t>(file.Read(mLine, mLineBytes)) != mLineBytes) return TbResult{-1};
    if (Be16(mInfo + 0xe) == 4) {
        for (uint32_t i = mLineBytes; i != 0; --i) {
            mLine[2 * i - 1] = mLine[i - 1] & 0xf;
            mLine[2 * i - 2] = mLine[i - 1] >> 4;
        }
    }
    return TbResult{0};
}

// 0x1001ede8. End-of-bitmap keeps its cursor, and every subsequent call frees the same buffer again.
TbResult _AndyBMPGLL::BMPLoadRLE(CFile&) {
    uint8_t* base = mLine;
    uint8_t* out = base;
    if (mRleEnd != 0) {
        LbMem_Free(mRleData);
        return TbResult{-1};
    }
    if (mSkipLines > 0) {
        --mSkipLines;
        return TbResult{0};
    }
    if (mNextX != 0) {
        out += mNextX;
        mNextX = 0;
    }
    for (;;) {
        uint8_t* p = mRleCursor;
        if (p[0] != 0) {
            std::memset(out, p[1], p[0]);
            uint8_t* q = mRleCursor;
            const uint8_t count = q[0];
            mRleCursor = q + 2;
            out += count;
            continue;
        }
        const uint8_t escape = p[1];
        if (escape == 1) {
            mRleEnd = 1;
            return TbResult{0};
        }
        if (escape == 0) {
            mRleCursor = p + 2;
            return TbResult{0};
        }
        if (escape >= 3) {
            mRleCursor = p + 2;
            std::memcpy(out, mRleCursor, escape);
            out += escape;
            mRleCursor += escape;
            if ((escape & 1) != 0) ++mRleCursor;
            continue;
        }
        mRleCursor = p + 2;
        mNextX = *mRleCursor++;
        mSkipLines = *mRleCursor++;
        if (mSkipLines != 0) {
            --mSkipLines;
            mNextX = static_cast<uint32_t>(out - base) + mNextX;
            return TbResult{0};
        }
        out += mNextX;
        mNextX = 0;
    }
}

// 0x1001f004. Keep the wrapped multiply and signed shift even for malformed widths.
uint32_t _AndyBMPGLL::GetSourceLineLength() {
    const uint32_t bpp = Be16(mInfo + 0xe);
    uint32_t bits = Be32(mInfo + 4) * bpp;
    if (bpp == 4) bits <<= 1;
    uint32_t bytes = static_cast<uint32_t>(static_cast<int32_t>(bits) >> 3);
    if (bpp == 4) bytes += 4;
    return (bytes + 3) & ~3u;
}

// IFF, GIF and TIM readers of Lbgfsave.cpp (bullfrog symbols shared), unit w223.

namespace {
bool Succeeded_2(const TbResult& r) { return r.mCode >= 0; }
void SetFormat_2(TbRenderArea& a, const TbPixFormat& f) {
    std::memcpy(a.mF00, &f.mRedMask, 4);
    std::memcpy(a.mF00 + 4, &f.mGreenMask, 4);
    std::memcpy(a.mF00 + 8, &f.mBlueMask, 4);
    std::memcpy(a.mF00 + 12, &f.mAlphaMask, 4);
    a.mDepth = f.mBitsPerPixel;
    a.mPalettised = f.mPalettised;
}
uint32_t Le16(const uint8_t* p) { return p[0] | (uint32_t(p[1]) << 8); }
uint32_t Le32(const uint8_t* p) { return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }
uint32_t Slw(uint32_t v, uint32_t n) { n &= 63; return n < 32 ? v << n : 0; }
uint32_t Sraw(int32_t v, uint32_t n) { n &= 63; return n < 32 ? uint32_t(v >> n) : uint32_t(v >> 31); }
// A one-byte read into a Mac word writes its high byte, even on a short read.
int32_t ReadWordByte(CFile& file, uint32_t& word) {
    uint8_t bytes[4] = {uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word)};
    const int32_t result = file.Read(bytes, 1);
    word = Be32(bytes);
    return result;
}
// Invented names; the adjacent tables preserve the original one-past reads.
const uint32_t kCodeMask[13] = {0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095}; // 0x10072da8
const uint32_t kInterlace[9] = {8, 8, 4, 2, 0, 4, 2, 1, 0}; // 0x10072ddc, 0x10072dec
}
uint8_t _AndyIFFGLL::sChunk[8];
uint8_t _AndyGIFGLL::sPalette[0x300];
uint8_t _AndyGIFGLL::sB1;
// __sinit_Lbgfsave_cpp, 0x1002419c: the two file-static formats, names invented.
const TbPixFormat LbgfsaveTim16(0x1f, 0x3e0, 0x7c00, 0, 16, 0);
const TbPixFormat LbgfsaveTim24(0xff, 0xff00, 0xff0000, 0, 24, 0);

// 0x1001f078
TbResult _AndyIFFGLL::ReadData(const TbRenderArea& area, CFile& file, TbIFFLoader* loader) {
    if (!Succeeded_2(ReadHeaderInfo(file, loader))) return {-1};
    return DoRead(area, file, loader);
}
// 0x1001f184
TbResult _AndyIFFGLL::ReadHeaderInfo(CFile& file, TbIFFLoader* loader) {
    uint8_t* h = loader->mHeader;
    if (file.Read(h, 0x28) != 0x28 || !IffMagic(h) || h[0x1c] != 8) return {-1};
    loader->mArea.mWidth = Le16(h + 0x14);
    loader->mArea.mStride = loader->mArea.mWidth;
    loader->mArea.mPictureHeight = Le16(h + 0x16);
    SetFormat_2(loader->mArea, TbPixFormat8);
    loader->mHasPalette = 0;
    std::memcpy(mHeader, h, sizeof mHeader);
    while (file.Read(sChunk, 8) == 8) {
        if (Be32(sChunk) == 0x50414d43) {
            if (Le32(sChunk + 4) != 0x300) return {-1};
            auto* p = static_cast<uint8_t*>(LbMem_Alloc(0x300));
            if (!p) return {-1};
            if (file.Read(p, 0x300) == 0x300) {
                loader->mHasPalette = 1;
                for (unsigned i = 0; i < 256; ++i) {
                    loader->mPalette[4 * i] = p[3 * i];
                    loader->mPalette[4 * i + 1] = p[3 * i + 1];
                    loader->mPalette[4 * i + 2] = p[3 * i + 2];
                }
            }
            LbMem_Free(p);
        } else file.Seek((Le32(sChunk + 4) + 1) & 0x7ffffffe, 1);
    }
    return {0};
}
// 0x1001f578
TbResult _AndyIFFGLL::ReadFirstLine(CFile& file) {
    file.Seek(0x28, 0);
    mBottomUp = 0;
    mLinesRead = 0;
    if (Be32(mHeader + 8) == 0x4d424c49) {
        mPlanes = mHeader[0x1c];
        mPlaneBytes = (Le16(mHeader + 0x14) / mPlanes + 1) & ~1u;
        mLineBytes = mPlaneBytes * mPlanes;
        mRead = mLine + mLineBytes;
    } else {
        mPlanes = 1;
        mPlaneBytes = 0;
        mLineBytes = (Le16(mHeader + 0x14) + 1) & ~1u;
        mRead = mLine;
    }
    mHeight = Le16(mHeader + 0x16);
    for (;;) {
        if (file.Read(sChunk, 8) != 8) return {-1};
        if (Be32(sChunk) == 0x59444f42) return ReadNextLine(file);
        file.Seek((Le32(sChunk + 4) + 1) & 0x7ffffffe, 1);
    }
}
// 0x1001f7b8
TbResult _AndyIFFGLL::ReadNextLine(CFile& file) {
    uint8_t* p = mRead;
    ++mLinesRead;
    if (mHeight < mLinesRead) return {-1};
    if (mHeader[0x1e] == 0) {
        if (uint32_t(file.Read(p, mLineBytes)) != mLineBytes) return {-1};
    } else {
        uint32_t count = 0;
        do {
            uint8_t byte;
            if (file.Read(&byte, 1) != 1) return {-1};
            int32_t n = static_cast<int8_t>(byte);
            if (n >= 0) {
                ++n;
                if (uint32_t(file.Read(p, n)) != uint32_t(n)) return {-1};
            } else if (n != -128) {
                if (file.Read(&byte, 1) != 1) return {-1};
                n = 1 - n;
                std::memset(p, byte, n);
            }
            count += uint32_t(n);
            p += n; // -128 moves backwards in the original.
        } while (int32_t(count) < int32_t(mLineBytes));
    }
    if (Be32(mHeader + 8) == 0x4d424c49) {
        const uint8_t* source = mRead;
        uint8_t* dest = mLine;
        for (uint32_t col = 0; col < mLineBytes; col += mPlanes, ++source) {
            for (int bit = 7; bit >= 0; --bit) {
                *dest = 0;
                const uint8_t* plane = source;
                for (uint32_t i = 0; int32_t(i) < int32_t(mPlanes); ++i, plane += mPlaneBytes)
                    *dest |= uint8_t(Slw((*plane >> bit) & 1, i));
                ++dest;
            }
        }
    }
    return {0};
}
// 0x1001fa14: width is deliberately unswapped here.
uint32_t _AndyIFFGLL::GetSourceLineLength() {
    uint32_t n = (Be16(mHeader + 0x14) + 7) & ~7u;
    return Be32(mHeader + 8) == 0x4d424c49 ? n << 1 : n;
}

// The constructors and destructors are host only.
TbBMPLoader::TbBMPLoader() {}
TbBMPLoader::~TbBMPLoader() {}

// 0x1001fa70
TbResult _AndyPCXGLL::ReadData(const TbRenderArea& area, CFile& file, TbPCXLoader* loader) {
    if (!Succeeded(ReadHeaderInfo(file, loader))) return TbResult{-1};
    return DoRead(area, file, loader);
}

// 0x1001fb7c. Failed palette seeks and reads deliberately leave the allocation live.
TbResult _AndyPCXGLL::ReadHeaderInfo(CFile& file, TbPCXLoader* loader) {
    uint8_t* header = loader->mHeader;
    if (static_cast<uint32_t>(file.Read(header, 0x80)) != 0x80) return TbResult{-1};
    if (header[3] != 8) return TbResult{-1};
    if (header[0x41] == 1) SetFormat(loader->mArea, TbPixFormat8);
    else if (header[0x41] == 3) SetFormat(loader->mArea, TbPixFormat24);
    else return TbResult{-1};
    TbRenderArea& area = loader->mArea;
    area.mWidth = static_cast<int32_t>(Be16(header + 8)) - static_cast<int32_t>(Be16(header + 4)) + 1;
    area.mPictureHeight = static_cast<int32_t>(Be16(header + 0xa)) - static_cast<int32_t>(Be16(header + 6)) + 1;
    area.mStride = static_cast<int32_t>(static_cast<uint32_t>(area.mWidth) * BytesOfPixel(area.mDepth));
    loader->mHasPalette = 0;
    uint8_t* buffer = static_cast<uint8_t*>(LbMem_Alloc(0x301));
    if (buffer == nullptr) return TbResult{-1};
    const uint32_t want = static_cast<uint32_t>(file.GetSize()) - 0x301;
    if (static_cast<uint32_t>(file.Seek(0xfffffcffu, 2)) != want) return TbResult{-1};
    if (static_cast<uint32_t>(file.Read(buffer, 0x301)) != 0x301) return TbResult{-1};
    if (buffer[0] == 0xc) {
        loader->mHasPalette = 1;
        for (uint32_t i = 0; i < 256; ++i) {
            loader->mPalette[4 * i] = buffer[1 + 3 * i];
            loader->mPalette[4 * i + 1] = buffer[2 + 3 * i];
            loader->mPalette[4 * i + 2] = buffer[3 + 3 * i];
        }
    }
    LbMem_Free(buffer);
    std::memcpy(mHeader, header, 0x80);
    if (static_cast<uint32_t>(file.Seek(0x80, 0)) != 0x80) return TbResult{-1};
    return TbResult{0};
}

// 0x1001ff38
TbResult _AndyPCXGLL::ReadFirstLine(CFile& file) {
    mBottomUp = 0;
    mDecodeBytes = mHeader[0x41] * Be16(mHeader + 0x42);
    mLinesLeft = static_cast<int32_t>(Be16(mHeader + 0xa)) - static_cast<int32_t>(Be16(mHeader + 6)) + 1;
    mRunCount = 0;
    mRunValue = 0;
    return ReadNextLine(file);
}

// 0x1001fff0. A carried run fills the start of the row without advancing the output pointer.
TbResult _AndyPCXGLL::ReadNextLine(CFile& file) {
    const uint32_t old = mLinesLeft;
    mLinesLeft = old - 1;
    if (old == 0) return TbResult{-1};
    uint8_t* out = mLine;
    uint32_t left = mDecodeBytes;
    if (mHeader[0x41] == 3) out += mLineSize >> 1;
    if (mRunCount != 0) {
        const uint32_t count = left < mRunCount ? left : mRunCount;
        std::memset(out, mRunValue, count);
        left -= count;
        mRunCount -= count;
    }
    while (left != 0) {
        if (static_cast<uint32_t>(file.Read(&mRunCount, 1)) != 1) return TbResult{-1};
        const uint8_t byte = mRunCount;
        if ((byte & 0xc0) == 0xc0) {
            if (static_cast<uint32_t>(file.Read(&mRunValue, 1)) != 1) return TbResult{-1};
            mRunCount &= 0x3f;
            const uint32_t count = left < mRunCount ? left : mRunCount;
            std::memset(out, mRunValue, count);
            out += count;
            left -= count;
            mRunCount -= count;
        } else {
            *out++ = byte;
            mRunCount = 0;
            --left;
        }
    }
    if (mHeader[0x41] == 3) {
        uint8_t* dest = mLine;
        uint8_t* red = mLine + (mLineSize >> 1);
        const uint32_t bytesPerLine = Be16(mHeader + 0x42);
        uint8_t* green = red + bytesPerLine;
        uint8_t* blue = green + bytesPerLine;
        uint32_t count = static_cast<int32_t>(Be16(mHeader + 8)) - static_cast<int32_t>(Be16(mHeader + 4)) + 1;
        while (count-- != 0) {
            dest[0] = *blue++;
            dest[1] = *green++;
            dest[2] = *red++;
            dest += 3;
        }
    }
    return TbResult{0};
}

// 0x10020294
uint32_t _AndyPCXGLL::GetSourceLineLength() {
    const uint32_t bytes = mHeader[0x41] * Be16(mHeader + 0x42);
    return mHeader[0x41] == 3 ? bytes << 1 : bytes;
}

// The constructors and destructors are host only.
TbPCXLoader::TbPCXLoader() {}
TbPCXLoader::~TbPCXLoader() {}

// 0x100202e8
TbResult _AndyGIFGLL::ReadData(const TbRenderArea& area, CFile& file, TbGIFLoader* loader) {
    if (!Succeeded_2(ReadHeaderInfo(file, loader))) return {-1};
    mStack = static_cast<uint8_t*>(LbMem_Alloc(0x1000));
    if (!mStack) return {-1};
    mSuffix = static_cast<uint8_t*>(LbMem_Alloc(0x1000));
    if (!mSuffix) { LbMem_Free(mStack); return {-1}; }
    mPrefix = static_cast<uint16_t*>(LbMem_Alloc(0x2000));
    if (!mPrefix) { LbMem_Free(mSuffix); LbMem_Free(mStack); return {-1}; }
    TbResult r = DoRead(area, file, loader);
    LbMem_Free(mStack);
    LbMem_Free(mSuffix);
    LbMem_Free(mPrefix);
    mStack = nullptr;
    mSuffix = nullptr;
    mPrefix = nullptr;
    return r;
}
// 0x100204b8
TbResult _AndyGIFGLL::ReadHeaderInfo(CFile& file, TbGIFLoader* loader) {
    uint8_t* h = loader->mHeader;
    if (file.Read(h, 13) != 13 || std::strncmp(reinterpret_cast<char*>(h), "GIF", 3)) return {-1};
    loader->mArea.mWidth = Be16(h + 6);
    loader->mArea.mStride = Be16(h + 6);
    loader->mArea.mPictureHeight = Be16(h + 8);
    mPalette = reinterpret_cast<TbPalette*>(loader->mPalette);
    if ((h[10] & 7) != 7) return {-1};
    SetFormat_2(loader->mArea, TbPixFormat8);
    loader->mHasPalette = 0;
    if (h[10] & 0x80) {
        if (!Succeeded_2(_GifReadPalette(file, mPalette))) return {-1};
        loader->mHasPalette = 1;
    }
    std::memcpy(mHeader, h, sizeof mHeader);
    return {0};
}
// 0x100206b4
TbResult _AndyGIFGLL::ReadFirstLine(CFile& file) {
    mBottomUp = 0;
    uint8_t intro, label, count, buffer[256], descriptor[9];
    if (file.Read(&intro, 1) != 1) return {-1};
    if (intro == '!') {
        if (file.Read(&label, 1) != 1 || file.Read(&count, 1) != 1) return {-1};
        while (count != 0) {
            if (uint32_t(file.Read(buffer, count)) != count || file.Read(&count, 1) != 1) return {-1};
        }
        if (file.Read(&intro, 1) != 1) return {-1};
    }
    if (intro != ',' || file.Read(descriptor, 9) != 9) return {-1};
    if (descriptor[8] & 0x80) {
        if (Slw(1, descriptor[8] & 7) != 0x100) return {-1};
        if (!Succeeded_2(_GifReadPalette(file, mPalette))) return {-1};
    }
    mInterlaced = descriptor[8] & 0x40;
    mSize = 0;
    if (ReadWordByte(file, mSize) != 1 || int32_t(mSize) < 2 || int32_t(mSize) > 9) return {-1};
    mCurrSize = mSize + 1;
    mTopSlot = Slw(1, mCurrSize);
    mClear = Slw(1, mSize);
    mEnding = mClear + 1;
    mNewCodes = mEnding + 1;
    mSlot = mEnding + 1;
    mBitsLeft = 0;
    mAvailBytes = 0;
    if (!mStack || !mSuffix || !mPrefix) return {-1};
    mSp = mStack;
    mPass = mRow = 0;
    mFirstDest = nullptr;
    mFc = mOc = 0;
    return ReadNextLine(file);
}
// 0x10020a68
TbResult _AndyGIFGLL::ReadNextLine(CFile& file) {
    uint8_t* dest = mLine;
    uint32_t left = mSourceBytes;
    while (mSp > mStack) {
        --left;
        *dest++ = *--mSp;
        if (left == 0) return {0};
    }
    for (;;) {
        int32_t c = _GifGetNextCode(file);
        if (uint32_t(c) == mEnding || c < 0) return {-1};
        if (uint32_t(c) == mClear) {
            mCurrSize = mSize + 1;
            mSlot = mNewCodes;
            mTopSlot = Slw(1, mCurrSize);
            do { c = _GifGetNextCode(file); } while (uint32_t(c) == mClear);
            if (uint32_t(c) == mEnding) return {-1};
            if (c >= int32_t(mSlot)) c = 0;
            mFc = uint32_t(c);
            --left;
            mOc = uint32_t(c);
            *dest++ = uint8_t(c);
            if (left == 0) return {0};
        } else {
            uint32_t code = uint32_t(c);
            if (c >= int32_t(mSlot)) {
                *mSp++ = uint8_t(mFc);
                code = mOc;
            }
            while (int32_t(code) >= int32_t(mNewCodes)) {
                *mSp++ = mSuffix[code];
                code = mPrefix[code];
            }
            *mSp++ = uint8_t(code);
            if (int32_t(mSlot) < int32_t(mTopSlot)) {
                mFc = code;
                mSuffix[mSlot] = uint8_t(mFc);
                const uint32_t slot = mSlot++;
                mPrefix[slot] = uint16_t(mOc);
                mOc = uint32_t(c);
            }
            if (int32_t(mSlot) >= int32_t(mTopSlot) && int32_t(mCurrSize) < 12) {
                mTopSlot <<= 1;
                ++mCurrSize;
            }
            while (mSp > mStack) {
                --left;
                *dest++ = *--mSp;
                if (left == 0) return {0};
            }
        }
    }
}
// 0x10020d10: unlike the line functions, this returns a scalar code in r3.
int32_t _AndyGIFGLL::_GifGetNextCode(CFile& file) {
    auto block = [&]() {
        if (int32_t(mAvailBytes) <= 0) {
            mBytes = mByteBuff;
            mAvailBytes = 0;
            if (ReadWordByte(file, mAvailBytes) != 1) return false;
            if (mAvailBytes != 0 && uint32_t(file.Read(mByteBuff, mAvailBytes)) != mAvailBytes) return false;
        }
        return true;
    };
    if (mBitsLeft == 0) {
        if (!block()) return -1;
        sB1 = *mBytes++;
        mBitsLeft = 8;
        --mAvailBytes;
    }
    uint32_t code = Sraw(sB1, 8 - mBitsLeft);
    while (int32_t(mCurrSize) > int32_t(mBitsLeft)) {
        if (!block()) return -1;
        sB1 = *mBytes++;
        const uint32_t bits = mBitsLeft;
        mBitsLeft = bits + 8;
        code |= Slw(sB1, bits);
        --mAvailBytes;
    }
    mBitsLeft -= mCurrSize;
    return int32_t(code & kCodeMask[mCurrSize]);
}
// 0x10020f3c
TbResult _AndyGIFGLL::_GifReadPalette(CFile& file, TbPalette* palette) {
    if (file.Read(sPalette, 0x300) != 0x300) return {-1};
    auto* p = reinterpret_cast<uint8_t*>(palette);
    for (unsigned i = 0; i < 256; ++i) {
        p[4 * i] = sPalette[3 * i];
        p[4 * i + 1] = sPalette[3 * i + 1];
        p[4 * i + 2] = sPalette[3 * i + 2];
    }
    return {0};
}
// 0x100210e8
void _AndyGIFGLL::WriteLine() {
    if (!mFirstDest) mFirstDest = mDest;
    std::memcpy(mDest, mLine, mDestBytes);
    mDest += mDestStep;
    if (mInterlaced) {
        mRow += kInterlace[mPass];
        if (Be16(mHeader + 8) <= mRow) {
            ++mPass;
            mRow = kInterlace[4 + mPass];
        }
        mDest = mFirstDest + int32_t(mRow * uint32_t(mDestStep));
    }
}
// 0x100211e8
uint32_t _AndyGIFGLL::GetSourceLineLength() { return Be16(mHeader + 6); }

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

// 0x10021d70
TbResult _TimGLL::ReadData(const TbRenderArea& area, CFile& file, TbTIMLoader* loader) {
    if (!Succeeded_2(ReadHeaderInfo(file, loader))) return {-1};
    return DoRead(area, file, loader);
}
// 0x10021e78
TbResult _TimGLL::ReadHeaderInfo(CFile& file, TbTIMLoader* loader) {
    uint8_t* h = loader->mHeader;
    if (file.Read(h, 8) != 8 || Be32(h) != 0x10 || (Be32(h + 4) & 7) >= 4) return {-1};
    const uint32_t flags = Be32(h + 4);
    loader->mHasPalette = 0;
    if (flags == 8 || flags == 9) {
        loader->mHasPalette = 1;
        uint8_t clut[12];
        if (file.Read(clut, 12) != 12) return {-1};
        const uint32_t n = Be16(clut + 8) * Be16(clut + 10);
        const uint32_t bytes = Be32(clut) - 12;
        auto* p = new uint8_t[n * 2];
        // These three returns leak the array in the original.
        if (n > 256 || bytes != n * 2 || uint32_t(file.Read(p, bytes)) != bytes) return {-1};
        std::memset(loader->mPalette, 0, 0x400);
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t c = Be16(p + 2 * i);
            loader->mPalette[4 * i + 2] = (c >> 7) & 0xf8;
            loader->mPalette[4 * i + 1] = (c >> 2) & 0xf8;
            loader->mPalette[4 * i] = uint8_t(c << 3);
        }
        delete[] p;
    }
    mBottomUp = 0;
    uint8_t image[12];
    if (file.Read(image, 12) != 12) return {-1};
    const uint32_t w = Be16(image + 8), height = Be16(image + 10);
    if (Be32(image) - 12 != w * height * 2) return {-1};
    const uint32_t mode = flags & 7;
    if (mode == 2) {
        const TbPixFormat format(0x1f, 0x3e0, 0x7c00, 0, 16, 0);
        SetFormat_2(loader->mArea, format);
        loader->mArea.mWidth = w;
    } else if (mode < 2) {
        SetFormat_2(loader->mArea, TbPixFormat8);
        loader->mArea.mWidth = mode == 0 ? w << 2 : w << 1;
    } else {
        SetFormat_2(loader->mArea, TbPixFormat24);
        loader->mArea.mWidth = (w << 1) / 3;
    }
    loader->mArea.mStride = w << 1;
    loader->mArea.mPictureHeight = height;
    mWidth = loader->mArea.mWidth;
    mHeight = loader->mArea.mPictureHeight;
    mWordWidth = w;
    std::memcpy(mHeader, h, 8);
    return {0};
}
// 0x10022448
TbResult _TimGLL::ReadFirstLine(CFile& file) {
    mLinesLeft = mHeight;
    mLineBytes = mWordWidth << 1;
    return ReadNextLine(file);
}
// 0x100224d8
TbResult _TimGLL::ReadNextLine(CFile& file) {
    if (mLinesLeft-- == 0) return {-1};
    if (uint32_t(file.Read(mLine, mLineBytes)) != mLineBytes) return {-1};
    const uint32_t mode = Be32(mHeader + 4) & 7;
    // Host-only representation rule: 16-bit pixels are native words.
    if (mode == 2) {
        for (uint32_t i = 0; i < mLineBytes / 2; ++i) {
            const uint16_t value = uint16_t(Be16(mLine + i * 2));
            std::memcpy(mLine + i * 2, &value, 2);
        }
    }
    if (mode == 0) {
        for (uint32_t i = mLineBytes; i != 0; --i) {
            const uint8_t b = mLine[i - 1];
            mLine[2 * i - 1] = b >> 4;
            mLine[2 * i - 2] = b & 15;
        }
    }
    if (mode == 3) {
        for (uint32_t i = 0; i < mLineBytes / 3; ++i) {
            uint8_t* p = mLine + 3 * i;
            p[0] ^= p[2];
            p[2] ^= p[0];
            p[0] ^= p[2];
        }
    }
    return {0};
}
// 0x10022818
uint32_t _TimGLL::GetSourceLineLength() { return mWordWidth << ((Be32(mHeader + 4) & 7) != 0 ? 1 : 2); }

TbIFFLoader::TbIFFLoader() {}
TbIFFLoader::~TbIFFLoader() {}
TbGIFLoader::TbGIFLoader() {}
TbGIFLoader::~TbGIFLoader() {}
TbTIMLoader::TbTIMLoader() {}
TbTIMLoader::~TbTIMLoader() {}

// Unit v51127: small bullfrog functions (LbSCBack.cpp application globals, the _LbSEFunction list instances, CConverter and TbJPEGLoader
// destructors, SamsUtilities swap helpers, TbPoint, _LbSemaLock stand-ins). Mac addresses are in the comments.

// SamsUtilities::UBinary::RotateLeft<unsigned long> 0x10019f3c: rotlw.
namespace SamsUtilities {
void USwapBlock32_ulong_10044590(uint32_t* words, int32_t count) { _USwapBlock32(words, count); }
}  // namespace SamsUtilities

// TbGraphicFileLoader::TbGraphicFileLoader 0x10022870 / ~TbGraphicFileLoader 0x100228e0 as callees (weak: a test replaces them).
__attribute__((weak, noinline)) void TbJPEGLoader_BaseCtor(void* self) { new (self) TbGraphicFileLoader(); }
