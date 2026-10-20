// IFF, GIF and TIM readers of Lbgfsave.cpp (bullfrog symbols shared), unit w223.
#include "TbGraphicFile.h"
#include <cstring>
#include <new>
#include "LbFile.h"
#include "bfl_FileSystem.h"
using bfl::FileSystem::CFile;
namespace {
bool Succeeded(const TbResult& r) { return r.mCode >= 0; }
void SetFormat(TbRenderArea& a, const TbPixFormat& f) {
    std::memcpy(a.mF00, &f.mRedMask, 4);
    std::memcpy(a.mF00 + 4, &f.mGreenMask, 4);
    std::memcpy(a.mF00 + 8, &f.mBlueMask, 4);
    std::memcpy(a.mF00 + 12, &f.mAlphaMask, 4);
    a.mDepth = f.mBitsPerPixel;
    a.mPalettised = f.mPalettised;
}
uint32_t Be16(const uint8_t* p) { return (uint32_t(p[0]) << 8) | p[1]; }
uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
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
bool IffMagic(const uint8_t* h) {
    return Be32(h) == 0x4d524f46 && (Be32(h + 8) == 0x204d4250 || Be32(h + 8) == 0x4d424c49) && Be32(h + 12) == 0x44484d42;
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
    if (!Succeeded(ReadHeaderInfo(file, loader))) return {-1};
    return DoRead(area, file, loader);
}
// 0x1001f184
TbResult _AndyIFFGLL::ReadHeaderInfo(CFile& file, TbIFFLoader* loader) {
    uint8_t* h = loader->mHeader;
    if (file.Read(h, 0x28) != 0x28 || !IffMagic(h) || h[0x1c] != 8) return {-1};
    loader->mArea.mWidth = Le16(h + 0x14);
    loader->mArea.mStride = loader->mArea.mWidth;
    loader->mArea.mPictureHeight = Le16(h + 0x16);
    SetFormat(loader->mArea, TbPixFormat8);
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

// 0x100202e8
TbResult _AndyGIFGLL::ReadData(const TbRenderArea& area, CFile& file, TbGIFLoader* loader) {
    if (!Succeeded(ReadHeaderInfo(file, loader))) return {-1};
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
    SetFormat(loader->mArea, TbPixFormat8);
    loader->mHasPalette = 0;
    if (h[10] & 0x80) {
        if (!Succeeded(_GifReadPalette(file, mPalette))) return {-1};
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
        if (!Succeeded(_GifReadPalette(file, mPalette))) return {-1};
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

// 0x10021d70
TbResult _TimGLL::ReadData(const TbRenderArea& area, CFile& file, TbTIMLoader* loader) {
    if (!Succeeded(ReadHeaderInfo(file, loader))) return {-1};
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
        SetFormat(loader->mArea, format);
        loader->mArea.mWidth = w;
    } else if (mode < 2) {
        SetFormat(loader->mArea, TbPixFormat8);
        loader->mArea.mWidth = mode == 0 ? w << 2 : w << 1;
    } else {
        SetFormat(loader->mArea, TbPixFormat24);
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
// 0x100233b0
TbResult TbIFFLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _AndyIFFGLL r; return r.ReadHeaderInfo(f, this); }
// 0x10023454
TbResult TbIFFLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _AndyIFFGLL r; return r.ReadData(a, f, this); }
// 0x10023500
bool TbIFFLoader::ValidImage(CFile& f) { uint8_t h[40]; return uint32_t(f.GetSize()) >= 40 && f.Read(h, 40) == 40 && IffMagic(h); }
// 0x10023850
TbResult TbGIFLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _AndyGIFGLL r; return r.ReadHeaderInfo(f, this); }
// 0x100238f4
TbResult TbGIFLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _AndyGIFGLL r; return r.ReadData(a, f, this); }
// 0x100239a0
bool TbGIFLoader::ValidImage(CFile& f) { char h[3]; return f.GetSize() >= 3 && f.Read(h, 3) == 3 && std::strncmp(h, "GIF", 3) == 0; }
// 0x10023f7c
TbResult TbTIMLoader::ReadHeaderInfo(CFile& f, TbRenderArea&) { _TimGLL r; return r.ReadHeaderInfo(f, this); }
// 0x10024020
TbResult TbTIMLoader::ReadData(const TbRenderArea& a, CFile& f, TbRenderArea&) { _TimGLL r; return r.ReadData(a, f, this); }
// 0x100240cc
bool TbTIMLoader::ValidImage(CFile& f) { uint8_t h[4]; return f.GetSize() >= 4 && f.Read(h, 4) == 4 && Be32(h) == 0x10; }
