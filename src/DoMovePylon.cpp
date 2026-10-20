// Unit v5026: dragging a pylon of the track editor (TPConfig.cpp on the Mac).
//   StartMovePylon 0x10057738 (604 bytes), DoMovePylon 0x100579cc (1,024 bytes)
#include <cstdint>

#include "ActionRecorder.h"
#include "ThingReceivers.h"
#include "TPCoaster.h"
#include "World.h"
#include "ppc_math.h"

// 0x10057738: picks the brace up. A pylon whose record has bit 2 set cannot be moved (answers false before anything is touched).
bool StartMovePylon(tag_sTrackBrace* brace) {
    if (brace != nullptr && (brace->mData->mFlags & 2) != 0) return false;
    gCoasterWord16BC = 0;
    bool second = false;
    if (brace == nullptr) return second;

    gCoasterInputFlags |= 0x100;
    second = brace->mData->mF30 > 1;
    gPylonMoveBrace = brace;
    gPylonMoveOrigData = const_cast<tag_sPylon*>(brace->mData);
    gPylonMoveHeight = CalculateBraceHeight(brace);
    gActionRecorder.RecordAction(0x3c, 0, 0, static_cast<uint16_t>(brace->mNumber));

    gCoasterEditInstance->mFlags &= 0xffffdfffu;
    if (brace->mNext != nullptr) {
        gPylonMoveMark1 = brace->mNext;
        gExtrudeTextureKind = 3;
    } else {
        gPylonMoveMark1 = brace;
        gExtrudeTextureKind = 3;
    }
    if (brace->mPrev != nullptr)
        SetTrackPreview2ndBrace(brace->mPrev, 0);
    else
        SetTrackPreview2ndBrace(brace, 0);

    int32_t x = static_cast<int32_t>(brace->mData->mCellX);
    int32_t z = static_cast<int32_t>(brace->mData->mCellZ);
    gPylonMoveX = x;
    gPylonMoveZ = z;
    if (x < 0) x = 0;
    if (z < 0) z = 0;
    if (x >= gHeightfieldWidth) x = gHeightfieldWidth - 1;
    if (z >= gHeightfieldHeight) z = gHeightfieldHeight - 1;
    LLCC_StartPylonMove(x, z, brace->mData->mF30 > 1);
    LockPointedAtRide(1);
    SetPotentialPylonArea();
    gCoasterCostBefore = CalculateBraceCost(gCoasterEditInstance, 0, brace, nullptr, nullptr, nullptr, nullptr);
    gPylonSecondCost = CalculateBraceCost(gCoasterEditInstance, 0, brace->mPrev, nullptr, nullptr, nullptr, nullptr);
    return second;
}

// 0x100579cc: drops the dragged brace on the cell (x, z) and pays what the move cost. Answers the brace's word 0x70 when the drop was taken.
uint32_t DoMovePylon(uint32_t x, uint32_t z) {
    gCoasterWord16BC = 0;
    bool flagged = false;
    bool moved = false;
    tag_sTrackBrace* brace;
    if (gCoasterWord16E0 != 2 || (brace = gPylonMoveBrace) == nullptr) {
        LLCC_ResetPylonMove();
        gCoasterInputFlags &= 0xfffffeffu;
        gPylonMoveBrace = nullptr;
        if (gActionRecorder.mRecording != 0) {
            DisableCurrentPylonEditState();
            gCoasterWord16E0 = 0;
        }
        return 0;
    }

    if (gPylonMoveMark2 != nullptr) {
        tag_sTrackBrace* lo;
        tag_sTrackBrace* hi;
        if (gPylonMoveMark1->mNumber > gPylonMoveMark2->mNumber) {
            lo = gPylonMoveMark2;
            hi = gPylonMoveMark1;
        } else {
            lo = gPylonMoveMark1;
            hi = gPylonMoveMark2;
        }
        for (; lo != hi; lo = lo->mPrev) lo->mFlags &= 0xfeffbfffu;
        flagged = true;
    }
    gPylonMoveMark1 = nullptr;
    gPylonMoveMark2 = nullptr;
    gExtrudeTextureKind = 0;
    if (flagged) FlagGenerateTrack(gCoasterEditInstance, 0);

    tag_sPylon* pylon = GetPylonAtCell(x, z, nullptr);
    bool different = false;
    if (pylon != nullptr) {
        const tag_sPylon* data = brace->mData;
        if (pylon->mF18 != data->mF18 || pylon->mF14 != data->mF14) different = true;
    }

    if (different) {
        MoveBrace(gCoasterEditInstance, brace, gPylonMoveOrigData, static_cast<uint32_t>(gPylonMoveX), static_cast<uint32_t>(gPylonMoveZ), nullptr,
                  nullptr, 1, 0);
        LLCC_AbortPylonMove();
        if ((pylon->mFlags & 2) == 0)
            Assign3DObjectToAIWorld(gCoasterEditInstance, pylon->mCellX, pylon->mCellZ, pylon->mOwner->mF70, 0);
    } else {
        float a;
        float b;
        MoveBrace(gCoasterEditInstance, brace, pylon, x, z, &a, &b, 1, 0);
        if (gActionRecorder.mRecording == 0 &&
            (static_cast<uint32_t>(gPylonMoveX) != x || static_cast<uint32_t>(gPylonMoveZ) != z)) {
            int32_t cost;
            if ((gCoasterEditInstance->mBlock3C & 1) != 0) {
                tag_sTrackBrace* b1 = gPylonMoveBrace;
                cost = RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, b1, &a, &b, nullptr, nullptr));
                // The original tests the address of gPylonSecondCost against NULL: always true.
                cost += RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, b1->mPrev, nullptr, nullptr, &a, &b));
                CBankAccount* bank = gGameWorld->GetBankAccount();
                bank->MakeDeposit(cw_cvt_fp2unsigned(gCoasterCostBefore));
                bank->MakeDeposit(cw_cvt_fp2unsigned(gPylonSecondCost));
                bank->GetBalance();
                bank->MakeWithdrawal(static_cast<uint32_t>(cost));
                gCoasterCostBefore = 0.0f;
            } else {
                cost = RoundUp(CalculateBraceCost(gCoasterEditInstance, 0, gPylonMoveBrace, &a, &b, nullptr, nullptr));
                CBankAccount* bank = gGameWorld->GetBankAccount();
                bank->MakeDeposit(cw_cvt_fp2unsigned(gCoasterCostBefore));
                bank->GetBalance();
                bank->MakeWithdrawal(static_cast<uint32_t>(cost));
                gCoasterCostBefore = 0.0f;
            }
        }
        moved = true;
    }

    FlagGenerateTrack(gCoasterEditInstance, 1);
    LockPointedAtRide(0);
    gPylonMoveBrace = nullptr;
    gPylonMoveOrigData = nullptr;
    gCoasterInputFlags &= 0xfffffeffu;
    if (gActionRecorder.mRecording != 0) {
        DisableCurrentPylonEditState();
        gCoasterWord16E0 = 0;
    }
    return moved ? brace->mWord70 : 0;
}
