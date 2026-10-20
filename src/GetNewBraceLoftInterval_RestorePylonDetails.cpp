// Unit v5667: the height of a brace in its pylon's stack, squishing the stack, inserting a brace into a pylon and restoring a saved pylon
// (TPCoaster.cpp): CalculateBraceHeight 0x1003e6e0, RestorePylonDetails 0x1003ec00, SquishPylonBracesIfRequired 0x1003ee10,
// InsertBraceIntoPylon 0x1003ef78.
//
// The host's records differ from the Mac's: the pylon a brace points at is an sPylonRecord (native pointers), the brace's word at +4 (its
// number in the pylon) is read through mF00, and the words RestorePylonDetails copies at Mac offsets 0x94..0xac are appended fields.
#include <cstdint>
#include <cstring>
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

namespace {

// The brace's number in its pylon (the Mac word at +4).
uint32_t& NumberOf(tag_sTrackBrace* brace) { return *reinterpret_cast<uint32_t*>(&brace->mF00[4]); }

// The pylon a brace stands in (the Mac word at +0x100).
sPylonRecord* PylonOf(const tag_sTrackBrace* brace) {
    return reinterpret_cast<sPylonRecord*>(const_cast<sTrackBraceData*>(brace->mData));
}

// The Mac word at +0x50 (BraceY of the neighbouring units).
float& HeightOf(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x50); }

sCoasterType* TypeOf(tag_sCoasterInstance* instance) {
    return reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(instance->mType));
}

// The interval clamped to the two constants at 0x1029cd30 / 0x1029cd34 (0.0f and 1.0f; a NaN becomes the first, as the compares leave it).
float ClampInterval(float value) {
    // fcmpo then bge: the branch is taken for greater, equal and unordered, so a NaN passes both compares unchanged.
    if (value < 0.0f) value = 0.0f;
    if (1.0f < value) value = 1.0f;
    return value;
}

}  // namespace

float CalculateBraceHeight(tag_sTrackBrace* brace) {
    if (brace == nullptr) return 0.0f;
    sPylonRecord* pylon = PylonOf(brace);
    sPylonControl* control = pylon->mControl;
    float total = 0.0f;
    for (uint32_t i = 0; i < NumberOf(brace); i++) {
        tag_sTrackBrace* below = pylon->mBraces[i];
        if (below != nullptr) {
            float height = GetHeightForInterval(below->mFE8w, control);
            total = total + height;
            total = total + control->mFC4;
        }
    }
    float own = GetHeightForInterval(brace->mFE8w, control);
    return total + own;
}

void RestorePylonDetails(tag_sCoasterInstance* instance, tag_sPylon* pylonArg) {
    sPylonRecord* pylon = reinterpret_cast<sPylonRecord*>(pylonArg);
    std::memcpy(pylon, &gLinkSavedPylon, sizeof(sPylonRecord));   // the Mac copies 0x48 bytes
    uint32_t i = 0;
    for (; i < gLinkSavedPylon.mCount; i++) {
        tag_sTrackBrace* brace = pylon->mBraces[i];
        tag_sTrackBrace* saved = &gLinkSavedBraces[i];
        NumberOf(brace) = NumberOf(saved);
        brace->mFlags = (brace->mFlags & 0x600000u) | saved->mFlags;
        std::memcpy(&brace->mF0C[0], &saved->mF0C[0], 0x58);   // 0x0c..0x63
        brace->mPrev = saved->mPrev;
        brace->mNext = saved->mNext;
        brace->mF6C = saved->mF6C;
        brace->mF90 = saved->mF90;
        brace->mF94w = saved->mF94w;
        brace->mW98 = saved->mW98;
        brace->mW9C = saved->mW9C;
        brace->mWA0 = saved->mWA0;
        brace->mWA4 = saved->mWA4;
        brace->mWA8 = saved->mWA8;
        brace->mWAC = saved->mWAC;
        brace->mFD4 = saved->mFD4;
        brace->mFD8 = saved->mFD8;
        brace->mFDC = saved->mFDC;
        brace->mAngle = saved->mAngle;
        brace->mFE4 = saved->mFE4;
        brace->mFE8w = saved->mFE8w;
        brace->mFEC = saved->mFEC;
        brace->mFF0 = saved->mFF0;
        brace->mFF4 = saved->mFF4;
        brace->mFF8 = saved->mFF8;
        brace->mSample = saved->mSample;
        brace->mData = saved->mData;
        HeightOf(brace) = gLinkSavedHeights[i];
        SetupBraceAnimFrameData(TypeOf(instance), brace);
    }
    for (; i < 5; i++) pylon->mBraces[i] = nullptr;
    gLinkSavedFlag = 0;
}

