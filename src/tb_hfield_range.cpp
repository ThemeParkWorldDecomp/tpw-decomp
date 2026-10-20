// The engine's procedural texture range (engine symbols shared, tb_hfield.cpp range; unit w111).
#include "tb_hfield.h"

// 0x1000bde4 (80 bytes): hands the range to the height field, when there is one.
void PT_SetRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (gei_HField != nullptr) gei_HField->ProcTextureRange(x0, y0, x1, y1);
}
