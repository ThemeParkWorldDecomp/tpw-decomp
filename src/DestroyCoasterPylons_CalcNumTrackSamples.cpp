#include <cstdint>
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

// 0x1005162c: walks the brace chain with a track position control, summing the distance of each step into *total and
// returning the sum of the sample counts of each step (the control lives on the stack: index, brace, indexB, braceB).
int32_t CalcNumTrackSamples(tag_sTrackBrace* brace, float step, float* total) {
    int32_t count = 0;
    sTrackPosnControl control;
    control.mIndex = 0;
    control.mBrace = brace;
    if (brace->mF90->mCount > 1) {
        control.mBraceB = brace;
        control.mIndexB = 1;
    } else {
        control.mBraceB = brace->mPrev;
        control.mIndexB = 0;
    }
    s3DPoint prev;
    s3DPoint next;
    GetTrackPosition(&control, 0.0f, &prev);
    while (control.mBraceB != nullptr) {
        GetTrackPosition(&control, 1.0f, &next);
        float dist = DistanceBetweenPoints(&prev, &next);
        float ratio = dist / step;
        *total = *total + dist;
        float arg = ppc_fmadds(3.0f, ratio, 1.0f);
        count += FloatToCell(static_cast<double>(arg));
        IncrementTrackPosnControl(&control);
        prev = next;
    }
    return count;
}
