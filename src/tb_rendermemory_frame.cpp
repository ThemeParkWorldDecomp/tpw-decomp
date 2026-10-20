// tb_rendermemory_frame.cpp (engine library of SimTheme Park 2.04b3): the small members of
// CRenderMemory that the primitive buffer calls each frame. The range is tb_rendermemory.cpp
// by its own assertions (the inventory files it under tb_resource_display.cpp); the
// constructor, Reset, Init, Close and the load records are in src/tb_rendermemory.cpp.
//
//   0x10027460 CRenderMemory::SaveInfo              (120 bytes)
//   0x1002750c CRenderMemory::GetVertexEndPosition  (36)
//   0x1002756c CRenderMemory::GetIndexEndPosition   (36)
#include "tb_rendermemory.h"

#include <cstdint>

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
