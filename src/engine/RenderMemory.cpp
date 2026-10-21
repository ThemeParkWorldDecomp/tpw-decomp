// RenderMemory.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "platform/msl.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

// tb_rendermemory.cpp (engine library of SimTheme Park 2.04b3): CRenderMemory, the pools
// that loaded geometry lives in. The file name is the one in the original's assertions;
// the inventory lists the range (0x100269d4..0x10027354) under tb_resource_display.cpp.

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

// One pool's two blocks (1 MB of vertices, 128 KB for indices of which 64 KB count, times kPoolScale: 1 as on
// the Mac, see tb_rendermemory.h); on failure nothing of the pool stays allocated.
static bool InitPool(CRenderMemoryType& pool) {
    pool.mData = static_cast<uint8_t*>(gMalloc(0x100000 * CRenderMemory::kPoolScale));
    if (pool.mData == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return false;
    }
    pool.mSize = 0x100000 * CRenderMemory::kPoolScale;
    pool.mIndexData = static_cast<uint8_t*>(gMalloc(0x20000 * CRenderMemory::kPoolScale));
    if (pool.mIndexData == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        if (pool.mIndexData != nullptr) gFree(reinterpret_cast<void*&>(pool.mIndexData));
        if (pool.mData != nullptr) gFree(reinterpret_cast<void*&>(pool.mData));
        pool.mSize = 0;
        pool.mIndexSize = 0;
        return false;
    }
    pool.mIndexSize = 0x10000 * CRenderMemory::kPoolScale;
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

// tb_rendermemory_frame.cpp (engine library of SimTheme Park 2.04b3): the small members of
// CRenderMemory that the primitive buffer calls each frame. The range is tb_rendermemory.cpp
// by its own assertions (the inventory files it under tb_resource_display.cpp); the
// constructor, Reset, Init, Close and the load records are in src/tb_rendermemory.cpp.
//
//   0x10027460 CRenderMemory::SaveInfo              (120 bytes)
//   0x1002750c CRenderMemory::GetVertexEndPosition  (36)
//   0x1002756c CRenderMemory::GetIndexEndPosition   (36)

// Closes the record LoadInfo started: the pool it draws from now counts everything up to the
// two end pointers as used (byte counts, the index end too), and the record number moves on,
// unless LoadInfo was told to keep the remembered record. The pool is chosen by
// mCurrentRenderType (1 static, 2 dynamic); the static records count down, the dynamic up.
void CRenderMemory::SaveInfo(char* vertexEnd, uint16_t* indexEnd) {
    if ((mLoadFlags & kKeepCurrent) != 0) return;
    CRenderMemoryType& pool = mPools[mCurrentRenderType - 1];
    pool.mUsed = static_cast<int32_t>(reinterpret_cast<uintptr_t>(vertexEnd) - reinterpret_cast<uintptr_t>(pool.mData));
    pool.mIndexUsed =
        static_cast<int32_t>(reinterpret_cast<uintptr_t>(indexEnd) - reinterpret_cast<uintptr_t>(pool.mIndexData));
    if (mCurrentRenderType == kStatic) {
        --mNextStatic;
    } else {
        ++mNextDynamic;
    }
}

// The end of the current pool's vertex memory: its start plus its size field.
uint8_t* CRenderMemory::GetVertexEndPosition() {
    const CRenderMemoryType& pool = mPools[mCurrentRenderType - 1];
    return pool.mData + pool.mSize;
}

// The end of the current pool's index memory: its start plus the index size field counted in
// BYTES (0x10000 of the 0x20000 bytes allocated), as a pointer to 16-bit indices.
uint16_t* CRenderMemory::GetIndexEndPosition() {
    const CRenderMemoryType& pool = mPools[mCurrentRenderType - 1];
    return reinterpret_cast<uint16_t*>(pool.mIndexData + pool.mIndexSize);
}

// tb_rendermemory_execute.cpp (engine library of SimTheme Park 2.04b3; the render memory,
// whose asserts name tb_rendermemory.cpp and which the inventory files under
// tb_resource_display.cpp): the deferred drawing of the render memory.
//
//   0x10027a8c CRenderMemory::ExecuteUnrenderedPrimitives      (784 bytes)
//   0x100275cc CRenderMemory::qSortPrimitivesAscending_Texture (332)
//   0x10027764 CRenderMemory::qSortPrimitivesAscending_ZPos    (332)
//   0x100278f8 CRenderMemory::qSortPrimitivesDescending_ZPos   (332)
//
// CRenderList::Render calls ExecuteUnrenderedPrimitives after RenderByTexture. The primitives
// the frame made (CDrawPrimitive::ExecuteCurrentPrimitive leaves the ones that cannot be drawn
// at once in the table) are drawn here, sorted for the renderer: with the OpenGL renderer back
// to front by depth, or in the order the display flags ask for.

namespace {

// The record table holds the primitives (the render memory's records are the same objects).
static_assert(sizeof(SPrimitive) == sizeof(SRenderLoadInfo), "a record is a primitive");
SPrimitive* Primitive(CRenderMemory& memory, int32_t number) { return reinterpret_cast<SPrimitive*>(&memory.mInfos[number]); }

// The renderer that sorts only by texture and the display flags that ask for the nearest first.
constexpr int32_t kRendererSortedByTexture = 4;
constexpr uint32_t kDisplayDepthMask = 0x0000000c;
constexpr uint32_t kDisplayNearestFirst = 0x20000000;

// The three sorts are one: the middle element (the 32-bit sum shifted right arithmetically) is
// the pivot, the key of which is read once; the first scan runs up while the element is before it
// and stops at `high`, the second runs down while the pivot is before the element and stops at
// `low`; elements out of place are swapped and the two parts are sorted in turn, the lower first.
// A NaN key (the depth sorts) stops both scans.
template <class Key, class KeyOf, class Before>
void SortRange(int32_t* list, int32_t low, int32_t high, KeyOf keyOf, Before before) {
    const int32_t middle = static_cast<int32_t>(static_cast<uint32_t>(low) + static_cast<uint32_t>(high)) >> 1;
    const Key pivot = keyOf(list[middle]);
    int32_t first = low;
    int32_t last = high;
    do {
        while (first <= high && before(keyOf(list[first]), pivot)) ++first;
        while (last > low && before(pivot, keyOf(list[last]))) --last;
        if (last < first) break;
        std::swap(list[first], list[last]);
        --last;
        ++first;
    } while (first <= last);
    if (low < last) SortRange<Key>(list, low, last, keyOf, before);
    if (first < high) SortRange<Key>(list, first, high, keyOf, before);
}

}  // namespace

// Sorted by texture, the lowest address first (unsigned).
void CRenderMemory::qSortPrimitivesAscending_Texture(int32_t* list, int32_t low, int32_t high) {
    SortRange<uintptr_t>(
        list, low, high, [this](int32_t number) { return reinterpret_cast<uintptr_t>(Primitive(*this, number)->mTexture); },
        [](uintptr_t a, uintptr_t b) { return a < b; });
}

// Sorted by depth, the smallest first.
void CRenderMemory::qSortPrimitivesAscending_ZPos(int32_t* list, int32_t low, int32_t high) {
    SortRange<float>(
        list, low, high, [this](int32_t number) { return Primitive(*this, number)->mDepth; },
        [](float a, float b) { return a < b; });
}

// Sorted by depth, the largest first.
void CRenderMemory::qSortPrimitivesDescending_ZPos(int32_t* list, int32_t low, int32_t high) {
    SortRange<float>(
        list, low, high, [this](int32_t number) { return Primitive(*this, number)->mDepth; },
        [](float a, float b) { return b < a; });
}

int32_t CRenderMemory::ExecuteUnrenderedPrimitives() {
    // The records in use: the static ones are the numbers above mNextStatic, the dynamic ones
    // those below mNextDynamic.
    const int32_t records = (mInfoCount - mNextStatic) + mNextDynamic - 1;
    tpw_probe::Hit(tpw_probe::kExecuteUnrendered);
    if (records == 0) return 1;
    void* block = gMalloc(static_cast<uint32_t>(records) * 4);
    if (block == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    int32_t* list = static_cast<int32_t*>(block);
    int32_t used = 0;
    for (int32_t number = 0; number < mNextDynamic; ++number) {
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) == 0) list[used++] = number;
    }
    // A static record that is drawn at once by its blending mode is done with here.
    for (int32_t number = mNextStatic + 1; number < mInfoCount; ++number) {
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) != 0) continue;
        gei_DrawPrimitive->ExecutePrimitiveIfInstant(Primitive(*this, number));
        if ((Primitive(*this, number)->mFlags & SPrimitive::kDrawn) == 0) list[used++] = number;
    }

    if (gei_System.mRenderer == kRendererSortedByTexture) {
        // The records with blending flag 0x1000 first (the scans meet in the middle), sorted by
        // texture; the others sorted by depth, farthest first.
        int32_t first = 0;
        int32_t last = used - 1;
        while (first < last) {
            while (first < used && (Primitive(*this, list[first])->mBlendingMode & SPrimitive::kSortSkipped) != 0) ++first;
            while (last > 0 && (Primitive(*this, list[last])->mBlendingMode & SPrimitive::kSortSkipped) == 0) --last;
            if (first <= last) {
                std::swap(list[first], list[last]);
                ++first;
                --last;
            }
        }
        if (first > 1) qSortPrimitivesAscending_Texture(list, 0, first - 1);
        if (used - 1 > first) qSortPrimitivesDescending_ZPos(list, first, used - 1);
    } else if (used > 1) {
        const uint32_t flags = gei_Display.mFlags;
        if ((flags & kDisplayDepthMask) == kDisplayDepthMask && (flags & kDisplayNearestFirst) != 0) {
            qSortPrimitivesAscending_ZPos(list, 0, used - 1);
        } else {
            qSortPrimitivesDescending_ZPos(list, 0, used - 1);
        }
    }

    tpw_probe::Hit(tpw_probe::kRecordsListed, static_cast<uint32_t>(used));
    for (int32_t k = 0; k < used; ++k) {
        if (gei_DrawPrimitive->ExternExecute(Primitive(*this, list[k])) == 0) {
            gFree(block);
            return 0;
        }
    }
    gFree(block);
    return 1;
}
