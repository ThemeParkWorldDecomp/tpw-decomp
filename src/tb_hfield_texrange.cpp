// The engine height field's procedural texture window setter (engine symbols shared, tb_hfield.cpp range; unit w112a).
#include "tb_hfield.h"

// 0x1000db80 (20 bytes): four stores, columns x0..x1 and rows y0..y1 of the window (PT_SetRange's callee).
void CHField::ProcTextureRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    mF234 = x0;
    mF238 = y0;
    mF23C = x1;
    mF240 = y1;
}
