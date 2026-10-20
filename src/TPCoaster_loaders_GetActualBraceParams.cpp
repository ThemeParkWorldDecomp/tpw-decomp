// Unit v5278: GetActualBraceParams (0x1003d4cc) and UpdatePylonStackStructures (0x1003d844).
#include <cstdint>

#include "TPCoaster.h"
#include "m3d2.h"

// 0x1003d4cc: for every brace of the current coaster (a list linked by +0x64) asks GetAlignmentParamsSlow for each of its 0x58-byte
// records, then clears flag 0x800 unless 0x1000 is set. The result of the call goes unused (the original also loads a dead word).
void GetActualBraceParams() {
    sBraceWalkData* data = static_cast<sBraceWalkData*>(gCoasterCurrentData);
    for (sBraceWalkNode* node = data->mHead; node != nullptr; node = node->mNext) {
        sBraceWalkRecord* record = node->mList->mRecords;
        for (uint32_t i = 0; i < node->mList->mCount; ++i) {
            sCoasterOrientation orientation;
            float sum = node->mF6C + record->mF0C;
            GetAlignmentParamsSlow(static_cast<sCoasterTrackParams*>(gCoasterCurrentData), &record->mPoint, sum, &orientation, 1, nullptr);
            ++record;
        }
        if ((node->mFlags & 0x1000) == 0) node->mFlags &= 0xfffff7ffu;
    }
}

// 0x1003d844: for each pylon of the stack (count at +0x30, pointers from +0x34) sets or clears 0x100 (set unless last) and points its +0x90 and
// +0x74 words at the mesh areas of its kind.
void UpdatePylonStackStructures(sPylonControl* control, tag_sPylon* pylon) {
    (void)control;
    if (gPylonStackSkip != 0) return;
    sPylonStackHead* stack = reinterpret_cast<sPylonStackHead*>(pylon);
    for (uint32_t i = 0; i < stack->mCount; ++i) {
        uint32_t count = stack->mCount;
        sStackPylon* p = stack->mPylons[i];
        if (count > 1 && i < count - 1)
            p->mFlags |= 0x100u;
        else
            p->mFlags &= 0xfffffeffu;
        sStackQ* q = p->mCtl->mQ;
        if ((p->mFlags & 0x2000) == 0 && (p->mFlags & 0x100) != 0 && (q->mFlags & 8) != 0) {
            p->mF90 = p->mArea84;
            p->mF74 = q->mArea44;
        } else {
            p->mF90 = p->mArea78;
            p->mF74 = q->mArea30;
        }
    }
}
