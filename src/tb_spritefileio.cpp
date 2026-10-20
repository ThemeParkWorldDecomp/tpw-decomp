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
#include "sprite_engine.h"

#include <cstddef>
#include <cstdint>

#include "MapWho.h"
#include "Wavelet.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_decode_wavelet.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

SSpriteWaveHeader gSpriteWaveHeader;

namespace {

// The pixel formats the palette is converted between (SamsUtilities::UPixels): the file's
// 32-bit BGRA entries to the texture format's.
constexpr intptr_t kPaletteSourceFormat = SamsUtilities::kPixels32BGRA;

// The partial textures of a sprite (the array mTexture points at when the sprite has a pixel
// block): each entry that the cache placed comes off its page, each lets go of its pixels
// (they belong to the block), and the array is destroyed. The original repeats these lines in
// RefreshPartialTexture, Close, SetUsedPosition and SetUsedSize.
void FreePartialTextures(SSpriteEngineData* sprite) {
    for (int32_t i = 0; i < sprite->mPartCount; ++i) {
        CTextureEntry* entry = &sprite->mTexture[i];
        if ((entry->mFlags & CTextureEntry::kOnPage) != 0) gei_TCache->Remove(entry);
        sprite->mTexture[i].mData = nullptr;
    }
    delete[] sprite->mTexture;
    sprite->mTexture = nullptr;
    sprite->mPartCount = 0;
}

// The sprite's aspect: width over height, 0.0 without a height.
float AspectOf(uint16_t width, uint16_t height) {
    if (height == 0) return 0.0f;
    return static_cast<float>(width) / static_cast<float>(height);
}

}  // namespace

// ---- CSpriteFileIO ------------------------------------------------------------------------------

int32_t CSpriteFileIO::Load(CSpriteFileIO* io, void* file) {
    int16_t kind = 0;
    int16_t unused = 0;
    gFileRead(&kind, 2, 1, file);
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(&kind), 1);
    gFileRead(&unused, 2, 1, file);
    SamsUtilities::_USwapBlock16(reinterpret_cast<uint16_t*>(&unused), 1);
    if (kind < 2) msl_assertion_failed("io_version>1", "tb_sprite.cpp", 0x37);
    gFileRead(&io->mCount, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&io->mCount), 1);
    // The entries first, then the sprites, each as an array made and constructed in one go.
    io->mTextures = new CTextureEntry[static_cast<std::size_t>(io->mCount)];
    io->mSprites = new SSpriteEngineData[static_cast<std::size_t>(io->mCount)];
    if (io->mSprites == nullptr || io->mTextures == nullptr) {   // dead: the runtime's new throws
        io->Free();
        gFileClose(file);
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    SSpriteEngineData* sprite = io->mSprites;
    CTextureEntry* texture = io->mTextures;
    if (kind == 3) {
        io->mPalette = gMalloc(0x400);
        if (io->mPalette == nullptr) {   // the engine's allocator may refuse
            io->Free();
            gFileClose(file);
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            return 0;
        }
        gFileRead(io->mPalette, 4, 0x100, file);
        // The palette becomes the texture format's: 32 bits for format 5, 16 for format 4, and for
        // any other a format number the converter has no implementation for.
        const int32_t format = gei_TextureSettings.mFormat;
        intptr_t paletteFormat = 13;
        if (format == 5) paletteFormat = SamsUtilities::kPixels32RGBA;
        if (format == 4) paletteFormat = SamsUtilities::kPixels16RGBA;
        // Both converters live until the pictures are loaded (destroyed destination first).
        SamsUtilities::UPixels source(kPaletteSourceFormat, io->mPalette, 0x100, 1, -1, nullptr);
        SamsUtilities::UPixels dest(paletteFormat, io->mPalette, 0x100, 1, -1, nullptr);
        dest.Convert(source, false);
        for (int32_t i = 0; i < io->mCount; ++i) {
            if (sprite->Load_RLE(file, io->mPalette, texture) == 0) {
                // dead: the loader is only refused without memory. The original destroys the
                // array element and then deletes it (flag 1), and carries on with a null pointer.
                delete sprite;
                sprite = nullptr;
            }
            ++sprite;
            ++texture;
        }
    } else if (kind == 2) {
        for (int32_t i = 0; i < io->mCount; ++i) {
            if (sprite->Load_WCT(file, texture) == 0) {
                delete sprite;   // dead, as above
                sprite = nullptr;
            }
            ++sprite;
            ++texture;
        }
    }
    gFileClose(file);
    if ((gei_System.mFlags & 1) != 0) gei_Splash.InterpolateSplash();
    return 1;
}

