#include "BMP_InsideArea.h"
#include "Advisor.h"
#include "TPSound.h"
#include "m3d2.h"
#include "rsse.h"

#include <cmath>

// 0x1002b318: the length of (dx, dy) in integers; both halves are shifted right until they fit, the root is shifted back.
int32_t BMP_Distance(int32_t dx, int32_t dy) {
    int32_t a = dx < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(dx)) : dx;   // labs
    int32_t b = dy < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(dy)) : dy;
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        shift++;
        b >>= 1;
    }
    int32_t sum = a * a + b * b;
    double root = std::sqrt(static_cast<double>(sum));
    int32_t r = static_cast<int32_t>(root);
    return static_cast<int32_t>(static_cast<uint32_t>(r) << (shift & 0x3f));
}

// 0x1002b248: moves the bump's sound (handle at 0x20) to its position over 302.7.
void PossitionSound1(tagBUMP* bump) {
    int32_t* handle = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(bump) + 0x20);
    if (*handle != 0) {
        float fy = static_cast<float>(bump->y) / 302.7f;
        float fx = static_cast<float>(bump->x) / 302.7f;
        *handle = SOUND_MoveSoundEffect(*handle, static_cast<int32_t>(fx), 0, static_cast<int32_t>(fy));
    }
}

// 0x1002b180: one event on the sound category [2] at the bump's position.
void SingleSoundAtBump(tagBUMP* bump, int32_t event) {
    float fy = static_cast<float>(bump->y) / 302.7f;
    float fx = static_cast<float>(bump->x) / 302.7f;
    SOUND_SendEvent(0, gSoundBanks[2], event, static_cast<int32_t>(fx), 0, static_cast<int32_t>(fy));
}

// 0x1002b0dc: out = (x, y) / 302.7 truncated.
void MineToSystemCords(int32_t x, int32_t y, NS_MacDoze::tagPOINT* out) {
    float fx = static_cast<float>(x) / 302.7f;
    float fy = static_cast<float>(y) / 302.7f;
    int32_t* pt = reinterpret_cast<int32_t*>(out);
    pt[0] = static_cast<int32_t>(fx);
    pt[1] = static_cast<int32_t>(fy);
}

// 0x1002ad34
void NormaliseVector(s3DPoint* v) {
    double dx = static_cast<double>(v->x) * static_cast<double>(v->x);
    float yy = v->y * v->y;
    float zz = v->z * v->z;
    double sum = static_cast<double>(zz) + (dx + static_cast<double>(yy));
    float len = static_cast<float>(std::sqrt(sum));
    v->x = v->x / len;
    v->y = v->y / len;
    v->z = v->z / len;
}
