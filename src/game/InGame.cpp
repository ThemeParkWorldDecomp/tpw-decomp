// InGame.cpp: name proven by __sinit_InGame_cpp in the main application.

#include "ActionRecorder.h"
#include "bfl_FileSystem.h"
#include "Colours.h"
#include "Debug.h"
#include "ExitErr.h"
#include "Game.h"
#include "InGame.h"
#include "keytable_actions.h"
#include "LbFile.h"
#include "LbPalette.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "TbTList.h"
#include "ui_main.h"
#include "World.h"
#include "WorldSavvy.h"
#include "WorldThings.h"
#include <cstdint>
#include <new>

// InGame.cpp (main app of SimTheme Park 2.04b3). PARTIAL: the file's global object
// (__sinit_InGame_cpp, 0x1014b5cc) and CInGame::Init so far.

InterfaceFont gInGameFont;   // 0x1037f1a0

// Only calls Start (untranslated).
void CInGame::Init() { Start(); }   // 0x1014b2bc

// CInGame::Start (main app, SimTheme Park 2.04b3, InGame.cpp; unit w75).

namespace {

// One colour of a ramp from palette entry `index`: the Mac builds the four bytes on the stack (0xff, then the entry's first three
// bytes, which are red, green and blue) and stores them with one stw at the colour's address (unaligned), then stores the index
// byte after them.
void SetFromPalette(TbColour& colour, uint8_t index) {
    const uint8_t* entry = reinterpret_cast<const uint8_t*>(&_lbGlobalPalette) + index * 4;
    colour.mAlpha = 0xff;
    colour.mRed = entry[0];
    colour.mGreen = entry[1];
    colour.mBlue = entry[2];
    colour.mIndex = index;
}

}  // namespace

// 0x1014b304 (676 bytes). Eight colours of gColours are set from _lbGlobalPalette: the yellow ramp's colours 8, 6, 4 and 2 (Mac
// offsets 0xd0, 0xc6, 0xbc, 0xb2) from the entries 0x85, 0x87, 0x89 and 0x8b, and the green ramp's colours 8, 6, 4 and 2 (Mac offsets
// 0x60, 0x56, 0x4c, 0x42) from the entries 0xa7, 0xa9, 0xab and 0xad. Nothing else is touched (the ramps' selected colours are left).
void CInGame::Start() {
    SetFromPalette(gColours.mYellow.mColours[8], 0x85);
    SetFromPalette(gColours.mYellow.mColours[6], 0x87);
    SetFromPalette(gColours.mYellow.mColours[4], 0x89);
    SetFromPalette(gColours.mYellow.mColours[2], 0x8b);
    SetFromPalette(gColours.mGreen.mColours[8], 0xa7);
    SetFromPalette(gColours.mGreen.mColours[6], 0xa9);
    SetFromPalette(gColours.mGreen.mColours[4], 0xab);
    SetFromPalette(gColours.mGreen.mColours[2], 0xad);
}

// Unit v51072: CActionRecorder::ProcessAction (count, text), the 8 byte stand-ins of the recorder and the visitor panel, the destructors
// of the wide string, CMemFile and the thing id list, the file initialisers of InGame.cpp and ExitErr.cpp and a few key table actions.

extern "C" void __register_global_object(void* object, void* destructor, void* node);

uint8_t gInGameFontNode[16];   // 0x1037f1b4
uint8_t gInGameFontDtor[8];    // stands for the destructor word 0x102673f0

// 0x1014b5cc
void __sinit_InGame_cpp() {
    new (&gInGameFont) InterfaceFont();
    __register_global_object(&gInGameFont, gInGameFontDtor, gInGameFontNode);
}
