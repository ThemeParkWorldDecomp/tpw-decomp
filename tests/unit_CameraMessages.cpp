#include "TPCamera.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "calllog.h"
#include <cstring>
#include <vector>
using namespace calllog;
uint32_t gFirstPersonFlags;
uint32_t gCameraModeA;
float gCameraF68;
float gCameraF5BF4;
int16_t gUIRealScreenWidth;
int16_t gUIRealScreenHeight;
static float F(uint32_t bits) { float v; std::memcpy(&v, &bits, 4); return v; }
static uint32_t B(float v) { uint32_t bits; std::memcpy(&bits, &v, 4); return bits; }
extern "C" void tpw_cmsg_case(int32_t message, int32_t a, int32_t b, int32_t width, int32_t height, const uint32_t* s, uint32_t* out) {
    gUIRealScreenWidth = static_cast<int16_t>(width);
    gUIRealScreenHeight = static_cast<int16_t>(height);
    gFirstPersonFlags = s[0];
    gCameraModeA = s[1];
    gCameraF68 = F(s[2]);
    gCameraF5BF4 = F(s[3]);
    gPointerScreenXY[0] = F(s[4]);
    gPointerScreenXY[1] = F(s[5]);
    gCameraYawDragScaleOnce = static_cast<int8_t>(s[6]);
    gCameraYawDragScale = F(s[7]);
    gCameraMessagesF5A10Once = static_cast<int8_t>(s[8]);
    gCameraMessagesF5A14 = F(s[9]);
    Will_HandleCameraMessages(message, a, b);
    const uint32_t result[10] = {gFirstPersonFlags, gCameraModeA, B(gCameraF68), B(gCameraF5BF4), B(gPointerScreenXY[0]),
                                 B(gPointerScreenXY[1]), static_cast<uint8_t>(gCameraYawDragScaleOnce), B(gCameraYawDragScale),
                                 static_cast<uint8_t>(gCameraMessagesF5A10Once), B(gCameraMessagesF5A14)};
    std::memcpy(out, result, sizeof result);
}
extern "C" int32_t tpw_cmsg_state(int32_t count, int32_t id, int32_t n, const int32_t* flat) {
    std::vector<CKeyTableEntry> entries(n > 0 ? n : 1);
    for (int32_t i = 0; i < n; ++i) {
        entries[i].mId = static_cast<int16_t>(flat[2 * i]);
        entries[i].mState = static_cast<int16_t>(flat[2 * i + 1]);
    }
    CKeyTable table{};
    table.mEntries = entries.data();
    table.mCount = static_cast<int16_t>(count);
    return table.GetState(static_cast<int16_t>(id));
}
