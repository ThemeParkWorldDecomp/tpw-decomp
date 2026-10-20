// The globe's destructor, its key function: the vtable is emitted here (lead 2026-10-20, v51031, out of
// src/MapCell_kinds_LLCC_DrawLine_2.cpp and in place of the empty one src/lobby_globe_close_CLBScreenComponentOnlineGlobe_Show_2.cpp had).
#include "lobby.h"

// 0x100ac534 (112 bytes): the Mac stores the vtable, then calls its screen's slot 0x3c with the globe (no NULL test on the screen);
// the delete storage flag (r4 > 0 as a short: operator delete) is the compiler's job here.
CLBGlobe::~CLBGlobe() { mScreen->UnRegisterGlobe(this); }
