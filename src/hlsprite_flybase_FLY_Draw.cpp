// FLY_Draw (HLSprite.cpp, 0x10077ae4, 1776 bytes): moves every active slot of the fly bases one step and poses its ride and emitter.
#include <cstdint>

#include "HLSprite.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"

// The flies' working registers (bss; the original names are unknown): position x y z, then three angles (twelve-bit units).
float gFlyDrawPosX;      // 0x102da960
float gFlyDrawPosY;      // 0x102da95c
float gFlyDrawPosZ;      // 0x102da958
float gFlyDrawAngA;      // 0x102da954
float gFlyDrawAngB;      // 0x102da950
float gFlyDrawAngC;      // 0x102da94c
uint32_t gFlyDrawOrigin[3];   // 0x1029d2f8

namespace {

inline int32_t& W(uint8_t* p, uint32_t off) { return *reinterpret_cast<int32_t*>(p + off); }
inline float& Fl(uint8_t* p, uint32_t off) { return *reinterpret_cast<float*>(p + off); }

sMatrix* RideMatrix(int32_t ride) {
    tag_sNode* node = gRideAnimSets[ride]->mHeads->mHeader->mF78.get();
    return &node->mMatrix;
}

// The angle step: (new + old * (k - 1)) / k on twelve-bit angles, taking the short way round; k is 4 or 8.
float Blend(float target, float current, int32_t k) {
    const int32_t a = FLY_FloatToInt(target);
    const int32_t b = FLY_FloatToInt(current);
    const int32_t lo = a & 0xfff;
    int32_t hi = b & 0xfff;
    const int32_t diff = hi - lo;
    if (diff > 0x800) hi -= 0x1000;
    else if (diff < -0x800) hi += 0x1000;
    const int32_t sum = hi + lo * (k - 1);
    return static_cast<float>((sum / k) & 0xfff);
}

}  // namespace

void FLY_Draw(float step) {
    if (step > 2.0f) step = 2.0f;
    for (int32_t i = 1; i < 100; ++i) {
        uint8_t* base = static_cast<uint8_t*>(gFlyBases[i]);
        if (base == nullptr) continue;
        for (int32_t n = 0; n < 20; ++n) {
            uint8_t* p = base + static_cast<uint32_t>(n) * 0xe4 + 0x5c;
            if (W(p, 0x80) == 0) continue;
            if (W(p, 0xc8) != 0) {
                gFlyDrawPosX = static_cast<float>(W(p, 4)) / 300.0f;
                gFlyDrawPosY = static_cast<float>(W(p, 8)) / 300.0f;
                gFlyDrawPosZ = static_cast<float>(W(p, 0xc)) / 300.0f;
                gFlyDrawAngA = static_cast<float>(W(p, 0x10));
                gFlyDrawAngB = static_cast<float>(W(p, 0x18));
                gFlyDrawAngC = static_cast<float>(W(p, 0x14));
                W(p, 0xc8) = 0;
            } else {
                gFlyDrawPosX = ppc_fmadds(step, static_cast<float>(W(p, 4) - W(p, 0x1c)), static_cast<float>(W(p, 0x1c))) / 300.0f;
                gFlyDrawPosY = ppc_fmadds(step, static_cast<float>(W(p, 8) - W(p, 0x20)), static_cast<float>(W(p, 0x20))) / 300.0f;
                gFlyDrawPosZ = ppc_fmadds(step, static_cast<float>(W(p, 0xc) - W(p, 0x24)), static_cast<float>(W(p, 0x24))) / 300.0f;
                int32_t t = (W(p, 0x10) - W(p, 0x28)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngA = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W(p, 0x28)));
                t = (W(p, 0x18) - W(p, 0x30)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngB = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W(p, 0x30)));
                t = (W(p, 0x14) - W(p, 0x2c)) & 0xfff;
                if (t >= 0x800) t -= 0xfff;
                gFlyDrawAngC = ppc_fmadds(step, static_cast<float>(t), static_cast<float>(W(p, 0x2c)));
                gFlyDrawPosX = ppc_fmadds(3.0f, Fl(p, 0x34), gFlyDrawPosX) * 0.25f;
                gFlyDrawPosY = ppc_fmadds(3.0f, Fl(p, 0x38), gFlyDrawPosY) * 0.25f;
                gFlyDrawPosZ = ppc_fmadds(3.0f, Fl(p, 0x3c), gFlyDrawPosZ) * 0.25f;
                gFlyDrawAngA = Blend(Fl(p, 0x48), gFlyDrawAngA, 4);
                gFlyDrawAngB = Blend(Fl(p, 0x40), gFlyDrawAngB, 4);
                gFlyDrawAngC = Blend(Fl(p, 0x44), gFlyDrawAngC, 8);
            }
            Fl(p, 0x34) = gFlyDrawPosX;
            Fl(p, 0x38) = gFlyDrawPosY;
            Fl(p, 0x3c) = gFlyDrawPosZ;
            Fl(p, 0x48) = gFlyDrawAngA;
            Fl(p, 0x40) = gFlyDrawAngB;
            Fl(p, 0x44) = gFlyDrawAngC;
            float v[3];
            v[0] = gFlyDrawOrigin[0];
            v[1] = gFlyDrawOrigin[1];
            v[2] = gFlyDrawOrigin[2];
            const int32_t ride = W(p, 0xc0);
            if (static_cast<uint32_t>(ride) == 0) continue;
            SetLMatrixIdentityNoTrans(RideMatrix(ride));
            sMatrix* m = RideMatrix(ride);
            m->m[3][0] = gFlyDrawPosX;
            m->m[3][1] = gFlyDrawPosY;
            m->m[3][2] = gFlyDrawPosZ;
            const float pi = 3.1415927f;
            v[0] = pi * (2.0f * -gFlyDrawAngB) / 4096.0f;
            v[2] = pi + pi * (2.0f * (3072.0f - gFlyDrawAngA)) / 4096.0f;
            v[1] = pi * (2.0f * (4.0f * -gFlyDrawAngC)) / 4096.0f;
            RotateLMatrixXYZNoTrans(RideMatrix(ride), static_cast<eRotateParams>(0xe), v);
            if (static_cast<uint32_t>(W(p, 0xc4)) == 0) continue;
            const uint32_t index = FindDataNodeIndexByID(gRideAnimSets[ride], 0x100, 1);
            if (index == 0xffffffffu) continue;
            sEmbeddedSet* set = static_cast<sEmbeddedSet*>(gRideAnimSets[ride]->mHeads->mF28);
            uint8_t* block = set->mRecords[index].mBlock;
            float x = 0.0f, y = 0.0f, z = 0.0f;
            if (block != nullptr) {
                x = Fl(block, 0x30);
                y = Fl(block, 0x34);
                z = Fl(block, 0x38);
            }
            const int32_t ix = static_cast<int32_t>(x);
            const int32_t iy = static_cast<int32_t>(y);
            const int32_t iz = static_cast<int32_t>(z);
            PAR_SetEmitterPos(W(p, 0xc4), static_cast<int32_t>(static_cast<uint32_t>(ix) << 10),
                              static_cast<int32_t>(static_cast<uint32_t>(iy) << 10), static_cast<int32_t>(static_cast<uint32_t>(iz) << 10));
        }
    }
}
