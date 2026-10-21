// SpriteData.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of SpriteData code, named after it.

#include "m3d2.h"
#include "MapWho.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "sprite_engine.h"
#include "tb_decode_wavelet.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_loaders.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_shadow.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "Wavelet.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

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

// The sprite classes' remaining members (engine library of SimTheme Park 2.04b3): CSpriteList's Close,
// Prepare, GetData and GetLinkType (0x10031408, 0x10031488, 0x10031fb0, 0x10031fe4; the original file is
// tb_spritelist.cpp by its assertions, the inventory files it under tb_starfield.cpp), SSpriteEngineData's
// Init(char*), SetUsedPart and the partial texture generator and destroyer (0x1002e810, 0x1002eb00,
// 0x1002ebb8, 0x1002ee28; tb_sprite.cpp) and CSpriteFileIO::Load(char*, int) (0x1002dea0). The rest of
// these classes is in src/tb_spritefileio.cpp.
//
// Each `new` is the original's operator new[] followed by the constructor loop; a null answer from it is
// dead code here (the runtime's new throws) and is kept where the original tests it.

namespace {

// Pushed when a sprite file cannot be opened (the neighbour of kErrOutOfMemory, 0xb0754000).
constexpr int32_t kErrSpriteFileOpen = static_cast<int32_t>(0xb0754001);

// A signed halfway value rounded toward zero: the processor's srawi followed by addze.
int16_t HalfNegated(int32_t value) { return static_cast<int16_t>(-value / 2); }

}  // namespace

// ---- CSpriteFileIO -----------------------------------------------------------------------------------------

// Opens the file `name` ("rb") and reads it with the open-file Load, which closes the file itself; the original
// then closes it AGAIN (nothing translated calls this overload, and the host's file close deletes the file, so a
// caller would delete it twice). A file that cannot be opened pushes error 0xb0754001 (level 2) and answers 0.
// The third argument is passed on and never read; the first is the Mac's `this`.
int32_t CSpriteFileIO::Load(CSpriteFileIO* io, char* name, int32_t unused) {
    void* file = gFileOpen(name, "rb");
    if (file == nullptr) {
        gei_ErrorHandler.Push(2, kErrSpriteFileOpen);
        return 0;
    }
    const int32_t result = Load(io, file);
    gFileClose(file);
    return result;
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

// ---- SSpriteEngineData -----------------------------------------------------------------------------------

// Loads the TGA file `name` as the sprite's picture; a name that ends in B or b (any case) also loads the
// alpha of it, from the same file (the second load sets the size again, so the alpha's size is the sprite's).
// The picture is in the texture format's pixels and semitransparent, the offsets put the sprite's centre on
// its position, and the part in use is the whole. 0 when the file could not be loaded.
// Quirks: a refused CreateTextureEntry (dead here: it only fails without memory) frees both images and the
// function carries on with a sprite that has no texture; a name of no characters is looked at one byte before
// its start for the B (the original does the same).
int32_t SSpriteEngineData::Init(char* name) {
    int32_t width = 0;
    int32_t height = 0;
    void* alpha = nullptr;
    void* image = nullptr;
    image = TGALoadAndConvert(name, &width, &height, 0, nullptr, 0);
    if (image == nullptr) return 0;
    int32_t length = 0;
    for (const char* p = name; *p != '\0'; ++p) ++length;
    if (name[length - 1] == 'B' || name[length - 1] == 'b') alpha = TGALoadAndConvert(name, &width, &height, 1, nullptr, 0);
    if (CreateTextureEntry(image, alpha, nullptr, width, height, nullptr, 0) == 0) {
        gFree(alpha);
        gFree(image);
    }
    mTexture->mFlags = (mTexture->mFlags & ~0x300u) | 0x200u;
    mWidth = static_cast<uint16_t>(width);
    mUsedWidth = static_cast<uint16_t>(width);
    mHeight = static_cast<uint16_t>(height);
    mUsedHeight = static_cast<uint16_t>(height);
    mOffsetX = HalfNegated(width);
    mOffsetY = HalfNegated(height);
    mAspect = AspectOf(mWidth, mHeight);
    return 1;
}

// Moves the part of the picture in use. A sprite that has a texture and whose part changes loses its partial
// textures first (they cover the old part). The compare is of the stored halfword as unsigned against the
// argument as signed, so a negative part is always a change.
void SSpriteEngineData::SetUsedPart(int16_t width, int16_t height) {
    if (mTexture != nullptr && (static_cast<int32_t>(mUsedWidth) != width || static_cast<int32_t>(mUsedHeight) != height))
        DestroyPartialTexture();
    mUsedWidth = static_cast<uint16_t>(width);
    mUsedHeight = static_cast<uint16_t>(height);
}

// Cuts the used part of a sprite that has a pixel block (Init(int, int)) into texture entries of the cache's UI
// texture size, row by row, the last column and row the remainder: each entry looks into the block at its place,
// with the picture's width as its pitch, offset by (x, y) pixels. mPartCount is the number made (the field the
// offset of y shares). The size remainder is ((used - 1) & (size - 1)) + 1, which for a size that is not a power
// of two is not the remainder. Answers 0 (after asserting) when the array cannot be made, which only fails without
// memory.
int32_t SSpriteEngineData::GeneratePartialTexture(int32_t x, int32_t y) {
    const int32_t size = gei_TCache->mUITextureSize;
    const uint32_t mask = static_cast<uint32_t>(size) - 1;
    const int32_t columns = static_cast<int32_t>(mUsedWidth + mask) / size;
    const int32_t rows = static_cast<int32_t>(mUsedHeight + mask) / size;
    const int32_t count = static_cast<int32_t>(static_cast<uint32_t>(columns) * static_cast<uint32_t>(rows));
    mTexture = new CTextureEntry[static_cast<std::size_t>(count)];
    if (mTexture == nullptr) {   // dead: the runtime's new throws
        msl_assertion_failed("0", "tb_sprite.cpp", 0x1f8);
        return 0;
    }
    mPartCount = static_cast<uint16_t>(count);
    int32_t entry = 0;
    int32_t rowStart = 0;
    for (int32_t row = 0; row < rows; ++row) {
        const int32_t partHeight = row < rows - 1 ? size : static_cast<int32_t>(((mUsedHeight - 1u) & mask) + 1);
        const int32_t pixelRow = static_cast<int32_t>(static_cast<uint32_t>(rowStart) + static_cast<uint32_t>(y));
        int32_t columnStart = 0;
        for (int32_t column = 0; column < columns; ++column) {
            const int32_t partWidth = column < columns - 1 ? size : static_cast<int32_t>(((mUsedWidth - 1u) & mask) + 1);
            CTextureEntry* part = &mTexture[entry++];
            part->mFlags = 0x5180200;
            if (!(partWidth > 0 && partHeight > 0))
                msl_assertion_failed("(curr_usize>0) && (curr_vsize>0)", "tb_sprite.cpp", 0x22a);
            part->SetSize(partWidth, partHeight);
            part->mPitch = static_cast<int16_t>(mWidth);
            const uint32_t pixel = static_cast<uint32_t>(x) + static_cast<uint32_t>(mWidth) * static_cast<uint32_t>(pixelRow) +
                                   static_cast<uint32_t>(columnStart);
            const int32_t offset =
                static_cast<int32_t>(pixel * static_cast<uint32_t>(gTextureBytesPerPixel[gei_TextureSettings.mFormat]));
            part->mData = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mData) + static_cast<intptr_t>(offset));
            columnStart += size;
        }
        rowStart += size;
    }
    return 1;
}

