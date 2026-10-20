// Unit v5062: the advisor's bump (ride launch) functions, main app:
//   SetupHeadIDs 0x1002cddc, BMP_AddJoiningListToBump 0x1002cc70, BMP_LAUNCH 0x1002cff4, BMP_Collision 0x1002c7ec.
//
// The bump (0xac bytes apart in the list at 0x102b56c0 + 0x10), the ride record (0xd0 bytes apart, the list's +8) and the head
// nodes (0x14 bytes, a chain) are structures of the Mac's 32-bit layout, read here by Mac offset; the words that hold addresses
// hold host addresses that fit in 32 bits (the arena). The ride anim sets and meshes use the host's own layout.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "BMP_InsideArea.h"
#include "RideDB.h"
#include "analyser.h"
#include "TPEmbed.h"
#include "TPEngine.h"
#include "effect_manager.h"
#include "m3d2.h"
#include "rsse.h"

namespace {

uint32_t& R(uintptr_t base, uint32_t off) { return *reinterpret_cast<uint32_t*>(base + off); }
int32_t& S(uintptr_t base, uint32_t off) { return *reinterpret_cast<int32_t*>(base + off); }

// A word that holds an address is a Ptr32 offset from the arena base (0 is null).
uintptr_t Deref(uint32_t offset) {
    Ptr32<char> pointer;
    pointer.mOffset = offset;
    return reinterpret_cast<uintptr_t>(pointer.get());
}
uint32_t Offset(uintptr_t address) {
    Ptr32<char> pointer;
    pointer.set(reinterpret_cast<char*>(address));
    return pointer.mOffset;
}
uintptr_t P(uintptr_t base, uint32_t off) { return Deref(R(base, off)); }

uintptr_t ListBase() { return reinterpret_cast<uintptr_t>(&gColList); }   // 0x102b56c0

// fctiwz: round toward zero, a NaN or an out-of-range value gives 0x80000000 (a positive overflow saturates).
int32_t Cvt(double d) {
    if (d != d) return INT32_MIN;
    if (d >= 2147483648.0) return INT32_MAX;
    if (d <= -2147483649.0) return INT32_MIN;
    return static_cast<int32_t>(d);
}

int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }
int32_t Mul(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

// srawi 10 then addze: division by 1024 toward zero.
int32_t Q(int32_t v) { return v / 1024; }

// divw (the emulator answers the dividend for 0 and INT_MIN / -1).
int32_t Div(int32_t a, int32_t b) {
    if (b == 0 || (a == INT32_MIN && b == -1)) return a;
    return a / b;
}

// The ride record the id names: the list's magic, the record array at +8, the id = (record[0] << 8) | index.
bool ListIsLive() { return R(ListBase(), 0) == 0x4a454647u; }

// The jump table at 0x1029cb70 (12 words, kind + 14): 3 and 5 are the two targets that mark both bumps and set the +0x14 word.
const int kKindAction[12] = {5, 0, 0, 5, 3, 3, 0, 3, 5, 0, 3, 5};

}  // namespace

// 0x1002cddc, 296 bytes: numbers the bump's chain of head nodes from 1, finds each head's data node in the ride's anim set,
// enables it and (flag set) starts an emitter at the head node's position.
void SetupHeadIDs(tagBUMP* bump, int32_t flag) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(bump);
    uintptr_t node = P(b, 0x30);
    if (node == 0) return;
    uint32_t id = 1;
    do {
        tag_sRideAnimSet* set = gRideAnimSets[S(b, 8)];
        const uint32_t index = FindDataNodeIndexByID(set, 0x80, id);
        R(node, 8) = index;
        R(node, 0xc) = id;
        id++;
        if (R(node, 8) != 0xffffffffu) {
            set = gRideAnimSets[S(b, 8)];
            EnableRideHead(set->mHeads, R(node, 8), R(node, 0));
            if (flag != 0) {
                set = gRideAnimSets[S(b, 8)];
                const uintptr_t fnode = Deref(set->mHeads->mHeader->mF78.mOffset);
                float f0, f1, f2;
                std::memcpy(&f0, reinterpret_cast<const void*>(fnode + 0x40), 4);
                std::memcpy(&f1, reinterpret_cast<const void*>(fnode + 0x44), 4);
                std::memcpy(&f2, reinterpret_cast<const void*>(fnode + 0x48), 4);
                const float scale = 1024.0f;
                const float x = scale * f0;
                const float y = scale * f1;
                const float z = scale * f2;
                PAR_InitEmitter(0x13, Cvt(static_cast<double>(x)), Cvt(static_cast<double>(y)), Cvt(static_cast<double>(z)));
            }
        }
        const uintptr_t ride = P(b, 0x9c);
        R(ride, 0x60) = R(ride, 0x60) + 1;
        node = P(node, 0x10);
    } while (node != 0);
}

