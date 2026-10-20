// Unit v5366: the track editor's preview brace (TPCoaster.cpp on the Mac).
//   LinkPreviewBrace 0x1003f9fc (1,184 bytes), LinkPreviewBraceToPylon 0x1003f4fc (740 bytes)
#include <cstdint>
#include <cstring>

#include "ActionRecorder.h"
#include "LLCellSystem.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"

// The copies LinkPreviewBraceToPylon keeps (the Mac's BSS words).
uint32_t gLinkSavedFlag;               // 0x102b6efc
sPylonRecord gLinkSavedPylon;          // 0x102b6eb4
tag_sTrackBrace gLinkSavedBraces[5];   // 0x102b69a0
float gLinkSavedHeights[5];            // 0x102b698c
sPylonRecord* gLinkFoundPylon;         // 0x102b6f00

namespace {

// The words of a brace the layout leaves inside a byte block (the same offsets on the Mac and the host).
float& BraceX(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x4c); }
float& BraceY(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x50); }
float& BraceZ(tag_sTrackBrace* brace) { return *reinterpret_cast<float*>(reinterpret_cast<char*>(brace) + 0x54); }

sCoasterTrackParams* ParamsOf(tag_sCoasterInstance* instance) { return reinterpret_cast<sCoasterTrackParams*>(&instance->mBlock3C); }

}  // namespace

void LinkPreviewBraceToPylon(tag_sCoasterInstance* instance, tag_sTrackBrace* brace, sPylonRecord* pylon, float unused, int32_t copy) {
    (void)unused;
    // r22: the brace of the drag (the second one when the block's flag 2 is set)
    tag_sTrackBrace* drag = (instance->mBlock3C & 2) != 0 ? instance->mBrace64 : instance->mLastBrace;
    sPylonControl* control;
    if (copy != 0) {
        std::memcpy(&gLinkSavedPylon, pylon, sizeof(sPylonRecord));
        for (uint32_t i = 0; i < gLinkSavedPylon.mCount; i++) {
            tag_sTrackBrace* source = pylon->mBraces[i];
            if (source != nullptr) std::memcpy(&gLinkSavedBraces[i], source, sizeof(tag_sTrackBrace));
            gLinkSavedHeights[i] = BraceY(source);
        }
        gLinkSavedFlag = 1;
        control = pylon->mControl;
        if ((gCoasterInputFlags & 0x40) != 0) {
            brace->mFlags |= 0x40;
            if (gCoasterWord1790 == 2) brace->mFlags |= 0x80;
        }
    } else {
        if ((gCoasterInputFlags & 0x40) != 0) {
            brace->mFlags |= 0x40;
            control = reinterpret_cast<sPylonControl*>(reinterpret_cast<char*>(const_cast<uint32_t*>(instance->mType)) + 0x1c8);
            if (gCoasterWord1790 == 2) brace->mFlags |= 0x80;
        } else {
            control = reinterpret_cast<sPylonControl*>(reinterpret_cast<char*>(const_cast<uint32_t*>(instance->mType)) + 0xa8);
        }
        pylon->mControl = control;
    }

    float height;
    bool reuse;
    if (copy != 0 && reinterpret_cast<const sPylonRecord*>(drag->mData) == pylon) {
        reuse = true;
        uint32_t index = *drag->mF74;
        const char* table = reinterpret_cast<const char*>(drag->mF90->mControlsA);
        height = *reinterpret_cast<const float*>(table + index * 0x58 + 0x44);
    } else {
        reuse = false;
        height = GetNewBraceLoftInterval(reinterpret_cast<sLinkPylon*>(pylon), control, brace->mNext, instance->mBraceB5C,
                                         static_cast<int32_t>(brace->mFlags & 0x80), 1, nullptr);
    }

    uint32_t level;
    float linkHeight;
    DetermineLinkLevel(height, reinterpret_cast<sLinkPylon*>(pylon), &level, &linkHeight, 1);
    BraceY(brace) = linkHeight;
    float inserted;
    InsertBraceIntoPylon(instance, brace, reinterpret_cast<sLinkPylon*>(pylon), level, linkHeight, &inserted, 1, 0, 1, 0);
    if (reuse) {
        BraceY(brace) = height;
    } else {
        BraceY(brace) = inserted;
        int32_t cellX = ppc_fctiwz(static_cast<double>(pylon->mX / 10.0f));
        int32_t cellZ = ppc_fctiwz(static_cast<double>(pylon->mZ / 10.0f));
        float offset = GetRideRequiredYOffset(cellX, cellZ, nullptr, 1, 1, 0x3fa, 0);
        BraceY(brace) = BraceY(brace) + offset;
    }
    brace->mFE8w = 0.0f;
    BraceX(brace) = ppc_fmadds(10.0f, static_cast<float>(pylon->mCellX), 5.0f);
    BraceZ(brace) = ppc_fmadds(10.0f, static_cast<float>(pylon->mCellZ), 5.0f);
}