// Takes the partial textures off the cache's pages and destroys them, clearing the count; nothing happens
// without a count (so an array of no entries, made for a used part of no size, is left where it is).
int32_t SSpriteEngineData::DestroyPartialTexture() {
    if (mPartCount != 0) {
        for (int32_t i = 0; i < mPartCount; ++i) {
            CTextureEntry* part = &mTexture[i];
            if ((part->mFlags & CTextureEntry::kOnPage) != 0) gei_TCache->Remove(part);
            mTexture[i].mData = nullptr;   // the pixels belong to the sprite's block
        }
        delete[] mTexture;
        mTexture = nullptr;
        mPartCount = 0;
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

// tb_sprite.cpp, the regular sprite's draw path (engine library of SimTheme Park 2.04b3; the
// name is the one in its assertions, the inventory files the range under tb_starfield.cpp):
// the members of SSpriteGameData that list a sprite for the frame and turn it into a quad.
//
//   0x1002f3d0 SSpriteGameData::Prepare        (648 bytes)
//   0x1002f688 SSpriteGameData::SetupVertices  (844)
//   0x1003042c SSpriteGameData::CalcRegular    (1,472)
//   0x10030a38 SSpriteGameData::DrawRegular    (84)
//   0x10030ac0 SSpriteGameData::AddRegular     (84)
//
// Every frame CSpriteManager::Prepare calls Prepare on each instance of each list. A sprite that
// is on screen is added to the manager (AddSprite, or AddPartialSprite for the text objects'
// pixel blocks) and has the texture cache prepare its texture. CSpriteManager::RenderSprites
// then calls AddRegular on the sprites of one kind (or DrawRegular on each, with display flag
// 0x80): CalcRegular works the quad out in a stack record and four vertices, and DrawSprite or
// AddSprite draws it.

namespace {

// SSpriteGameData::mF10 (names invented).
constexpr uint32_t kSpriteHidden = 0x00000001;       // not drawn
constexpr uint32_t kSpriteFlipX = 0x00000010;        // the picture is mirrored left to right
constexpr uint32_t kSpriteFlipY = 0x00000060;        // and top to bottom (either of two bits)
constexpr uint32_t kSpriteRotate = 0x00000080;       // the quad is turned by Angle()
constexpr uint32_t kSpriteHot = 0x00000200;          // may become the hot sprite; also a depth bias in CalcRegular
constexpr uint32_t kSpriteOnTerrain = 0x00000400;    // CalcRegular lifts it by the height field's height at its position
constexpr uint32_t kSpriteTurnToView = 0x00001000;   // turned about its own position by the camera's heading
constexpr uint32_t kSpriteFlipV = 0x00008000;        // the picture's v coordinates are swapped
constexpr uint32_t kSpriteNoClipTest = 0x00040000;   // listed without a test against the view: the position is on the screen
                                                     
// already (x and y from -1 to 1; the interface's sprites)
constexpr uint32_t kSpriteOnGround = 0x00080000;     // the quad lies flat: it spreads over x and z at the world position and the
                                                     
// math unit transforms it
constexpr uint32_t kSpriteListed = 0x10000000;       // listed this frame

// gei_Display.mFlags: the hot sprite test needs this one (name invented).
constexpr uint32_t kDisplayHotSprites = 0x01000000;
constexpr uint32_t kDisplayFog = 0x2000;

// Prepare: a sprite counts as seen when a square of 20 times its size at its projected position
// reaches into the view; the cache is told the sprite's depth or, without the test, a fixed distance.
constexpr float kReachPerSize = 20.0f;
constexpr float kNoTestDistance = 0.5f;
constexpr float kPartialDistance = 0.1f;

// SetupVertices: a scale of 0.0 means 20 times the sprite's size; the hot sprite test compares
// the squared distance of the cursor from the sprite with the squared size of the quad times this.
constexpr float kHotFactor = 0.0002f;

// CalcRegular: a screen-space sprite's quad is a twentieth of its size (times the aspect in x) and
// its z is lowered by this when above it; a hot sprite is pushed forward by 0.2 / (1 + z).
constexpr float kPanelScale = 0.05f;
constexpr float kPanelDepthBias = 0.00125f;
constexpr float kDepthPush = 0.2f;

// SSpriteDrawParameters::mFlags: the quad's winding is reversed.
constexpr uint32_t kReverseWinding = 0x2;
// The blending bit CalcRegular adds when the quad is transformed by the math unit (a sprite on the ground).
constexpr uint32_t kBlendGround = 0x800;
// A sprite that is not on a texture page, or has no texture: the blending mode CalcRegular gives.
constexpr uint32_t kBlendNoTexture = 0x1256;
// The projection type of the quad's vertices (CTransformBase::Project's last argument): 2 for a screen-space sprite.
constexpr int32_t kProjectPanel = 2;

}  // namespace

// ---- Prepare ---------------------------------------------------------------------------------

// Always answers 1. A hidden sprite (flag 1) loses its "listed" bit. A sprite without engine data
// is left alone. Three kinds of sprite after that: a partial one (its engine data has a pixel
// block) frees its partial textures when the part of the block in use changed, makes them when
// they are absent and joins the manager's partial list with the cache prepared for each at 0.1; a
// screen-space sprite (flag 0x40000) is listed at once with the cache prepared at 0.5; any other is
// projected (CMatrix::TransformVertex into the first four words) and listed, with the cache
// prepared at its depth, only if its depth is in front of the camera and behind w and the square
// of 20 times its size around it reaches into the view (a NaN in any of the six comparisons
// counts as outside); otherwise it loses its "listed" bit.
int32_t SSpriteGameData::Prepare() {
    uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kSpriteHidden) != 0) {
        mF10 = static_cast<int32_t>(flags & ~kSpriteListed);
        return 1;
    }
    SSpriteEngineData* sprite = mSprite;
    if (sprite == nullptr) return 1;

    if (sprite->mData != nullptr) {
        // A partial sprite: the part of the block in use is this instance's used rectangle.
        const int16_t usedWidth = mUsedWidth;
        const int16_t usedHeight = mUsedHeight;
        if (sprite->mTexture != nullptr &&
            (static_cast<int32_t>(sprite->mUsedWidth) != static_cast<int32_t>(usedWidth) ||
             static_cast<int32_t>(sprite->mUsedHeight) != static_cast<int32_t>(usedHeight)) &&
            sprite->mPartCount != 0) {
            // The partial textures: each that the cache placed comes off its page and lets go of its pixels
            // (they belong to the block), and the array is destroyed (the same lines as
            // SSpriteEngineData::RefreshPartialTexture).
            for (int32_t i = 0; i < sprite->mPartCount; ++i) {
                CTextureEntry* entry = &sprite->mTexture[i];
                if ((entry->mFlags & CTextureEntry::kOnPage) != 0) gei_TCache->Remove(entry);
                sprite->mTexture[i].mData = nullptr;
            }
            delete[] sprite->mTexture;
            sprite->mTexture = nullptr;
            sprite->mPartCount = 0;
        }
        sprite->mUsedWidth = static_cast<uint16_t>(usedWidth);
        sprite->mUsedHeight = static_cast<uint16_t>(usedHeight);

        SSpriteEngineData* data = mSprite;
        if (data->mTexture == nullptr) data->GeneratePartialTexture(mUsedX, mUsedY);
        for (int32_t i = 0; i < data->mPartCount; ++i) gei_TCache->Prepare(&data->mTexture[i], kPartialDistance);
        gei_SpriteManager->AddPartialSprite(this);
        return 1;
    }

    if ((flags & kSpriteNoClipTest) != 0) {
        mF10 = static_cast<int32_t>(flags | kSpriteListed);
        gei_TCache->Prepare(sprite->mTexture, kNoTestDistance);
        gei_SpriteManager->AddSprite(this);
        return 1;
    }

    gei_FinalMatrix->TransformVertex(this, &mPosition, 0);
    const float reach = kReachPerSize * mF2C;
    const float w = mScreenW;
    const bool inView = mDepth > 0.0f && mDepth < w && mScreenX - reach < w && mScreenX + reach > -w &&
                        mScreenY - reach < w && mScreenY + reach > -w;
    if (!inView) {
        mF10 = static_cast<int32_t>(static_cast<uint32_t>(mF10) & ~kSpriteListed);
        return 1;
    }
    gei_SpriteManager->AddSprite(this);
    mF10 = static_cast<int32_t>(static_cast<uint32_t>(mF10) | kSpriteListed);
    gei_TCache->Prepare(mSprite->mTexture, mDepth);
    return 1;
}