int32_t CSpriteFileIO::Free() {
    if (mTextures != nullptr) {
        delete[] mTextures;
        mTextures = nullptr;
    }
    if (mSprites != nullptr) {
        // The sprites point into the array just destroyed: they must not close those entries.
        for (int32_t i = 0; i < mCount; ++i) mSprites[i].mTexture = nullptr;
        delete[] mSprites;
        mSprites = nullptr;
    }
    if (mPalette != nullptr) gFree(mPalette);
    mCount = 0;
    return 1;
}

void CSpriteFileIO::FlagAsSemitransparent() {
    for (int32_t i = 0; i < mCount; ++i) {
        CTextureEntry* texture = mSprites[i].mTexture;
        texture->mFlags = (texture->mFlags & ~0x300u) | 0x200u;
    }
}

// ---- SSpriteEngineData ----------------------------------------------------------------------------

SSpriteEngineData::SSpriteEngineData()
    : mTexture(nullptr), mData(nullptr), mWidth(0), mHeight(0), mUsedWidth(0), mUsedHeight(0), mOffsetX(0) {
    mPartCount = 0;
}

SSpriteEngineData::~SSpriteEngineData() { Close(); }

int32_t SSpriteEngineData::CreateTextureEntry(void* image, void* alpha, CCompBase* source, int32_t width,
                                              int32_t height, CTextureEntry* entry, int32_t flags) {
    if (entry == nullptr) {
        mTexture = new CTextureEntry[1];
        if (mTexture == nullptr) {   // dead: the runtime's new throws
            gei_ErrorHandler.Push(2, kErrOutOfMemory);
            return 0;
        }
    } else {
        mTexture = entry;
    }
    mTexture->mData = source != nullptr ? static_cast<void*>(source) : image;
    // An alpha image goes in the entry's own allocation slot, with flag 0x400.
    mTexture->mF38 = alpha;
    uint32_t kind = 0x1000040;
    if (alpha != nullptr) kind = 0x1000440;
    mTexture->mFlags = static_cast<uint32_t>(flags) | kind | 0x100;
    mTexture->SetSize(width, height);
    mTexture->mPitch = static_cast<int16_t>(width);
    return 1;
}

