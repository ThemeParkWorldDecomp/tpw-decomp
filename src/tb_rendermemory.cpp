// tb_rendermemory.cpp (engine library of SimTheme Park 2.04b3): CRenderMemory, the pools
// that loaded geometry lives in. The file name is the one in the original's assertions;
// the inventory lists the range (0x100269d4..0x10027354) under tb_resource_display.cpp.
#include "tb_rendermemory.h"

#include <cstring>
#include <new>

#include "Wavelet.h"
#include "platform/msl.h"
#include "tb_error.h"

CRenderMemory* gei_RenderMemory = nullptr;

int32_t RenderMemory_Init() {
    if (gei_RenderMemory != nullptr) return 0;
    gei_RenderMemory = new (std::nothrow) CRenderMemory;
    if (gei_RenderMemory == nullptr) return 0;
    if (!gei_RenderMemory->Init()) {
        delete gei_RenderMemory;
        gei_RenderMemory = nullptr;
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t RenderMemory_Close() {
    CRenderMemory* memory = gei_RenderMemory;
    if (memory == nullptr) return 0;
    memory->Close();
    delete gei_RenderMemory;
    gei_RenderMemory = nullptr;
    return 1;
}

CRenderMemory::CRenderMemory() {
    mLoadFlags = 0;
    mInfos = nullptr;
    mInfoCount = 0;
    mRemembered = 0;
    mScratchBlock = nullptr;
    mScratchNext = nullptr;
    mScratch = nullptr;
    mNextDynamic = 0;
    mNextStatic = 0;
}

// Bit 0 set: the static pool starts again from nothing. Bit 0 clear: the static records
// already handed out lose their first word instead. Bit 1: the dynamic pool starts again.
void CRenderMemory::Reset(int32_t pools) {
    if ((pools & 1) == 0) {
        for (int32_t i = mNextStatic + 1; i < mInfoCount; ++i) std::memset(mInfos[i].mF00, 0, 4);
    } else {
        mPools[0].mUsed = 0;
        mPools[0].mIndexUsed = 0;
        mNextStatic = mInfoCount - 1;
    }
    if ((pools & 2) != 0) {
        mPools[1].mUsed = 0;
        mPools[1].mIndexUsed = 0;
        mNextDynamic = 0;
    }
    mScratchNext = mScratch;
}

// One pool's two blocks; on failure nothing of the pool stays allocated.
static bool InitPool(CRenderMemoryType& pool) {
    pool.mData = static_cast<uint8_t*>(gMalloc(0x100000));
    if (pool.mData == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return false;
    }
    pool.mSize = 0x100000;
    pool.mIndexData = static_cast<uint8_t*>(gMalloc(0x20000));
    if (pool.mIndexData == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        if (pool.mIndexData != nullptr) gFree(reinterpret_cast<void*&>(pool.mIndexData));
        if (pool.mData != nullptr) gFree(reinterpret_cast<void*&>(pool.mData));
        pool.mSize = 0;
        pool.mIndexSize = 0;
        return false;
    }
    pool.mIndexSize = 0x10000;
    pool.mUsed = 0;
    pool.mIndexUsed = 0;
    return true;
}

int32_t CRenderMemory::Init() {
    // The dynamic pool first, as the original does.
    if (!InitPool(mPools[1])) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    if (!InitPool(mPools[0])) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        Close();
        return 0;
    }
    mInfos = static_cast<SRenderLoadInfo*>(gMalloc(kInfoCount * sizeof(SRenderLoadInfo)));
    if (mInfos == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        Close();
        return 0;
    }
    mInfoCount = kInfoCount;
    mScratchBlock = static_cast<uint8_t*>(gMalloc(0x10000f));
    if (mScratchBlock == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        Close();
        return 0;
    }
    uintptr_t aligned = reinterpret_cast<uintptr_t>(mScratchBlock);
    while ((aligned & 0xf) != 0) ++aligned;
    mScratch = reinterpret_cast<uint8_t*>(aligned);
    mScratchEnd = mScratch + 0x100000;
    mPools[0].mUsed = 0;
    mPools[0].mIndexUsed = 0;
    mNextStatic = mInfoCount - 1;
    mPools[1].mUsed = 0;
    mPools[1].mIndexUsed = 0;
    mNextDynamic = 0;
    mScratchNext = mScratch;
    return 1;
}

int32_t CRenderMemory::Close() {
    if (mPools[1].mIndexData != nullptr) gFree(reinterpret_cast<void*&>(mPools[1].mIndexData));
    if (mPools[1].mData != nullptr) gFree(reinterpret_cast<void*&>(mPools[1].mData));
    mPools[1].mSize = 0;
    mPools[1].mIndexSize = 0;
    if (mPools[0].mIndexData != nullptr) gFree(reinterpret_cast<void*&>(mPools[0].mIndexData));
    if (mPools[0].mData != nullptr) gFree(reinterpret_cast<void*&>(mPools[0].mData));
    mPools[0].mSize = 0;
    mPools[0].mIndexSize = 0;
    if (mInfos != nullptr) gFree(reinterpret_cast<void*&>(mInfos));
    if (mScratchBlock != nullptr) {
        gFree(reinterpret_cast<void*&>(mScratchBlock));
        mScratch = nullptr;
    }
    return 1;
}

uint8_t* CRenderMemory::GetVertexBufferPtr(int32_t count) {
    uint8_t* block = mScratchNext;
    // The original compares the 32-bit addresses, so a negative count that wraps below
    // the block also passes; the host compares the same way within its address width.
    uint8_t* end = block + static_cast<intptr_t>(count) * 0x10;
    if (reinterpret_cast<uintptr_t>(mScratchEnd) < reinterpret_cast<uintptr_t>(end)) return nullptr;
    mScratchNext = end;
    return block;
}

SRenderLoadInfo* CRenderMemory::LoadInfo(int32_t flags) {
    const int32_t pool = flags & 3;
    if (pool != kStatic && pool != kDynamic) {
        msl_assertion_failed(
            "((flags&CRENDERMEMORYFLAGS_BITMASK)==CRENDERMEMORYFLAGS_STATIC) || "
            "((flags&CRENDERMEMORYFLAGS_BITMASK)==CRENDERMEMORYFLAGS_DYNAMIC)",
            "tb_rendermemory.cpp", 0x14b);
    }
    const int32_t index = pool == kStatic ? mNextStatic : mNextDynamic;
    if ((flags & kKeepCurrent) == 0) mRemembered = index;
    mLoadFlags = flags;
    mCurrentRenderType = pool;
    mCurrent = &mInfos[index];
    std::memset(mCurrent, 0, sizeof *mCurrent);
    const CRenderMemoryType& memory = mPools[mCurrentRenderType - 1];
    mCurrent->mData = memory.mData + memory.mUsed;
    mCurrent->mIndexData = memory.mIndexData + memory.mIndexUsed;
    return mCurrent;
}

SRenderLoadInfo* CRenderMemory::LoadInfo_CopyVDB(int32_t flags) {
    if (mCurrentRenderType != (flags & 3)) {
        msl_assertion_failed("md_CurrentRenderType==(flags&CRENDERMEMORYFLAGS_BITMASK)", "tb_rendermemory.cpp", 0x16a);
    }
    const int32_t index = (flags & 3) == kStatic ? mNextStatic : mNextDynamic;
    if (mRemembered != index) {
        mCurrent = &mInfos[index];
        std::memcpy(mCurrent, &mInfos[mRemembered], sizeof *mCurrent);
        const CRenderMemoryType& memory = mPools[mCurrentRenderType - 1];
        mCurrent->mIndexData = memory.mIndexData + memory.mIndexUsed;
    }
    return mCurrent;
}