// ---- SetupVertices ---------------------------------------------------------------------------

// The quad's four vertices are copies of one point (x, y, z, w) on entry. The sprite's image
// has the part in use (mUsedWidth x mUsedHeight of its data) and the corner of the sprite in it
// (mOffsetX, mOffsetY from the position); the left and right of the quad are the offsets of
// those edges in used widths times xScale, the top and bottom likewise in used heights times
// yScale, so a sprite drawn at its natural size is xScale by yScale across its used part. The
// corners are 0 (left, top), 1 (right, top), 2 (right, bottom), 3 (left, bottom).
//
// Flags: 0x10 negates the horizontal offsets and 0x60 (either bit) the vertical ones, each
// toggling the answer. A flat sprite (0x80000) spreads its vertical offsets over z instead of
// y, toggles the answer once more, and lifts the four y by the display's +0x938 (0.5); an
// upright one spreads them over y. Flag 0x80 then turns the quad by Angle() (RotateVertex_Y for
// an upright sprite, with the middle of the quad as centre; RotateVertex_Z for a flat one); flag
// 0x1000 of an upright one turns it again by the camera's heading about the position it came
// with.
//
// The hot sprite: with flag 0x200 and display flag 0x1000000 the sprite whose projected position
// (x / w, y / w) is nearest the cursor (gSpriteHotX, gSpriteHotY) becomes gHotSprite, if the
// squared distance is below the squared size of the quad times 0.0002 and its w is smaller than
// the current hot sprite's (the nearer one). The test runs on the x offsets already added and
// the y ones not yet; its fused multiply-adds follow the listing.
//
// QUIRK: when the image has no used width or height the divisions give infinities or NaNs.
int32_t SSpriteGameData::SetupVertices(SVertex* vertices, float xScale, float yScale) {
    if (0.0f == xScale) xScale = gei_Display.mF04 * (kReachPerSize * mF2C);
    if (0.0f == yScale) yScale = kReachPerSize * mF2C;

    const SSpriteEngineData* sprite = mSprite;
    const int32_t offsetX = sprite->mOffsetX;
    const int32_t offsetY = sprite->mOffsetY;
    const int32_t width = sprite->mWidth;
    const int32_t height = sprite->mHeight;
    const float usedWidth = static_cast<float>(sprite->mUsedWidth);
    const float usedHeight = static_cast<float>(sprite->mUsedHeight);

    // The point the quad grew from, for the second turn.
    const SVertex origin = vertices[0];
    const uint32_t flags = static_cast<uint32_t>(mF10);
    bool reversed = false;

    float left = xScale * (static_cast<float>(offsetX) / usedWidth);
    float right = xScale * (static_cast<float>(offsetX + width) / usedWidth);
    float bottom = yScale * (static_cast<float>(offsetY + height) / usedHeight);
    float top = yScale * (static_cast<float>(offsetY) / usedHeight);
    if ((flags & kSpriteFlipX) != 0) {
        reversed = !reversed;
        left = left * -1.0f;
        right = right * -1.0f;
    }
    if ((flags & kSpriteFlipY) != 0) {
        reversed = !reversed;
        bottom = bottom * -1.0f;
        top = top * -1.0f;
    }

    const float x0 = vertices[0].x + left;
    vertices[0].x = x0;
    vertices[3].x = x0;
    const float x1 = vertices[1].x + right;
    vertices[1].x = x1;
    vertices[2].x = x1;

    if ((flags & kSpriteHot) != 0 && (gei_Display.mFlags & kDisplayHotSprites) != 0) {
        const float acrossX = right - left;
        const float acrossY = bottom - top;
        const float w = vertices[0].w;
        const float inverse = 1.0f / w;
        const float offX = ppc_fnmsubs(vertices[0].x, inverse, gSpriteHotX);
        const float offY = ppc_fnmsubs(vertices[0].y, inverse, gSpriteHotY);
        const float acrossXSquared = acrossX * acrossX;
        const float offXSquared = offX * offX;
        const float sizeSquared = ppc_fmadds(acrossY, acrossY, acrossXSquared);
        const float distanceSquared = ppc_fmadds(offY, offY, offXSquared);
        if (sizeSquared * kHotFactor > distanceSquared) {
            if (gHotSprite == nullptr || w < gHotSprite->mScreenW) gHotSprite = this;
        }
    }

    if ((flags & kSpriteOnGround) == 0) {
        const float y0 = vertices[0].y + top;
        vertices[1].y = y0;
        vertices[0].y = y0;
        const float y2 = vertices[2].y + bottom;
        vertices[3].y = y2;
        vertices[2].y = y2;
        if ((flags & kSpriteRotate) != 0) RotateVertex_Y(vertices, Angle(), nullptr);
        if ((flags & kSpriteTurnToView) != 0) {
            SVertex centre = origin;
            RotateVertex_Y(vertices, gViewAngle, &centre);
        }
    } else {
        reversed = !reversed;
        const float z0 = vertices[0].z + top;
        vertices[1].z = z0;
        vertices[0].z = z0;
        const float z2 = vertices[2].z + bottom;
        vertices[3].z = z2;
        vertices[2].z = z2;
        for (int32_t i = 0; i < 4; ++i) vertices[i].y = vertices[i].y + gei_Display.mF938;
        if ((flags & kSpriteRotate) != 0) RotateVertex_Z(vertices, Angle());
    }
    return reversed ? 1 : 0;
}