// 0x1002cc70, 316 bytes: gives the first live bump of the ride that has no head chain the ride's waiting chain (+0xc4).
// With a list that is not live and the id 0 the original reads through a null ride (not reproduced as a case).
int32_t BMP_AddJoiningListToBump(int32_t rideId) {
    const uintptr_t g = ListBase();
    uintptr_t ride = 0;
    int32_t id = 0;
    if (ListIsLive()) {
        ride = P(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
        const int32_t index = static_cast<int32_t>((ride - P(g, 8)) / 0xd0);
        id = static_cast<int32_t>((R(ride, 0) << 8) | static_cast<uint32_t>(index));
    }
    if (rideId == id && S(ride, 0x50) != 0) {
        uintptr_t bump = P(g, 0x10);
        int32_t count = S(g, 0x14);
        while (count != 0) {
            if ((R(bump, 0) & 1) != 0) {
                count--;
                if (P(bump, 0x9c) == ride && R(bump, 0x30) == 0) {
                    R(bump, 0x30) = R(ride, 0xc4);
                    R(ride, 0xc4) = 0;
                    R(bump, 0) = R(bump, 0) | 0x84000;
                    R(bump, 0x88) = R(ride, 4);
                    SetupHeadIDs(reinterpret_cast<tagBUMP*>(bump), 1);
                    SetControlType(reinterpret_cast<tagBUMPRIDE*>(ride), reinterpret_cast<tagBUMP*>(bump));
                    return 1;
                }
            }
            bump += 0xac;
        }
    }
    return 0;
}

// 0x1002cff4, 808 bytes: launches a rider bump from the ride; with flags 0 it also instances the sub ride and sets up the heads.
// The answer is the new bump (a 32-bit address) or 0.
int32_t BMP_LAUNCH(int32_t rideId, int32_t flags) {
    const uintptr_t g = ListBase();
    uintptr_t ride = 0;
    int32_t id = 0;
    if (ListIsLive()) {
        ride = P(g, 8) + static_cast<uint32_t>(rideId & 0xff) * 0xd0;
        const int32_t index = static_cast<int32_t>((ride - P(g, 8)) / 0xd0);
        id = static_cast<int32_t>((R(ride, 0) << 8) | static_cast<uint32_t>(index));
    }
    if (rideId != id) return 0;
    if (!(S(ride, 0x5c) < S(ride, 0x64))) return 0;
    if (S(ride, 0x5c) >= 0x40) return 0;
    if (S(ride, 0x50) == 0) return 0;
    tagBUMP* started = BMP_StartBump(S(ride, 0x74), S(ride, 0x78), S(ride, 8));
    if (started == nullptr) return 0;
    const uintptr_t bump = reinterpret_cast<uintptr_t>(started);

    R(bump, 0) = R(bump, 0) | 0x4000;
    R(bump, 0x9c) = Offset(ride);
    R(ride, 0x5c) = R(ride, 0x5c) + 1;
    R(bump, 0x88) = R(ride, 4);

    uintptr_t col = P(g, 0x18);
    const int32_t y = S(ride, 0x78);
    const int32_t x = S(ride, 0x74);
    if (col != 0) {
        int32_t count = S(g, 0x1c);
        bool found = false;
        while (count != 0) {
            const uint32_t f = R(col, 0);
            if ((f & 1) != 0) {
                count--;
                if ((f & 0x40) == 0 && PointInColObject(x, y, reinterpret_cast<tabCOLOBJECT*>(col)) != 0) {
                    found = true;
                    break;
                }
            }
            col += 0x20;
        }
        if (!found) col = 0;
    }
    R(bump, 0x98) = Offset(col);
    R(bump, 0x30) = R(ride, 0xc4);
    R(ride, 0xc4) = 0;

    if (flags == 0) {
        R(bump, 0) = R(bump, 0) | 0x100000;
        const int32_t count = S(ride, 0x5c);
        const int32_t size = S(ride, 0x18);
        S(bump, 4) = Add(S(ride, 0x14), Sub(count, Mul(Div(count, size), size)));
        tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> rideType;
        rideType.mIndex = static_cast<uint16_t>(R(ride, 0x10) & 0xffff);
        RideInfo* info = gRideDatabase.GetRideInfo(rideType);
        S(bump, 8) = static_cast<int32_t>(InstanceRide(info->mSubRideTypes[S(bump, 4)], 0x101, nullptr, 0, 0, nullptr, 0, 0, 0xffffffffu, 0));
        if (R(bump, 8) != 0) SetCurRideAnim(gRideAnimSets[R(bump, 8)], 0xc, 0, 2, 1.0f, 0);
        R(bump, 0x10) = 0xc;
        if (R(ride, 0x58) == 0) {
            R(ride, 0x58) = Offset(bump);
            R(bump, 0) = R(bump, 0) | 0x400000;
        }
        uintptr_t node = P(bump, 0x30);
        if (node != 0) {
            uint32_t headId = 1;
            do {
                tag_sRideAnimSet* set = gRideAnimSets[S(bump, 8)];
                R(node, 8) = FindDataNodeIndexByID(set, 0x80, headId);
                R(node, 0xc) = headId;
                headId++;
                if (R(node, 8) != 0xffffffffu) {
                    set = gRideAnimSets[S(bump, 8)];
                    EnableRideHead(set->mHeads, R(node, 8), R(node, 0));
                }
                const uintptr_t owner = P(bump, 0x9c);
                R(owner, 0x60) = R(owner, 0x60) + 1;
                node = P(node, 0x10);
            } while (node != 0);
        }
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(ride), started);
        R(bump, 0) = R(bump, 0) | 0x4000000;
    } else {
        R(bump, 0) = R(bump, 0) | static_cast<uint32_t>(flags);
        R(bump, 0x80) = 0;
        R(bump, 0x7c) = 0;
    }
    return static_cast<int32_t>(Offset(bump));
}

