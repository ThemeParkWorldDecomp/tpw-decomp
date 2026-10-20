// UI_Base.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: UI_GetMouseXY (0x101ca900, 28 bytes).
#include "UI_Base.h"
#include "ui_main.h"

// The last pointer position UI_DevicePointerMove stored.
void UI_GetMouseXY(int16_t& x, int16_t& y) {
    x = gUIPointerX;
    y = gUIPointerY;
}