// tb_sprites_shadow.cpp: the sprites' shadows (engine library of SimTheme Park 2.04b3; the original file is
// tb_sprite.cpp by its assertions, the inventory files the range under tb_starfield.cpp):
//
//   0x1002fa14 SSpriteGameData::CreateShadow      (1,220 bytes)
//   0x1002ff20 SSpriteGameData::DrawRegularShadow   (692)
//   0x10030210 SSpriteGameData::AddRegularShadow    (468)
//
// A regular sprite that has flags 0x200 and 0x10000000 casts a shadow for every light source of the shadow object
// (gShadow, src/tb_shadow.cpp): the sprite's picture, flattened onto the ground by the light's matrix, lying on the height
// field (the ground's height under each of its four corners). CreateShadow works the four corners out;
// DrawRegularShadow draws each shadow as a primitive of its own (the sprite manager's draw-each-sprite path) and
// AddRegularShadow adds them to the primitive being built (the grouped path; the manager's blob shadow texture may take
// the sprite's place).

namespace {

// SSpriteGameData::mF10 (names invented).
constexpr uint32_t kFlagFlipX = 0x00000010;       // the shadow's two horizontal extents change sign
constexpr uint32_t kFlagFlipZ = 0x00000020;       // and the two along the ground
constexpr uint32_t kFlagTurned = 0x00000080;      // the corners are turned about the vertical axis by mRotation
constexpr uint32_t kFlagCasts = 0x00000200;       // the sprite casts a shadow (both these two flags are needed) ...
constexpr uint32_t kFlagLifted = 0x00000800;      // mShadowLift is added to the ground height
constexpr uint32_t kFlagShadows = 0x10000000;     // ... and has shadows at all
constexpr uint32_t kFlagBlend20000 = 0x00020000;  // the shadow's blending mode gets this bit too

// gei_Display.mFlags: each shadow takes its picture from the sprite file's frames (mFrameFile), not from the sprite.
constexpr uint32_t kDisplayShadowFrames = 0x00100000;

// The texture entry's flag that says it is on a cache page, and the one the sprite code copies to flag 4 of the
// draw record (the sky and the particles do the same).
constexpr uint32_t kTextureOnPage = 0x00001000;
constexpr uint32_t kTextureFlag100000 = 0x00100000;
constexpr uint32_t kSpriteFlag4 = 4;

// The blending mode of a sprite without a texture entry, and the bits every shadow adds to its entry's (0x200, the
// semitransparent bit, is taken off again before each shadow is drawn).
constexpr uint32_t kNoTextureBlendingMode = 0x1256;
constexpr uint32_t kShadowBlendingBit = 0x00000800;
constexpr uint32_t kBlendingSemitransparent = 0x00000200;

// The corners of a shadow are scaled by 20 times the sprite's scale, as the original's constant (engine data 0x1004ff90).
constexpr float kShadowSize = 20.0f;

// The integer to single conversion of the original's (exact for halfwords).
float ToFloat(int32_t value) { return static_cast<float>(value); }

// The draw record's page, blending mode, picture corners and flag 4 for a texture entry; no entry is a picture of nothing.
void TakeTexture(SSpriteDrawParameters& draw, CTextureEntry* entry) {
    if (entry == nullptr) {
        draw.mTexture = nullptr;
        draw.mBlendingMode = kNoTextureBlendingMode;
        draw.mU0 = 0.0f;
        draw.mV0 = 0.0f;
        draw.mU1 = 0.0f;
        draw.mV1 = 0.0f;
        return;
    }
    draw.mTexture = (entry->mFlags & kTextureOnPage) != 0 ? entry->mTexture : nullptr;
    draw.mBlendingMode = entry->GetBlendingMode();
    draw.mU0 = entry->mUOffset;
    draw.mV0 = entry->mVOffset;
    draw.mU1 = draw.mU0 + entry->mUSize;
    draw.mV1 = draw.mV0 + entry->mVSize;
    if ((entry->mFlags & kTextureFlag100000) != 0) draw.mFlags |= kSpriteFlag4;
}

// The blending bits a shadow adds to its texture's.
void AddShadowBlending(SSpriteDrawParameters& draw, uint32_t spriteFlags) {
    draw.mBlendingMode |= kShadowBlendingBit;
    if ((spriteFlags & kFlagBlend20000) != 0) draw.mBlendingMode |= kFlagBlend20000;
}

}  // namespace

