// The sprite classes' remaining members (engine library of SimTheme Park 2.04b3): CSpriteList's Close,
// Prepare, GetData and GetLinkType (0x10031408, 0x10031488, 0x10031fb0, 0x10031fe4; the original file is
// tb_spritelist.cpp by its assertions, the inventory files it under tb_starfield.cpp), SSpriteEngineData's
// Init(char*), SetUsedPart and the partial texture generator and destroyer (0x1002e810, 0x1002eb00,
// 0x1002ebb8, 0x1002ee28; tb_sprite.cpp) and CSpriteFileIO::Load(char*, int) (0x1002dea0). The rest of
// these classes is in src/tb_spritefileio.cpp.
//
// Each `new` is the original's operator new[] followed by the constructor loop; a null answer from it is
// dead code here (the runtime's new throws) and is kept where the original tests it.
#include <cstdint>

#include "Wavelet.h"
#include "platform/msl.h"
#include "sprite_engine.h"
#include "tb_error.h"
#include "tb_loaders.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

namespace {

// Pushed when a sprite file cannot be opened (the neighbour of kErrOutOfMemory, 0xb0754000).
constexpr int32_t kErrSpriteFileOpen = static_cast<int32_t>(0xb0754001);

// The sprite's aspect: width over height, 0.0 without a height.
float AspectOf(uint16_t width, uint16_t height) {
    if (height == 0) return 0.0f;
    return static_cast<float>(width) / static_cast<float>(height);
}

// A signed halfway value rounded toward zero: the processor's srawi followed by addze.
int16_t HalfNegated(int32_t value) { return static_cast<int16_t>(-value / 2); }

}  // namespace

// ---- CSpriteList ---------------------------------------------------------------------------------------

// Frees the instance array (the allocator's free clears the pointer) and forgets its size. The count of
// instances in use (mCount) is the owner's.
int32_t CSpriteList::Close() {
    if (mInstances != nullptr) gFree(reinterpret_cast<void*&>(mInstances));
    mCapacity = 0;
    return 1;
}

// Gives the sprite manager this list once per display frame: the two frame words of the display (the
// frame count and the word a flip zeroes) are kept here and compared on the next call. A list the
// manager refused (its table is full) is remembered all the same.
int32_t CSpriteList::Prepare() {
    const int32_t frame = gei_Display.mFrameCount;
    const int32_t flipped = gei_Display.mF84;
    if (mFrameA == frame && mFrameB == flipped) return 1;
    const int32_t result = gei_SpriteManager->AddList(this);
    mFrameA = frame;
    mFrameB = flipped;
    return result;
}

void* CSpriteList::GetData() { return nullptr; }

// The kind of link the map lists hold it as (3 for sprite lists).
int32_t CSpriteList::GetLinkType() { return 3; }

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
