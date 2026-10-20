// The login panel's window description (main app, SimTheme Park 2.04b3, data at 0x102abbb0, 371 words), for InterfaceWindow::Build.
// CreateLoginPanel (0x101ae9e0, a later unit) builds the panel from it when the lobby starts without a player. Unit w36 carries the
// words over so that the three lobby scripts live together (the others are gLobbyWindowData in src/lobby.cpp and
// gIslandLobbyWindowData in src/UI_IslandLobby.cpp); the format is described in src/InterfaceWindow_script.cpp. Each line is one
// record of the script; the window names in the comments are invented. The panel is the whole 2048 x 1536 screen with four list
// rows (0x7a14, 0x7a19, 0x7a1a, 0x7a1b: a name area with a button, a picture, a draw object), a panel at the lower left and a fifth
// row that starts to the right of the screen (its left edge is 1264 and its right 2428; it has a client rectangle but no children).
// The test compares all 371 words with the program image.
#include "lobby.h"

int16_t gLoginPanelWindowData[] = {
    0, 1, 1, 0, 31251, 0, 0, 0, 2048, 1536,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a13 (the login panel, the whole screen), rect (0, 0, 2048, 1536)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      0, 1, 1, 0, 31252, 0, 884, 46, 2048, 193,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a14 (list row 1), rect (884, 46, 2048, 193)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 75, 1716, 164,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 75, 1716, 164)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 68, 2021, 171,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 68, 2021, 171)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 68, 1865, 171,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 68, 1865, 171)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31256, 0, 32, 1321, 442, 1497,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a18 (lower left panel), rect (32, 1321, 442, 1497)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -630, -4961,   // 1: draw object 0xec9ffd8a
        5,   // 5: end of this level
      0, 1, 1, 0, 31257, 0, 884, 245, 2048, 392,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a19 (list row 2), rect (884, 245, 2048, 392)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 274, 1716, 363,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 274, 1716, 363)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 267, 2021, 370,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 267, 2021, 370)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 267, 1865, 370,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 267, 1865, 370)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31258, 0, 884, 444, 2048, 591,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1a (list row 3), rect (884, 444, 2048, 591)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 473, 1716, 562,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 473, 1716, 562)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 466, 2021, 569,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 466, 2021, 569)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 466, 1865, 569,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 466, 1865, 569)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31259, 0, 884, 643, 2048, 790,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1b (list row 4), rect (884, 643, 2048, 790)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 1, 1, 0, 31253, 0, 941, 672, 1716, 761,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a15 (a row's name area), rect (941, 672, 1716, 761)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          0, 2, 1, 0, 31254, 0, 1919, 665, 2021, 768,   // 0: child, kind 2 (button), attributes 0x1, id 0x7a16 (a row's button), rect (1919, 665, 2021, 768)
            17, -1, -1,   // 17: help text number -1
            18, -1, -1,   // 18: word at +0x90 is -1
            1, 658, 352,   // 1: draw object 0x01600292
            5,   // 5: end of this level
          5,   // 5: end of this level
        0, 6, 1, 0, 31255, 0, 1762, 665, 1865, 768,   // 0: child, kind 6 (image), attributes 0x1, id 0x7a17 (a row's picture), rect (1762, 665, 1865, 768)
          17, -1, -1,   // 17: help text number -1
          18, -1, -1,   // 18: word at +0x90 is -1
          1, 4062, 31049,   // 1: draw object 0x79490fde
          5,   // 5: end of this level
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      0, 1, 1, 0, 31260, 0, 1264, 895, 2428, 1042,   // 0: child, kind 1 (window), attributes 0x1, id 0x7a1c (row sliding in from the right), rect (1264, 895, 2428, 1042)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        3, 1339, 924, 2039, 1013,   // 3: client rectangle (1339, 924, 2039, 1013)
        1, -16748, -4558,   // 1: draw object 0xee32be94
        5,   // 5: end of this level
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gLoginPanelWindowData) == 371 * sizeof(int16_t), "gLoginPanelWindowData has 371 words");