// The shadow's four corners, in the order of the picture's corners (the first vertex is the far left, the
// second the far right, the third the near right, the fourth the near left; here "far" is the side of the used part's top):
// the sprite's rectangle (the offsets and size of its sprite, over its used size) scaled by 20 times the
// sprite's scale, the display's aspect (mF04) and the caller's two scales, stretched along the light's cross
// product (gShadow.mLightCross: the ground's direction at right angles to the light) and the ground's up
// axis (y of gUpVector) and placed at the sprite's position. The vertices are then lowered so that the lowest
// is at the position's height, turned by mRotation when flag 0x80 asks, moved by the shadow matrix, laid on the ground
// (each y gets the height field's height under its x and z, a triangle interpolation of the cell, the lift of flag 0x800
// and the display's constant mF938) and moved again by the final matrix. Answers the light's shade.
//
// Quirks kept. The ground lookup does not clamp the cell: it clamps only the neighbour (to the grid size, one past
// the last sample), so a corner off the field reads samples around the table, as the original does. The sprite's
// offset y is the halfword the partial sprites use as a part count (it is a file sprite's here). The math unit is
// asserted (tb_matrix.h line 152) before each of its two uses, which the original continues past. Not reproduced:
// the stack word the original leaves in the draw record's last field in DrawRegularShadow is zero here.
int32_t SSpriteGameData::CreateShadow(SVertex* vertices, CMatrix* shadowMatrix, float scaleX, float scaleZ) {
    SSpriteEngineData* sprite = mSprite;
    const int32_t offsetX = sprite->mOffsetX;
    const int32_t offsetY = sprite->mOffsetY;
    const int32_t width = sprite->mWidth;
    const int32_t height = sprite->mHeight;
    const float usedWidth = ToFloat(sprite->mUsedWidth);
    const float usedHeight = ToFloat(sprite->mUsedHeight);
    const int32_t light = gShadow.mCurrent;

    const float size = kShadowSize * mF2C;
    const float widthSize = size * gei_Display.mF04;
    const float up = gUpVector.y;
    float farZ = scaleZ * (up * (size * (ToFloat(offsetY + height) / usedHeight)));
    float nearZ = scaleZ * (up * (size * (ToFloat(offsetY) / usedHeight)));
    float rightX = scaleX * (widthSize * (ToFloat(offsetX + width) / usedWidth));
    float leftX = scaleX * (widthSize * (ToFloat(offsetX) / usedWidth));
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagFlipX) != 0) {
        leftX = -leftX;
        rightX = -rightX;
    }

    const D3DVECTOR& cross = gShadow.mLightCross[light];
    const float leftAlongX = ppc_fmadds(leftX, cross.x, mPosition.x);
    vertices[0].x = leftAlongX;
    vertices[3].x = leftAlongX;
    const float rightAlongX = ppc_fmadds(rightX, cross.x, mPosition.x);
    vertices[1].x = rightAlongX;
    vertices[2].x = rightAlongX;
    const float leftAlongZ = ppc_fmadds(leftX, cross.z, mPosition.z);
    vertices[0].z = leftAlongZ;
    vertices[3].z = leftAlongZ;
    const float rightAlongZ = ppc_fmadds(rightX, cross.z, mPosition.z);
    vertices[1].z = rightAlongZ;
    vertices[2].z = rightAlongZ;

    if ((flags & kFlagFlipZ) != 0) {
        nearZ = -nearZ;
        farZ = -farZ;
    }
    const float farY = mPosition.y + farZ;
    vertices[1].y = farY;
    vertices[0].y = farY;
    const float nearY = mPosition.y + nearZ;
    vertices[3].y = nearY;
    vertices[2].y = nearY;

    // The lowest corner is at the position's height.
    float lowest = vertices[0].y;
    if (vertices[1].y < lowest) lowest = vertices[1].y;
    if (vertices[2].y < lowest) lowest = vertices[2].y;
    if (vertices[3].y < lowest) lowest = vertices[3].y;
    vertices[0].y = vertices[0].y - lowest;
    vertices[1].y = vertices[1].y - lowest;
    vertices[2].y = vertices[2].y - lowest;
    vertices[3].y = vertices[3].y - lowest;

    if ((flags & kFlagTurned) != 0) RotateVertex_Y(vertices, -mRotation, nullptr);

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(shadowMatrix, vertices, vertices, 0x10, 0x10, 4, 0);

    // The ground under each corner, last corner first.
    const float lift = (flags & kFlagLifted) != 0 ? mShadowLift : 0.0f;
    const float base = lift + gei_Display.mF938;
    for (int32_t i = 3; i >= 0; --i) {
        SVertex& corner = vertices[i];
        CHField* field = gei_HField;
        sHField* grid = field->mF00;
        const float x = corner.x;
        const float z = corner.z;
        const float originX = grid->mF08[0];
        const float originZ = grid->mF08[1];
        const int32_t columns = field->mGridWidth;
        const int32_t cellX = ppc_fctiwz(field->mInvCellX * (x - originX));
        const int32_t cellZ = ppc_fctiwz(field->mInvCellZ * (z - originZ));
        // The next cell along each axis, kept in 0..the grid's size.
        int32_t nextX = cellX + 1;
        if (nextX > columns) {
            nextX = columns;
        } else if (nextX < 0) {
            nextX = 0;
        }
        const int32_t rows = field->mGridHeight;
        int32_t nextZ = cellZ + 1;
        if (nextZ > rows) {
            nextZ = rows;
        } else if (nextZ < 0) {
            nextZ = 0;
        }
        const float* heights = grid->mHeights;
        const int32_t rowStart = columns * cellZ;
        const float corner00 = heights[cellX + rowStart];
        const float corner10 = heights[nextX + rowStart];
        const float corner01 = heights[cellX + columns * nextZ];
        // How far into the cell the corner is, as a fraction of it.
        const float fractionX = field->mInvCellX * (x - ppc_fmadds(ToFloat(cellX), grid->mF08[2], originX));
        const float fractionZ = field->mInvCellZ * (z - ppc_fmadds(ToFloat(cellZ), grid->mF08[3], originZ));
        float ground = ppc_fmadds(fractionX, corner10 - corner00, corner00);
        ground = ppc_fmadds(fractionZ, corner01 - corner00, ground);
        corner.y = corner.y + (base + ground);
    }

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    gei_MathUnit->Transform(gei_FinalMatrix, vertices, vertices, 0x10, 0x10, 4, 0);
    return static_cast<int32_t>(gShadow.mLightShades[gShadow.mCurrent]);
}