void LinkPreviewBrace(tag_sCoasterInstance* instance) {
    sCoasterTrackParams* params = ParamsOf(instance);
    uint32_t flags = instance->mBlock3C;

    uint32_t cellZ = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(gPointerWorldPosn.z / 10.0f)));
    uint32_t cellX = static_cast<uint32_t>(ppc_fctiwz(static_cast<double>(gPointerWorldPosn.x / 10.0f)));
    uint32_t stride = gHeightfieldHeader->mHField->mF18[0];
    uint32_t index = cellX + cellZ * stride;

    sPylonRecord* found = nullptr;
    for (tag_sCoasterInstance* it = gCoasterInstances; it != nullptr; it = it->mNext) {
        sPylonRecord* p = reinterpret_cast<sPylonRecord*>(it->mPylons);
        while (p != nullptr && ((p->mFlags & 5) != 0 || p->mIndex != index)) p = p->mNext;
        if (p != nullptr) {
            found = p;
            break;
        }
    }
    gLinkFoundPylon = found;

    float dz = ppc_fnmsubs(10.0f, static_cast<float>(cellZ), BraceZ(instance->mPreviewBrace));
    float dx = ppc_fnmsubs(10.0f, static_cast<float>(cellX), BraceX(instance->mPreviewBrace));
    dz = dz * dz;
    float distance = ppc_fmadds(dx, dx, dz);
    if (distance > 90000.0f) return;

    int32_t valid = IsValidPylonPosition(cellX, cellZ, nullptr, 0, 0, 0, 0);
    float cost;
    if (found == nullptr) {
        LinkPylon(instance->mPreviewPylon, params, cellX, cellZ, index);
        LinkBrace(instance->mBrace6C, params, valid);
        LinkPreviewBraceToPylon(instance, instance->mBrace6C, instance->mPreviewPylon, instance->mF118, 0);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        CorrectBraceNumbering(instance->mBrace6C);
    } else {
        uint32_t two = flags & 2;
        if (two != 0) {
            bool hit = false;
            for (uint32_t i = 0; i < found->mCount; i++) {
                if (two != 0 && found->mBraces[i] == instance->mBrace64) {
                    hit = true;
                    break;
                }
                if (two == 0 && found->mBraces[i] == instance->mLastBrace) {
                    hit = true;
                    break;
                }
            }
            if (hit) {
                instance->mFlags |= 8;
                instance->mPreviewBrace->mFlags &= 0xfffaffffu;
                instance->mBrace64->mFlags |= 0x800000;
                if (valid != 0)
                    instance->mBrace64->mFlags |= 0x800000;
                else
                    instance->mBrace64->mFlags |= 0x804000;
                instance->mBrace64->mFlags &= 0xfff5ffffu;
                FlagCollisionUpdateTrackAroundBrace(instance->mPreviewBrace);
                FlagCollisionUpdateTrackAroundBrace(instance->mBrace64);
                return;
            }
        }
        uint32_t count = found->mCount;
        bool hit = false;
        for (uint32_t i = 0; i < found->mCount; i++) {
            if ((instance->mBlock3C & 2) != 0 && found->mBraces[i] == instance->mPreviewBrace) {
                hit = true;
                break;
            }
            if ((instance->mBlock3C & 2) == 0 && found->mBraces[i] == instance->mBraces) {
                hit = true;
                break;
            }
            count = found->mCount;
        }
        if (hit) return;
        count = found->mCount;
        uint32_t limit = *reinterpret_cast<const uint32_t*>(reinterpret_cast<const char*>(found->mControl) + 0xcc);
        if (count >= limit) return;
        uint32_t input = gCoasterInputFlags & 0x40;
        uint32_t controlFlags = *reinterpret_cast<const uint32_t*>(found->mControl) & 0x4000;
        if (!(input != 0 && controlFlags != 0)) {
            if (input != 0) return;
            if (controlFlags != 0) return;
        }
        LinkBrace(instance->mBrace6C, params, valid);
        LinkPreviewBraceToPylon(instance, instance->mBrace6C, found, instance->mF118, 1);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C);
        FlagCollisionUpdateTrackAroundBrace(instance->mBrace6C->mNext);
        CorrectBraceNumbering(instance->mBrace6C);
    }

    tag_sTrackBrace* brace = instance->mBrace6C;
    if (brace->mData != nullptr) {
        cost = CalculateBraceCost(instance, 1, brace, &BraceX(brace), &BraceZ(brace), nullptr, nullptr);
    } else {
        cost = 0.0f;
    }
    if (gActionRecorder.mRecording == 0) {
        int32_t whole = ppc_fctiwz(static_cast<double>(cost));
        if (whole < 0)
            LLCC_SystemAddToSessionCost(whole - 0xeeeeeee);
        else
            LLCC_SystemAddToSessionCost(whole);
    }
}