int32_t SquishPylonBracesIfRequired(tag_sCoasterInstance* instance, sPylonControl* control, sLinkPylon* pylon) {
    int32_t changed = 0;
    uint32_t count = pylon->mCount;
    if (count > 1) {
        tag_sTrackBrace* top = pylon->mBraces[count - 1];
        for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; i--) {
            tag_sTrackBrace* brace = pylon->mBraces[i];
            if (!(CalculateBraceHeight(top) > control->mF6C)) return changed;   // ble: also taken for a NaN
            float interval = GetIntervalForHeight(control->mF68, control);
            brace->mFE8w = ClampInterval(interval);
            SetupBraceAnimFrameData(TypeOf(instance), brace);
            changed = 1;
        }
    }
    return changed;
}

void InsertBraceIntoPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sLinkPylon* pylon, uint32_t level, float height, float* out,
                          int32_t a, int32_t b, int32_t c, int32_t d) {
    sPylonControl* control = pylon->mControl;
    tag_sTrackBrace** braces = pylon->mBraces;
    tag_sTrackBrace* next;
    float nextHeight;
    float room;   // the stack height the next brace leaves, 0x48(r1)
    if (level < pylon->mCount) {
        next = braces[level];
        nextHeight = CalculateBraceHeight(next);
        float gap = control->mF68 + control->mFC4;
        room = nextHeight - gap;
    } else {
        nextHeight = 0.0f;
        next = nullptr;
        room = nextHeight;
    }
    float below;   // f2: the height under the new brace
    if (level != 0) {
        float under = CalculateBraceHeight(braces[level - 1]);
        below = control->mFC4 + under;
    } else {
        below = 0.0f;
    }
    if (b == 0) {
        for (uint32_t i = pylon->mCount; i > level; i--) {
            braces[i] = braces[i - 1];
            NumberOf(braces[i]) = i;
        }
    }
    braces[level] = brace;
    if (a != 0) pylon->mFlags |= 0x20;
    NumberOf(brace) = level;
    brace->mData = reinterpret_cast<const sTrackBraceData*>(pylon);
    pylon->mCount = pylon->mCount + 1;

    if (next == nullptr || b != 0) {
        if (control != nullptr) {
            float local = height;
            if (NumberOf(brace) != 0) {
                float under = CalculateBraceHeight(PylonOf(brace)->mBraces[NumberOf(brace) - 1]);
                local = local - (under + control->mFC4);
            }
            if (d == 0) {
                if (local < control->mF68) local = control->mF68;
                if (control->mF6C < local) local = control->mF6C;
            }
            float interval = GetIntervalForHeight(local, control);
            brace->mFE8w = ClampInterval(interval);
            SetupBraceAnimFrameData(TypeOf(instance), brace);
        }
        if (out != nullptr) *out = height;
    } else {
        if (!(height < room)) height = room;
        float local = height - below;
        if (local < control->mF68) local = control->mF68;
        float result = local + below;   // f30
        float interval = GetIntervalForHeight(local, control);
        brace->mFE8w = ClampInterval(interval);
        SetupBraceAnimFrameData(TypeOf(gCoasterEditInstance), brace);
        float nextLocal = nextHeight;
        if (NumberOf(next) != 0) {
            float under = CalculateBraceHeight(PylonOf(next)->mBraces[NumberOf(next) - 1]);
            nextLocal = nextLocal - (under + control->mFC4);
        }
        if (d == 0) {
            if (nextLocal < control->mF68) nextLocal = control->mF68;
            if (control->mF6C < nextLocal) nextLocal = control->mF6C;
        }
        float nextInterval = GetIntervalForHeight(nextLocal, control);
        next->mFE8w = ClampInterval(nextInterval);
        SetupBraceAnimFrameData(TypeOf(instance), next);
        if (out != nullptr) *out = result;
    }

    if (control != nullptr && b == 0) {
        if (c == 0) SquishPylonBracesIfRequired(instance, control, pylon);
        if (gPylonStackSkip == 0) {
            for (uint32_t i = 0; i < pylon->mCount; i++) {
                tag_sTrackBrace* stacked = braces[i];
                uint32_t count = pylon->mCount;
                if (count > 1 && i < count - 1)
                    stacked->mFlags |= 0x100;
                else
                    stacked->mFlags &= ~0x100u;
                sPylonControl* stackedControl = PylonOf(stacked)->mControl;
                if ((stacked->mFlags & 0x2000) == 0 && (stacked->mFlags & 0x100) != 0 && (stackedControl->mFlags & 8) != 0) {
                    stacked->mF90 = reinterpret_cast<sBraceSpline*>(&stacked->mCount84);
                    stacked->mF74 = reinterpret_cast<const uint32_t*>(&stackedControl->mF44);
                } else {
                    stacked->mF90 = reinterpret_cast<sBraceSpline*>(&stacked->mCount78);
                    stacked->mF74 = reinterpret_cast<const uint32_t*>(&stackedControl->mF30);
                }
            }
        }
        UpdatePylonStackMeshes(control, reinterpret_cast<tag_sPylon*>(pylon), 0, 0);
    }
}
