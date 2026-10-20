// The name entry dialog's window description (main app, SimTheme Park 2.04b3, data at 0x102abe96, 183 words), for InterfaceWindow::Build.
// NewName (0x101af60c, src/ui_newname.cpp) builds the dialog from it. It follows the login panel's description in the image (the
// panel's 371 words end at 0x102abe96; see src/ui_loginpanel_data.cpp). Each line is one record of the script (the format is described in
// src/InterfaceWindow_script.cpp); the window names in the comments are invented from what NewName does with the ids. The dialog 0x708 holds
// the title 0x709, the name entry 0x70a, a picture 0x70b, the check buttons 0x70c with their two buttons 0x70d and 0x70e, and the two
// buttons with the ids -2 and -1 that come first. The test compares all 183 words with the program image.
#include "lobby.h"

int16_t gNewNameWindowData[] = {
    0, 1, 1, 0, 1800, 0, 500, 233, 1516, 830,   // 0: child, kind 1 (window), attributes 0x1, id 0x708 (the dialog), rect (500, 233, 1516, 830)
      17, -1, -1,   // 17: help text number -1
      18, -1, -1,   // 18: word at +0x90 is -1
      1, 4894, -12909,   // 1: draw object 0xcd93131e
      0, 2, 1, 0, -2, -1, 1387, 613, 1470, 696,   // 0: child, kind 2 (button), attributes 0x1, id -2 (a button), rect (1387, 613, 1470, 696)
        17, 2, 0,   // 17: help text number 2
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 16207, 5804,   // 1: draw object 0x16ac3f4f
        5,   // 5: end of this level
      0, 2, 1, 0, -1, -1, 1376, 524, 1460, 608,   // 0: child, kind 2 (button), attributes 0x1, id -1 (a button), rect (1376, 524, 1460, 608)
        17, 360, 0,   // 17: help text number 360
        18, -1, -1,   // 18: word at +0x90 is -1
        1, -7029, 5287,   // 1: draw object 0x14a7e48b
        5,   // 5: end of this level
      0, 1, 1, 0, 1801, 0, 655, 285, 1315, 365,   // 0: child, kind 1 (window), attributes 0x1, id 0x709 (the title), rect (655, 285, 1315, 365)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        5,   // 5: end of this level
      0, 5, 1, 0, 1802, 0, 593, 616, 1216, 698,   // 0: child, kind 5 (edit box), attributes 0x1, id 0x70a (the name entry), rect (593, 616, 1216, 698)
        17, 356, 0,   // 17: help text number 356
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 15424, -25732,   // 1: draw object 0x9b7c3c40
        3, 623, 632, 1186, 677,   // 3: client rectangle (623, 632, 1186, 677)
        5,   // 5: end of this level
      0, 6, 1, 0, 1803, 0, 1241, 606, 1343, 708,   // 0: child, kind 6 (image), attributes 0x1, id 0x70b (a picture), rect (1241, 606, 1343, 708)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        1, 4062, 31049,   // 1: draw object 0x79490fde
        5,   // 5: end of this level
      0, 8, 17, 0, 1804, 0, 735, 377, 1235, 597,   // 0: child, kind 8 (check buttons), attributes 0x11, id 0x70c (the check buttons), rect (735, 377, 1235, 597)
        17, -1, -1,   // 17: help text number -1
        18, -1, -1,   // 18: word at +0x90 is -1
        0, 2, 17, 0, 1805, 0, 764, 405, 1206, 476,   // 0: child, kind 2 (button), attributes 0x11, id 0x70d (the first check button), rect (764, 405, 1206, 476)
          17, 358, 0,   // 17: help text number 358
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -11071, -30933,   // 1: draw object 0x872bd4c1
          5,   // 5: end of this level
        0, 2, 17, 0, 1806, 0, 764, 498, 1206, 569,   // 0: child, kind 2 (button), attributes 0x11, id 0x70e (the second check button), rect (764, 498, 1206, 569)
          17, 359, 0,   // 17: help text number 359
          18, -1, -1,   // 18: word at +0x90 is -1
          1, -11071, -30933,   // 1: draw object 0x872bd4c1
          5,   // 5: end of this level
        1, -12040, -17669,   // 1: draw object 0xbafbd0f8
        5,   // 5: end of this level
      5,   // 5: end of this level
    5,   // 5: end of this level
};
static_assert(sizeof(gNewNameWindowData) == 183 * sizeof(int16_t), "gNewNameWindowData has 183 words");
