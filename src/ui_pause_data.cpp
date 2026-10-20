// ui_mainpanel2.cpp's data (main app, unit w88): the pause window's script (0x102aacfc) and its animation (0x102aadbc). Names invented.
#include "ui_main.h"

int16_t gPauseDialogDescription[21] = {
    0, 1, 1, 0, 5094, 0, 174, 500, 1874, 900,   // level 0, record 0
    17, -1, -1,   // level 1, record 17
    18, -1, -1,   // level 1, record 18
    1, 17852, -22616,   // level 1, record 1
    5,   // level 1, record 5
    5,   // level 0, record 5
};

// Four frames of deltas (left, top, right, bottom), then the command "go to frame 0".
UI_RectDelta gPauseAnim[5] = {
    {-2, -6, 2, 2},
    {-2, -6, 2, 2},
    {2, 6, -2, -2},
    {2, 6, -2, -2},
    {kAnimGoto, 0, 0, 0},
};
