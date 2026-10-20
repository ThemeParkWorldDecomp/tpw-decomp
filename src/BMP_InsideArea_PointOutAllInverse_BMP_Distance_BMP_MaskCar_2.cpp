// Unit v5926: the bump AI and its small helpers, main app.
//   BMP_MaskCar 0x1002ae74, BMP_Thrust 0x10029ac8, SetNextBuoyTarget 0x100296f4, BMP_AI 0x10029128
// The bump (0xac bytes), its ride record (the owner) and the buoys are read by Mac offset.
#include "BMP_InsideArea.h"
#include "Advisor.h"
#include "TPSound.h"
#include "m3d2.h"
#include "rsse.h"
#include "TPEngine.h"
#include "TPEmbed.h"
#include "Debug.h"
#include "World.h"
#include <cmath>

extern int32_t gHostSineTable[512];   // 0x102b4ec0

int32_t gBumpAiMinDist = 1;      // 0x1029cab0 (name invented): the lower clamp of the chase distance
int32_t gBumpAiMaxStep = 0x18;   // 0x1029cab4 (name invented): the upper clamp of the chase step

namespace {

inline uint32_t& Wu(const void* p, uint32_t off) {
    return *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(p)) + off);
}
inline int32_t& Wd(const void* p, uint32_t off) {
    return *reinterpret_cast<int32_t*>(const_cast<uint8_t*>(static_cast<const uint8_t*>(p)) + off);
}
// A Ptr32 word of a record.
inline uint8_t* Pw(const void* p, uint32_t off) {
    Ptr32<uint8_t> q;
    q.mOffset = Wu(p, off);
    return q.get();
}
inline uint8_t* OwnerOf(tagBUMP* b) { return Pw(b, 0x9c); }

// The jump tables at 0x1029caf0 and 0x1029cab8, by the owner kind + 14.
// Kinds -14, -11, -6, -3 and -1 take the first target of each table, every other kind the default.
const bool kClearAnim[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};
const bool kLeave[14] = {true, false, false, true, false, false, false, false, true, false, false, true, false, true};

void DisableSet(uint32_t index) {
    tag_sRideAnimSet* set = gRideAnimSets[index];
    if (set->mFlags & 0x4000)
        DisableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        DisableMeshDraw(set->mHeads);
}
void EnableSet(uint32_t index) {
    tag_sRideAnimSet* set = gRideAnimSets[index];
    if (set->mFlags & 0x4000)
        EnableMeshDraw(set->mHeads, set->mFC0, set->mFC4);
    else
        EnableMeshDraw(set->mHeads);
}

}  // namespace

// 0x1002ae74
void BMP_MaskCar(tagBUMP* b, int32_t mask) {
    if (mask != 0) {
        if (b->flags & 0x200) return;
        b->flags |= 0x200;
        DisableSet(Wu(b, 8));
        if ((b->flags & 0x2002000) && Wu(b, 0xc) != 0) DisableSet(Wu(b, 0xc));
        for (uint8_t* node = Pw(b, 0x30); node != nullptr; node = Pw(node, 0x10)) {
            if (Wu(node, 8) != 0xffffffffu) DisableRideHead(gRideAnimSets[Wu(b, 8)]->mHeads, Wu(node, 8));
        }
    } else {
        if (!(b->flags & 0x200)) return;
        b->flags &= ~0x200u;
        EnableSet(Wu(b, 8));
        if ((b->flags & 0x2002000) && Wu(b, 0xc) != 0) EnableSet(Wu(b, 0xc));
        for (uint8_t* node = Pw(b, 0x30); node != nullptr; node = Pw(node, 0x10)) {
            if (Wu(node, 8) != 0xffffffffu) EnableRideHead(gRideAnimSets[Wu(b, 8)]->mHeads, Wu(node, 8), 0);
        }
    }
}

