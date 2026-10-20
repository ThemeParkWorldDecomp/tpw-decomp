#include <cstdint>
#include "DrawRenderClasses.h"
#include "InterfaceWindow.h"
#include "ui_main.h"

// 0x1022bff8: forwards the four arguments to the global map object's MapAreaCallback.
void UI_2DMap_MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    gUI2DMap.MapAreaCallback(window, message, a, b);
}
