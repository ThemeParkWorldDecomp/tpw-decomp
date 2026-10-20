// CSwitch's start-up and shutdown (engine library of SimTheme Park 2.04b3,
// 0x100326f0..0x1003287c; the inventory files the range under Wavelet.cpp).
#include "tb_switch.h"

#include <new>

#include "tb_error.h"

CSwitch* gei_Switch = nullptr;

int32_t Switch_Init() {
    if (gei_Switch != nullptr) return 0;
    gei_Switch = new (std::nothrow) CSwitch;
    if (gei_Switch == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t Switch_Close() {
    if (gei_Switch == nullptr) return 0;
    delete gei_Switch;
    gei_Switch = nullptr;
    return 1;
}