// Draws the shadows one by one, DrawSprite each (a primitive of its own). Answers 1 for a sprite that casts none, and 0 as
// soon as a DrawSprite fails. Quirks kept. The sprite's own picture is worked out once, before the shadows, with the fog
// flag 4 of its entry in the record for all of them; with the display's flag 0x100000 each shadow takes the entry of its own frame
// instead (the frame of the sprite's file: mFrameBase plus the shadow's byte of mLightFrames, the first sprite of the file when that
// is not below the file's count) and the record's blending mode then loses the bits 0x800 and 0x20000 the code adds
// before the loop (the loop sets the mode afresh; 0x200 is taken off each time as before). The record's last field
// is stack garbage in the original (zero here).
int32_t SSpriteGameData::DrawRegularShadow() {
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagCasts) == 0) return 1;
    if ((flags & kFlagShadows) == 0) return 1;

    SVertex corners[4];
    SSpriteDrawParameters draw{};
    draw.mVertices = corners;
    if ((gei_Display.mFlags & kDisplayShadowFrames) == 0) TakeTexture(draw, mSprite->mTexture);
    AddShadowBlending(draw, static_cast<uint32_t>(mF10));

    CMatrix* matrix = gShadow.GetFirstShadowMatrix();
    int32_t index = 0;
    while (matrix != nullptr) {
        if ((gei_Display.mFlags & kDisplayShadowFrames) != 0) {
            CSpriteFileIO* file = mFrameFile;
            const int32_t frame = static_cast<int32_t>(mFrameBase) + static_cast<int32_t>(mLightFrames[index]);
            SSpriteEngineData* picture = (frame < file->mCount && frame >= 0) ? &file->mSprites[frame] : file->mSprites;
            TakeTexture(draw, picture->mTexture);
        }
        draw.mF08 = CreateShadow(corners, matrix, 1.0f, 1.0f);
        draw.mBlendingMode &= ~kBlendingSemitransparent;
        if (gei_DrawPrimitive->DrawSprite(&draw, -1) == 0) return 0;
        matrix = gShadow.GetNextShadowMatrix();
        ++index;
    }
    return 1;
}

// The same for the grouped path: the shadows go to the primitive being built with AddSprite. The picture is `shadowTexture` (the
// manager's blob shadow) when it is not NULL, else the sprite's; it is worked out once. The record's last field is 0 here.
int32_t SSpriteGameData::AddRegularShadow(CTextureEntry* shadowTexture) {
    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kFlagCasts) == 0) return 1;
    if ((flags & kFlagShadows) == 0) return 1;

    SVertex corners[4];
    SSpriteDrawParameters draw{};
    draw.mVertices = corners;
    TakeTexture(draw, shadowTexture != nullptr ? shadowTexture : mSprite->mTexture);
    draw.mF28 = 0;
    AddShadowBlending(draw, static_cast<uint32_t>(mF10));

    CMatrix* matrix = gShadow.GetFirstShadowMatrix();
    while (matrix != nullptr) {
        draw.mF08 = CreateShadow(corners, matrix, 1.0f, 1.0f);
        draw.mBlendingMode &= ~kBlendingSemitransparent;
        if (gei_DrawPrimitive->AddSprite(&draw, -1) == 0) return 0;
        matrix = gShadow.GetNextShadowMatrix();
    }
    return 1;
}

// ---- CalcRegular -----------------------------------------------------------------------------

// Works out the record DrawSprite and AddSprite take for one sprite, in `params` (flags, the
// four vertices at params->mVertices, the colour, the blending mode, the page and the picture's
// corners, the projection type). Always answers 1. A sprite that is not listed this frame (flag
// 0x10000000 clear) asserts (tb_sprite.cpp line 0x4e4) and leaves the record alone.
//
// The picture: without a texture the page is none, the blending mode 0x1256 and the four
// coordinates 0.0. With one, the page is the texture's page when the cache placed it (flag
// 0x1000) else none, the blending mode is GetBlendingMode's, (u0, v0) the entry's offsets and
// (u1, v1) those plus its sizes. Entry flag 0x100000 sets bit 4 of the record's flags, which
// the next line clears (a dead store). Flag 0x8000 of the sprite swaps v0 and v1 at the end.
//
// The position. Flag 0x400 lifts the sprite's y (+0x18) by the height field's height under its
// x and z (bilinear between the four samples of the cell it is in: the cell is the position
// less the field's origin times the inverse cell size cut toward zero; the next column and row
// are clamped to 0..width and 0..height, NOT to width - 1, so the last column reads the next
// row's first sample) plus the display's +0x938. Flag 0x40000 (screen space) takes the position
// as it is with y negated, w = 1 and z lowered by 0.00125 when above it, projection type 2,
// and spreads it into a quad of a twentieth of the size (the x scale times the display's
// aspect). Flag 0x80000 (flat) takes the position as it is (w is whatever the record's
// vertices held), spreads it with SetupVertices (scales 0), and has the math unit transform the
// four vertices by the final matrix (strides 0x10, type 0), adding blending bit 0x800. Any
// other sprite takes the projected x, y, depth and w of Prepare and spreads them; with flag
// 0x200 and no fog (display flag 0x2000) all four z move toward the camera by 0.2 / (1 + z)
// of the last corner. The record's colour is the sprite's, and its flags get bit 2 when
// SetupVertices answered 1.
int32_t SSpriteGameData::CalcRegular(SSpriteDrawParameters* params) {
    if ((static_cast<uint32_t>(mF10) & kSpriteListed) == 0) {
        msl_assertion_failed("0", "tb_sprite.cpp", 0x4e4);
        return 1;
    }

    CTextureEntry* texture = mSprite->mTexture;
    if (texture == nullptr) {
        params->mTexture = nullptr;
        params->mBlendingMode = kBlendNoTexture;
        params->mU0 = 0.0f;
        params->mV0 = 0.0f;
        params->mU1 = 0.0f;
        params->mV1 = 0.0f;
    } else {
        params->mTexture = (texture->mFlags & CTextureEntry::kOnPage) != 0 ? texture->mTexture : nullptr;
        params->mBlendingMode = texture->GetBlendingMode();
        params->mU0 = texture->mUOffset;
        params->mV0 = texture->mVOffset;
        params->mU1 = params->mU0 + texture->mUSize;
        params->mV1 = params->mV0 + texture->mVSize;
        if ((texture->mFlags & 0x100000) != 0) params->mFlags |= 4;
    }
    params->mFlags = 0;

    const uint32_t flags = static_cast<uint32_t>(mF10);
    if ((flags & kSpriteOnTerrain) != 0) {
        const CHField* field = gei_HField;
        const sHField* heightField = field->mF00;
        const int32_t columns = field->mGridWidth;
        const int32_t rows = field->mGridHeight;
        const float x = mPosition.x;
        const float z = mPosition.z;
        const int32_t column = ppc_fctiwz(static_cast<double>(field->mInvCellX * (x - heightField->mF08[0])));
        const int32_t row = ppc_fctiwz(static_cast<double>(field->mInvCellZ * (z - heightField->mF08[1])));
        int32_t nextColumn = column + 1;
        if (nextColumn > columns) {
            nextColumn = columns;
        } else if (nextColumn < 0) {
            nextColumn = 0;
        }
        int32_t nextRow = row + 1;
        if (nextRow > rows) {
            nextRow = rows;
        } else if (nextRow < 0) {
            nextRow = 0;
        }
        const float cellX = ppc_fmadds(static_cast<float>(column), heightField->mF08[2], heightField->mF08[0]);
        const float cellZ = ppc_fmadds(static_cast<float>(row), heightField->mF08[3], heightField->mF08[1]);
        const float* heights = heightField->mHeights;
        const int32_t rowStart = columns * row;
        const float h00 = heights[column + rowStart];
        const float h10 = heights[nextColumn + rowStart];
        const float h01 = heights[column + columns * nextRow];
        const float fx = field->mInvCellX * (x - cellX);
        const float fz = field->mInvCellZ * (z - cellZ);
        const float bias = gei_Display.mF938;
        float height = ppc_fmadds(fx, h10 - h00, h00);
        height = ppc_fmadds(fz, h01 - h00, height);
        mPosition.y = mPosition.y + (bias + height);
    }

    SVertex* vertices = static_cast<SVertex*>(params->mVertices);
    bool reversed;
    if ((flags & kSpriteNoClipTest) != 0) {
        std::memcpy(&vertices[0], &mPosition, 12);
        vertices[0].y = -vertices[0].y;
        vertices[0].w = 1.0f;
        if (vertices[0].z > kPanelDepthBias) vertices[0].z = vertices[0].z - kPanelDepthBias;
        params->mF28 = kProjectPanel;
        vertices[1] = vertices[0];
        vertices[2] = vertices[1];
        vertices[3] = vertices[2];
        const float scale = kPanelScale * mF2C;
        reversed = SetupVertices(vertices, gei_Display.mF04 * scale, scale) != 0;
    } else {
        params->mF28 = 0;
        if ((flags & kSpriteOnGround) != 0) {
            std::memcpy(&vertices[0], &mPosition, 12);
            vertices[1] = vertices[0];
            vertices[2] = vertices[1];
            vertices[3] = vertices[2];
            reversed = SetupVertices(vertices, 0.0f, 0.0f) != 0;
            if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
            gei_MathUnit->Transform(gei_FinalMatrix, vertices, vertices, 0x10, 0x10, 4, 0);
            params->mBlendingMode |= kBlendGround;
        } else {
            std::memcpy(&vertices[0], static_cast<const void*>(this), sizeof(SVertex));
            vertices[1] = vertices[0];
            vertices[2] = vertices[1];
            vertices[3] = vertices[2];
            reversed = SetupVertices(vertices, 0.0f, 0.0f) != 0;
            if ((flags & kSpriteHot) != 0 && (gei_Display.mFlags & kDisplayFog) == 0) {
                const float z = vertices[3].z;
                const float pushed = z - kDepthPush / (1.0f + z);
                vertices[3].z = pushed;
                vertices[2].z = pushed;
                vertices[1].z = pushed;
                vertices[0].z = pushed;
            }
        }
    }

    params->mF08 = mF24;
    if (reversed) params->mFlags |= kReverseWinding;
    if ((flags & kSpriteFlipV) != 0) {
        const float v = params->mV0;
        params->mV0 = params->mV1;
        params->mV1 = v;
    }
    return 1;
}

