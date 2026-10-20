// BMP_InsideArea 0x1002c260 and BMP_OutsideArea 0x1002be04 (TPAdvisor.cpp): keep a moving bump inside its current collision
// object (circle or box) and push it out of the "outside area" objects, bouncing its velocity and nudging mPush.
#include <cmath>
#include <cstdint>

#include "BMP_InsideArea.h"
#include "ppc_math.h"

extern int32_t gHostSineTable[512];   // main app data 0x102b4ec0: 256 * sin(2 pi i / 512), cut toward zero

namespace {


inline int32_t Labs(int32_t v) { return v < 0 ? static_cast<int32_t>(0u - static_cast<uint32_t>(v)) : v; }

// divw: the emulator answers the dividend for a zero divisor (the hardware is undefined).
inline int32_t DivW(int32_t a, int32_t b) {
    if (b == 0) return a;
    if (b == -1) return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
    return a / b;
}

// (a * b) >> 10 rounded toward zero (mullw, srawi 10, addze).
inline int32_t Mul10(int32_t a, int32_t b) {
    int32_t p = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
    return (p >> 10) + ((p < 0 && (p & 0x3ff) != 0) ? 1 : 0);
}

// neg then convert (the original negates the integer, so 0 stays +0.0).
inline double NegD(int32_t v) { return static_cast<double>(static_cast<int32_t>(0u - static_cast<uint32_t>(v))); }

// 256 - 256 * atan2(y, x) / 3.14159 as a truncated int: the angle in 512ths of a turn.
inline int32_t Angle(double y, double x) {
    double a = std::atan2(y, x);
    return static_cast<int32_t>(ppc_fnmsub(256.0, a / 3.14159, 256.0));
}

inline int32_t Table(int32_t i) { return gHostSineTable[i & 0x1ff]; }

inline int32_t IntSqrt(int32_t a, int32_t b) {
    int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(a) +
                                       static_cast<uint32_t>(b) * static_cast<uint32_t>(b));
    return static_cast<int32_t>(std::sqrt(static_cast<double>(sum)));
}

// The length of (dx, dy) as sqrt scaled down by halving until it fits, then shifted back up.
inline int32_t Length(int32_t dx, int32_t dy) {
    int32_t a = Labs(dx), b = Labs(dy);
    uint32_t shift = 0;
    while (a > 0x8000 || b > 0x7fff) {
        a >>= 1;
        b >>= 1;
        shift++;
    }
    int32_t root = IntSqrt(a, b);
    return (shift & 0x3f) >= 32 ? 0 : static_cast<int32_t>(static_cast<uint32_t>(root) << shift);
}

// The tail both functions share: nudge mPush by the distance moved, toward the direction the velocity turned.
inline void Nudge(tagBUMP* b, int32_t moved, int32_t angleBefore) {
    int32_t angleNow = Angle(static_cast<double>(b->vx), NegD(b->vy));
    int32_t m;
    int32_t v;
    if (((angleNow - angleBefore) & 0x1ff) < 0x100) {
        m = static_cast<int32_t>(0u - static_cast<uint32_t>(moved)) * 0x30;
        v = b->push + m;
        b->push = (m >= v) ? m : v;
    } else {
        m = moved * 0x30;
        v = b->push + m;
        b->push = (v >= m) ? m : v;
    }
}

inline bool KindPushes(int32_t kind) { return kind == -5 || kind == -8 || kind == -0xc || kind == -0xd; }

}  // namespace

