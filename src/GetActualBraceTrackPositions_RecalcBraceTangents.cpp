#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "BMP_InsideArea.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "ppc_math.h"
#include "rsse.h"
#include "tb_matrix.h"

// 0x1003d080 (1,056 bytes): recomputes the braces of the current coaster: their positions from their pylons, the required tangents, the
// alignment angles (yaw, pitch, bank) and the animated sets, then the actual tangents. The Mac reads the instance's +0x3c block
// (gCoasterCurrentData): its +0x10 is the instance's pylon list and +0x18 the chain of braces, here the instance's mPylons and mLastBrace.
// The brace's pylon (+0x100) is read as a sTrackBraceData: control at the mF18Table, x y z at 0x1c, the braces from the Mac's 0x34 (mOwner).
// Bug kept: the animated set of the previous brace is used by a brace that has none (r23 is not reset; NULL for the first).
void RecalcBraceTangents() {
    tag_sCoasterInstance* instance = gCoasterCurrent;
    sCoasterType* type = const_cast<sCoasterType*>(reinterpret_cast<const sCoasterType*>(instance->mType));
    sCoasterTrackParams* params = reinterpret_cast<sCoasterTrackParams*>(gCoasterCurrentData);
    tag_sRideAnimSet* set = nullptr;

    for (sCoasterPylon* pylon = instance->mPylons; pylon != nullptr; pylon = pylon->mNext) {
        for (uint32_t i = 0; i < pylon->mCount; ++i) {
            tag_sTrackBrace* brace = pylon->mBraces[i];
            const sTrackBraceData* data = brace->mData;
            sPylonControl* control = reinterpret_cast<sPylonControl*>(const_cast<float*>(data->mF18Table));
            if ((data->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0 && control->mF2C != -1) {
                std::memcpy(&brace->mX4C, &data->mF1C[0], 4);
                std::memcpy(&brace->mL50, &data->mF1C[4], 4);
                std::memcpy(&brace->mZ54, &data->mF1C[8], 4);
                const float five = 5.0f;
                brace->mX4C = brace->mX4C + five;
                brace->mZ54 = brace->mZ54 + five;
                uint32_t index;
                std::memcpy(&index, &brace->mF00[4], 4);
                if (index != 0) {
                    brace->mL50 = GetHeightForInterval(brace->mFE8w, control);
                    brace->mL50 = brace->mL50 + control->mFC4;
                    tag_sTrackBrace* const* braces = reinterpret_cast<tag_sTrackBrace* const*>(
                        reinterpret_cast<const uint8_t*>(brace->mData) + offsetof(sTrackBraceData, mOwner));
                    brace->mL50 = brace->mL50 + braces[index - 1]->mL50;
                } else {
                    float h = GetHeightForInterval(brace->mFE8w, control);
                    brace->mL50 = brace->mL50 + h;
                }
            }
        }
    }

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev)
        GetRequiredBraceTangents(brace, type, params);

    uint32_t typeFlags;
    std::memcpy(&typeFlags, type, 4);
    uint32_t banked = typeFlags & 0x10;
    float speed = 0.0f;
    float typeF48;
    std::memcpy(&typeF48, &type->mF00[0x48], 4);
    const float twoPi = 6.2831854820251465f;
    const float half = 0.5f;
    const float one = 1.0f;

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        tag_sTrackBrace* next = brace->mPrev;
        s3DPoint point;
        std::memcpy(&point, &brace->mF0C[0x34 - 0x0c], 12);
        sCoasterOrientation orientation;
        std::memset(&orientation, 0, sizeof(orientation));
        GetAlignmentParamsSlow(params, &point, brace->mF6C, &orientation, 1, nullptr);
        brace->mFD4 = orientation.mF04;
        if ((brace->mFlags & 1) == 0) brace->mAngle = orientation.mF04;
        if ((brace->mFlags & 2) == 0) brace->mFDC = orientation.mF08;
        if ((brace->mFlags & 4) == 0) brace->mFD8 = 0.0f;
        if (gCoasterWord7310 != 0) {
            uint32_t number = brace->mNumber;
            CTRACE(5, "Required brace %d params: Dirn %+.2f  Pitch %+.2f\n", number, static_cast<double>(orientation.mF04),
                   static_cast<double>(orientation.mF08));
        }
        if (banked != 0 && (brace->mFlags & 4) == 0 && next != nullptr) {
            brace->mFD8 = GetBankAtBrace(gCoasterCurrent, brace, speed);
            speed = GetCarWantedSpeedAtHeight(type, speed, typeF48, brace->mL50, next->mL50);
        }
        brace->mFEC = brace->mAngle / twoPi;
        double whole;
        float frac = static_cast<float>(std::modf(static_cast<double>(brace->mFEC), &whole));
        if (frac < 0.0f) frac = one + frac;
        brace->mFEC = frac;
        brace->mFF0 = brace->mFDC / twoPi;
        brace->mFF4 = brace->mFD8;
        brace->mFF4 = brace->mFF4 / twoPi;
        float shifted = half + brace->mFF4;
        frac = static_cast<float>(std::modf(static_cast<double>(shifted), &whole));
        if (frac < 0.0f) frac = one + frac;
        brace->mFF4 = frac - half;
        if ((brace->mData->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0) {
            SetupBraceAnimFrameData(type, brace);
            set = gRideAnimSets[brace->mWord70];
            AnimateRideSet(set, 1, 0);
        }
        GetActualBraceTrackPositions(set, brace);
    }

    for (tag_sTrackBrace* brace = instance->mLastBrace; brace != nullptr; brace = brace->mPrev) {
        tag_sRideAnimSet* animSet = nullptr;
        if ((brace->mData->mFlags & 1) == 0 && (brace->mFlags & 0x2000) == 0) animSet = gRideAnimSets[brace->mWord70];
        GetActualBraceTangents(brace, animSet);
    }
}