// 0x10029ac8
void BMP_Thrust(tagBUMP* b, int32_t amount) {
    if ((b->flags & 0x84000) != 0x4000) return;
    if (Wd(OwnerOf(b), 0x54) == 2) return;
    uint8_t* col = Pw(b, 0x98);
    int32_t a = amount + Wd(b, 0x68);
    if ((Wu(col, 0) & 0x1e) != 0x1e) a = (a * 3) / 4;
    uint32_t angle = Wu(b, 0x50);
    int32_t sx = gHostSineTable[angle & 0x1ff];
    int32_t sy = gHostSineTable[(angle + 0x80) & 0x1ff];
    int32_t dx = (a * (sx * 4)) / 1024;
    int32_t dy = (a * (sy * 4)) / 1024;
    Wd(b, 0x3c) = Wd(b, 0x3c) + dx;
    Wd(b, 0x40) = Wd(b, 0x40) + dy;
}

// 0x100296f4
void SetNextBuoyTarget(tabBUOY* buoy, tagBUMP* b) {
    if (buoy == nullptr) return;
    uint8_t* cur = reinterpret_cast<uint8_t*>(buoy);
    do {
        cur = Pw(cur, 0x14);
        if (cur == nullptr) cur = Pw(OwnerOf(b), 0xa8);
    } while (Wu(cur, 0) & 2);
    Wd(b, 0x7c) = Wd(cur, 4);
    Wd(b, 0x78) = Wd(b, 0x74);
    uint32_t m = Wu(OwnerOf(b), 0x68);
    uint32_t r = gGameWorld->GetRand();
    int32_t half = static_cast<int32_t>(m) / 2;
    Wd(b, 0x74) = half - static_cast<int32_t>(r - (r / m) * m);
}