int32_t SSpriteEngineData::Load_WCT(void* file, CTextureEntry* entry) {
    CWaveLoad* wave = new CWaveLoad;
    if (wave == nullptr) {   // dead: the runtime's new throws
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    wave->LoadSprite(file);   // the answer is not looked at: a file that ran short leaves the loader empty
    // The sizes come from the header the reader left in engine data, not from the loader.
    mWidth = gSpriteWaveHeader.mWidth;
    mHeight = gSpriteWaveHeader.mHeight;
    mUsedWidth = gSpriteWaveHeader.mUsedWidth;
    mUsedHeight = gSpriteWaveHeader.mUsedHeight;
    mOffsetX = static_cast<int16_t>(gSpriteWaveHeader.mOffsetX);
    mOffsetY = static_cast<int16_t>(gSpriteWaveHeader.mOffsetY);
    mAspect = AspectOf(mWidth, mHeight);
    if (CreateTextureEntry(nullptr, nullptr, wave, mWidth, mHeight, entry, 0x10200000) == 0) {
        delete wave;
        return 0;
    }
    return 1;
}

int32_t SSpriteEngineData::Load_RLE(void* file, void* palette, CTextureEntry* entry) {
    CRLELoad* rle = new CRLELoad;
    if (rle == nullptr) {   // dead: the runtime's new throws
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    // A read that runs past the end of the file leaves these as they were; the original's two
    // words are not even cleared first (stack), so the tests never let a read of one come up short.
    uint32_t size = 0;
    uint32_t word = 0;
    gFileRead(&size, 4, 1, file);
    SamsUtilities::_USwapBlock32(&size, 1);
    gFileRead(&mWidth, 2, 1, file);
    SamsUtilities::_USwapBlock16(&mWidth, 1);
    gFileRead(&mHeight, 2, 1, file);
    SamsUtilities::_USwapBlock16(&mHeight, 1);
    gFileRead(&mUsedWidth, 2, 1, file);
    SamsUtilities::_USwapBlock16(&mUsedWidth, 1);
    gFileRead(&mUsedHeight, 2, 1, file);
    SamsUtilities::_USwapBlock16(&mUsedHeight, 1);
    gFileRead(&word, 4, 1, file);
    SamsUtilities::_USwapBlock32(&word, 1);
    mOffsetX = static_cast<int16_t>(word);
    gFileRead(&word, 4, 1, file);
    SamsUtilities::_USwapBlock32(&word, 1);
    mOffsetY = static_cast<int16_t>(word);
    rle->LoadSprite(file, static_cast<int32_t>(size));   // the answer is not looked at
    rle->SetSize(mWidth, mHeight);
    rle->SetPalette(palette);
    mAspect = AspectOf(mWidth, mHeight);
    if (CreateTextureEntry(nullptr, nullptr, rle, mWidth, mHeight, entry, 0x10200000) == 0) {
        delete rle;
        return 0;
    }
    return 1;
}

int32_t SSpriteEngineData::Init(int32_t width, int32_t height) {
    mWidth = static_cast<uint16_t>(width);
    mHeight = static_cast<uint16_t>(height);
    const uint32_t bytesPerPixel = static_cast<uint32_t>(gTextureBytesPerPixel[gei_TextureSettings.mFormat]);
    mData = gMalloc(bytesPerPixel * (static_cast<uint32_t>(mWidth) * static_cast<uint32_t>(mHeight)));
    if (mData == nullptr) {
        msl_assertion_failed("0", "tb_sprite.cpp", 0x1cc);
        gei_ErrorHandler.Push(2, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t SSpriteEngineData::RefreshPartialTexture() {
    if (mTexture != nullptr && mPartCount != 0) FreePartialTextures(this);
    return 1;
}

// A sprite from a file or a TGA closes its texture entry (an array of its own, or its place in
// the file's array: only the first is safe to destroy here, which is why CSpriteFileIO::Free clears
// the pointers first); a sprite with a pixel block lets go of its partial textures and the block.
int32_t SSpriteEngineData::Close() {
    if (mData == nullptr) {
        if (mTexture != nullptr) {
            mTexture->Close();
            delete[] mTexture;
            mTexture = nullptr;
        }
    } else {
        if (mTexture != nullptr && mPartCount != 0) FreePartialTextures(this);
        gFree(mData);
    }
    return 1;
}

// ---- SSpriteGameData --------------------------------------------------------------------------------

// The Mac's word at 0x34 is one 32-bit pointer (a regular sprite's frame file) or the two halfwords of the used position, and the
// constructor zeroes all of it; the host's pointer is wider than the two halfwords, so it is cleared whole.
SSpriteGameData::SSpriteGameData()
    : mF10(0), mF24(-1), mSprite(nullptr), mF2C(1.0f), mUsedWidth(0), mUsedHeight(0), mUsedX(0), mUsedY(0) {
    mFrameFile = nullptr;
}

void SSpriteGameData::SetUsedPosition(int32_t x, int32_t y) {
    if ((mUsedX != x || mUsedY != y) && mSprite != nullptr) {
        SSpriteEngineData* sprite = mSprite;
        if (sprite->mTexture != nullptr && sprite->mPartCount != 0) FreePartialTextures(sprite);
    }
    mUsedX = static_cast<int16_t>(x);
    mUsedY = static_cast<int16_t>(y);
}

// The test of a change compares the used width with both arguments (the height is never looked
// at), and unlike SetUsedPosition it does not look for a texture before it counts the partial ones.
void SSpriteGameData::SetUsedSize(int32_t width, int32_t height) {
    if ((mUsedWidth != width || mUsedWidth != height) && mSprite != nullptr) {
        SSpriteEngineData* sprite = mSprite;
        if (sprite->mPartCount != 0) FreePartialTextures(sprite);
    }
    mUsedWidth = static_cast<int16_t>(width);
    mUsedHeight = static_cast<int16_t>(height);
}

// ---- CSpriteList --------------------------------------------------------------------------------------

CSpriteList::CSpriteList()
    : mCount(0), mInstances(nullptr), mCapacity(0), mFrameA(-1), mFrameB(0), mF18(0) {
    RegisterLinkBase(this);
}

CSpriteList::~CSpriteList() {
    Close();
    FreeLinkBase(this);
}

SSpriteGameData** CSpriteList::Init(int32_t count) {
    mInstances = static_cast<SSpriteGameData**>(
        gMalloc(static_cast<uint32_t>(static_cast<uint32_t>(count) * sizeof(SSpriteGameData*))));
    if (mInstances == nullptr) return nullptr;
    mCapacity = count;
    return mInstances;
}

// ---- CRLELoad -----------------------------------------------------------------------------------------

CRLELoad::CRLELoad() {
    SetSize(0, 0);
    SetPalette(nullptr);
    mData = nullptr;
}

void CRLELoad::SetSize(int32_t width, int32_t height) {
    mWidth = width;
    mHeight = height;
}

void CRLELoad::SetPalette(void* palette) { mPalette = palette; }

int32_t CRLELoad::LoadSprite(void* file, int32_t size) {
    if (mData != nullptr) msl_assertion_failed("!mpv_Data", "tb_decode_rle.cpp", 0x2b);
    mData = gMalloc(static_cast<uint32_t>(size));
    if (mData == nullptr) return 0;
    gFileRead(mData, 1, static_cast<uint32_t>(size), file);
    return 1;
}

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
