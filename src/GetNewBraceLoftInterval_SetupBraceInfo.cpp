#include <cstdint>
#include <cstring>
#include "Debug.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "ppc_math.h"

namespace {
// One entry of a brace info block: 0x58 bytes on the Mac, the brace pointer at 0x10 (native on the host, the entry size is the same).
struct sBraceEntry {
    uint32_t mF00;                    // 0x00
    uint32_t mIndex;                  // 0x04  the data node index
    uint8_t mF08[8];                  // 0x08
    tag_sTrackBrace* mBrace;          // 0x10
    uint8_t mF18[0x58 - 0x18];        // 0x18
};
static_assert(sizeof(sBraceEntry) == 0x58, "entry size");
}  // namespace

// 0x1003df2c (212 bytes)
void SetupBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info, sBraceControl* control, sPylonControl* pylon) {
    info->mCount = control->mCount;
    info->mBlockA = MReleaseAllocF(info->mCount * 0x58, 1);
    info->mBlockB = MReleaseAllocF(info->mCount * 0x58, 1);
    sBraceEntry* a = static_cast<sBraceEntry*>(info->mBlockA);
    sBraceEntry* b = static_cast<sBraceEntry*>(info->mBlockB);
    tag_sRideAnimSet* set = gRideTypes[pylon->mRideType];
    for (uint32_t i = 0; i < info->mCount; i++) {
        a->mIndex = FindDataNodeIndexByID(set, 0x400000, i + control->mBase);
        b->mIndex = a->mIndex;
        a->mBrace = brace;
        a++;
        b->mBrace = brace;
        b++;
    }
}

// 0x1003e064 (92 bytes)
void DestroyBraceInfo(tag_sTrackBrace* brace, sBraceInfo* info) {
    if ((brace->mData->mFlags & 1) == 0) MReleaseFreeF(info->mBlockA);
    MReleaseFreeF(info->mBlockB);
    info->mBlockA = nullptr;
    info->mBlockB = nullptr;
}

// 0x1003e344 (256 bytes)
void LinkPylon(sPylonRecord* pylon, sCoasterTrackParams* params, uint32_t x, uint32_t z, uint32_t index) {
    pylon->mFlags |= 8;
    pylon->mIndex = index;
    pylon->mCellX = x;
    pylon->mCellZ = z;
    float fx = static_cast<float>(x);
    float fz = static_cast<float>(z);
    pylon->mX = 10.0f * fx;
    pylon->mZ = 10.0f * fz;
    pylon->mY = GetRideRequiredYOffset(static_cast<int32_t>(x), static_cast<int32_t>(z), nullptr, 1, 1, 0, 0);
    pylon->mPrev = params->mPylonTail;
    pylon->mNext = params->mPylonTail->mNext;
    if (pylon->mPrev != nullptr) pylon->mPrev->mNext = pylon;
    if (pylon->mNext != nullptr) pylon->mNext->mPrev = pylon;
    params->mPylonTail = pylon;
    pylon->mNumber = params->mPylonCount;
    params->mPylonCount = params->mPylonCount + 1;
}

// 0x1003e48c (236 bytes)
void LinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params, int32_t valid) {
    if (valid != 0) {
        brace->mFlags &= ~0x4000u;
    } else {
        brace->mFlags |= 0x4000;
    }
    brace->mFlags |= 0x8000;
    brace->mNext = params->mBraceTail;
    brace->mPrev = params->mBraceTail->mPrev;
    if (brace->mNext != nullptr) brace->mNext->mPrev = brace;
    if (brace->mPrev != nullptr) brace->mPrev->mNext = brace;
    tag_sTrackBrace* tail = params->mBraceTail;
    if (params->mBraceHead == tail) {
        params->mBraceHead = brace;
    } else {
        uint32_t f = tail->mFlags;
        if ((f & 0x200) != 0) {
            tail->mFlags = f & ~0x10000u;
        } else {
            tail->mFlags = f & 0xfffaffffu;
        }
        brace->mFlags |= 0x50000;
    }
    params->mBraceTail = brace;
    params->mBraceCount = params->mBraceCount + 1;
    CorrectBraceNumbering(params->mBraceTail);
}

// 0x1003e5c0 (212 bytes)
void UnLinkBrace(tag_sTrackBrace* brace, sCoasterTrackParams* params) {
    uint32_t f = brace->mFlags;
    if ((f & 0x8000) == 0) return;
    if (params->mBraceHead == params->mBraceTail) {
        params->mBraceHead = brace->mNext;
    } else {
        brace->mFlags = f & 0xfffaffffu;
        brace->mNext->mFlags |= 0x50000;
    }
    params->mBraceTail = brace->mNext;
    if (brace->mNext != nullptr) brace->mNext->mPrev = brace->mPrev;
    if (brace->mPrev != nullptr) brace->mPrev->mNext = brace->mNext;
    brace->mNext = nullptr;
    brace->mPrev = nullptr;
    brace->mNumber = 0;
    params->mBraceCount = params->mBraceCount - 1;
    CorrectBraceNumbering(params->mBraceTail);
    brace->mFlags &= ~0x8000u;
}
