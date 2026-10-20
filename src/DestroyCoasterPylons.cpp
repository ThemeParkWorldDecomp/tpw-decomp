// TPConfig.cpp (main app): the destroyers of a coaster's track and pylons, and the track position control. Unit v5038.
#include <cstdint>

#include "TPCoaster.h"

// 0x10050df4: the chain of mesh sets (linked by the set's +0x38) is taken off the instance one set at a time and destroyed.
void DestroyCoasterTrack(tag_sCoasterInstance* instance) {
    while (tag_sDMeshSet* set = instance->mF38) {
        instance->mF38 = set->mF38;
        DestroyDynamicMeshSetInstance(set, nullptr, 0);
    }
}

// 0x10050e84: every node of the pylon list is destroyed and freed. A node with flag 1 holds one brace and its info (brace + 0x78); the others
// destroy their braces one by one when the control's flag 4 is set, then the 3D object.
void DestroyCoasterPylons(tag_sCoasterInstance* instance) {
    sCoasterPylon* node = instance->mPylons;
    while (node != nullptr) {
        if ((node->mFlags & 1) == 0) {
            if ((node->mControl->mFlags & 4) != 0) {
                for (uint32_t i = 0; i < node->mCount; ++i) DestroyTrackBrace(node->mBraces[i], node->mControl);
            }
            Destroy3DObjectFromAIWorld(node->mF0C, node->mF10);
        } else {
            tag_sTrackBrace* brace = node->mBraces[0];
            DestroyBraceInfo(brace, reinterpret_cast<sBraceInfo*>(reinterpret_cast<char*>(brace) + 0x78));
        }
        sCoasterPylon* next = node->mNext;
        MReleaseFreeF(node);
        node = next;
    }
}

// 0x10051300: one control further along the track.
void IncrementTrackPosnControl(sTrackPosnControl* control) {
    tag_sTrackBrace* brace = control->mBrace;
    tag_sTrackBrace* braceB = control->mBraceB;
    if (brace == braceB) {
        if (control->mIndexB < brace->mF90->mCount - 1u) {
            control->mIndex = control->mIndex + 1;
            control->mIndexB = control->mIndexB + 1;
            return;
        }
        control->mBraceB = braceB->mPrev;
        control->mIndex = control->mIndex + 1;
        control->mIndexB = 0;
        return;
    }
    if (braceB->mF90->mCount > 1) {
        control->mBrace = braceB;
        control->mIndex = 0;
        control->mIndexB = control->mIndexB + 1;
        return;
    }
    control->mBrace = braceB;
    control->mBraceB = control->mBraceB->mPrev;
    control->mIndex = control->mBrace->mF90->mCount - 1;
}

// 0x100513fc: one control back.
void DecrementTrackPosnControl(sTrackPosnControl* control) {
    tag_sTrackBrace* brace = control->mBrace;
    if (brace == control->mBraceB) {
        if (control->mIndex != 0) {
            control->mIndex = control->mIndex - 1;
            control->mIndexB = control->mIndexB - 1;
            return;
        }
        control->mBrace = brace->mNext;
        control->mIndex = control->mBrace->mF90->mCount - 1;
        control->mIndexB = 0;
        return;
    }
    if (brace->mF90->mCount > 1) {
        control->mBraceB = brace;
        control->mIndexB = control->mIndex;
        control->mIndex = control->mIndex - 1;
        return;
    }
    control->mBraceB = brace;
    control->mBrace = control->mBrace->mNext;
    control->mIndex = control->mBrace->mF90->mCount - 1;
}

// 0x100514f0: the point at t between the two controls (both read from the spline's first array, +4; the 0x58 stride is the Mac's).
void GetTrackPosition(sTrackPosnControl* control, float t, s3DPoint* point) {
    char* a = reinterpret_cast<char*>(control->mBrace->mF90->mControlsA) + control->mIndex * 0x58;
    char* b = reinterpret_cast<char*>(control->mBraceB->mF90->mControlsA) + control->mIndexB * 0x58;
    BezierPosition(reinterpret_cast<tag_sBezierControl*>(a), reinterpret_cast<tag_sBezierControl*>(b), t, point);
}