// 0x1002c7ec, 1112 bytes: pushes every live bump that overlaps this one apart. The direction angles of both decide: a head-on
// meeting (the angle between 0x80 and 0x180 of 512) moves the positions back, otherwise the velocities are exchanged along
// the line, and for two kinds of owner a hard hit marks both bumps (flag 0x200000, +0x14 = 3 or 5).
void BMP_Collision(tagBUMP* self) {
    const uintptr_t p = reinterpret_cast<uintptr_t>(self);
    const uintptr_t g = ListBase();
    uintptr_t q = P(g, 0x10);
    if ((R(P(p, 0x98), 0) & 0x80) != 0) return;
    if ((R(p, 0) & 0x200) != 0) return;
    int32_t left = S(g, 0x14);
    for (; left != 0; q += 0xac) {
        if ((R(q, 0) & 1) == 0) continue;
        left--;
        if (q == p) continue;
        if ((R(P(q, 0x98), 0) & 0x80) != 0) continue;
        if ((R(q, 0) & 0x200) != 0) continue;

        const int32_t dx = Sub(S(q, 0x34), S(p, 0x34));
        const int32_t dy = Sub(S(q, 0x38), S(p, 0x38));
        const int32_t radii = Add(S(q, 0x64), S(p, 0x64));
        int32_t ax = static_cast<int32_t>(std::labs(static_cast<long>(dx)));
        int32_t ay = static_cast<int32_t>(std::labs(static_cast<long>(dy)));
        uint32_t shifts = 0;
        while (ax > 0x8000 || ay > 0x7fff) {
            ax >>= 1;
            ay >>= 1;
            shifts++;
        }
        const int32_t sum = Add(Mul(ax, ax), Mul(ay, ay));
        const uint32_t scaled = static_cast<uint32_t>(Cvt(std::sqrt(static_cast<double>(sum)))) << (shifts & 0x3f);
        const int32_t dist = (shifts & 0x3f) >= 32 ? 0 : static_cast<int32_t>(scaled);
        if (dist == 0) continue;
        if (dist >= radii) continue;

        const int32_t ey = Sub(S(p, 0x48), S(q, 0x48));
        const int32_t ex = Sub(S(p, 0x44), S(q, 0x44));
        const double a1 = std::atan2(static_cast<double>(ex), static_cast<double>(Sub(0, ey)));
        const int32_t angleDir = Cvt(std::fma(-256.0, a1 / 3.14159, 256.0));
        const double a2 = std::atan2(static_cast<double>(dx), static_cast<double>(Sub(0, dy)));
        const int32_t angleLine = Cvt(std::fma(-256.0, a2 / 3.14159, 256.0));
        const int32_t rel = Sub(angleDir, angleLine) & 0x1ff;

        if (rel < 0x80 || rel > 0x180) {
            const int32_t len = Cvt(std::sqrt(static_cast<double>(Add(Mul(ex, ex), Mul(ey, ey)))));
            const int32_t t8 = gHostSineTable[(rel + 0x80) & 0x1ff] * 4;
            const int32_t t7 = gHostSineTable[(angleLine + 0x80) & 0x1ff] * 4;
            const int32_t t6 = gHostSineTable[angleLine & 0x1ff] * 4;
            const int32_t scale = S(P(q, 0x9c), 0x2c);
            const int32_t a = Q(Mul(len, t8));
            const int32_t part = Q(Mul(a, scale));
            S(q, 0x3c) = Add(S(q, 0x3c), Q(Mul(t6, part)));
            S(q, 0x40) = Add(S(q, 0x40), Q(Mul(t7, part)));
            const int32_t rest = Sub(a, part);
            R(q, 0) = R(q, 0) | 0x50000;
            S(p, 0x3c) = Sub(S(p, 0x3c), Q(Mul(t6, rest)));
            S(p, 0x40) = Sub(S(p, 0x40), Q(Mul(t7, rest)));
            const uint32_t idx = static_cast<uint32_t>(S(P(p, 0x9c), 0) + 0xe);
            if (idx <= 0xb) {
                const int action = kKindAction[idx];
                if (action != 0 && part > 0x82) {
                    if (((R(p, 0) | R(q, 0)) & 0x200000u) == 0) {
                        R(p, 0) = R(p, 0) | 0x200000;
                        R(q, 0) = R(q, 0) | 0x200000;
                        S(p, 0x14) = action;
                    }
                }
            }
        } else {
            const int32_t scale = S(P(q, 0x9c), 0x2c);
            const int32_t oldX = S(q, 0x34);
            const int32_t nx = Sub(dx, Div(Mul(dx, radii), dist));
            const int32_t ny = Sub(dy, Div(Mul(dy, radii), dist));
            const int32_t qx = Q(Mul(nx, scale));
            S(q, 0x34) = Sub(oldX, qx);
            const int32_t qy = Q(Mul(ny, scale));
            S(q, 0x38) = Sub(S(q, 0x38), qy);
            S(p, 0x34) = Add(S(p, 0x34), Sub(nx, qx));
            S(p, 0x38) = Add(S(p, 0x38), Sub(ny, qy));
        }
    }
}
