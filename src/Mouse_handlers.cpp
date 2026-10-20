// Mouse.cpp (main app), unit w142: CMouse's three handlers the frame and the window messages call, and DoubleClick. All four hand the
// pointer to the current mouse mode (gMouseMode, re-read at every call, never tested for NULL) through its vtable.
#include <cstdint>

#include "Mouse.h"

// The two button flags of CMouse's data (names invented): ProcessIngameMouse sets the left's (0x102dc1ec) and the right's (0x102dc1e8),
// ImmediateMouseHandler passes them to OnMouseMove and picks WhileLeftDown or WhileRightDown by them.
int32_t gMouseLeftDown;    // 0x102dc1ec
int32_t gMouseRightDown;   // 0x102dc1e8

// 0x1007e094 (216 bytes). `this` is not used (the Mac's r3 is gMouse). The positions compare as signed halfwords-in-words. The last
// position's two words both get x (the Mac stores x at 0x102dc1e4 and again at 0x102dc1e0, never y).
void CMouse::ImmediateMouseHandler(int32_t x, int32_t y) {
    if (x != gMouseWord1E4 || y != gMouseWord1E0) {
        gMouseMode->OnMouseMove(x, y, gMouseLeftDown, gMouseRightDown);
    }
    if (gMouseLeftDown != 0) {
        gMouseMode->WhileLeftDown(x, y);
    } else if (gMouseRightDown != 0) {
        gMouseMode->WhileRightDown(x, y);
    }
    gMouseWord1E4 = x;
    gMouseWord1E0 = x;
}

// 0x1007de84 (248 bytes): key 0xf0 is the left button, 0xf1 the right; `down` 1 presses, anything else releases. Any other key does
// nothing. The Mac answers 1 in r3 on every path; the callers ignore it, so it is void here. `data` is not used.
void CMouse::ProcessIngameMouse(int32_t key, int32_t down, int32_t x, int32_t y, void* data) {
    (void)data;
    if (key == 0xf0) {
        if (down == 1) {
            gMouseMode->OnLeftClick(x, y);
            gMouseLeftDown = 1;
        } else {
            gMouseMode->OnLeftRelease(x, y);
            gMouseLeftDown = 0;
        }
    } else if (key == 0xf1) {
        if (down == 1) {
            gMouseMode->OnRightClick(x, y);
            gMouseRightDown = 1;
        } else {
            gMouseMode->OnRightRelease(x, y);
            gMouseRightDown = 0;
        }
    }
}

// 0x1007e248 (52 bytes).
void CMouse::DoubleClick(int32_t x, int32_t y) {
    gMouseMode->OnDoubleClick(x, y);
}