// ---- DrawRegular and AddRegular ----------------------------------------------------------------

// The record and its four vertices live on the stack of the original (the vertices' unwritten
// words are whatever was there; here zero). Each answers 0 if CalcRegular does (it never does)
// and else what DrawSprite or AddSprite answers (DrawSprite: 0 when the renderer fails, else 1;
// AddSprite: always 1). The mask is -1, every clip plane.
int32_t SSpriteGameData::DrawRegular() {
    SSpriteDrawParameters params = {};
    SVertex vertices[4] = {};
    params.mVertices = vertices;
    if (CalcRegular(&params) == 0) return 0;
    return gei_DrawPrimitive->DrawSprite(&params, -1);
}

int32_t SSpriteGameData::AddRegular() {
    SSpriteDrawParameters params = {};
    SVertex vertices[4] = {};
    params.mVertices = vertices;
    if (CalcRegular(&params) == 0) return 0;
    return gei_DrawPrimitive->AddSprite(&params, -1);
}

// tb_sprites_partial.cpp: the partial sprite draw (engine library of SimTheme Park 2.04b3; the original file is
// tb_sprite.cpp by its assertions, the inventory files the range under tb_starfield.cpp):
//
//   0x10030b48 SSpriteGameData::DrawPartial   (1,296 bytes)
//
// A partial sprite is a pixel block (SSpriteEngineData::Init(int, int): the interface's text objects draw their text
// into one) cut into texture entries of the cache's UI texture size by GeneratePartialTexture. DrawPartial puts the
// block on the screen: one quad for each entry, row by row, through CDrawPrimitive::DrawSprite. CSpriteManager::
// RenderPartialSprites calls it for every partial sprite the frame added.

namespace {

// SSpriteGameData::mF10 (names invented): where the picture hangs from the sprite's position, vertically (any of the
// three bits asks for an offset; the middle bit wins over the lower one) and horizontally.
constexpr uint32_t kHangVertical = 0x07000000;
constexpr uint32_t kHangMiddleY = 0x02000000;   // half the picture's height up
constexpr uint32_t kHangBottom = 0x04000000;    // all of it
constexpr uint32_t kHangHorizontal = 0x70000000;
constexpr uint32_t kHangMiddleX = 0x20000000;   // half the picture's width to the left
constexpr uint32_t kHangRight = 0x40000000;     // all of it

// SSpriteDrawParameters::mFlags: 0x400 marks the primitive for round-off (DrawSprite).
constexpr uint32_t kSpriteRoundOff = 0x400;
// The projection type of the sprite's depth (CTransformBase::Project's last argument).
constexpr int32_t kProjectionType = 2;
// The blending mode every partial quad is drawn with, whatever its texture entry says (the entry's own
// GetBlendingMode is called and dropped).
constexpr uint32_t kPartialBlendingMode = 0x07060556;

// Twice the sum of two pixel counts, in 32 bits (a pixel is two steps of the screen's 2 / size).
float TwiceToFloat(int32_t a, int32_t b) {
    return ToFloat(static_cast<int32_t>((static_cast<uint32_t>(a) + static_cast<uint32_t>(b)) << 1));
}

}  // namespace

