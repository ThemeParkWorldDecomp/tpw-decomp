// Unit v51012: the rest of the interface's ui_utils.cpp helpers (main app of SimTheme Park 2.04b3):
//   UI_DrawPostcardText 0x1018bbc4, __sinit_ui_utils_cpp 0x1018bda8, MakeRenderAreaFromRAW 0x1018b434,
//   InterfaceDrawEngineHGradient::CalcUVs 0x10189d70, InterfaceVisitorTimeTextObject::Change 0x1018efcc.
#include <cstring>
#include <new>

#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "Text.h"
#include "TbITable.h"
#include "TbResource.h"
#include "keyboard.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

#include "LbPalette.h"
#include "TbFontRenderParams.h"
#include "TbIRLE4bitFont.h"
#include "TbGraphicFile.h"
#include "TbColour.h"
#include "UI_Base.h"
#include "ui_Postcard.h"

// 0x1038cd2c: the colour the module's static initialiser makes (opaque black and its palette index); the original name is unknown.
uint8_t gUIUtilsColour[5];

// 0x1018b434: describes `width` x `height` pixels at `pixels` as raw data of the given type. NULL area: nothing. Type 0: 24-bit
// (stride 3 x width, no alpha mask); 2: 32-bit ARGB; 3: 32-bit with the channels in the other order; any other type leaves the
// masks, depth and stride alone (the width, height and pixels are still stored).
void MakeRenderAreaFromRAW(void* pixels, int32_t width, int32_t height, RAWSurfaceType type, TbRenderArea* area) {
    if (area == nullptr) return;
    area->mData = static_cast<uint8_t*>(pixels);
    area->mWidth = width;
    area->mPictureHeight = height;
    uint32_t masks[4];
    if (type == 2) {
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) << 2);
        masks[0] = 0xff0000;
        masks[1] = 0xff00;
        masks[2] = 0xff;
        masks[3] = 0xff000000u;
        area->mDepth = 0x20;
        area->mPalettised = 0;
    } else if (type > 1) {
        if (type > 3) return;
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) << 2);
        masks[0] = 0xff00;
        masks[1] = 0xff0000;
        masks[2] = 0xff000000u;
        masks[3] = 0xff;
        area->mDepth = 0x20;
        area->mPalettised = 0;
    } else {
        if (type != 0) return;
        area->mStride = static_cast<int32_t>(static_cast<uint32_t>(width) * 3u);
        masks[0] = 0xff0000;
        masks[1] = 0xff00;
        masks[2] = 0xff;
        masks[3] = 0;
        area->mDepth = 0x18;
        area->mPalettised = 0;
    }
    std::memcpy(area->mF00, masks, sizeof(masks));
}

// 0x1018bbc4: draws the postcard's message (font 12, left and top justified, wrapped) into the postcard's pixels, through the UI's
// render area. `colour` has the red in the top byte, then green and blue (the lowest byte is not read); the last argument is unused.
void UI_DrawPostcardText(char16_t* text, void* pixels, int32_t width, int32_t height, SamsUtilities::URectangle::LongRect rect,
                         uint32_t colour, int32_t unused) {
    (void)unused;
    // TbColour's constructor is not translated (and the original does not run it for this local): build it over raw storage.
    alignas(TbColour) uint8_t storage[sizeof(TbColour)];
    TbColour& textColour = *reinterpret_cast<TbColour*>(storage);
    uint8_t red = static_cast<uint8_t>(colour >> 24);
    uint8_t green = static_cast<uint8_t>(colour >> 16);
    uint8_t blue = static_cast<uint8_t>(colour >> 8);
    textColour.mAlpha = 0xff;
    textColour.mRed = red;
    textColour.mGreen = green;
    textColour.mBlue = blue;
    textColour.mIndex = LbPalette_FindColour(&_lbGlobalPalette, red, green, blue);
    MakeRenderAreaFromRAW(pixels, width, height, static_cast<RAWSurfaceType>(3), &gUIRenderArea);
    gUIRenderAreaInUse = 1;
    gText.mRenderParams.UseBlendedMode();
    TbRect box;
    box.left = rect.left;
    box.top = rect.top;
    box.right = rect.right;
    box.bottom = rect.bottom;
    UI_DrawText(text, box, textColour, 12, 0, 0, 1, nullptr, 0);
    gText.mRenderParams.UseAlphaWriteMode();
    gUIRenderAreaInUse = 0;
}

// 0x1018bda8: the static initialiser of ui_utils.cpp.
void __sinit_ui_utils_cpp() {
    gUIUtilsColour[1] = 0;
    gUIUtilsColour[2] = 0;
    gUIUtilsColour[3] = 0;
    gUIUtilsColour[0] = 0xff;
    gUIUtilsColour[4] = LbPalette_FindColour(&_lbGlobalPalette, 0, 0, 0);
    new (&gUIRenderArea) TbRenderArea();
}

// 0x10189d70: the texture coordinates of the quad: the fraction / 100 on the edge between the halves.
void InterfaceDrawEngineHGradient::CalcUVs() {
    const float edge = mFraction / 100.0f;
    sUVSet* uvs = mMeshSet.mUVSets;
    uvs[0].mF00[1] = edge;
    uvs[0].mF00[5] = 1.0f;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[0] = edge;
    uvs[1].mF00[4] = 0.5f;
    uvs = mMeshSet.mUVSets;
    uvs[0].mF00[2] = edge;
    uvs[0].mF00[6] = 0.5f;
    uvs = mMeshSet.mUVSets;
    uvs[1].mF00[1] = edge;
    uvs[1].mF00[5] = 0.0f;
}

// 0x1018efcc: with flag 0x80 the text is made from the time (in hours, as the first word of the change): string 0x1b9 for up to
// 24 (the number itself, negative ones as their magnitude with the flag set), 0x1ba up to 48 and 0x1bb above, those two with the
// number divided by 24 (the days). Then the text object draws it (flag 0x100). Any other flags go straight to the base.
void InterfaceVisitorTimeTextObject::Change(uint32_t flags, DOChange* change) {
    if ((flags & 0x80) == 0) {
        InterfaceTextObjectSprite::Change(flags, change);
        return;
    }
    const int32_t time = static_cast<int32_t>((static_cast<uint32_t>(change->mF00[0]) << 24) |
                                              (static_cast<uint32_t>(change->mF00[1]) << 16) |
                                              (static_cast<uint32_t>(change->mF00[2]) << 8) | change->mF00[3]);
    char16_t text[0x44];
    TbITable* const table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    if (time > 0x18) {
        const uint32_t days = static_cast<uint32_t>((static_cast<uint64_t>(0xaaaaaaabu) * static_cast<uint32_t>(time)) >> 32) >> 4;
        RideAgeTag tag(static_cast<int32_t>(days), 0);
        tag.mId = 8;
        table->FormatString(text, 0x3f, time > 0x30 ? 0x1bb : 0x1ba, static_cast<TbVTag*>(&tag));
    } else {
        RideAgeTag tag(time, 0);
        if (time < 0) {
            tag.mValue = static_cast<int32_t>(0u - static_cast<uint32_t>(time));
            tag.mNegative = 1;
        }
        tag.mId = 0xb;
        table->FormatString(text, 0x3f, 0x1b9, static_cast<TbVTag*>(&tag));
    }
    DOTextStyle style;
    style.mText = text;
    DOChange shown;
    shown.mText = &style;
    InterfaceTextObjectSprite::Change(0x100, &shown);
}
