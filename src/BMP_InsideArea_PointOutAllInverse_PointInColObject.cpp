// Unit v5367: the collision object tests of the advisor world, main app.
//   PointInColObject 0x1002bc34, DoAreasIntersect 0x1002b6c0, ClosestIntersectingColObjectToPoint 0x1002b4cc
#include <cmath>
#include <cstdint>

#include "BMP_InsideArea.h"

namespace {

// The Mac's labs on a 4-byte long (wraps for INT_MIN).
inline int32_t Labs32(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// Distance of (dx, dy) scaled down until both fit, as the original: sqrt of the squares, then shifted back up.
int32_t ScaledDistance(int32_t dx, int32_t dy) {
    int32_t a = Labs32(dx);
    int32_t b = Labs32(dy);
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        b >>= 1;
        shift++;
    }
    int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(a) +
                                       static_cast<uint32_t>(b) * static_cast<uint32_t>(b));
    double root = std::sqrt(static_cast<double>(sum));
    int32_t q = (root >= 0.0 && root < 2147483648.0) ? static_cast<int32_t>(root) : INT32_MIN;
    return static_cast<int32_t>(shift >= 32 ? 0u : (static_cast<uint32_t>(q) << shift));
}

inline int32_t BigRadius(const tabCOLOBJECT* o) { return o->radiusX < o->radiusY ? o->radiusY : o->radiusX; }

}  // namespace

int32_t PointInColObject(int32_t x, int32_t y, tabCOLOBJECT* object) {
    if (object == nullptr) return 0;
    int32_t dx = object->x - x;
    uint32_t sides = dx < 0 ? 4 : 2;
    int32_t dy = object->y - y;
    sides |= dy < 0 ? 8 : 0x10;
    if (dx == 0 && dy == 0) return 1;
    uint32_t flags = object->flags;
    if (sides != (sides & flags)) return 0;
    if ((flags & 0x20) != 0) {
        if (Labs32(dx) <= object->radiusX && Labs32(dy) <= object->radiusY) return 1;
        return 0;
    }
    int32_t d = ScaledDistance(dx, dy);
    return object->radiusX >= d ? 1 : 0;
}

int32_t DoAreasIntersect(tabCOLOBJECT* a, tabCOLOBJECT* b) {
    if (a == b) return 1;
    uint32_t fa = a->flags & 0x1e;
    if (fa == 0x1e && (b->flags & 0x1e) == 0x1e) {
        int32_t d = ScaledDistance(a->x - b->x, a->y - b->y);
        int32_t sum = BigRadius(b) + BigRadius(a);
        return sum > d ? 1 : 0;
    }
    if (((a->flags | b->flags) & 0x1e) == 0x1e) {
        bool bothPartial;
        tabCOLOBJECT* other;
        tabCOLOBJECT* full;
        if (fa == 0x1e) {
            bothPartial = false;
            other = b;
            full = a;
        } else {
            other = a;
            full = b;
            bothPartial = (b->flags & 0x1e) != 0x1e;
        }
        uint32_t f = other->flags;
        if (((f & 2) == 0 || other->x <= full->x) && ((f & 4) == 0 || full->x <= other->x) &&
            ((f & 8) == 0 || full->y <= other->y) && ((f & 0x10) == 0 || other->y <= full->y)) {
            return 0;
        }
        if (bothPartial) {
            int32_t dy = Labs32(other->y - full->y);
            int32_t dx = Labs32(other->x - full->x);
            return dx + dy == 0x1800 ? 1 : 0;
        }
        int32_t d = ScaledDistance(other->x - full->x, other->y - full->y);
        int32_t sum = BigRadius(other) + BigRadius(full);
        return sum > d ? 1 : 0;
    }
    return (a->y == b->y && a->x == b->x) ? 1 : 0;
}

tabCOLOBJECT* ClosestIntersectingColObjectToPoint(tabCOLOBJECT* area, int32_t px, int32_t py, int32_t includeSelf) {
    int32_t best = INT32_MAX;
    tabCOLOBJECT* found = nullptr;
    tabCOLOBJECT* p = gColList.first.get();
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                int32_t x = p->x;
                int32_t y = p->y;
                n--;
                if ((f & 8) == 0) {
                    y -= 0x87c;
                } else if ((f & 0x10) == 0) {
                    y += 0x87c;
                }
                if ((f & 2) == 0) {
                    x += 0x87c;
                } else if ((f & 4) == 0) {
                    x -= 0x87c;
                }
                if ((f & 0x40) == 0 && (includeSelf != 0 || area != p)) {
                    if (DoAreasIntersect(area, p) != 0 && PointOutAllInverse(x, y) != 0) {
                        int32_t d = ScaledDistance(p->x - px, p->y - py);
                        if (d < best) {
                            best = d;
                            found = p;
                        }
                    }
                }
            }
            p++;
        }
    }
    return found;
}