void BMP_InsideArea(tagBUMP* b) {
    if (PointInColObject(b->x, b->y, b->col.get()) != 0) return;

    tabCOLOBJECT* p = gColList.first.get();
    int32_t y = b->y;
    int32_t x = b->x;
    bool found = false;
    if (p != nullptr) {
        int32_t n = gColList.count;
        while (n != 0) {
            uint32_t f = p->flags;
            if ((f & 1) != 0) {
                n--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, p) != 0) {
                    found = true;
                    break;
                }
            }
            p++;
        }
    }
    if (found) {
        b->col = p;
        b->flags |= 0x10000;
        return;
    }

    tabCOLOBJECT* cur = b->col.get();
    if (cur == nullptr) return;
    if (b->counter >= 0) b->counter++;

    int32_t dx = b->x - cur->x;
    int32_t dy = b->y - cur->y;
    int32_t ang = Angle(static_cast<double>(b->dirX), NegD(b->dirY));
    int32_t moved = 0;   // r23: the caller's value when nothing moves (the original leaves it unset)
    tagBUMPOwner* owner = b->owner.get();

    if ((cur->flags & 0x20) != 0) {
        if (Labs(dx) > cur->radiusX) {
            b->vx = Mul10(0 - b->vx, owner->scale);
            b->x = cur->x + (dx < 0 ? -cur->radiusX : cur->radiusX);
            moved = Labs(cur->radiusX - Labs(dx));
        }
        if (Labs(dy) > cur->radiusY) {
            b->vy = Mul10(0 - b->vy, owner->scale);
            b->y = cur->y + (dy < 0 ? -cur->radiusY : cur->radiusY);
            moved = Labs(cur->radiusY - Labs(dy));
        }
    } else {
        int32_t ang2 = Angle(static_cast<double>(dx), NegD(dy));
        int32_t a = (ang + 0x100) & 0x1ff;
        int32_t bb = (ang2 - 0x80) & 0x1ff;
        int32_t c = ((2 * bb - a) + 0x100) & 0x1ff;
        int32_t d = c - ang;
        int32_t e = d & 0x1ff;
        int32_t s = Table(d);
        int32_t co = Table(e + 0x80);
        int32_t vyScaled = Mul10(b->vy, owner->scale);
        int32_t vxScaled = Mul10(b->vx, owner->scale);
        int32_t newVx = Mul10(vyScaled, s * 4) ;
        // vx' = (vyScaled * s * 4 + vxScaled * co * 4) >> 10 rounded toward zero
        {
            int32_t sum = static_cast<int32_t>(static_cast<uint32_t>(vxScaled) * static_cast<uint32_t>(co * 4) +
                                               static_cast<uint32_t>(vyScaled) * static_cast<uint32_t>(s * 4));
            newVx = (sum >> 10) + ((sum < 0 && (sum & 0x3ff) != 0) ? 1 : 0);
        }
        b->vx = newVx;
        {
            int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(vyScaled) * static_cast<uint32_t>(co * 4) -
                                                static_cast<uint32_t>(vxScaled) * static_cast<uint32_t>(Table(e) * 4));
            b->vy = (diff >> 10) + ((diff < 0 && (diff & 0x3ff) != 0) ? 1 : 0);
        }
        int32_t len = Length(dx, dy);
        moved = Labs(cur->radiusX - len);
        b->x = cur->x + DivW(static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(cur->radiusX)), len);
        b->y = cur->y + DivW(static_cast<int32_t>(static_cast<uint32_t>(dy) * static_cast<uint32_t>(cur->radiusX)), len);
    }

    if (KindPushes(owner->kind)) Nudge(b, moved, ang);
}

void BMP_OutsideArea(tagBUMP* b) {
    tabCOLOBJECT* p = gColList.first.get();
    if (p == nullptr) return;
    int32_t n = gColList.count;
    while (n != 0) {
        uint32_t f = p->flags;
        if ((f & 1) != 0) {
            n--;
            if ((f & 0x40) != 0 && PointInColObject(b->x, b->y, p) != 0) {
                int32_t dx = b->x - p->x;
                int32_t dy = b->y - p->y;
                int32_t ang27 = Angle(NegD(b->dirX), static_cast<double>(b->dirY));
                int32_t ang28 = Angle(static_cast<double>(dx), NegD(dy));
                int32_t rel = (ang27 - ang28) & 0x1ff;
                if (rel < 0x80 || rel > 0x180) {
                    int32_t mag = IntSqrt(b->dirX, b->dirY);
                    tagBUMPOwner* owner = b->owner.get();
                    int32_t t = Mul10(mag, Table(rel + 0x80) * 4);
                    t = Mul10(t, owner->scale + 0x400);
                    b->vx = b->vx + Mul10(t, Table(ang28) * 4);
                    b->vy = b->vy + Mul10(t, Table(ang28 + 0x80) * 4);
                    b->flags |= 0x50000;
                    int32_t len = Length(dx, dy);
                    b->x = p->x + DivW(static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(p->radiusX)), len);
                    b->y = p->y + DivW(static_cast<int32_t>(static_cast<uint32_t>(dy) * static_cast<uint32_t>(p->radiusX)), len);
                    if (KindPushes(b->owner->kind)) {
                        int32_t moved = Labs(len - p->radiusX);
                        // the push uses the angle of (-vx, vy) against ang27
                        int32_t angleNow = Angle(NegD(b->vx), static_cast<double>(b->vy));
                        int32_t m;
                        int32_t v;
                        if (((angleNow - ang27) & 0x1ff) < 0x100) {
                            m = static_cast<int32_t>(0u - static_cast<uint32_t>(moved)) * 0x30;
                            v = b->push + m;
                            b->push = (m >= v) ? m : v;
                        } else {
                            m = moved * 0x30;
                            v = b->push + m;
                            b->push = (v >= m) ? m : v;
                        }
                    }
                }
            }
        }
        p++;
    }
}