// Quirks kept. The answer is always 1: a DrawSprite that fails only ends the drawing. The instance's position is
// taken in the screen's -1..1 space with y turned over (down is positive), the picture hangs from it by the instance's
// two halfwords at 0x30 and 0x32 (mUsedWidth, mUsedHeight: the picture's size on the screen, which the hang flags
// use and the height also always lifts the picture by, so the position is its bottom left corner). The quads walk the
// entry array in order, so a texture of fewer entries than the used part has cells would be read past its end, and a NULL texture
// (all four texture coordinates zero) makes the following cells read from the address one entry past NULL (the
// original does the same arithmetic): such a sprite has at most one cell. The cache's UI texture size is
// read once (the original reads it three times; nothing in between can change it) and divides without a
// guard, as the original's divw does. The record handed to DrawSprite is one and the same for every quad: flag 4
// stays set once an entry asked for it, and the colour DrawSprite masks is masked again for the next quad.
// The blending mode is set to the constant after every texture entry's own is worked out (and dropped).
int32_t SSpriteGameData::DrawPartial() {
    SSpriteEngineData* sprite = mSprite;
    const int32_t cell = gei_TCache->mUITextureSize;
    const int32_t usedWidth = sprite->mUsedWidth;
    const int32_t usedHeight = sprite->mUsedHeight;
    CTextureEntry* entry = sprite->mTexture;

    float corners[16];
    SSpriteDrawParameters draw{};
    draw.mFlags = kSpriteRoundOff;
    draw.mVertices = corners;
    draw.mF08 = mF24;
    draw.mF28 = kProjectionType;

    // The cells: the used part divided by the cell size, rounded up.
    const uint32_t cellLess = static_cast<uint32_t>(cell) - 1u;
    const int32_t columns = static_cast<int32_t>(static_cast<uint32_t>(usedWidth) + cellLess) / cell;
    const int32_t rows = static_cast<int32_t>(static_cast<uint32_t>(usedHeight) + cellLess) / cell;

    // The picture's top left corner in the screen's space: the position with y turned over, moved by the hang flags
    // and lifted by the instance's height (in pixels, times the scale, as a fraction of the display's height).
    const uint32_t flags = static_cast<uint32_t>(mF10);
    const float scale = mF2C;
    float x = mPosition.x;
    float y = -mPosition.y;
    const float z = mPosition.z;
    if ((flags & kHangVertical) != 0) {
        const float hang = 2.0f * (ToFloat(mUsedHeight) * (scale / ToFloat(sprite->mHeight)));
        if ((flags & kHangMiddleY) != 0) {
            y = ppc_fmadds(0.5f, hang, y);
        } else if ((flags & kHangBottom) != 0) {
            y = y + hang;
        }
    }
    y = ppc_fnmsubs((2.0f * scale) / ToFloat(gei_Display.mHeight), ToFloat(mUsedHeight), y);
    if ((flags & kHangHorizontal) != 0) {
        const float hang = 2.0f * (gei_Display.mF04 * (ToFloat(mUsedWidth) * (scale / ToFloat(sprite->mWidth))));
        if ((flags & kHangMiddleX) != 0) {
            x = ppc_fnmsubs(0.5f, hang, x);
        } else if ((flags & kHangRight) != 0) {
            x = x - hang;
        }
    }

    // The screen space is -1..1; a pixel is two steps of 1 / width or 1 / height.
    const float inverseWidth = 1.0f / ToFloat(gei_Display.mWidth);
    const float inverseHeight = 1.0f / ToFloat(gei_Display.mHeight);
    const float top = 1.0f + y;
    const float left = 1.0f + x;
    int32_t pixelY = ppc_fctiwz(0.5f * (top * ToFloat(gei_Display.mHeight)));

    for (int32_t row = 0; row < rows; ++row) {
        const float y0 = ppc_fmsubs(inverseHeight, TwiceToFloat(pixelY, 0), 1.0f);
        int32_t pixelX = ppc_fctiwz(0.5f * (left * ToFloat(gei_Display.mWidth)));
        for (int32_t column = 0; column < columns; ++column) {
            if (entry == nullptr) {
                draw.mTexture = nullptr;
                draw.mU0 = 0.0f;
                draw.mV0 = 0.0f;
                draw.mU1 = 0.0f;
                draw.mV1 = 0.0f;
            } else {
                draw.mTexture = (entry->mFlags & kTextureOnPage) != 0 ? entry->mTexture : nullptr;
                entry->GetBlendingMode();
                draw.mU0 = entry->mUOffset;
                draw.mV0 = entry->mVOffset;
                draw.mU1 = draw.mU0 + entry->mUSize;
                draw.mV1 = draw.mV0 + entry->mVSize;
                if ((entry->mFlags & kTextureFlag100000) != 0) draw.mFlags |= kSpriteFlag4;
            }
            draw.mBlendingMode = kPartialBlendingMode;

            // The cell's size in pixels: the rest of the used part at the last column and row, else the cell size.
            int32_t width = static_cast<int32_t>(static_cast<uint32_t>(usedWidth) -
                                                 static_cast<uint32_t>(column) * static_cast<uint32_t>(cell));
            int32_t height = static_cast<int32_t>(static_cast<uint32_t>(usedHeight) -
                                                  static_cast<uint32_t>(row) * static_cast<uint32_t>(cell));
            if (width > cell) width = cell;
            if (height > cell) height = cell;

            const float x0 = ppc_fmsubs(inverseWidth, TwiceToFloat(pixelX, 0), 1.0f);
            const float x1 = ppc_fmsubs(inverseWidth, TwiceToFloat(pixelX, width), 1.0f);
            const float y1 = ppc_fmsubs(inverseHeight, TwiceToFloat(pixelY, height), 1.0f);
            // The corners, clockwise from the top left: x, y, z, w.
            corners[0] = x0;
            corners[1] = y0;
            corners[2] = z;
            corners[3] = 1.0f;
            corners[4] = x1;
            corners[5] = y0;
            corners[6] = z;
            corners[7] = 1.0f;
            corners[8] = x1;
            corners[9] = y1;
            corners[10] = z;
            corners[11] = 1.0f;
            corners[12] = x0;
            corners[13] = y1;
            corners[14] = z;
            corners[15] = 1.0f;
            if (gei_DrawPrimitive->DrawSprite(&draw, -1) == 0) return 1;

            pixelX += cell;
            ++entry;
        }
        pixelY += cell;
    }
    return 1;
}
