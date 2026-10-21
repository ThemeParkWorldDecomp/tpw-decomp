// InterfaceBFCursor.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of InterfaceBFCursor code, named after it.

#include "Debug.h"
#include "Light.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_system.h"
#include "TbGraphicFile.h"
#include "TPConfig.h"
#include "TPSettings.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UIInterface.h"
#include <cstdio>

// The interface's colour cursors and the lobby's interface set-up (main app, ui_main.cpp range:
// LoadBFCurs 0x10185670, InitialiseCOLOURCursors 0x1018575c, InitLobbyUI 0x10186324; the
// InterfaceBFCursor constructor and LoadData 0x101fe208, 0x101fe52c, which the inventory files
// under TPEmbed.cpp; UIInterface::InitLobby 0x101ef750).

int32_t gTbReadHeaderFailed = -1;         // 0x102b155c
int32_t gTbLoadFailed = -1;               // 0x102b1560

// The interface's light (0x1038cc78). __sinit_ui_main_cpp (0x101877b8, untranslated) makes it a directional light
// (flags 2) of white pointing (0, 0, 1) when the program starts; until that function is translated this does the
// same here (InitLobbyUI sets the colour and direction again each time).
CLight gUILight;
namespace {
struct UILightSetup {
    UILightSetup() {
        gUILight.mFlags = 2;
        gUILight.mNormal.x = 0.0f;
        gUILight.mNormal.y = 0.0f;
        gUILight.mNormal.z = 1.0f;
        for (float& channel : gUILight.mColor) channel = 1.0f;
        gUILight.mColorWord = 0xffffffffu;
    }
} sUILightSetup;
}  // namespace

// ---- the cursor made from a picture

InterfaceBFCursor::InterfaceBFCursor() : mCursor(nullptr) {}

// The cursor made from a picture file, InterfaceBFCursor (main app 2.04b3): its destructor and four virtual members
// (0x101fe27c, 0x101fe338, 0x101fe390, 0x101fe404, 0x101fe478), which talk to the platform's cursor (Sam's Utilities'
// UCursor). The constructor and LoadData, which makes that cursor from the picture, are in src/ui_cursors.cpp; the base
// class InterfaceCursor and CursorTable (0x101cc6a0..0x101ccc8c) are in src/ui_DrawObject.cpp, where unit w21 put them.

using namespace SamsUtilities;

// Lets the platform's cursor go (a UCursorOpenGL: its virtual destructor with the delete flag), then the base class'
// destructor runs. mCursor is not cleared.
InterfaceBFCursor::~InterfaceBFCursor() {
    if (mCursor != nullptr) delete static_cast<UCursorOpenGL*>(mCursor);
}

// Whether the platform's cursor is shown. The library's IsVisible is static: it does not look at this cursor, and a
// cursor without a picture answers too.
uint8_t InterfaceBFCursor::GetVisible() { return UCursor::IsVisible(); }

// Show and Hide tell the library to show or hide the cursor (they nest: it keeps a count) and keep the count it
// answers, as a byte, in the visible flag. A cursor without a picture does nothing.
void InterfaceBFCursor::Show() {
    if (mCursor != nullptr) mVisible = static_cast<uint8_t>(UCursor::Show());
}

void InterfaceBFCursor::Hide() {
    if (mCursor != nullptr) mVisible = static_cast<uint8_t>(UCursor::Hide());
}

// Takes the cursor in use (a non-zero argument: its hot spot, the two signed bytes, goes to the platform's cursor
// first) or gives it up (0: the host's arrow comes back).
void InterfaceBFCursor::Use(int16_t use) {
    if (mCursor == nullptr) return;
    if (use != 0) {
        UPoint hotSpot;
        hotSpot.x = mHotX;
        hotSpot.y = mHotY;
        static_cast<UCursorOpenGL*>(mCursor)->SetHotspot(hotSpot);
        mCursor->Set();
    } else {
        mCursor->Reset();
    }
}

namespace {
// How the loader's answers are tested: the sign of the answer against whether the constant is 0. The
// constants are -1, so this is "the answer is negative" (a constant of 0 would turn the test round).
bool LoaderFailed(int32_t answer, int32_t constant) { return (answer >= 0) == (constant == 0); }
}  // namespace

// Reads a TGA of cursor frames 32 pixels wide side by side (the cursors are 128 x 32: four frames) and makes
// the platform's cursor from them: one UPixels per frame that points into the picture, in the format its depth
// gives (1 byte a pixel indexed, 2: 5, 3: 8, 4: 0xb, anything else 0xd), with the palette (converted from the
// loader's 256 entries to 32-bit pixels) when the file has one, magenta (0xff00ff) for transparent. The cursor
// changes frame every 300 milliseconds and has its hot spot at (0, 0). Both failures (the header, the pixels)
// log a line and answer 0; everything else answers 1, a cursor made or not.
int32_t InterfaceBFCursor::LoadData(const char* path) {
    using namespace SamsUtilities;
    TbTGALoader loader;
    TbRenderArea area;
    UPixels palette(kPixels32ARGB, 0x100, 1, nullptr);
    TbResult result = loader.ReadHeader(path, nullptr, &area);
    if (LoaderFailed(result.mCode, gTbReadHeaderFailed)) {
        TRACE("Cursor Failed on ReadHeader\n");
        return 0;
    }
    area.AllocMemory();
    result = loader.Load(area, path, nullptr, nullptr);
    if (LoaderFailed(result.mCode, gTbLoadFailed)) {
        TRACE("Failed on Load\n");
        return 0;
    }
    const uint32_t bytesPerPixel = static_cast<uint32_t>(area.mDepth + 7) >> 3;
    const uint32_t frames = static_cast<uint32_t>(area.mWidth) >> 5;
    const uint32_t stride = static_cast<uint32_t>(area.mStride);
    intptr_t format;
    switch (bytesPerPixel) {
    case 1: format = kPixels8Indexed; break;
    case 2: format = kPixels16BGRA; break;
    case 3: format = kPixels24BGR; break;
    case 4: format = kPixels32BGRA; break;
    default: format = 0xd; break;   // no such format: the pixels cannot be read
    }
    void* paletteData = nullptr;
    if (loader.mHasPalette != 0) {
        UPixels source(kPixels32RGBA, loader.mPalette, 0x100, 1, -1, nullptr);
        palette.Convert(source, false);
        paletteData = palette.GetData();
    }
    UPixels* frameArray = new UPixels[frames];
    if (frameArray != nullptr) {
        uint8_t* data = area.mData;
        for (int32_t i = 0; i < static_cast<int32_t>(frames); ++i) {
            frameArray[i].SetData(format, data, 0x20, 0x20, static_cast<int32_t>(stride), static_cast<UPixel*>(paletteData));
            frameArray[i].SetTransparentColour(0x00ff00ff);
            data += bytesPerPixel * 0x20;
        }
        UPoint hotSpot = {0, 0};
        mCursor = new UCursorOpenGL(hotSpot, frameArray, static_cast<int32_t>(frames), nullptr);
        if (mCursor != nullptr) {
            const UMicroseconds rate = {static_cast<int64_t>(300) * 1000};   // 300 ms
            mCursor->SetRate(rate);
        }
        delete[] frameArray;
    }
    return 1;
}
