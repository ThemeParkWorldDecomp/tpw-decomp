// InGame.cpp (main app): CInGame. PARTIAL; untranslated.
#pragma once
#include "InterfaceFont.h"

class CInGame {
public:
    static void Init();   // 0x1014b2bc (called with no object): Start, nothing else (src/game/InGame.cpp)
    // 0x1014b304 (676 bytes, src/game/InGame.cpp): fills eight colours of gColours from the global palette (see there).
    static void Start();
};

// The file's one global object (__sinit_InGame_cpp, 0x1014b5cc): the font InitGeneralUI
// makes the debug font of. Original name unknown.
extern InterfaceFont gInGameFont;   // 0x1037f1a0
