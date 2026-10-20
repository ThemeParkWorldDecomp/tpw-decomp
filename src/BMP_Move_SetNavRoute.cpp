#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "World.h"
#include "m3d2.h"
#include "platform/arena.h"
#include "ppc_math.h"
#include "rsse.h"

// The two thrust limits BumpToPoint reads (initial words 0x30 and 0x16 on the Mac).
int32_t gBumpThrustLimitA = 0x30;  // 0x1029cb28
int32_t gBumpThrustLimitB = 0x16;  // 0x1029cb2c

namespace {
inline int32_t& W(void* base, uint32_t offset) {
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(base) + offset);
}
}  // namespace

// 0x1002992c, 368 bytes: steers toward the point, answers the distance.
int32_t BumpToPoint(tagBUMP* b, int32_t px, int32_t py) {
    int32_t dx = px - b->x;
    int32_t dy = py - b->y;
    double a = std::atan2(static_cast<double>(dx), static_cast<double>(-dy));
    double q = a / 3.14159;
    double t = ppc_fnmsub(256.0, q, 256.0);
    int32_t heading = ppc_fctiwz(t);
    int32_t w = (heading - W(b, 0x50)) & 0x1ff;
    int32_t dist = BMP_Distance(dx, dy);    void* owner = b->owner.get();
    int32_t turn;
    if (w > 0x100) {
        w = 0x200 - w;
        turn = -W(owner, 0x28);
    } else {
        turn = W(owner, 0x28);
    }
    int32_t rate = W(owner, 0x28);
    if (w > rate / 2 && W(owner, 0x54) != 2 && (b->flags & 0x4000)) {
        W(b, 0x50) = (W(b, 0x50) + turn) & 0x1ff;
    }
    int32_t quotient = dist / 512;
    const int32_t* p = &gBumpThrustLimitA;
    if (quotient < *p) p = &quotient;
    if (gBumpThrustLimitB < *p) {
    } else {
        p = &gBumpThrustLimitB;
    }
    if (w < *p) BMP_Thrust(b, W(owner, 0x20));
    return dist;
}

// 0x100297cc, 308 bytes: picks the point to steer to, then bumps toward it.
int32_t SetNavRoute(tagBUMP* b, int32_t x, int32_t y) {
    if ((b->flags & 0x10000) && b->col.mOffset != 0) {
        tabCOLOBJECT* hit = PointInAnyColObject(x, y);
        if (hit == nullptr) {
            W(b, 0x6c) = x;
            W(b, 0x70) = y;
            return -1;
        }
        if (DoAreasIntersect(b->col.get(), hit) != 0) {
            W(b, 0x6c) = W(b, 0x74) + x;
            W(b, 0x70) = W(b, 0x78) + y;
        } else {
            tabCOLOBJECT* near = ClosestIntersectingColObjectToPoint(b->col.get(), x, y, 0);
            if (near == nullptr) {
                TRACE("The impossible has happened\n");
                b->vy = 0;
                b->vx = 0;
                return -1;
            }
            int32_t pt[2] = {0, 0};
            NavPointInArea(near, reinterpret_cast<NS_MacDoze::tagPOINT*>(pt));
            W(b, 0x6c) = pt[0];
            W(b, 0x70) = pt[1];
            b->flags |= 0x20000;
        }
        b->flags &= ~0x10000u;
    }
    return BumpToPoint(b, W(b, 0x6c), W(b, 0x70));
}