// 0x10029128
void BMP_AI(tagBUMP* b) {
    uint8_t* list = reinterpret_cast<uint8_t*>(&gColList);
    Wd(b, 0x90) = Wd(b, 0x90) + 1;
    int32_t c = Wd(b, 0x88);
    if (c >= 0 && Wd(OwnerOf(b), 0x50) == 2 && Wd(OwnerOf(b), 0x54) != 2) {
        c = c - 1;
        Wd(b, 0x88) = c;
        if (c == 0) {
            uint32_t index = static_cast<uint32_t>(Wd(OwnerOf(b), 0)) + 14;
            if (index <= 13 && kClearAnim[index]) b->flags &= ~0x4000u;
            b->flags &= 0xffffff81u;
            b->flags |= 0x10020;
        }
    }

    uint32_t f = b->flags;
    if (f & 4) {
        uint8_t* t = Pw(list, 0x10) + static_cast<int32_t>(Wd(b, 0x80) * 0xac);
        int32_t count = Wd(b, 0x8c) - 1;
        Wd(b, 0x8c) = count;
        if (count <= 0 || (Wu(t, 0) & 0x40021) != 1) {
            SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
            return;
        }
        b->flags |= 0x10000;
        int32_t d1 = BMP_Distance(Wd(b, 0x3c), Wd(b, 0x40));
        int32_t m = gBumpAiMinDist < d1 ? d1 : gBumpAiMinDist;
        int32_t d2 = BMP_Distance(Wd(b, 0x34) - Wd(t, 0x34), Wd(b, 0x38) - Wd(t, 0x38));
        int32_t q = d2 / m;
        int32_t step = q < gBumpAiMaxStep ? q : gBumpAiMaxStep;
        int32_t lx = Wd(t, 0x34) + step * Wd(t, 0x3c);
        int32_t ly = Wd(t, 0x38) + step * Wd(t, 0x40);
        tabCOLOBJECT* col = reinterpret_cast<tabCOLOBJECT*>(Pw(t, 0x98));
        if (PointInColObject(lx, ly, col) == 0 && PointInAnyColObject(lx, ly) == nullptr) {
            tabCOLOBJECT* cc = ClosestIntersectingColObjectToPoint(col, lx, ly, 1);
            int32_t dx = (lx - cc->x) / 8;
            int32_t dy = (ly - cc->y) / 8;
            int32_t dist = BMP_Distance(dx, dy);
            int32_t cx = cc->x;
            int32_t cy = cc->y;
            int32_t rr = cc->radiusX;
            lx = cx + (dx * rr) / (dist + 0x20);
            ly = cy + (dy * rr) / (dist + 0x20);
        }
        int32_t rc = SetNavRoute(b, lx, ly);
        if ((b->flags & 0x20000) && rc <= Wd(OwnerOf(b), 0x68)) b->flags &= ~0x20000u;
        return;
    }

    if (f & 0x20) {
        uint8_t* own = OwnerOf(b);
        uint32_t index = static_cast<uint32_t>(Wd(own, 0)) + 14;
        if (index <= 13 && kLeave[index]) {
            BMP_LeaveBump(b);
            int32_t handle = Wd(b, 0x20);
            if (handle != 0) SOUND_FadeSoundEffect(handle);
            return;
        }
        int32_t rc = SetNavRoute(b, Wd(own, 0x90), Wd(own, 0x94));
        if ((b->flags & 0x20000) && rc <= Wd(OwnerOf(b), 0x68)) {
            b->flags &= ~0x20000u;
            b->flags |= 0x10000;
            return;
        }
        if (rc < Wd(OwnerOf(b), 0x68)) BMP_DOCK(b);
        return;
    }

    if ((f & 0x58) == 0) return;
    uint8_t* buoys = Pw(list, 0x20);
    if (buoys == nullptr) return;
    uint8_t* buoy = buoys + static_cast<int32_t>(Wd(b, 0x7c) * 0x1c);
    if (!(Wu(buoy, 0) & 1)) return;
    int32_t rc = SetNavRoute(b, Wd(buoy, 8), Wd(buoy, 0xc));
    uint32_t g = b->flags;
    if ((g & 0x20000) && rc <= Wd(OwnerOf(b), 0x68)) {
        b->flags = g & ~0x20000u;
        b->flags |= 0x10000;
        return;
    }
    if ((g & 0x48000) == 0x48000) {
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
        b->flags |= 0x10000;
        return;
    }
    if (rc > Wd(OwnerOf(b), 0x68)) return;
    int32_t left = Wd(b, 0x8c) - 1;
    Wd(b, 0x8c) = left;
    if (left == 0) {
        SetControlType(reinterpret_cast<tagBUMPRIDE*>(OwnerOf(b)), b);
        b->flags |= 0x10000;
        return;
    }
    g = b->flags;
    if (!(g & 8)) {
        if (g & 0x10) {
            uint32_t r = gGameWorld->GetRand();
            uint32_t n = Wu(list, 0x24);
            Wd(b, 0x7c) = static_cast<int32_t>(r - (r / n) * n);
        }
        b->flags |= 0x10000;
        return;
    }
    if (g & 0x40) {
        if (rc <= Wd(OwnerOf(b), 0x68) / 4) {
            b->flags = g | 0x80000;
            Wd(b, 0x7c) = Wd(Pw(OwnerOf(b), 0xa4), 4);
        }
        b->flags |= 0x10000;
        return;
    }
    if (Wu(buoy, 0) & 4) {
        uint8_t* own = OwnerOf(b);
        bool reset = false;
        if (Wd(own, 0x50) == 3 || Wd(own, 0x50) == 0) {
            reset = true;
        } else {
            int32_t v = Wd(b, 0x88) + 1;
            Wd(b, 0x88) = v;
            reset = (v == 0);
        }
        if (reset) {
            int32_t kind = Wd(own, 0);
            if (kind == -7 || kind == -9 || kind == -10 || kind == -4) {
                if (Wd(own, 0x50) == 2) {
                    Wd(own, 0x50) = 3;
                    TRACE("GoKart Race Over\n");
                }
            }
            Wd(b, 0x74) = 0;
            Wd(b, 0x78) = 0;
            b->flags &= 0xffffff81u;
            b->flags |= 0x10020;
        } else {
            SetControlType(reinterpret_cast<tagBUMPRIDE*>(own), b);
        }
    }
    if (Wu(buoy, 0x18) == 0) {
        SetNextBuoyTarget(reinterpret_cast<tabBUOY*>(buoy), b);
    } else {
        if (PointInColObject(Wd(b, 0x34), Wd(b, 0x38), reinterpret_cast<tabCOLOBJECT*>(Pw(buoy, 0x18))) != 0) {
            Wu(b, 0x98) = Wu(buoy, 0x18);
            SetNextBuoyTarget(reinterpret_cast<tabBUOY*>(buoy), b);
        } else {
            Wd(b, 0x78) = 0;
            Wd(b, 0x74) = 0;
        }
    }
    b->flags |= 0x10000;
}
