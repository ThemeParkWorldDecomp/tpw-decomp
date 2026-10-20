// The interface's colour cursors and the lobby's interface set-up (main app, ui_main.cpp range:
// LoadBFCurs 0x10185670, InitialiseCOLOURCursors 0x1018575c, InitLobbyUI 0x10186324; the
// InterfaceBFCursor constructor and LoadData 0x101fe208, 0x101fe52c, which the inventory files
// under TPEmbed.cpp; UIInterface::InitLobby 0x101ef750).
#include <cstdio>

#include "Debug.h"
#include "Light.h"
#include "MapWho.h"
#include "TPConfig.h"
#include "TPSettings.h"
#include "TbGraphicFile.h"
#include "UIInterface.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "tb_resource_system.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

char gBFCursorFolderSet;                  // 0x10380b8c
const char* gBFCursorFolder;              // 0x10380b90
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

// The cursor of the file "data:UI:cursors:<name>" (found with FindFilePath, which answers the path to read), or
// NULL when there is no such file. The folder's name is set on the first call (a static in the original).
InterfaceBFCursor* LoadBFCurs(const char* name) {
    InterfaceBFCursor* cursor = nullptr;
    if (gBFCursorFolderSet == 0) {
        gBFCursorFolder = "data:UI:cursors";
        gBFCursorFolderSet = 1;
    }
    char path[1024];
    // (sprintf in the original; the names are short)
    std::snprintf(path, sizeof path, "%s:%s", gBFCursorFolder, name);
    const char* file = FindFilePath(path, 1, nullptr);
    if (file != nullptr) {
        cursor = new InterfaceBFCursor;
        if (cursor != nullptr) cursor->LoadData(file);
    }
    return cursor;
}

// Loads the 22 cursors into gCursorTable by number: a picture that is not found leaves its number empty. The names
// are those of the files in data:UI:cursors (CNor normal, Cbus, Cpat, Cque, Clin, Cpla, Cnog, Cpic, Ccar, Cera, Cpyl,
// Ccam, Cash, Ctra, Chei, Crot, Cpiv, Csdn, Csup, CEnd, CMov, CCro). The car cursor (number 9) has its hot spot at
// (10, 64) and the last (number 0x15, CCro) at (16, 16).
void InitialiseCOLOURCursors() {
    CursorTable* table = &gCursorTable;
    InterfaceBFCursor* cursor;
    cursor = LoadBFCurs("CNor.tga");
    if (cursor != nullptr) table->SetEntry(0, cursor);
    cursor = LoadBFCurs("Cbus.tga");
    if (cursor != nullptr) table->SetEntry(1, cursor);
    cursor = LoadBFCurs("Cpat.tga");
    if (cursor != nullptr) table->SetEntry(3, cursor);
    cursor = LoadBFCurs("Cque.tga");
    if (cursor != nullptr) table->SetEntry(4, cursor);
    cursor = LoadBFCurs("Clin.tga");
    if (cursor != nullptr) table->SetEntry(5, cursor);
    cursor = LoadBFCurs("Cpla.tga");
    if (cursor != nullptr) table->SetEntry(7, cursor);
    cursor = LoadBFCurs("Cnog.tga");
    if (cursor != nullptr) table->SetEntry(8, cursor);
    cursor = LoadBFCurs("Cpic.tga");
    if (cursor != nullptr) table->SetEntry(2, cursor);
    cursor = LoadBFCurs("Ccar.tga");
    if (cursor != nullptr) {
        table->SetEntry(9, cursor);
        cursor->SetHot(10, 64);
    }
    cursor = LoadBFCurs("Cera.tga");
    if (cursor != nullptr) table->SetEntry(10, cursor);
    cursor = LoadBFCurs("Cpyl.tga");
    if (cursor != nullptr) table->SetEntry(11, cursor);
    cursor = LoadBFCurs("Ccam.tga");
    if (cursor != nullptr) table->SetEntry(0x13, cursor);
    cursor = LoadBFCurs("Cash.tga");
    if (cursor != nullptr) table->SetEntry(0x14, cursor);
    cursor = LoadBFCurs("Ctra.tga");
    if (cursor != nullptr) table->SetEntry(6, cursor);
    cursor = LoadBFCurs("Chei.tga");
    if (cursor != nullptr) table->SetEntry(12, cursor);
    cursor = LoadBFCurs("Crot.tga");
    if (cursor != nullptr) table->SetEntry(13, cursor);
    cursor = LoadBFCurs("Cpiv.tga");
    if (cursor != nullptr) table->SetEntry(14, cursor);
    cursor = LoadBFCurs("Csdn.tga");
    if (cursor != nullptr) table->SetEntry(15, cursor);
    cursor = LoadBFCurs("Csup.tga");
    if (cursor != nullptr) table->SetEntry(16, cursor);
    cursor = LoadBFCurs("CEnd.tga");
    if (cursor != nullptr) table->SetEntry(0x12, cursor);
    cursor = LoadBFCurs("CMov.tga");
    if (cursor != nullptr) table->SetEntry(0x11, cursor);
    cursor = LoadBFCurs("CCro.tga");
    if (cursor != nullptr) {
        table->SetEntry(0x15, cursor);
        cursor->SetHot(16, 16);
    }
}

// ---- the lobby

// Empties the message queues, lights the interface's light white from the front (its colour word all ones,
// the four colours 1.0, direction (-0.3, -0.5, 0.8): each change tells the engine the lights changed), links it
// into the map's light list 0x20 and turns the help text on or off by the player's tooltips setting.
void InitLobbyUI() {
    UI_FlushAllMessages();
    CLight* light = &gUILight;
    gei_System.UpdateLights();
    light->mColorWord = 0xffffffffu;
    light->mColor[0] = 1.0f;
    light->mColor[1] = 1.0f;
    light->mColor[2] = 1.0f;
    light->mColor[3] = 1.0f;
    light = &gUILight;
    gei_System.UpdateLights();
    light->mNormal.x = -0.3f;
    light->mNormal.y = -0.5f;
    light->mNormal.z = 0.8f;
    gei_MapWho->Link(static_cast<CMapLinkBase*>(light), 0x20);
    if (gPlayerSettings.mCurrent.mTooltipsOn)
        UI_EnableHelpText();
    else
        UI_DisableHelpText();
}

void UIInterface::InitLobby() { InitLobbyUI(); }
