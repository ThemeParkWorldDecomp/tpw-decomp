// Unit v5672: BMP_DrawCarsForRide (0x10028030, 3,156 bytes): places the bumper cars of one ride.
// Pass 1 collects the ride's live bumps (four corner points each), pass 2 looks up the ground height under every corner
// (one SetRideSpritePosnFocus per distinct mesh), pass 3 tilts the cars along the ground and sets each car's matrices.
#include <cmath>
#include <cstdint>

#include "BMP_InsideArea.h"
#include "LevelSystems.h"
#include "TPAdvisor.h"
#include "TPEngine.h"
#include "effect_manager.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

float gDrawCarsAngles[3];          // 0x1029ca9c (initial 0): the rotation angles, copied to the stack and overwritten before each use
float gDrawCarsClampLo = -1.0f;    // 0x1029caa8
float gDrawCarsClampHi = 1.0f;     // 0x1029caac
int8_t gDrawCarsInit;              // 0x102b4eb8 (a char): set once the scale below holds its value
float gDrawCarsScale;              // 0x102b4ebc: 49143.0 once gDrawCarsInit is set

namespace {

// One corner point of a car: the Mac keeps 0x20 bytes per point (+0 the bump of the first point of a car, +4 the mesh, +8 the angle,
// +0xc and +0x10 the car's centre, +0x14, +0x18 and +0x1c the corner's x, z and height).
struct Corner {
    uint8_t* bump;
    tag_sRideAnimSet* mesh;
    uint32_t angle;
    float cx, cz;
    float x, z, y;
};

inline int32_t& W(void* p, uint32_t off) { return *reinterpret_cast<int32_t*>(static_cast<uint8_t*>(p) + off); }
inline float& Fl(void* p, uint32_t off) { return *reinterpret_cast<float*>(static_cast<uint8_t*>(p) + off); }

inline float Sine(int32_t i) { return static_cast<float>(gHostSineTable[i & 0x1ff]); }
inline float Sine4(int32_t i) {   // the table word times four, as the listing shifts it
    return static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(gHostSineTable[i & 0x1ff]) << 2));
}

// cur - (cur - prev) * (-step), the listing's fsubs + fnmsubs
inline float Slope(float cur, float prev, float negStep) { return ppc_fnmsubs(cur - prev, negStep, cur); }

inline sMatrix* MatrixOf(uint32_t index) {
    return &gRideAnimSets[index]->mHeads->mHeader->mF78.get()->mMatrix;
}

inline int32_t Idx(int32_t v, int32_t mul) { return static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(mul)); }

}  // namespace

