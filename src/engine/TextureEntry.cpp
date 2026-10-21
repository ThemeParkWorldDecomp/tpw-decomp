// TextureEntry.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of TextureEntry code, named after it.

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_proctexture.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "Wavelet.h"
#include <cstring>

// tb_textureentry.cpp (engine library of SimTheme Park 2.04b3; the name is the one in its
// assertions, 0x10040d30..0x10041848; the inventory files the range under
// tb_printf_opengl.cpp): CTextureEntry.

CTextureEntry::CTextureEntry() {
    mPlace = &mOwnPlace;
    std::memset(mOwnPlace.mF00, 0, sizeof mOwnPlace.mF00);
    mOwnPlace.mF10 = 0;
    mOwnPlace.mF14 = 0;
    mFlags = 0;
    mHeight = 0;
    mWidth = 0;
    mCacheHeight = 0;
    mCacheWidth = 0;
    mTexture = nullptr;
    mVSize = 0.0f;
    mUSize = 0.0f;
    mVOffset = 0.0f;
    mUOffset = 0.0f;
    mData = nullptr;
    mF38 = nullptr;
    mPitch = 0;
    mNext = nullptr;
    mPage = nullptr;
    mPriority = 0.0f;
}

CTextureEntry::~CTextureEntry() { Close(); }

int32_t CTextureEntry::Create(int32_t size, int32_t flags) {
    mData = gMalloc(static_cast<uint32_t>(size * size * gTextureBytesPerPixel[gei_TextureSettings.mFormat]));
    if (mData == nullptr) {
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    mFlags = static_cast<uint32_t>(flags << 6) | kOwnsData;
    SetSize(size, size);
    mPitch = static_cast<int16_t>(size);
    return 1;
}

// A further plane of the same image (the next entry of the array) goes first; then the
// cache lets go of the entry and the image is freed, as pixels or as the object they are
// unpacked from.
int32_t CTextureEntry::Close() {
    if ((mFlags & kNextIsPlane) != 0) this[1].Close();
    if ((mFlags & kOnPage) != 0) gei_TCache->Remove(this);
    if ((mFlags & kCompressed) == 0) {
        if (mData != nullptr) gFree(mData);
    } else {
        auto* source = static_cast<CCompBase*>(mData);
        if (source != nullptr) delete source;
        mData = nullptr;
    }
    if (mF38 != nullptr) gFree(mF38);
    mFlags = 0;
    SetSize(0, 0);
    return 1;
}

uint32_t CTextureEntry::GetBlendingMode() {
    uint32_t mode = StoreAsWrapping() ? 0x400000 : 0;
    if ((mFlags & 0x4000000) != 0) mode |= 0x7000000;
    switch (mFlags & 0x300) {
    case 0x200: return mode | 0x556;
    case 0x100: return mode | 0x2756;
    case 0x300: return mode | 0x200552;
    default: return mode | 0x1256;
    }
}

int32_t CTextureEntry::BestSize(int32_t size) {
    if (size > 0x80) return 0;
    if (size > 0x40) return 1;
    if (size > 0x20) return 2;
    if (size > 0x10) return 3;
    if (size > 8) return 4;
    return 5;
}

int32_t CTextureEntry::GetBestSize() {
    if ((mFlags & 0x30) == 0x30) return static_cast<int32_t>(mFlags & kSizeMask);
    return BestSize(mCacheWidth);
}

void* CTextureEntry::GetData() {
    if ((mFlags & kCompressed) == 0) return mData;
    SUnpackDestination dest;
    dest.mData = gProcTextureBuffer;
    dest.mWidth = mWidth;
    dest.mHeight = mHeight;
    dest.mPitch = mWidth * gTextureBytesPerPixel[gei_TextureSettings.mFormat];
    static_cast<CCompBase*>(mData)->Decompress(&dest);
    return gProcTextureBuffer;
}

// Rows are swapped eight bytes at a time (what is left of a row whose length is not a
// multiple of eight stays), and the partner of row i is counted from the WIDTH, as in the
// original: right for the square images the engine uses.
void* CTextureEntry::GetCorrectedData() {
    auto* data = static_cast<uint8_t*>(GetData());
    bool flipped = (mFlags & kFlipped) != 0;
    if ((mFlags & kCompressed) != 0 && static_cast<CCompBase*>(mData)->IsNaturallyFlipped()) flipped = !flipped;
    if (!flipped) return data;
    const uint32_t rowBytes = static_cast<uint32_t>(gTextureBytesPerPixel[gei_TextureSettings.mFormat] * mWidth);
    const uint32_t swapped = (rowBytes >> 3) * 8;
    for (int32_t row = 0; row < mHeight >> 1; ++row) {
        uint8_t* upper = data + static_cast<size_t>(row) * rowBytes;
        uint8_t* lower = data + static_cast<intptr_t>(rowBytes) * (mWidth - row - 1);
        for (uint32_t i = 0; i < swapped; ++i) {
            const uint8_t byte = upper[i];
            upper[i] = lower[i];
            lower[i] = byte;
        }
    }
    return data;
}

void CTextureEntry::SetSize(int32_t width, int32_t height) {
    mFlags &= ~static_cast<uint32_t>(kSizeMask);
    mCacheWidth = static_cast<int16_t>(width);
    mWidth = static_cast<int16_t>(width);
    mCacheHeight = static_cast<int16_t>(height);
    mHeight = static_cast<int16_t>(height);
    int16_t larger = mHeight;
    if (larger < mWidth) larger = mWidth;
    mFlags &= ~3u;
    mFlags |= static_cast<uint32_t>(BestSize(larger));
    mSortKey = static_cast<int32_t>((static_cast<uint32_t>(ppc_fctiwz(2047.0f * (1.5f - mPriority))) & 0xfff) << 16);
    mSortKey = mCacheWidth * 0x100 + mCacheHeight + mSortKey;
    if ((mFlags & kLocked) != 0) mSortKey |= 0x20000000;
    if ((mFlags & 0x80000) != 0) mSortKey |= 0x40000000;
}

void CTextureEntry::FlushEntryFlags() {
    if ((mFlags & kLocked) != 0 && (gei_System.mFlags & 0xc0000000u) == 0) {
        msl_assertion_failed("(!IsLocked()) || (gei_System.SceneChanged())", "tb_textureentry.cpp", 0x16d);
    }
    mFlags &= ~0x7000u;
}

void CTextureEntry::Lock() {
    if ((gei_System.mFlags & 0xc0000000u) == 0) {
        msl_assertion_failed("gei_System.SceneChanged()", "tb_textureentry.cpp", 0x185);
    }
    mFlags |= kLocked;
}

void CTextureEntry::Unlock() {
    if ((gei_System.mFlags & 0xc0000000u) == 0) {
        msl_assertion_failed("gei_System.SceneChanged()", "tb_textureentry.cpp", 0x18d);
    }
    mFlags &= ~static_cast<uint32_t>(kLocked);
}

int32_t CTextureEntry::StoreAsWrapping() { return (mFlags & kWrapAny) != 0 && gei_System.mRenderer != 4; }
