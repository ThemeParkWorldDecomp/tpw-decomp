#include "tpw_probe.h"
#include <cstdio>
#include <cstring>
#include "Debug.h"
#include "Game.h"
#include "Quickload.h"
#include "TPEngine.h"
#include "TPMain.h"
#include "rsse.h"
#include "tb_mesh.h"
#include "analyser.h"
#include "RideDB.h"
#include "SaveLoad.h"
#include "TPCoaster.h"

// 0x1006aa70: traces the time control's fields (nothing for a control of kind 0xc).
void OutputAnimTimeStats(sAnimTimeControl* control, tag_sRideAnimSet* set) {
    if (control->mF04 == 0xc) return;
    char buffer[256];
    int32_t diff = static_cast<int32_t>(reinterpret_cast<uintptr_t>(control) - reinterpret_cast<uintptr_t>(set->mTimeControls));
    int64_t magic = static_cast<int32_t>(0x92492493u);
    int32_t high = static_cast<int32_t>((magic * diff) >> 32);
    int32_t t = (high + diff) >> 5;
    int32_t index = t + static_cast<int32_t>(static_cast<uint32_t>(t) >> 31);
    const char* name = reinterpret_cast<const char*>(set->mHeads->mHeader) + 0x18;
    sprintf(buffer, "interesting AnimTimeControl %d/%d to ride instance %s\n", index, static_cast<int32_t>(set->mTimeCount), name);
    CTRACE(5, buffer);
    sprintf(buffer, "Flags                : %d\n", control->mFlags);
    CTRACE(5, buffer);
    sprintf(buffer, "AnimID               : %d\n", control->mF04);
    CTRACE(5, buffer);
    sprintf(buffer, "SubAnim              : %d\n", control->mF08);
    CTRACE(5, buffer);
    sprintf(buffer, "StartAnimTime        : %d\n", control->mF10);
    CTRACE(5, buffer);
    sprintf(buffer, "AnimTime             : %d\n", control->mF14);
    CTRACE(5, buffer);
    sprintf(buffer, "NoPauseAnimTime      : %d\n", control->mF18);
    CTRACE(5, buffer);
    sprintf(buffer, "TotalAnimFrames      : %.2f\n", static_cast<double>(control->mF1C));
    CTRACE(5, buffer);
    sprintf(buffer, "AnimFrame            : %.2f\n", static_cast<double>(control->mF20));
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredAnimID       : %d\n", control->mF24);
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredSubAnim      : %d\n", control->mF28);
    CTRACE(5, buffer);
    sprintf(buffer, "DeferredFlags        : %d\n\n", control->mF2C);
    CTRACE(5, buffer);
}

// 0x1006a140: finds the existing instance (or checks the given one) and marks it restored. `a`, `f` are unused.
uint32_t RestoreRideInstance(uint32_t instance, uint32_t type, uint32_t flags, sFixedRideParams* params, uint32_t a, void* c, int32_t d,
                             int32_t e, int32_t f) {
    int32_t width = d;
    if (width == 0) {
        if (params != nullptr && params->mF0C != nullptr) width = static_cast<const int32_t*>(params->mF0C)[0];
        else width = 0;
    }
    int32_t depth = e;
    if (depth == 0) {
        if (params != nullptr && params->mF0C != nullptr) depth = static_cast<const int32_t*>(params->mF0C)[1];
        else depth = 0;
    }
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> id;
    id.mIndex = static_cast<uint16_t>(gRideTypes[type]->mFF8);
    RideInfo* info = gRideDatabase.GetRideInfo(id);

    tag_sRideAnimSet* set = nullptr;
    bool found = false;
    if (instance != 0) {
        found = true;
        set = gRideAnimSets[instance];
    } else {
        for (uint32_t i = 1; i <= gRideCountB + gRideCountA; ++i) {
            set = gRideAnimSets[i];
            if (set == nullptr) continue;
            uint32_t k = gRideTypeCount;
            for (; k != 0; --k)
                if (set->mSelf == gRideTypes[k]) break;
            if ((set->mFlags & 0x10000) != 0) continue;
            if (k != type) continue;
            if (set->mFE4 != flags) continue;
            if (params == nullptr) {
                found = true;
                instance = i;
                break;
            }
            if (set->mFE8 != params->mX) continue;
            if (set->mFEC != params->mZ) continue;
            if (set->mHeads->mF10 != params->mRotation) continue;
            if (params->mF0C == nullptr) {
                found = true;
                instance = i;
                break;
            }
            if (info != nullptr) {
                // The record's Shape slot (the Mac's +0x18) against the params' shape: whole pointers, by name (the Mac's compare is of
                // its 32-bit pointers; the host's record is laid out wider and its slot holds a native pointer).
                if (info->Value(kRideSlot_Shape).mShape != params->mF0C) continue;
                found = true;
                instance = i;
                break;
            }
            if (width != set->mFF0) continue;
            if (depth != set->mFF4) continue;
            found = true;
            instance = i;
            break;
        }
    }
    if (!found) return 0;
    if ((set->mFlags & 0x40000) != 0) SetCoasterCObject(instance, c);
    set->mFlags |= 0x10000;
    return instance;
}
