// TPCamera.cpp (main app): Will_HandleCameraMessages 0x100331c4 (732 bytes), the park camera's share of the game window's
// messages (Engine_GameWinMsg and Engine_GameFirstPersonMsg pass every message on). Translated by the maintainer 2026-10-14
// from the listing; module test_CameraMessages.
//
// The pointer's position in a message is in the UI's 2048 x 1536 space (x in the high half, y in the low half); the camera
// keeps it in screen pixels in gPointerScreenXY, which HandleCamera reads for the edge scroll and the right-button drag. Bit
// 0x100 of gFirstPersonFlags stops the edge scroll: set while the pointer is outside the game window.
#include <cstdint>

#include "TPCamera.h"
#include "UI_Base.h"
#include "ppc_math.h"

float gPointerScreenXY[2];          // 0x102b5a94
int8_t gCameraYawDragScaleOnce;     // 0x102b5a18
float gCameraYawDragScale;          // 0x102b5a1c
int8_t gCameraMessagesF5A10Once;    // 0x102b5a10
float gCameraMessagesF5A14;         // 0x102b5a14

namespace {
constexpr int32_t kMsgFrame = 0x1e;
constexpr int32_t kMsgModeOff = 0x20;
constexpr int32_t kMsgReset = 0x22;
constexpr int32_t kMsgPointerEnter = 0x10001;
constexpr int32_t kMsgPointerLeave = 0x10002;
constexpr int32_t kMsgPointerMove = 0x10003;
constexpr int32_t kMsgButtonUp = 0x10004;
constexpr int32_t kMsgButtonDown = 0x10005;
constexpr int32_t kMsgWheel = 0x1000d;

// The Mac's int-to-float: the magic double minus 2^52 + 2^31, rounded to single by fsubs.
float FromInt(int32_t v) { return static_cast<float>(static_cast<double>(v)); }

// The camera mode bit of a mouse button (0 left, 1 right, 2 middle); 0 for any other value.
uint32_t ButtonBit(int32_t button) {
    if (button == 0) return 1;
    if (button == 1) return 4;
    if (button == 2) return 2;
    return 0;
}
}  // namespace

void Will_HandleCameraMessages(int32_t message, int32_t a, int32_t b) {
    (void)b;
    // Both scaled positions are worked out before the switch (32-bit products; the divisions truncate toward zero).
    const int32_t width = gUIRealScreenWidth;
    const int32_t height = gUIRealScreenHeight;
    const int32_t x = static_cast<int32_t>((static_cast<uint32_t>(a) >> 16) * static_cast<uint32_t>(width)) / 2048;
    const int32_t y = static_cast<int32_t>((static_cast<uint32_t>(a) & 0xffffu) * static_cast<uint32_t>(height)) / 1536;

    // Function statics: the Mac gives them their values on the first call.
    if (gCameraYawDragScaleOnce == 0) {
        gCameraYawDragScale = 1.0f / 2048.0f;   // 0x3a000000
        gCameraYawDragScaleOnce = 1;
    }
    if (gCameraMessagesF5A10Once == 0) {
        gCameraMessagesF5A14 = 0.0f;
        gCameraMessagesF5A10Once = 1;
    }

    switch (message) {
    case kMsgPointerEnter:
        gFirstPersonFlags &= ~0x100u;
        [[fallthrough]];
    case kMsgPointerMove:
        gPointerScreenXY[0] = FromInt(x);
        gPointerScreenXY[1] = FromInt(y);
        break;
    case kMsgPointerLeave:
        gFirstPersonFlags |= 0x100u;
        break;
    case kMsgButtonUp:
        gCameraModeA &= ~ButtonBit(a);
        break;
    case kMsgButtonDown:
        gCameraModeA |= ButtonBit(a);
        break;
    case kMsgWheel:
        // Wheel back (a negative delta) zooms out by 10, forward zooms in.
        if (a < 0) gCameraF68 = gCameraF68 + 10.0f;
        else gCameraF68 = gCameraF68 - 10.0f;
        break;
    case kMsgFrame:
        // With the middle button held the key yaw turns by the pointer's place across the screen: x / width - 0.5 radians
        // a frame (0 in the middle), worked as (2048 x / width - 1024) / 2048 with one fused multiply-add.
        if ((gCameraModeA & 2) != 0) {
            const float across = (2048.0f * gPointerScreenXY[0]) / FromInt(width) - 1024.0f;
            gCameraF5BF4 = ppc_fmadds(gCameraYawDragScale, across, gCameraF5BF4);
        }
        break;
    case kMsgModeOff:
        gCameraModeA &= ~0x78u;
        break;
    case kMsgReset:
        gCameraModeA &= ~0x7fu;
        gFirstPersonFlags |= 0x100u;
        break;
    default:
        break;
    }
}
