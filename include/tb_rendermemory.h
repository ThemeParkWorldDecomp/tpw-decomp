// tb_rendermemory.h: the engine's geometry memory (engine library; the original file is
// tb_rendermemory.cpp by its own assertions, 0x100269d4..0x10027354; the inventory files
// the range under tb_resource_display.cpp).
//
// Two pools, each a block of vertex data and a block of index data that only grow until
// Reset: pool 1 (CRENDERMEMORYFLAGS_STATIC in the assertion text) and pool 2 (DYNAMIC).
// A "load info" record describes one loaded mesh; static records are handed out from the
// end of the record table downwards and dynamic ones from the start.
//
// Mac offsets in the comments; the pointers make the host objects larger.
#pragma once
#include <cstdint>

// One pool: 0x18 bytes on the Mac (CRenderMemoryType::CRenderMemoryType, 0x10026be4).
struct CRenderMemoryType {
    CRenderMemoryType() : mData(nullptr), mUsed(0), mSize(0), mIndexData(nullptr), mIndexUsed(0), mIndexSize(0) {}

    uint8_t* mData;        // 0x00  0x100000 bytes
    int32_t mUsed;         // 0x04
    int32_t mSize;         // 0x08
    uint8_t* mIndexData;   // 0x0c  0x20000 bytes
    int32_t mIndexUsed;    // 0x10
    int32_t mIndexSize;    // 0x14  0x10000
};

// One mesh's record: 0x38 bytes on the Mac. Only the two pointers LoadInfo fills are
// known so far.
struct SRenderLoadInfo {
    uint8_t mF00[0x28];
    uint8_t* mData;        // 0x28  where the pool's vertex data stood when the record was made
    uint8_t* mIndexData;   // 0x2c  the same for the index data
    uint8_t mF30[8];
    uint8_t mHostTail[0x10];   // host only: the two pointers SPrimitive keeps after the Mac's
                               // fields (mTexture1 and mTexture2, Mac 0x0c and 0x10)
};

class CRenderMemory {
public:
    enum : int32_t {
        kStatic = 1,        // the pool, in the low two bits of LoadInfo's flags
        kDynamic = 2,
        kKeepCurrent = 4,   // LoadInfo: do not remember this record as the one to copy from
        kInfoCount = 0x1000,
    };

    CRenderMemory();                             // 0x10026b48
    // Empties the pools named by bit 0 (static) and bit 1 (dynamic).
    void Reset(int32_t pools);                   // 0x10026c34
    int32_t Init();                              // 0x10026cdc
    int32_t Close();                             // 0x1002707c
    // `count` 16-byte vertices of scratch space, or NULL when it is used up.
    uint8_t* GetVertexBufferPtr(int32_t count);  // 0x100271a8
    // Draws what is in the pools and has not been drawn yet (src/tb_rendermemory_execute.cpp);
    // 0 on failure. The records not yet drawn (flag 0x10) are listed by number, the static ones
    // after CDrawPrimitive::ExecutePrimitiveIfInstant has had its go at them, the list is
    // sorted (renderer 4: the records whose blending mode has 0x1000 first, by texture, then the
    // others from the farthest; otherwise by depth, nearest first when the display flags have
    // 0xc and 0x20000000, else farthest first) and CDrawPrimitive::ExternExecute draws them in
    // that order. 0 when no memory for the list (error 3, 0xb0754000) or an execution fails.
    int32_t ExecuteUnrenderedPrimitives();       // 0x10027a8c
    // The sorts of ExecuteUnrenderedPrimitives: they order `list`, record numbers in mInfos,
    // from `low` to `high` inclusive, by the texture (+8, unsigned) or by the depth (+0x30)
    // ascending or descending; Hoare's scheme with the middle element as the pivot, not stable.
    void qSortPrimitivesAscending_Texture(int32_t* list, int32_t low, int32_t high);   // 0x100275cc
    void qSortPrimitivesAscending_ZPos(int32_t* list, int32_t low, int32_t high);      // 0x10027764
    void qSortPrimitivesDescending_ZPos(int32_t* list, int32_t low, int32_t high);     // 0x100278f8
    // The next record of the pool in `flags`, cleared and pointing at the pool's free space.
    SRenderLoadInfo* LoadInfo(int32_t flags);    // 0x1002720c
    // The same record as a copy of the remembered one, with fresh index space.
    SRenderLoadInfo* LoadInfo_CopyVDB(int32_t flags);   // 0x10027354
    // Counts what the record just made used: the current pool's used bytes become `vertexEnd`
    // and `indexEnd` less the pool starts, and the record is counted (unless kKeepCurrent).
    void SaveInfo(char* vertexEnd, uint16_t* indexEnd);   // 0x10027460
    // The end of the current pool's vertex memory (data + size) and index memory.
    uint8_t* GetVertexEndPosition();             // 0x1002750c
    uint16_t* GetIndexEndPosition();             // 0x1002756c

    int32_t mLoadFlags;              // 0x00  LoadInfo's argument
    int32_t mCurrentRenderType;     // 0x04  its low two bits (md_CurrentRenderType in the assertion text)
    CRenderMemoryType mPools[2];     // 0x08  static, dynamic
    SRenderLoadInfo* mCurrent;       // 0x38
    uint8_t* mScratchEnd;            // 0x3c  mScratch + 0x100000
    int32_t mF40;                    // 0x40  not written by the functions translated so far
    SRenderLoadInfo* mInfos;         // 0x44  kInfoCount records
    int32_t mInfoCount;              // 0x48
    int32_t mRemembered;             // 0x4c  the record LoadInfo last handed out without kKeepCurrent
    int32_t mNextDynamic;            // 0x50  counts up from 0
    int32_t mNextStatic;             // 0x54  counts down from mInfoCount - 1
    uint8_t* mScratch;               // 0x58  mScratchBlock rounded up to 16 bytes
    uint8_t* mScratchNext;           // 0x5c
    uint8_t* mScratchBlock;          // 0x60  0x10000f bytes
};

extern CRenderMemory* gei_RenderMemory;   // engine data 0x10165630 (not exported; name invented)

int32_t RenderMemory_Init();    // 0x100269d4
int32_t RenderMemory_Close();   // 0x10026ac0
