// intrface.cpp: name proven by __sinit_intrface_cpp in the main application.

#define TPW_GLOBE_SHOWHIDE_REAL   // the download globe's Show calls the online globe's real Show (0x100ad788), not the inline base
#include "analyser.h"
#include "BMP_InsideArea.h"
#include "CellBase.h"
#include "Debug.h"
#include "ESP_sprites.h"
#include "FS_Online.h"
#include "HLSprite.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "LbDrawText.h"
#include "LLCellSystem.h"
#include "lobby.h"
#include "MainBalance.h"
#include "MapCell.h"
#include "ObjectParts.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "rsse.h"
#include "tb_printf_opengl.h"
#include "TbColour.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "TrackCell.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

// Bullfrog TbColour (main app, 0x1007d9d4): the default constructor.

// 0x1007d9d4: the alpha, red, green, blue and palette index bytes are all cleared.
TbColour::TbColour() : mAlpha(0), mRed(0), mGreen(0), mBlue(0), mIndex(0) {}

// CFont::Set (main 0x1007da14, 144 bytes): selects an old sprite font, unit w37b.

CFont::FontEntry* gCFontTable; // Mac 0x102dc1a8

int32_t CFont::Set(FontId font, RenderType render) {
    if (gCFontTable == nullptr || gCFontTable[font].mFont == nullptr) return 0;
    if (render == kSprite) {
        LbDraw_SetTextFont(&_lbSpriteRender, gCFontTable[font].mFont);
    } else if (render == kOneColour) {
        LbDraw_SetTextFont(&_lbOneColourSpriteRender, gCFontTable[font].mFont);
    }
    return 1;
}

// Unit v51046: nine small functions of the lobby, the interface and the ESP sprites (main app).
//   OnlineIslandParkDownloadCompleteCallback 0x100b4420, PostGetTop10ParksListCallbackFunction 0x100b3e28,
//   CLBScreenComponentIslands::ProcessCancel 0x100b1f3c, __sinit_lobby_parser_cpp 0x100b6ae4,
//   CLBScreenComponentDownloadGlobe::Show 0x100ae590, CLBScreenComponentDownloadGlobe::ProcessCancel 0x100ae46c,
//   CIntrface::DrawFPSCounter 0x1007dae8, __sinit_HLSprite_cpp 0x1007d53c, ESP_GetRandomActionFrom 0x1007c9f8.

// ---- the frame rate

void CIntrface::DrawFPSCounter(float fps) {
    char* text = reinterpret_cast<char*>(mF10);   // this + 0x10, 0x14 bytes on the Mac
    std::sprintf(text, "FPS: %4g\n", static_cast<double>(fps));   // 0x1023de3e
    gOpenGLPrintf.printf_xy(0, 0, text);                           // the engine's gei_Printf
}

// intrface_process.cpp (main, unit w131): CIntrface::Process 0x1007db78 (24 bytes).

// Counts the call (the word at +0xc, read before anything is written) and clears the word at +0.
void CIntrface::Process() {
    const int32_t count = mProcessCount;
    mPointedThing = nullptr;
    mProcessCount = count + 1;
}

// intrface.cpp range of the main app (SimTheme Park 2.04b3), unit w133: CIntrface::DoPointedThingComputations (0x1007dbb8, 500 bytes).

// Works out what the pointer is over. Both the staff-or-guest slot and the under-pointer slot are cleared. When there is a pointed thing
// and its kind is one of 1, 4, 5, 6, 7, 8 or 0x12 (a person, a staff member, a guest, a mechanic and so on), it goes in the staff-or-guest
// slot. The cell column of the id the pointer is over (mF24 - 1, low 7 bits) is looked up with LLCR_OverRide; unless it answers the "no
// cell" halfword, the thing array's row 0 must be free (GAME_ASSERTMSG) and the thing in the answered row goes in the under-pointer slot.
// Last, a pointed thing that is not set becomes the thing under the pointer.
void CIntrface::DoPointedThingComputations() {
    mPointedStaffOrGuest = nullptr;
    mThingUnderPointer = nullptr;
    if (mPointedThing != nullptr) {
        const uint8_t kind = mPointedThing->mKind;
        if (kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12) mPointedStaffOrGuest = mPointedThing;
    }
    uint16_t answer;
    const uint32_t cell = static_cast<uint32_t>(mF24) - 1;
    LLCR_OverRide(&answer, static_cast<int32_t>(cell & 0x7f), static_cast<int32_t>(cell >> 7));
    if (answer != gLLCRNoCell) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
        mThingUnderPointer = static_cast<CThing*>(gThingArray[answer].mThing);
    }
    if (mPointedThing == nullptr) mPointedThing = mThingUnderPointer;
}

// intrface.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: CIntrface::SetCurrentThing (0x1007dde8, 8 bytes).

void CIntrface::SetCurrentThing(CThing* thing) { mPointedThing = thing; }

int32_t gIntrfaceDC1D8; // 0x102dc1d8
int32_t gIntrfaceDC1DC; // 0x102dc1dc

// 0x1007de28
void __sinit_intrface_cpp() {
    gIntrfaceDC1DC = 0;
    gIntrfaceDC1D8 = 0x10000;
    gIntrface.mF24 = 1;
    gIntrface.mShowFPS = 0;
}
