// Global objects whose constructors read other global objects, defined in ONE file in the
// order the original's static initialisers run. C++ fixes the order of construction
// within a file and leaves it open between files; the original relied on its link order.
//
// gTPGlobals (__sinit_TPGlobals_cpp, 0x1023331c; object at 0x103b74c8): the default names.
// gGame (__sinit_Game_cpp, 0x1014b118; object at 0x1037f134): its constructor copies one of
// those names.
#include "Game.h"
#include "TPGlobals.h"

TPGlobals gTPGlobals;
CGame gGame;
