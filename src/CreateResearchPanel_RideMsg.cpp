// The five bar message functions of the research panel: a button press (0x100) tells TankButton which bar it was and
// whether it was the button 0xd2d03, then every message goes on to AnimatingPumpMsg, whose answer is returned.
#include <cstdint>

#include "InterfaceWindow.h"
#include "ui_main.h"

void TankButton(int32_t which, InterfaceWindow* window, int32_t flag);                          // 0x101b4224
int32_t AnimatingPumpMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101b49d0

static int32_t BarMsg(int32_t which, InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) TankButton(which, window, a == 0xd2d03 ? 1 : 0);
    return AnimatingPumpMsg(window, message, a, b);
}

int32_t RideMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(0, window, message, a, b); }
int32_t UpgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(4, window, message, a, b); }
int32_t ShopMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(1, window, message, a, b); }
int32_t ShowMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(2, window, message, a, b); }
int32_t FeatureMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) { return BarMsg(3, window, message, a, b); }