void BMP_DrawCarsForRide(int32_t ride, float step) {
    const float kPi = 3.1415927f;
    const float kHalfPi = 1.5707964f;
    tagBUMPWORLD* world = reinterpret_cast<tagBUMPWORLD*>(&gColList);

    float angles[3];
    angles[0] = gDrawCarsAngles[0];
    angles[1] = gDrawCarsAngles[1];
    angles[2] = gDrawCarsAngles[2];

    const float* pick = &gDrawCarsClampLo;
    if (*pick < step) pick = &step;
    if (!(*pick < gDrawCarsClampHi)) pick = &gDrawCarsClampHi;
    const float s = *pick;
    const float negStep = -s;

    int32_t n = 0;
    tagBUMPRIDE* rec = nullptr;
    uint32_t id = 0;
    if (world->magic == 0x4a454647u) {
        rec = world->rides.get() + (ride & 0xff);
        id = (static_cast<uint32_t>(rec->kind) << 8) | static_cast<uint32_t>(ride & 0xff);
    }
    if (ride != static_cast<int32_t>(id)) return;
    uint8_t* bump = reinterpret_cast<uint8_t*>(world->bumps.get());
    if (bump == nullptr) return;
    int32_t count = world->bumpCount;

    static Corner cars[1024];
    Corner* c = cars;

    // Pass 1: four corner points per live bump of this ride.
    while (count != 0) {
        if ((W(bump, 0) & 1) != 0) {
            count--;
            if (reinterpret_cast<tagBUMPRIDE*>(reinterpret_cast<tagBUMPSLOT*>(bump)->owner.get()) == rec) {
                const int32_t shift = ppc_fctiwz(static_cast<double>(static_cast<float>(W(bump, 0x5c)) * negStep));
                const uint32_t angle = (static_cast<uint32_t>(W(bump, 0x54)) - static_cast<uint32_t>(shift)) & 0x1ffu;
                const float rideW = static_cast<float>(W(rec, 8));
                const float cx = ppc_fnmsubs(static_cast<float>(W(bump, 0x3c)), negStep, static_cast<float>(W(bump, 0x34))) / 307.2f;
                const float cz = ppc_fnmsubs(static_cast<float>(W(bump, 0x40)), negStep, static_cast<float>(W(bump, 0x38))) / 307.2f;
                const float a = ((rideW * Sine4(static_cast<int32_t>(angle))) * 0.0009765625f) / 307.2f;
                const float b = ((rideW * Sine4(static_cast<int32_t>(angle) + 0x80)) * 0.0009765625f) / 307.2f;
                c[0].bump = bump;
                c[0].angle = angle;
                c[0].cx = cx;
                c[0].cz = cz;
                const float xs[4] = {cx + a, cx - a, cx - b, cx + b};
                const float zs[4] = {cz + b, cz - b, cz + a, cz - a};
                for (int k = 0; k < 4; k++) {
                    c[k].x = xs[k];
                    c[k].z = zs[k];
                    c[k].y = Fl(bump, 0xa0);
                    c[k].mesh = GetMeshPtrAtHFieldCell(static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(c[k].x / 10.0f))),
                                                       static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(c[k].z / 10.0f))));
                }
                c += 4;
                n += 4;
            }
        }
        bump += 0xac;
    }

    // Pass 2: the height under every corner, one mesh at a time.
    int32_t first = 0;
    while (first != -1 && first < n) {
        const int32_t start = first;
        tag_sRideAnimSet* mesh = cars[start].mesh;
        first = -1;
        if (mesh != nullptr) SetRideSpritePosnFocus(mesh->mHeads);
        for (int32_t k = start; k < n; k++) {
            tag_sRideAnimSet* m = cars[k].mesh;
            if (m == nullptr) continue;
            if (m == mesh) {
                GetRideHeightAtPoint(cars[k].x, cars[k].z, cars[k].y);
                if (-100.0f == cars[k].y) cars[k].y = Fl(cars[k & ~3].bump, 0xa0);
                cars[k].mesh = nullptr;
            } else if (first == -1) {
                first = k;
            }
        }
    }

    // Pass 3: the cars.
    for (int32_t g = 0; g < n; g += 4) {
        Corner* q = &cars[g];
        uint8_t* bp = q->bump;
        const float f27 = kPi * (2.0f * (static_cast<float>(static_cast<int32_t>((q->angle + 0x100u) & 0x1ffu)) * 0.001953125f));
        if ((static_cast<uint32_t>(W(bp, 0)) & 0x1000000u) != 0) {
            const int32_t idx = W(bp, 0x90);
            q[0].y = q[0].y + Slope(Sine(Idx(idx, 6)), Sine(Idx(idx - 1, 6)), negStep) / 800.0f;
            q[1].y = q[1].y + Slope(Sine(((static_cast<uint32_t>(idx) * 16u) & 0x7f0u) >> 2),
                                    Sine(((static_cast<uint32_t>(idx - 1) * 16u) & 0x7f0u) >> 2), negStep) / 800.0f;
            q[2].y = q[2].y + Slope(Sine(Idx(idx, 3)), Sine(Idx(idx - 1, 3)), negStep) / 800.0f;
            q[3].y = q[3].y + Slope(Sine(Idx(idx, 5)), Sine(Idx(idx - 1, 5)), negStep) / 800.0f;
        }
        const float h0 = q[0].y, h1 = q[1].y, h2 = q[2].y, h3 = q[3].y;
        const float rideW = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(W(rec, 8)) << 1));
        const float atanA = static_cast<float>(std::atan2(static_cast<double>(rideW / 307.2f), static_cast<double>(h1 - h0)));
        const float atanB = static_cast<float>(std::atan2(static_cast<double>(rideW / 307.2f), static_cast<double>(h3 - h2)));
        float f30, f28;
        if ((static_cast<uint32_t>(W(bp, 0)) & 0x800u) != 0) {
            f30 = (h0 + h1) * 0.5f;
            f28 = kHalfPi;
        } else {
            f28 = atanB;
            f30 = (h3 + (h2 + (h0 + h1))) * 0.25f;
        }
        Fl(bp, 0xa0) = f30;
        const uint32_t flags = static_cast<uint32_t>(W(bp, 0));
        if ((flags & 0x400u) != 0) {
            const int32_t idx = W(bp, 0x90);
            const float cur = Sine(Idx(idx, 6)), prev = Sine(Idx(idx - 1, 6));
            f30 = f30 + (1.6f + Slope(cur, prev, negStep) / 600.0f);
        }
        if ((flags & 0x1000u) != 0) {
            const float t = static_cast<float>(W(bp, 0x60)) * negStep;
            const float u = static_cast<float>(W(bp, 0x5c)) - t;
            f28 = ppc_fmadds(kHalfPi, u / 13.0f, f28);
        }

        sMatrix* m = MatrixOf(static_cast<uint32_t>(W(bp, 8)));
        SetLMatrixIdentityNoTrans(m);
        m = MatrixOf(static_cast<uint32_t>(W(bp, 8)));
        m->m[3][0] = q->cx;
        m->m[3][1] = f30;
        m->m[3][2] = q->cz;
        angles[0] = f28 - kHalfPi;
        angles[2] = f27;
        angles[1] = atanA - kHalfPi;
        RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W(bp, 8))), static_cast<eRotateParams>(0xe), angles);

        if ((static_cast<uint32_t>(W(bp, 0)) & 0x4000000u) != 0) {
            W(bp, 0) = static_cast<int32_t>(static_cast<uint32_t>(W(bp, 0)) & 0xfbffffffu);
            const int32_t ex = ppc_fctiwz(static_cast<double>(1024.0f * q->cx));
            const int32_t ey = ppc_fctiwz(static_cast<double>(1024.0f * f30));
            const int32_t ez = ppc_fctiwz(static_cast<double>(1024.0f * q->cz));
            PAR_InitEmitter(0xf, ex, ey, ez);
        }
        if ((static_cast<uint32_t>(W(bp, 0)) & 0x2000u) != 0) {
            SetLMatrixIdentityNoTrans(MatrixOf(static_cast<uint32_t>(W(bp, 0xc))));
            sMatrix* m2 = MatrixOf(static_cast<uint32_t>(W(bp, 0xc)));
            const float avg = (h3 + (h2 + (h0 + h1))) * 0.25f;
            m2->m[3][0] = q->cx;
            m2->m[3][1] = avg;
            m2->m[3][2] = q->cz;
            angles[0] = atanB - kHalfPi;
            angles[2] = f27;
            angles[1] = atanA - kHalfPi;
            RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W(bp, 0xc))), static_cast<eRotateParams>(0xe), angles);
        }
        if ((static_cast<uint32_t>(W(bp, 0)) & 0x2000000u) != 0) {
            if (gDrawCarsInit == 0) {
                gDrawCarsScale = 49143.0f;
                gDrawCarsInit = 1;
            }
            SetLMatrixIdentityNoTrans(MatrixOf(static_cast<uint32_t>(W(bp, 0xc))));
            const int32_t sa = static_cast<int32_t>(static_cast<uint32_t>(W(bp, 0x4c)) *
                                                    (static_cast<uint32_t>(gHostSineTable[q->angle & 0x1ff]) << 2));
            q->cx = q->cx - static_cast<float>(sa) / gDrawCarsScale;
            const int32_t sb = static_cast<int32_t>(static_cast<uint32_t>(W(bp, 0x4c)) *
                                                    (static_cast<uint32_t>(gHostSineTable[(q->angle + 0x80) & 0x1ff]) << 2));
            q->cz = q->cz - static_cast<float>(sb) / gDrawCarsScale;
            sMatrix* m3 = MatrixOf(static_cast<uint32_t>(W(bp, 0xc)));
            m3->m[3][0] = q->cx;
            m3->m[3][1] = 0.7f + f30;
            m3->m[3][2] = q->cz;
            angles[0] = 0.0f;
            angles[2] = f27;
            angles[1] = 0.0f;
            RotateLMatrixXYZNoTrans(MatrixOf(static_cast<uint32_t>(W(bp, 0xc))), static_cast<eRotateParams>(0xe), angles);
        }
    }
}
