// CWaveLoad and CCompBase (engine library of SimTheme Park 2.04b3; the original file is
// tb_decode_wavelet.cpp by its assertions, 0x100051d0, 0x10005bf8 and 0x10005de4..
// 0x100067b8; kept apart from the wave loader's start-up in src/tb_decode_wavelet.cpp so
// a test library can link one without the other). PARTIAL: LoadSprite (0x10006260) is
// still to do.
#include "tb_decode_wavelet.h"

#include <cstring>

#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "tb_loaders.h"
#include "tb_resource_texture.h"

CCompBase::~CCompBase() {}

int32_t CCompBase::IsNaturallyFlipped() { return 0; }

CWaveLoad::CWaveLoad() {
    std::memset(static_cast<SWaveHeader*>(this), 0, sizeof(SWaveHeader));
    mData = nullptr;
    mAlphaData = nullptr;
}

CWaveLoad::~CWaveLoad() { Close(); }

int32_t CWaveLoad::LoadTexture(char* name) {
    void* file = gFileOpen(name, "rb");
    if (file == nullptr) return 0;
    gFileRead(static_cast<SWaveHeader*>(this), sizeof(SWaveHeader), 1, file);
    // The file is little-endian: the Mac swapped these, a little-endian host has them right.
    SamsUtilities::_USwapBlock16(&mWidth, 6);
    SamsUtilities::_USwapBlock32(&mDataSize, 2);
    // The average colour is the one field the Mac did NOT swap: it used the four bytes in
    // file order as a big-endian word, and so does every host.
    const auto* colour = reinterpret_cast<const uint8_t*>(&mAverageColour);
    mAverageColour = static_cast<uint32_t>(colour[0]) << 24 | static_cast<uint32_t>(colour[1]) << 16 |
                     static_cast<uint32_t>(colour[2]) << 8 | colour[3];
    if (!(mVersion > 0x11 && mVersion < 0x14 && mWidth != 0 && mHeight != 0)) {
        msl_assertion_failed(
            "(ms_Header.version>=WCT_VERSION_MIN) && (ms_Header.version<=WCT_VERSION_MAX) && "
            "(ms_Header.width>0) && (ms_Header.height>0)",
            "tb_decode_wavelet.cpp", 0x75);
    }
    if (mVersion < 0x12 || mVersion > 0x13) {
        gFileClose(file);
        return 0;
    }
    mData = static_cast<uint8_t*>(gMalloc(mDataSize));
    if (mData == nullptr) {
        gFileClose(file);
        return 0;
    }
    gFileRead(mData, 1, mDataSize, file);
    if (mF06 == 0x20) {
        mAlphaData = static_cast<uint8_t*>(gMalloc(mAlphaSize));
        if (mAlphaData == nullptr) {
            gFileClose(file);
            gFree(reinterpret_cast<void*&>(mData));
            return 0;
        }
        gFileRead(mAlphaData, 1, mAlphaSize, file);
    }
    mLevel &= 3;
    gFileClose(file);
    return 1;
}

int32_t CWaveLoad::Close() {
    if (mAlphaData != nullptr) gFree(reinterpret_cast<void*&>(mAlphaData));
    if (mData != nullptr) gFree(reinterpret_cast<void*&>(mData));
    std::memset(static_cast<SWaveHeader*>(this), 0, sizeof(SWaveHeader));
    return 1;
}

// The decompressor takes the file's four scales as floats: half the first plus one, a
// quarter of the third and of the second plus one, the fourth plus one.
int32_t CWaveLoad::Decompress(SUnpackDestination* dest) {
    if (dest->mData == nullptr) msl_assertion_failed("dest->pv_Data", "tb_decode_wavelet.cpp", 0xfe);
    gWaveLoadWavelet->Decompress(mVersion, mWidth, mHeight, gTextureBytesPerPixel[gei_TextureSettings.mFormat], mData,
                                 mAlphaData, dest->mData, ppc_fmadds(0.5f, static_cast<float>(mQuality[0]), 1.0f),
                                 ppc_fmadds(0.25f, static_cast<float>(mQuality[2]), 1.0f),
                                 ppc_fmadds(0.25f, static_cast<float>(mQuality[1]), 1.0f),
                                 1.0f + static_cast<float>(mQuality[3]));
    return 1;
}

uint32_t CWaveLoad::AverageColor() { return mAverageColour; }

// Unpacks into a destination of its own size and row length; `flip` turns it upside down
// (the destination then starts at its last row and steps backwards). Version 0x12 files
// cannot do this.
int32_t CWaveLoad::Blit(SUnpackDestination* dest, int32_t flip) {
    if (mVersion < 0x13) return 0;
    if (flip != 0) {
        const int32_t pitch = dest->mPitch;
        dest->mPitch = -pitch;
        dest->mData = static_cast<uint8_t*>(dest->mData) + static_cast<intptr_t>(pitch) * (dest->mHeight - 1);
    }
    gWaveLoadWavelet->Decompress(mVersion, mWidth, mHeight, dest->mWidth, dest->mHeight, dest->mPitch,
                                 gTextureBytesPerPixel[gei_TextureSettings.mFormat], mData, mAlphaData, dest->mData,
                                 ppc_fmadds(0.5f, static_cast<float>(mQuality[0]), 1.0f),
                                 ppc_fmadds(0.25f, static_cast<float>(mQuality[2]), 1.0f),
                                 ppc_fmadds(0.25f, static_cast<float>(mQuality[1]), 1.0f),
                                 1.0f + static_cast<float>(mQuality[3]));
    return 1;
}

int32_t CWaveLoad::IsNaturallyFlipped() { return mVersion == 0x12; }
