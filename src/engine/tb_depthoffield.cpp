// tb_depthoffield.cpp: name proven by __sinit_tb_depthoffield_cpp in the engine library.

#include "helpers/TbTextFormatterHelpers.h"
#include "MapWho.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_proctexture.h"
#include "tb_rendermemory.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "TbTextFormatter.h"
#include "Wavelet.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <stdexcept>
#include <utility>

// tb_decode_wavelet.cpp (engine library of SimTheme Park 2.04b3; the name is the one in
// its assertions, the inventory files the range under tb_depthoffield.cpp): the engine's
// reference to the wavelet decompressor. PARTIAL: CWaveLoad (0x10005de4..0x100067b8), the
// loader that uses it, is still to do.

CWavelet* gWaveLoadWavelet = nullptr;

int32_t InitWaveLoad() {
    if (gProcTextureBuffer != nullptr) {
        msl_assertion_failed("0", "tb_decode_wavelet.cpp", 0x29);
        return 0;
    }
    gWaveLoadWavelet = CWavelet::Instance();
    if (gWaveLoadWavelet == nullptr) {
        msl_assertion_failed("0", "tb_decode_wavelet.cpp", 0x3a);
        return 0;
    }
    gProcTextureBuffer = gei_RenderMemory->LoadInfo(CRenderMemory::kDynamic)->mData + 0x400;
    return 1;
}

int32_t CloseWaveLoad() {
    if (gWaveLoadWavelet == nullptr) return 0;
    gWaveLoadWavelet->Release();
    gWaveLoadWavelet = nullptr;
    return 1;
}

// CWaveLoad and CCompBase (engine library of SimTheme Park 2.04b3; the original file is
// tb_decode_wavelet.cpp by its assertions, 0x100051d0, 0x10005bf8 and 0x10005de4..
// 0x100067b8; kept apart from the wave loader's start-up in src/tb_decode_wavelet.cpp so
// a test library can link one without the other). PARTIAL: LoadSprite (0x10006260) is
// still to do.

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

// tb_sprite.cpp, the engine's sprite files (engine library of SimTheme Park 2.04b3; the file
// name is the one in its assertions; functions at 0x1002da98..0x1002f398 and 0x1003108c..
// 0x10031378, with the RLE loader of tb_decode_rle.cpp at 0x10005104..0x10005368 and the
// wavelet sprite reader of tb_decode_wavelet.cpp at 0x10006260; the inventory files them
// under tb_starfield.cpp and tb_depthoffield.cpp): the sprite file reader, a sprite's engine
// data and texture entry, a sprite instance's used part, the list of instances, and the two
// loaders a sprite's texture unpacks from. The file format is described in
// include/sprite_engine.h.
//
// PARTIAL, by design of the unit: the sprite's TGA loader (Init(char*)), the partial texture
// generator and the sprite drawing are other units; CSpriteList::Close, Prepare, GetLinkType and
// GetData, and the unpacking members of CRLELoad are untranslated.
//
// Each `new` is the original's operator new[] / operator new followed by the constructor loop or
// constructor; a null answer from it is dead code here (the runtime's new throws) and is kept
// where the original tests it.
//
// msl_assertion_failed calls are the original's failed asserts (platform/msl.h).

extern SSpriteWaveHeader gSpriteWaveHeader;

// ---- CWaveLoad ----------------------------------------------------------------------------------------

// The wavelet sprite's header is read raw (see sprite_engine.h) into engine data; the loader
// keeps the sizes, the four scales turned into the integers its decompressor starts from, and
// the two blocks. Only these fields are set: the version, the threshold, the level and the
// average colour stay as the constructor left them, and the alpha flag is always on.
int32_t CWaveLoad::LoadSprite(void* file) {
    gFileRead(&gSpriteWaveHeader, 0x2c, 1, file);
    mF06 = 0x20;
    mWidth = gSpriteWaveHeader.mWidth;
    mHeight = gSpriteWaveHeader.mHeight;
    mQuality[0] = static_cast<uint16_t>(ppc_fctiwz(ppc_fmsubs(2.0f, gSpriteWaveHeader.mQuality[0], 2.0f)));
    mQuality[2] = static_cast<uint16_t>(ppc_fctiwz(ppc_fmsubs(4.0f, gSpriteWaveHeader.mQuality[1], 4.0f)));
    mQuality[1] = static_cast<uint16_t>(ppc_fctiwz(ppc_fmsubs(4.0f, gSpriteWaveHeader.mQuality[2], 4.0f)));
    mQuality[3] = static_cast<uint16_t>(ppc_fctiwz(gSpriteWaveHeader.mQuality[3] - 1.0f));
    mDataSize = gSpriteWaveHeader.mDataSize;
    mAlphaSize = gSpriteWaveHeader.mAlphaSize;
    mData = static_cast<uint8_t*>(gMalloc(mDataSize));
    if (mData == nullptr) return 0;
    gFileRead(mData, 1, mDataSize, file);
    mAlphaData = static_cast<uint8_t*>(gMalloc(mAlphaSize));
    if (mAlphaData == nullptr) {
        gFree(reinterpret_cast<void*&>(mData));
        return 0;
    }
    gFileRead(mAlphaData, 1, mAlphaSize, file);
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

// ltms, the rest of the text formatter's callees (unit w44): the Metrowerks std::vector members of the formatter's three vectors that
// ltms_textformatter.cpp (unit w35a) called as stand-ins, and the line processor.
//   MslVector<CCharacterInfo>: clear 0x10007d74, push_back 0x10007e0c
//   MslVector<CLineInfo>:      clear 0x10008454, push_back 0x100084e4
//   MslVector<TbPoint>:        clear 0x1000884c, resize 0x100088d4 with erase 0x10008be0 and insert 0x10008cf4
//   ProcessLine 0x10006cfc and FixParenthesis 0x1000691c (file-static in the original)
// The vector is three words: capacity, size and the data. clear only drops the size (the original's loop over the elements does nothing:
// they have no destructors). A vector that is full grows by doubling from 1 (new block, copy, delete the old one).

// tb_depthoffield.cpp static data at 0x10052b08: ten floats (100, then three (sin, cos, 0) triples).
float gDepthOfFieldTable[10];

// 0x10006864 (192 B): the file's static initialiser.
void __sinit_tb_depthoffield_cpp() {
    float* t = gDepthOfFieldTable;
    t[0] = 100.0f;
    t[1] = static_cast<float>(std::sin(0.0));
    t[2] = static_cast<float>(std::cos(0.0));
    float zero = 0.0f;
    t[3] = zero;
    t[4] = static_cast<float>(std::sin(0.0));
    t[5] = static_cast<float>(std::cos(0.0));
    t[6] = zero;
    t[7] = static_cast<float>(std::sin(3.1415927410125732));
    t[8] = static_cast<float>(std::cos(3.1415927410125732));
    t[9] = zero;
}
