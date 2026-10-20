// Unit v5190: the editor's pylon placement (TPCoaster.cpp on the Mac): SetPotentialPylonArea 0x10041a58, PlacePylon 0x10041c5c.
#include <cmath>
#include <cstdint>

#include "ActionRecorder.h"
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "World.h"
#include "m3d2.h"

// 0x10041a58 (472 bytes): the square of cells around the pylon the editor works on, as far as its control's reach (the root of the
// control's word 0xa0, truncated, plus 1), cut to the height field. The pylon is the dragged brace's, the preview brace's or the first
// brace of the instance without flag 0x2000. Bug kept: the second answer is cut with the x start (gHeightfieldXStart), not the y start.
void SetPotentialPylonArea() {
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    tag_sTrackBrace* brace;
    if (instance->mBlock3C & 2) {
        if (gCoasterInputFlags & 0x100) {
            brace = gPylonMoveBrace->mNext;
            if (brace->mFlags & 0x10000) brace = gPylonMoveBrace->mPrev;
        } else {
            brace = instance->mPreviewBrace;
        }
    } else if (gCoasterInputFlags & 0x100) {
        brace = gPylonMoveBrace->mNext;
    } else {
        brace = instance->mBraces;
        while (brace != nullptr) {
            if ((brace->mFlags & 0x2000) == 0) break;
            brace = brace->mNext;
        }
    }
    const int32_t x = static_cast<int32_t>(brace->mData->mCellX);
    const int32_t z = static_cast<int32_t>(brace->mData->mCellZ);
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    const sPylonControl* control = (gCoasterInputFlags & 0x40) ? &type->mPylonsB : &type->mPylonsA;
    const float reach = static_cast<float>(static_cast<uint32_t>(control->mFA0));
    const int32_t radius = static_cast<int32_t>(static_cast<float>(std::sqrt(static_cast<double>(reach)))) + 1;
    const int32_t xStart = gHeightfieldXStart;
    int32_t a = x - radius;
    int32_t b = z - radius;
    int32_t c = x + radius;
    int32_t d = z + radius;
    if (a < xStart) a = xStart;
    if (b < xStart) b = xStart;
    if (c >= xStart + gHeightfieldWidth) c = xStart + gHeightfieldWidth - 1;
    if (d >= gHeightfieldYStart + gHeightfieldHeight) d = gHeightfieldYStart + gHeightfieldHeight - 1;
    gCoasterWord689C = static_cast<uint32_t>(a);
    gCoasterWord6898 = static_cast<uint32_t>(b);
    gCoasterWord6894 = static_cast<uint32_t>(c);
    gCoasterWord6890 = static_cast<uint32_t>(d);
}

// 0x10041c5c (988 bytes). A pylon of the cell is looked for in every instance's list (flags 5 skipped); a missing one is made. With the
// editor in state bit 2 and the pylon holding the instance's brace +0x64 the track is rejoined and the call ends with 1. An existing pylon whose
// last brace is the last of the track ends with 0 (flag 0). Otherwise a brace is made, the cost paid (flag 0, no recording) and the loop counter run down.
int32_t PlacePylon(uint32_t x, uint32_t z, int32_t flag, tag_sPylon** outPylon, tag_sTrackBrace** outBrace, uint8_t level, uint32_t extra) {
    uint32_t brace_level = level;
    float loft = 0.0f;
    uint32_t linkLevel = 0;
    tag_sCoasterInstance* instance = gCoasterEditInstance;
    const tag_sCoasterType* type = reinterpret_cast<const tag_sCoasterType*>(instance->mType);
    sPylonControl* control = (gCoasterInputFlags & 0x40) ? const_cast<sPylonControl*>(&type->mPylonsB) : const_cast<sPylonControl*>(&type->mPylonsA);
    const uint32_t index = x + z * gHeightfieldHeader->mHField->mF18[0];

    sPylonRecord* pylon = nullptr;
    for (tag_sCoasterInstance* other = gCoasterInstances; other != nullptr; other = other->mNext) {
        sPylonRecord* node = reinterpret_cast<sPylonRecord*>(other->mPylons);
        while (node != nullptr) {
            if ((node->mFlags & 5) == 0 && node->mIndex == index) break;
            node = node->mNext;
        }
        if (node != nullptr) {
            pylon = node;
            break;
        }
    }

    if (pylon == nullptr) {
        pylon = NewPylon(x, z, 0, flag, control, nullptr);
    } else {
        if (instance->mBlock3C & 2) {
            tag_sTrackBrace* wanted = instance->mBrace64;
            for (uint32_t i = 0; i < pylon->mCount; ++i) {
                if (pylon->mBraces[i] == wanted) {
                    RejoinTrack(instance);
                    if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
                    if (outBrace != nullptr) *outBrace = wanted;
                    SetCoasterEditState(0x10);
                    CoasterPanelEndTrack();
                    return 1;
                }
            }
        }
        if (flag == 0 && pylon->mBraces[pylon->mCount - 1]->mNumber == instance->mBraceCount - 1) {
            if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
            return 0;
        }
    }

    if (flag == 0) {
        GetNewBraceLoftInterval(reinterpret_cast<sLinkPylon*>(pylon), control, instance->mPreviewBrace, instance->mBraceB5C,
                                gCoasterWord1790 == 2, 0, &loft);
        CTRACE(5, "Determining link level for new pylon\n");
        DetermineLinkLevel(loft, reinterpret_cast<sLinkPylon*>(pylon), &linkLevel, &loft, 0);
    }
    if (level == 0xff) brace_level = linkLevel & 0xff;

    tag_sTrackBrace* brace =
        NewTrackBrace(pylon, pylon->mX, pylon->mZ, loft, flag != 0, control, 2 - gCoasterWord1790, brace_level & 0xff, extra);
    SetupBraceAnimFrameData(reinterpret_cast<sCoasterType*>(const_cast<uint32_t*>(gCoasterEditInstance->mType)), brace);

    if (flag == 0) {
        tag_sCoasterInstance* edit = gCoasterEditInstance;
        RecalcTrackParams(edit);
        edit->mFlags |= 1;
        edit->mFlags |= 0x1000;
        float cellX = 5.0f + pylon->mX;
        float cellZ = 5.0f + pylon->mZ;
        const float cost = CalculateBraceCost(gCoasterEditInstance, 1, brace, &cellX, &cellZ, nullptr, nullptr);
        CBankAccount* account = gGameWorld->GetBankAccount();
        account->GetBalance();
        if (gActionRecorder.mRecording == 0) account->MakeWithdrawal(static_cast<uint32_t>(FloatToCell(static_cast<double>(cost))));
    }

    if (gCoasterInputFlags & 0x40) {
        gCoasterWord1790 = gCoasterWord1790 - 1;
        if (gCoasterWord1790 == 0) {
            gCoasterInputFlags &= ~0x40u;
            if (flag == 0 && gActionRecorder.mRecording == 0) UIHandleEndLoop();
        } else if (gCoasterWord1790 == 1 && flag == 0 && gActionRecorder.mRecording == 0) {
            UIHandleMidLoop();
        }
    }

    SetPotentialPylonArea();
    if (outPylon != nullptr) *outPylon = reinterpret_cast<tag_sPylon*>(pylon);
    if (outBrace != nullptr) *outBrace = brace;
    return 1;
}
