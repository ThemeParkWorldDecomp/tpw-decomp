// The BMP and PCX picture loaders of Lbgfsave.cpp (bullfrog symbols shared), unit w220.
// Succeeded, SetFormat and BytesOfPixel copy the local helpers of bfl_graphicfile.cpp, lines 26..44.
#include "TbGraphicFile.h"

#include <cstring>

#include "LbFile.h"
#include "TbPixFmt.h"
#include "bfl_FileSystem.h"

using bfl::FileSystem::CFile;

namespace {

bool Succeeded(const TbResult& result) { return result.mCode >= 0; }

void SetFormat(TbRenderArea& area, const TbPixFormat& format) {
    std::memcpy(area.mF00 + 0, &format.mRedMask, 4);
    std::memcpy(area.mF00 + 4, &format.mGreenMask, 4);
    std::memcpy(area.mF00 + 8, &format.mBlueMask, 4);
    std::memcpy(area.mF00 + 12, &format.mAlphaMask, 4);
    area.mDepth = format.mBitsPerPixel;
    area.mPalettised = format.mPalettised;
}

uint32_t BytesOfPixel(int32_t depth) { return (static_cast<uint32_t>(depth) + 7) >> 3; }

uint32_t Be16(const uint8_t* p) { return (uint32_t(p[0]) << 8) | p[1]; }

uint32_t Be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

}  // namespace

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

// The constructors and destructors are host only.
TbBMPLoader::TbBMPLoader() {}
TbBMPLoader::~TbBMPLoader() {}

// 0x10023190
TbResult TbBMPLoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _AndyBMPGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

// 0x10023234
TbResult TbBMPLoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _AndyBMPGLL reader;
    return reader.ReadData(area, file, this);
}

// 0x100232e0
bool TbBMPLoader::ValidImage(CFile& file) {
    if (file.GetSize() < 2) return false;
    uint8_t magic[2];
    if (static_cast<uint32_t>(file.Read(magic, 2)) != 2) return false;
    return Be16(magic) == 0x4d42;
}

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

// 0x10023630
TbResult TbPCXLoader::ReadHeaderInfo(CFile& file, TbRenderArea&) {
    _AndyPCXGLL reader;
    return reader.ReadHeaderInfo(file, this);
}

// 0x100236d4
TbResult TbPCXLoader::ReadData(const TbRenderArea& area, CFile& file, TbRenderArea&) {
    _AndyPCXGLL reader;
    return reader.ReadData(area, file, this);
}

// 0x10023780
bool TbPCXLoader::ValidImage(CFile& file) {
    if (file.GetSize() < 1) return false;
    uint8_t magic;
    if (static_cast<uint32_t>(file.Read(&magic, 1)) != 1) return false;
    return magic == 0xa;
}
