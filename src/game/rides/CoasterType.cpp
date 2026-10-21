// CoasterType.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "ActionRecorder.h"
#include "CoasterBalance.h"
#include "Debug.h"
#include "LevelInit.h"
#include "LevelSystems.h"
#include "Light.h"
#include "m3d2.h"
#include "Main.h"
#include "MapWho.h"
#include "platform/samsutils.h"
#include "ppc_math.h"
#include "rsse.h"
#include "TPCoaster.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "World.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// TPCoaster.cpp, main app (unit w148): the coaster type's remaining loaders. CreateTrackSectionMesh 0x10036564 (196 bytes),
// SetupTrainType 0x1003a150 (260), SetupCarType 0x10039bc0 (1,376), LoadCoasterPylons 0x1003a9a8 (380), SetupPylonData 0x1003a28c
// (1,748), CreateCrossSectionSelectsFromSamFile 0x1003851c (552), SetCrossSectionSelectDefaults 0x100369d8 (200),
// OptimiseCrossSections 0x100388cc (460), RepackCrossSection 0x100387b4 (208), GetTempFixedRideParams 0x1003d5b0 (44).
//
// Host names: the listing's sCrossSection is sCoasterRecord here, sCSPoint is sCrossSectionPoint; the invented member names are in
// include/TPCoaster.h. Float compares: the original's fcmpo + bge/ble branches are taken on an unordered result too, so every
// select below is a C ternary on `<` or `>` (a NaN falls to the side the comment names). No fused multiply-add and no lfd occur in
// any of the ten listings, so the plain float expressions are exact.

namespace {

inline const BalanceValue& Slot(const CCoasterBalance* balance, uint32_t offset) {
    return balance->mValues[(offset - 8) / 4];
}
inline float SlotFloat(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mFloat; }
inline int32_t SlotInt(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mInteger; }

inline float ReadFloat(const uint8_t* address) {
    float value;
    std::memcpy(&value, address, sizeof value);
    return value;
}

// The index kinds and indexes SetupPylonData walks, in the order of the flag bits 0x20 << i (the image's words at 0x1029ccbc and
// 0x1029ccd0: kind 3 index 0, kind 10 index 0, kind 2, kind 9, kind 10 index 1).
constexpr uint32_t kPylonAnimKinds[5] = {3, 10, 2, 9, 10};
constexpr uint32_t kPylonAnimIndexes[5] = {0, 0, 0, 0, 1};

// The first value of the embedded array's entry `index` of the set's mesh header (the Mac: header + 0x7c, 0x14 bytes each, word +4).
inline uint32_t EmbeddedValue(tag_sRideAnimSet* set, int32_t index) {
    const sEmbeddedData* embedded = set->mHeads->mHeader->mEmbedded.get();
    return embedded[index].mF04;
}

// What SetupPylonData reads after each AnimateRideSet: the float at +0x34 of the block of the embedded record `index` of the
// instance's mesh (the record table of the sEmbeddedSet at the mesh's +0x28), 0.0f without a block.
inline float EmbeddedBlockFloat(tag_sRideAnimSet* instance, int32_t index) {
    const sEmbeddedSet* embedded = static_cast<const sEmbeddedSet*>(instance->mHeads->mF28);
    const uint8_t* block = embedded->mRecords[index].mBlock;
    return block != nullptr ? ReadFloat(block + 0x34) : 0.0f;
}

}  // namespace

// 0x10036564: fills the first entry of the track's master mesh set from the type's counts and a 0..800 box, makes the mesh and, when
// DMCreateMesh answers 0, gives the textures back. The original answers the type's mesh set in r3 (the caller ignores it).
void CreateTrackSectionMesh(tag_sCoasterType* type) {
    sDTextureSet* textures = type->mTextures;
    sDMeshEntry* entry = static_cast<sDMeshEntry*>(type->mF14);
    tag_sDMeshSet* meshSet = type->mMeshSet;
    entry->mFaceCount = static_cast<uint16_t>(type->mF3C);
    entry->mVertexCount = static_cast<uint16_t>(type->mF30);
    entry->mUVCount = static_cast<uint16_t>(type->mF34);
    entry->mFaceSetCount = textures->mCount;
    entry->mPointCount = static_cast<uint16_t>(type->mF38);
    entry->mBox[0] = 0.0f;
    entry->mBox[1] = 0.0f;
    entry->mBox[2] = 0.0f;
    entry->mBox[3] = 800.0f;
    entry->mBox[4] = 800.0f;
    entry->mBox[5] = 800.0f;
    if (DMCreateMesh(meshSet, textures, "TrackMaster", 1) == 0) DMFreeTextures(textures);
    SetMatrixIdentity(entry->mMatrix);
}

// The loaders CreateCoasterFromSamFile builds a coaster type with (main app TPCoaster.cpp, unit w138):
// CreateCoasterFromSamFile 0x10039518 (1,212 bytes), SetCoasterTypeDefaults 0x10036664 (824), SetupPylonControlsFromSamFile
// 0x10038d00 (1,400), SetupCarAndTrainTypesFromSamFile 0x100392cc (500), CreateCoasterTrackMasterDMeshSet 0x10036b20 (480) and
// CreateCrossSectionsFromSamFile 0x10038ae4 (452).
//
// The .sam values are read out of a CCoasterBalance: the Mac's byte offset into it is 8 + 4 * slot (the header is 8 bytes, the slots
// 4), the host's slot is balance->mValues[slot]; Slot() below turns the Mac offset the listing shows into the slot. An "optional"
// float is set when it differs from 0.0f (fcmpu: -0.0f is unset, a NaN is set). Every loop count is unsigned (cmplw).

// The image's initialised data (names invented): 0x1029cdf0 and 0x1029cce4.
sDTextureEntry gCoasterStaticTextureEntries[3] = {
    {{const_cast<char*>("m_erase.tga")}, 7, 0, 0},
    {{const_cast<char*>("red.tga")}, 7, 0, 0},
    {{const_cast<char*>("blue.tga")}, 7, 0, 0},
};
float gCoasterDefaultPairs[10] = {4.0f, 8.0f, 4.0f, 0.0f, 0.0f, -3.0f, -4.0f, -1.0f, -4.0f, 8.0f};

namespace {

inline char* SlotString(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mString; }

// A float from its bit pattern (the listing's constants).
inline float Bits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
// The word's bits as a float / an int (lwz/stw of a float slot is a plain copy).
inline uint32_t Word(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

// fsubs of (0x43300000, word) and 2^52, then the double sqrt and frsp: the root of an unsigned word, as a single.
inline float RootOfWord(uint32_t word) {
    return static_cast<float>(std::sqrt(static_cast<double>(static_cast<float>(word))));
}

// Stores `source` when the .sam float is set.
inline void StoreIfSet(float source, float& destination) {
    if (source != 0.0f) destination = source;
}

// A copy of a string in a block of strlen + 1 bytes (MReleaseAllocF flag 1: zeroed).
inline char* CopyString(const char* text) {
    char* copy = static_cast<char*>(MReleaseAllocF(static_cast<uint32_t>(std::strlen(text)) + 1, 1));
    std::strcpy(copy, text);
    return copy;
}

}  // namespace

// 0x10036664 (824 bytes). The two pylon controls (+0xa8 and +0x1c8) get their defaults; the roots at +0xa4 and +0xa8 of each are
// the double sqrt of the unsigned words at +0x9c and +0xa0 rounded to single (9 and 0x40 for pylon A, 9 and 0x51 for B).
void SetCoasterTypeDefaults(tag_sCoasterType* type) {
    type->mFlags = 0;
    type->mF40 = 0.0f;
    type->mF08 = Bits(0x3fa00000);
    type->mF0C = Bits(0x3fa00000);
    type->mF10 = Bits(0x40800000);
    type->mF24 = Bits(0x3fc00000);
    type->mF28 = Bits(0x3f800000);
    type->mF2C = Bits(0x3f800000);
    type->mF2E8 = 1;
    type->mF2EC = 15000;
    type->mFA4 = Bits(0x41200000);
    type->mF30 = 100;
    type->mF34 = 100;
    type->mF38 = 0;
    type->mF3C = 0x96;
    type->mF70 = Bits(0x3f800000);
    type->mF44 = Bits(0x40a00000);
    type->mF48 = Bits(0x40a00000);
    type->mF4C = Bits(0x42a00000);
    type->mF50 = Bits(0x43020000);
    type->mF54 = Bits(0x3f4ccccd);
    type->mF58 = 0.0f;
    type->mF5C = 0.0f;
    type->mF60 = 0.0f;
    type->mF64 = Bits(0xbe99999a);
    type->mF68 = 0.0f;
    type->mF6C = Bits(0x3e4ccccd);
    type->mF320 = 0.0f;
    type->mF324 = Bits(0x3f800000);
    type->mF328 = 0.0f;
    type->mF32C = Bits(0x3f000000);
    type->mF330 = Bits(0x40400000);
    type->mF334 = Bits(0x3dcccccd);
    type->mF338 = Bits(0x3f800000);
    type->mF33C = Bits(0x3f800000);
    type->mF340 = Bits(0x40a00000);
    type->mF344 = Bits(0x3f800000);
    type->mF348 = Bits(0x40000000);
    type->mF34C = Bits(0x3f000000);
    type->mF350 = Bits(0x3f800000);
    type->mF358 = Bits(0x41a00000);
    type->mF354 = Bits(0x41200000);
    type->mPairs = gCoasterDefaultPairs;
    type->mPairCount = kCoasterDefaultPairCount;
    for (uint32_t i = 0; i < 5; ++i) {
        float* a = &type->mPylonsA.mFD0[4 * i];
        a[0] = 0.0f;
        a[1] = Bits(0x3f800000);
        a[2] = Bits(0x3f800000);
        a[3] = 0.0f;
        float* b = &type->mPylonsB.mFD0[4 * i];
        b[0] = 0.0f;
        b[1] = Bits(0x3f800000);
        b[2] = Bits(0x3f800000);
        b[3] = 0.0f;
    }
    sPylonControl& a = type->mPylonsA;
    a.mFlags = 4;
    a.mFB0 = Bits(0xbf7fbe77);
    a.mFB4 = 1;
    a.mFB8 = 1;
    a.mF58 = Bits(0x40a00000);
    a.mFCC = 1;
    a.mF68 = Bits(0x41200000);
    a.mF6C = Bits(0x41200000);
    a.mAngles[0] = Bits(0x401c61aa);
    a.mAngles[1] = Bits(0x40490fdb);
    a.mAngles[2] = Bits(0x3ff5be0b);
    a.mAngles[3] = Bits(0x40490fdb);
    a.mAngles[4] = Bits(0x3fb2b8c2);
    a.mAngles[5] = Bits(0x40490fdb);
    a.mAngles[6] = Bits(0x3f5f66f3);
    a.mAngles[7] = Bits(0x40490fdb);
    a.mF9C = 9;
    a.mFA0 = 0x40;
    a.mFA4 = RootOfWord(static_cast<uint32_t>(a.mF9C));
    a.mFA8 = RootOfWord(static_cast<uint32_t>(a.mFA0));
    a.mAngles[8] = Bits(0x3f490fdb);
    a.mAngles[9] = 0.0f;
    a.mFAC = 0.0f;
    sPylonControl& b = type->mPylonsB;
    b.mFlags = 0x4004;
    b.mFB0 = Bits(0xbf4ccccd);
    b.mFB4 = 1;
    b.mFB8 = 1;
    b.mF58 = Bits(0x40a00000);
    b.mFCC = 1;
    b.mF68 = Bits(0x41200000);
    b.mF6C = Bits(0x41200000);
    b.mAngles[0] = Bits(0x40060a92);
    b.mAngles[1] = Bits(0x40425c20);
    b.mAngles[2] = Bits(0x40060a92);
    b.mAngles[3] = Bits(0x40425c20);
    b.mAngles[4] = 0.0f;
    b.mAngles[5] = Bits(0x40425c20);
    b.mAngles[6] = 0.0f;
    b.mAngles[7] = Bits(0x40425c20);
    b.mF9C = 9;
    b.mFA0 = 0x51;
    b.mFA4 = RootOfWord(static_cast<uint32_t>(b.mF9C));
    b.mFA8 = RootOfWord(static_cast<uint32_t>(b.mFA0));
    b.mAngles[8] = Bits(0x3fc90fdb);
    b.mAngles[9] = Bits(0x3fc90fdb);
    b.mFAC = Bits(0x41c80000);
}

// 0x10036b20 (480 bytes). The .sam names n textures (count at +0xfc, then 3 slots each from +0xc: name, flag, scroll rate): the
// mesh entry (0x30 bytes), the mesh set (0x40), the texture set (0x20) and its n + gCoasterTextureBase entries (0x10 each) are
// allocated in that order; the n dynamic entries come FIRST, the static entries (gCoasterTextureBase of them) are copied after.
int32_t CreateCoasterTrackMasterDMeshSet(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0xfc));
    if (count == 0) return 0;
    sDMeshEntry* entry = static_cast<sDMeshEntry*>(MReleaseAllocF(sizeof(sDMeshEntry), 1));
    tag_sDMeshSet* meshSet = static_cast<tag_sDMeshSet*>(MReleaseAllocF(sizeof(tag_sDMeshSet), 1));
    sDTextureSet* textureSet = static_cast<sDTextureSet*>(MReleaseAllocF(sizeof(sDTextureSet), 1));
    sDTextureEntry* entries = static_cast<sDTextureEntry*>(
        MReleaseAllocF((count + gCoasterTextureBase) * static_cast<uint32_t>(sizeof(sDTextureEntry)), 1));
    meshSet->mEntries = entry;
    entry->mFlags = 3;
    meshSet->mF00 = 3;
    meshSet->mCount = 1;
    textureSet->mFlags = 0;
    textureSet->mCount = static_cast<uint16_t>(count + gCoasterTextureBase);
    textureSet->mEntries = entries;
    textureSet->mFolder = texturePath;
    sDTextureEntry* cursor = entries;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t row = 12 * i;
        cursor->mFlags = SlotInt(balance, 0x10 + row) != 0 ? 6 : 2;
        const float rate = SlotFloat(balance, 0x14 + row);
        if (rate != 0.0f) {
            cursor->mFlags |= 0x20;
            type->mFlags |= 4;
            cursor->mF0C = Word(rate);
        }
        const char* name = SlotString(balance, 0xc + row);
        cursor->mName = CopyString(name);
        ++cursor;
    }
    for (uint32_t j = 0; j < gCoasterTextureBase; ++j) {
        *cursor = gCoasterStaticTextureEntries[j];
        ++cursor;
    }
    type->mMeshSet = meshSet;
    type->mTextures = textureSet;
    type->mF14 = meshSet->mEntries;
    type->mTextureCount = count;
    return 1;
}

// The coaster cross-section builders (main app TPCoaster.cpp, unit w147): BuildCrossSectionPoints 0x10036d5c (3,308 bytes) and
// BuildCrossSectionEdges 0x10037aa8 (2,580 bytes). CreateCrossSectionsFromSamFile (src/TPCoaster_sam.cpp) calls them.
//
// The .sam holds 20 point rows (Mac balance offset 0x100 + 0x194 * k: 20 points of 0x14 bytes, the row's count at +0x190) and 20 edge
// rows (0x2090 + 0xf4 * k: 20 edges of 0xc bytes, the count at +0xf0). The builders pack every row into one block (the type's mF90 and
// mF94), then give each cross section (count at 0x3530, entries of 0x14 bytes from 0x33a0) the pointer and count of its row.
// The original unrolls the 20 rows; here they are one loop. The Mac offset of a balance word goes through Slot().
//
// Quirk kept: the two small tables (row pointers and row counts) are COMPACTED (an empty row takes no entry) but a section indexes
// them by the .sam's row number, so an empty row before a used one shifts every later index. A row number above the number of rows
// reads past the tables on the Mac (not reproduced as a guarantee: the host reads the same slots of its own tables).

namespace {

constexpr int kRows = 20;
constexpr uint32_t kPointRowCount = 0x290;      // the first point row's count; the rows are 0x194 apart
constexpr uint32_t kPointRowStride = 0x194;
constexpr uint32_t kPointRowData = 0x100;
constexpr uint32_t kEdgeRowCount = 0x2180;      // the first edge row's count; the rows are 0xf4 apart
constexpr uint32_t kEdgeRowStride = 0xf4;
constexpr uint32_t kEdgeRowData = 0x2090;

// The sum of the 20 counts (wraps), and the 1-based index of the last nonzero count (0 when all are zero).
void CountRows(const CCoasterBalance* balance, uint32_t first, uint32_t stride, uint32_t& total, uint32_t& rows) {
    total = 0;
    rows = 0;
    for (int k = 0; k < kRows; ++k) {
        const int32_t count = SlotInt(balance, first + stride * k);
        total += static_cast<uint32_t>(count);
        if (count != 0) rows = k + 1;
    }
}

}  // namespace

// 0x10036d5c. Returns 0 (nothing allocated) without rows or with a total of 0; 0 (the two tables leaked) when a section names row 0
// or its second row has another count than the first; else 1. *out is 0 from the start and 1 once a section has a second row.
int32_t BuildCrossSectionPoints(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance, int32_t* out) {
    *out = 0;
    uint32_t total;
    uint32_t rows;
    CountRows(balance, kPointRowCount, kPointRowStride, total, rows);
    if (rows == 0 || total == 0) return 0;
    sCrossSectionPoint* block = static_cast<sCrossSectionPoint*>(MReleaseAllocF(total * 0x14u, 1));
    sCrossSectionPoint** pointers = static_cast<sCrossSectionPoint**>(MReleaseAllocF(rows * static_cast<uint32_t>(sizeof(void*)), 1));
    uint32_t* counts = static_cast<uint32_t*>(MReleaseAllocF(rows * 4u, 1));
    type->mF90 = block;
    sCrossSectionPoint* write = block;
    uint32_t entry = 0;
    for (int k = 0; k < kRows; ++k) {
        const int32_t count = SlotInt(balance, kPointRowCount + kPointRowStride * k);
        if (count != 0) {
            pointers[entry] = write;
            counts[entry] = static_cast<uint32_t>(count);
            ++entry;
        }
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t at = kPointRowData + kPointRowStride * k + 0x14u * i;
            write->mF00 = SlotFloat(balance, at);
            write->mF04 = SlotFloat(balance, at + 4);
            write->mF08 = SlotFloat(balance, at + 8);
            write->mF0C = SlotFloat(balance, at + 12);
            write->mF10 = SlotFloat(balance, at + 16);
            write->mF04 = write->mF04 + type->mF40;
            ++write;
        }
    }
    const int32_t sections = SlotInt(balance, 0x3530);
    for (int32_t i = 0; i < sections; ++i) {
        const uint32_t entryAt = 0x33a0 + 0x14u * i;
        const uint32_t row = static_cast<uint32_t>(SlotInt(balance, entryAt));
        if (row == 0) return 0;
        sCoasterRecord& record = records[i];
        record.mPoints = pointers[row - 1];
        record.mF00 = counts[row - 1];
        const uint32_t row2 = static_cast<uint32_t>(SlotInt(balance, entryAt + 0xc));
        if (row2 != 0) {
            *out = 1;
            if (record.mF00 != counts[row2 - 1]) return 0;
            record.mPointsB = pointers[row2 - 1];
            record.mF14 = static_cast<uint32_t>(SlotInt(balance, entryAt + 0x10));
        }
    }
    MReleaseFreeF(counts);
    MReleaseFreeF(pointers);
    return 1;
}

// 0x10037aa8. The same shape over the edge rows; each edge keeps the low 16 bits of three balance words (its first word is never
// written). Returns 0 (tables leaked) when a section's edge row (entry +4) is 0, else 1.
int32_t BuildCrossSectionEdges(tag_sCoasterType* type, sCoasterRecord* records, CCoasterBalance* balance) {
    uint32_t total;
    uint32_t rows;
    CountRows(balance, kEdgeRowCount, kEdgeRowStride, total, rows);
    if (rows == 0 || total == 0) return 0;
    sCrossSectionEdge* block = static_cast<sCrossSectionEdge*>(MReleaseAllocF(total * 0xcu, 1));
    sCrossSectionEdge** pointers = static_cast<sCrossSectionEdge**>(MReleaseAllocF(rows * static_cast<uint32_t>(sizeof(void*)), 1));
    uint32_t* counts = static_cast<uint32_t*>(MReleaseAllocF(rows * 4u, 1));
    type->mF94 = block;
    sCrossSectionEdge* write = block;
    uint32_t entry = 0;
    for (int k = 0; k < kRows; ++k) {
        const int32_t count = SlotInt(balance, kEdgeRowCount + kEdgeRowStride * k);
        if (count != 0) {
            pointers[entry] = write;
            counts[entry] = static_cast<uint32_t>(count);
            ++entry;
        }
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t at = kEdgeRowData + kEdgeRowStride * k + 0xcu * i;
            write->mF04 = static_cast<uint16_t>(SlotInt(balance, at));
            write->mF06 = static_cast<uint16_t>(SlotInt(balance, at + 4));
            write->mF08 = static_cast<uint16_t>(SlotInt(balance, at + 8));
            ++write;
        }
    }
    const int32_t sections = SlotInt(balance, 0x3530);
    for (int32_t i = 0; i < sections; ++i) {
        const uint32_t row = static_cast<uint32_t>(SlotInt(balance, 0x33a4 + 0x14u * i));
        if (row == 0) return 0;
        sCoasterRecord& record = records[i];
        record.mBlock = pointers[row - 1];
        record.mPointCount = counts[row - 1];
    }
    MReleaseFreeF(counts);
    MReleaseFreeF(pointers);
    return 1;
}

// 0x10038ae4 (452 bytes). n cross sections (count at +0x3530) in a table of n + 1 records (0x18 bytes each, at +0x88 of the
// type, the count at +0x8c); the builders fill the first n, the extra record at [n] is a copy of the section the .sam names at
// +0x3b00 (1-based, 0 = the first) with its own copy of the point block. The two empty loops of the original are omitted.
int32_t CreateCrossSectionsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3530));
    if (count == 0) return 0;
    sCoasterRecord* records = static_cast<sCoasterRecord*>(
        MReleaseAllocF((count + 1) * static_cast<uint32_t>(sizeof(sCoasterRecord)), 1));
    type->mRecords = records;
    type->mRecordIndex = count;
    int32_t out = 0;
    if (BuildCrossSectionPoints(type, records, balance, &out) == 0) return 0;
    if (BuildCrossSectionEdges(type, records, balance) == 0) return 0;
    const int32_t chosen = SlotInt(balance, 0x3b00);
    const int32_t index = chosen != 0 ? chosen - 1 : 0;
    sCoasterRecord* source = records + index;
    sCoasterRecord* copy = records + count;
    const uint32_t points = source->mPointCount;
    copy->mF00 = source->mF00;
    copy->mPointCount = points;
    const uint32_t size = points * 0xc;
    copy->mPoints = source->mPoints;
    copy->mBlock = source->mBlock;
    copy->mPointsB = source->mPointsB;
    copy->mF14 = source->mF14;
    copy->mBlock = MReleaseAllocF(size, 1);
    if (copy->mBlock != nullptr && source->mBlock != nullptr && points != 0) std::memcpy(copy->mBlock, source->mBlock, size);
    if (CreateCrossSectionSelectsFromSamFile(type, records, count, balance, out) == 0) return 0;
    OptimiseCrossSections(type, records, count);
    return 1;
}

// 0x10038d00 (1,400 bytes). Up to the .sam's pylon count (+0x3a20) of rows (0xbc bytes each from +0x38a8; the first row is
// pylon A at type +0xa8, the others pylon B at +0x1c8). A row without a name, a word at +4 or a float at +0xc returns 0.
int32_t SetupPylonControlsFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3a20));
    for (uint32_t i = 0; i < count; ++i) {
        sPylonControl* pylon = i == 0 ? &type->mPylonsA : &type->mPylonsB;
        const uint32_t row = i * 0xbc;
        char* name = SlotString(balance, 0x38a8 + row);
        if (name == nullptr || SlotInt(balance, 0x38ac + row) == 0 || SlotFloat(balance, 0x38b4 + row) == 0.0f) return 0;
        pylon->mName = CopyString(name);
        pylon->mF0C = static_cast<uint32_t>(SlotInt(balance, 0x38ac + row));
        if (SlotInt(balance, 0x38b0 + row) != 0) pylon->mFlags |= 0x10;
        pylon->mF58 = SlotFloat(balance, 0x38b4 + row);
        StoreIfSet(SlotFloat(balance, 0x38b8 + row), pylon->mFB0);
        if (SlotInt(balance, 0x38bc + row) != 0) pylon->mFB4 = SlotInt(balance, 0x38bc + row);
        if (SlotInt(balance, 0x38c0 + row) != 0) pylon->mFB8 = SlotInt(balance, 0x38c0 + row);
        if (SlotInt(balance, 0x38c4 + row) != 0) pylon->mFCC = SlotInt(balance, 0x38c4 + row);
        StoreIfSet(SlotFloat(balance, 0x38c8 + row), pylon->mF68);
        StoreIfSet(SlotFloat(balance, 0x38cc + row), pylon->mF6C);
        StoreIfSet(SlotFloat(balance, 0x38d0 + row), pylon->mF70);
        for (uint32_t k = 0; k < 10; ++k) {
            const float degrees = SlotFloat(balance, 0x38d4 + 4 * k + row);
            if (degrees != 0.0f) pylon->mAngles[k] = Bits(0x3c8efa35) * degrees;
        }
        const int32_t first = SlotInt(balance, 0x38fc + row);
        if (first != 0) {
            pylon->mF9C = static_cast<int32_t>(static_cast<uint32_t>(first) * static_cast<uint32_t>(first));
            pylon->mFA4 = static_cast<float>(SlotInt(balance, 0x38fc + row));
        }
        const int32_t second = SlotInt(balance, 0x3900 + row);
        if (second != 0) {
            pylon->mFA0 = static_cast<int32_t>(static_cast<uint32_t>(second) * static_cast<uint32_t>(second));
            pylon->mFA8 = static_cast<float>(SlotInt(balance, 0x3900 + row));
        }
        const float tilt = SlotFloat(balance, 0x3904 + row);
        if (tilt != 0.0f) {
            const float scaled = Bits(0x41200000) * tilt;
            pylon->mFAC = Bits(0x3c8efa35) * scaled;
        }
        pylon->mF5C = SlotFloat(balance, 0x3908 + row);
        pylon->mF60 = SlotFloat(balance, 0x390c + row);
        pylon->mF64 = SlotFloat(balance, 0x3910 + row);
        for (uint32_t k = 0; k < 20; ++k) StoreIfSet(SlotFloat(balance, 0x3914 + 4 * k + row), pylon->mFD0[k]);
    }
    return 1;
}

// 0x100392cc (500 bytes). The .sam's car types (count at +0x3a78, rows of 0x1c bytes from +0x3a24) in one array of 0x34-byte
// records (zeroed, so the block at +0x2c stays NULL); a row without a value (+4) or a name (+8) returns 0 and leaves the array
// unreferenced (a leak). The type's three sets (+0x2fc, +0x300, +0x304) are the .sam's indices (+0x3a80 to +0x3a88) into it.
int32_t SetupCarAndTrainTypesFromSamFile(tag_sCoasterType* type, CCoasterBalance* balance) {
    const uint32_t count = static_cast<uint32_t>(SlotInt(balance, 0x3a78));
    if (count == 0) return 0;
    sCarType* cars = static_cast<sCarType*>(MReleaseAllocF(count * static_cast<uint32_t>(sizeof(sCarType)), 1));
    sCarType* car = cars;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t row = i * 0x1c;
        car->mFlags = 0;
        car->mValue = 0;
        car->mName = nullptr;
        car->mF14 = 0.0f;
        car->mF10 = 0.0f;
        car->mF18 = 0.0f;
        car->mF1C = Bits(0x3dcccccd);
        if (SlotInt(balance, 0x3a28 + row) == 0 || SlotString(balance, 0x3a2c + row) == nullptr) return 0;
        if (SlotInt(balance, 0x3a24 + row) != 0) car->mFlags |= 1;
        car->mValue = SlotInt(balance, 0x3a28 + row);
        car->mName = CopyString(SlotString(balance, 0x3a2c + row));
        car->mF10 = SlotFloat(balance, 0x3a30 + row);
        car->mF14 = SlotFloat(balance, 0x3a34 + row);
        car->mF18 = SlotFloat(balance, 0x3a38 + row);
        StoreIfSet(SlotFloat(balance, 0x3a3c + row), car->mF1C);
        ++car;
    }
    type->mCarTypes = cars;
    type->mF2F0 = 0;
    type->mF2F4 = 3;
    type->mCarTypeA = cars + SlotInt(balance, 0x3a80);
    type->mCarTypeB = cars + SlotInt(balance, 0x3a84);
    type->mCarTypeC = cars + SlotInt(balance, 0x3a88);
    if (type->mCarTypeA != type->mCarTypeB) type->mF2F0 |= 1;
    if (type->mCarTypeC != type->mCarTypeB) type->mF2F0 |= 2;
    if (SlotInt(balance, 0x3a7c) != 0) type->mF2F4 = SlotInt(balance, 0x3a7c);
    type->mF30C = SlotFloat(balance, 0x3a8c);
    type->mF310 = SlotFloat(balance, 0x3a90);
    type->mF314 = SlotFloat(balance, 0x3a94);
    return 1;
}

// 0x10039518 (1,212 bytes): builds the type's track, cross sections, pylon controls and car types (each failure shows its
// message and returns 0), then the optional .sam values.
int32_t CreateCoasterFromSamFile(tag_sCoasterType* type, char* texturePath, CCoasterBalance* balance) {
    StoreIfSet(SlotFloat(balance, 0x3b08), type->mF40);
    if (CreateCoasterTrackMasterDMeshSet(type, texturePath, balance) == 0) {
        ERRORBox("Failed to setup track for coaster");
        return 0;
    }
    if (CreateCrossSectionsFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup cross sections for coaster");
        return 0;
    }
    if (SetupPylonControlsFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup pylon controls for coaster");
        return 0;
    }
    if (SetupCarAndTrainTypesFromSamFile(type, balance) == 0) {
        ERRORBox("Failed to setup car and trains for coaster");
        return 0;
    }
    const int32_t pairs = SlotInt(balance, 0x3ac8);
    if (pairs != 0) {
        type->mPairCount = static_cast<uint32_t>(pairs);
        type->mPairs = static_cast<float*>(MReleaseAllocF(type->mPairCount << 3, 1));
        for (uint32_t i = 0; i < type->mPairCount; ++i) {
            type->mPairs[2 * i] = SlotFloat(balance, 0x3a98 + 8 * i);
            type->mPairs[2 * i + 1] = SlotFloat(balance, 0x3a9c + 8 * i);
        }
    }
    if (SlotInt(balance, 0x3ae8) != 0) type->mF2E8 = SlotInt(balance, 0x3ae8);
    if (SlotInt(balance, 0x3aec) != 0) type->mF2EC = SlotInt(balance, 0x3aec);
    StoreIfSet(SlotFloat(balance, 0x3ae4), type->mFA4);
    StoreIfSet(SlotFloat(balance, 0x3acc), type->mF08);
    StoreIfSet(SlotFloat(balance, 0x3ad0), type->mF0C);
    StoreIfSet(SlotFloat(balance, 0x3ad8), type->mF24);
    StoreIfSet(SlotFloat(balance, 0x3adc), type->mF28);
    StoreIfSet(SlotFloat(balance, 0x3ae0), type->mF2C);
    type->mF31C = SlotFloat(balance, 0x3b74);
    StoreIfSet(SlotFloat(balance, 0x3b0c), type->mF44);
    StoreIfSet(SlotFloat(balance, 0x3b10), type->mF48);
    StoreIfSet(SlotFloat(balance, 0x3b14), type->mF4C);
    StoreIfSet(SlotFloat(balance, 0x3b18), type->mF50);
    StoreIfSet(SlotFloat(balance, 0x3b28), type->mF60);
    StoreIfSet(SlotFloat(balance, 0x3b2c), type->mF64);
    StoreIfSet(SlotFloat(balance, 0x3b30), type->mF68);
    StoreIfSet(SlotFloat(balance, 0x3b34), type->mF6C);
    StoreIfSet(SlotFloat(balance, 0x3b38), type->mF320);
    StoreIfSet(SlotFloat(balance, 0x3b3c), type->mF324);
    StoreIfSet(SlotFloat(balance, 0x3b40), type->mF328);
    StoreIfSet(SlotFloat(balance, 0x3b44), type->mF32C);
    StoreIfSet(SlotFloat(balance, 0x3b48), type->mF330);
    StoreIfSet(SlotFloat(balance, 0x3b4c), type->mF334);
    StoreIfSet(SlotFloat(balance, 0x3b50), type->mF338);
    StoreIfSet(SlotFloat(balance, 0x3b54), type->mF33C);
    StoreIfSet(SlotFloat(balance, 0x3b58), type->mF340);
    StoreIfSet(SlotFloat(balance, 0x3b5c), type->mF344);
    StoreIfSet(SlotFloat(balance, 0x3b60), type->mF348);
    StoreIfSet(SlotFloat(balance, 0x3b64), type->mF34C);
    StoreIfSet(SlotFloat(balance, 0x3b68), type->mF350);
    StoreIfSet(SlotFloat(balance, 0x3b6c), type->mF354);
    StoreIfSet(SlotFloat(balance, 0x3b70), type->mF358);
    if (SlotInt(balance, 0x3af0) != 0) {
        type->mPylonsB.mFlags &= 0xffffbfffu;
        type->mPylonsB.mFlags |= 0x8000;
        type->mFlags |= 0x100;
    }
    StoreIfSet(SlotFloat(balance, 0x3b1c), type->mF54);
    StoreIfSet(SlotFloat(balance, 0x3b20), type->mF58);
    StoreIfSet(SlotFloat(balance, 0x3b24), type->mF5C);
    type->mFlags |= 0xa0;
    if (SlotInt(balance, 0x3afc) != 0) type->mFlags |= 0x20;
    if (SlotInt(balance, 0x3af8) == 0) type->mFlags |= 0x10;
    if (SlotInt(balance, 0x3af4) != 0) {
        type->mFlags |= 8;
        type->mFlags &= 0xffffffefu;
    }
    if (SlotInt(balance, 0x3b78) != 0) {
        type->mF30 = SlotInt(balance, 0x3b78);
        type->mF34 = SlotInt(balance, 0x3b78);
    }
    if (SlotInt(balance, 0x3b7c) != 0) type->mF3C = SlotInt(balance, 0x3b7c);
    return 1;
}

// TPCoaster.cpp, main app: LoadCoasterTypeDataFile 0x10039a28 (192 bytes).

namespace {
const char kSharedTextureFolder[] = "Data\\Generic\\Dynamic\\Textures"; // 0x1023cdc5
}

tag_sCoasterType* LoadCoasterTypeDataFile(uint32_t type, char*, char* texturePath, CCoasterBalance& balance) {
    CCoasterBalance local;
    local.Init();
    // The Mac asks for 0x35c bytes; the host's record holds native pointers and is bigger, so it asks for its own size (as
    // PAR_BuildDrawingList sizes its blocks by sizeof(PTCL)). The test maps the one size to the other.
    auto* coaster = static_cast<tag_sCoasterType*>(MReleaseAllocF(sizeof(tag_sCoasterType), 1));
    gCoasterTypes[gCoasterTypeCount] = coaster;
    gCoasterTypeCount = gCoasterTypeCount + 1u;
    coaster->mRideTypeNumber = type;
    SetCoasterTypeDefaults(coaster);
    CreateCoasterFromSamFile(coaster, texturePath, &balance);
    coaster->mTextures->mSharedFolder = kSharedTextureFolder;
    // The return runs ~CCoasterBalance (0x10039b30), which is the UnInit.
    return coaster;
}

// CCoasterBalance: main app balance descriptions at 0x1028ec14.

// 0x10039b30 (100 bytes). Same shape as CChallengeBalance's: when `this` is not NULL the Mac stores the base's vtable word (0x1029cd7c, TOC word
// 0x10260954) at +4, calls CBalanceTable::UnInit and, when the halfword flag it is given is above 0, operator delete(this); it answers `this`. The
// NULL test, the vptr store and the delete are the compiler's; the body is the UnInit.
CCoasterBalance::~CCoasterBalance() { UnInit(); }

// 0x10039bc0: loads the car's mesh as a ride type, takes its box relative to the root node, counts the heads it can receive and, for a
// car with flag 1, widens the boxes of the mesh and of its largest node by the longest corner distance plus 4.
void SetupCarType(sCarType* car, char* folder) {
    car->mRideType = LoadRideType(folder, car->mName, nullptr, nullptr, 0xc0, static_cast<uint32_t>(car->mValue), 1, nullptr,
                                  nullptr, 0, 0);
    if (car->mRideType == 0) return;
    tag_sRideAnimSet* set = gRideTypes[car->mRideType];
    if (set == nullptr) return;
    tag_sM3D2Header* header = set->mHeads->mHeader;
    float box[6];
    const float* headerBox = reinterpret_cast<const float*>(&header->mBox);   // mMin then mMax: six contiguous floats
    for (int i = 0; i < 6; i++) box[i] = headerBox[i];
    // The root node's translation: its matrix (node + 0x10) words +0x30, +0x34, +0x38 (node + 0x40 ...).
    const uint8_t* root = reinterpret_cast<const uint8_t*>(header->mF78.get());
    const float t0 = ReadFloat(root + 0x40);
    const float t1 = ReadFloat(root + 0x44);
    const float t2 = ReadFloat(root + 0x48);
    box[0] = box[0] - t0;
    box[1] = box[1] - t1;
    box[2] = box[2] - t2;
    box[3] = box[3] - t0;
    box[4] = box[4] - t1;
    box[5] = box[5] - t2;
    car->mF28 = (-box[2]) + car->mF14;
    car->mF24 = box[5] + car->mF10;
    car->mHeadCount = CountDataNodesByType(set, 0x80);
    if (car->mHeadCount != 0) {
        int32_t* heads = static_cast<int32_t*>(MReleaseAllocF(car->mHeadCount * 4, 1));
        car->mBlock = heads;
        for (uint32_t i = 0; i < car->mHeadCount; i++) heads[i] = FindNthDataNodeIndex(set, 0x80, i + 1);
    } else {
        car->mBlock = nullptr;
    }
    CTRACE(5, "Car type %s can receive %d heads\n", car->mName, static_cast<int32_t>(car->mHeadCount));
    if ((car->mFlags & 1) == 0) return;
    header = set->mHeads->mHeader;
    // The node with the most vertices (the first of them): the listing also has a branch for the mini nodes after the nodes, which the
    // loop's bound (the node count) never reaches.
    uint32_t best = 0;
    uint32_t bestIndex = 0;
    for (uint32_t i = 0; i < header->mNodeCount; i++) {
        const tag_sNode* node = header->mNodes.get() + i;
        if (node->mVertexCount > best) {
            best = node->mVertexCount;
            bestIndex = i;
        }
    }
    // The same widening for the header's box (six words at +0x80) and the chosen node's box (+0x78): the longer of the two corners'
    // lengths (the single of a double root; the x square is a double product, the other two single products), plus 4.0f, as the
    // limit: the minimum corner is lowered to -limit, the maximum raised to +limit.
    auto widen = [](float* corner) {
        auto length = [](float x, float y, float z) {
            const double xx = static_cast<double>(x) * static_cast<double>(x);   // fmul
            const float yy = y * y;                                              // fmuls
            const float zz = z * z;                                              // fmuls
            const double sum = xx + static_cast<double>(yy);
            const double sum2 = static_cast<double>(zz) + sum;
            return static_cast<float>(std::sqrt(sum2));
        };
        const float r1 = length(corner[0], corner[1], corner[2]);
        const float r2 = length(corner[3], corner[4], corner[5]);
        const float longest = (r1 < r2) ? r2 : r1;   // bge keeps r1 on an unordered compare
        const float limit = 4.0f + longest;
        const float negative = -limit;
        for (int i = 0; i < 3; i++) corner[i] = (negative < corner[i]) ? negative : corner[i];
        for (int i = 3; i < 6; i++) corner[i] = (corner[i] < limit) ? limit : corner[i];
    };
    widen(reinterpret_cast<float*>(&header->mBox));
    tag_sNode* node = header->mNodes.get() + bestIndex;
    widen(reinterpret_cast<float*>(&node->mBox));
    node->mFlags |= 0x200000;
    car->mNodeIndex = bestIndex;
}

// 0x1003a150: the three car types (B first; A and C only when they are not B), the offsets between them and the train's car count.
void SetupTrainType(tag_sCoasterType* type, char* folder) {
    sCarType* carA = type->mCarTypeA;
    sCarType* carB = type->mCarTypeB;
    sCarType* carC = type->mCarTypeC;
    SetupCarType(carB, folder);
    if (carA != carB) SetupCarType(carA, folder);
    if (carC != carB) SetupCarType(carC, folder);
    type->mF30C = carA->mF28 + carB->mF24;
    type->mF310 = carB->mF28 + carB->mF24;
    type->mF314 = carB->mF28 + carC->mF24;
    type->mF2F8 = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(type->mF2F4); i++) {
        if (i == 0) {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carA->mHeadCount);
        } else if (i == static_cast<uint32_t>(type->mF2F4) - 1) {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carC->mHeadCount);
        } else {
            type->mF2F8 = static_cast<int32_t>(static_cast<uint32_t>(type->mF2F8) + carB->mHeadCount);
        }
    }
}

// 0x1003a28c: reads the pylon mesh's embedded entries (the kinds are bit masks of CountDataNodesByType) into the pylon's index words,
// then instances the ride once, runs its animations to time 0 and time 1 and keeps the heights of the entries it found. `type` is
// not used.
void SetupPylonData(tag_sCoasterType* type, sPylonControl* pylon, uint32_t rideType) {
    (void)type;
    tag_sRideAnimSet* set = gRideTypes[rideType];
    if (set == nullptr) return;
    if (IsValidAnimation(set, 2, 0)) pylon->mFlags |= 0x80;
    if (IsValidAnimation(set, 3, 0)) pylon->mFlags |= 0x20;
    if (IsValidAnimation(set, 9, 0)) pylon->mFlags |= 0x100;
    if (IsValidAnimation(set, 10, 0)) pylon->mFlags |= 0x40;
    if (IsValidAnimation(set, 10, 1)) pylon->mFlags |= 0x200;
    // Kind 0x1000000: more than one entry sets flag 8 (two-part pylon), the value 1 entry going to mF48, the others to mF34.
    uint32_t count = CountDataNodesByType(set, 0x1000000);
    if (count != 0) {
        if (count > 1) {
            pylon->mFlags |= 8;
            pylon->mF40 = 2;
            pylon->mF54 = 1;
        } else {
            pylon->mF40 = 1;
        }
        for (uint32_t i = 0; i < count; i++) {
            int32_t index = FindNthDataNodeIndex(set, 0x1000000, i + 1);
            if ((pylon->mFlags & 8) != 0) {
                if (EmbeddedValue(set, index) == 1) {
                    pylon->mF48 = index;
                } else {
                    pylon->mF34 = index;
                }
            } else {
                pylon->mF34 = index;
            }
        }
    }
    if (CountDataNodesByType(set, 0x800000) != 0) {
        int32_t index = FindNthDataNodeIndex(set, 0x800000, 1);
        pylon->mF2C = index;
        if ((pylon->mFlags & 8) != 0) {
            pylon->mF30 = 0;
            pylon->mF44 = 1;
        } else {
            pylon->mF30 = static_cast<int32_t>(EmbeddedValue(set, index)) - 1;
        }
    }
    count = CountDataNodesByType(set, 0x2000000);
    if (count != 0) {
        for (uint32_t i = 0; i < count; i++) {
            int32_t index = FindNthDataNodeIndex(set, 0x2000000, i + 1);
            if ((pylon->mFlags & 8) != 0) {
                if (EmbeddedValue(set, index) == 3) {
                    pylon->mF4C = index;
                } else {
                    pylon->mF38 = index;
                }
            } else {
                pylon->mF38 = index;
            }
        }
    } else {
        pylon->mF3C = 0;
        pylon->mF50 = 0;
    }
    if (CountDataNodesByType(set, 0x100000) != 0) {
        pylon->mF18 = FindNthDataNodeIndex(set, 0x100000, 1);
    } else {
        pylon->mF18 = -1;
    }
    if (CountDataNodesByType(set, 0x80000) != 0) {
        for (uint32_t i = 0; i < 4; i++) {   // always four tries, whatever the count
            int32_t index = FindNthDataNodeIndex(set, 0x80000, i + 1);
            if (index == -1) continue;
            switch (static_cast<int32_t>(EmbeddedValue(set, index))) {
                case 1: pylon->mF1C = index; pylon->mFlags |= 0x400; break;
                case 2: pylon->mF20 = index; pylon->mFlags |= 0x800; break;
                case 3: pylon->mF24 = index; pylon->mFlags |= 0x1000; break;
                case 4: pylon->mF28 = index; pylon->mFlags |= 0x2000; break;
                default: break;
            }
        }
    }
    if ((pylon->mFlags & 8) != 0) {
        pylon->mF3C = 1;
        pylon->mF50 = 3;
    } else {
        pylon->mF3C = static_cast<int32_t>(CountDataNodesByType(set, 0x400000));
    }
    sFixedRideParams* params = GetTempFixedRideParams(pylon->mFB4, pylon->mFB8, 0, 0);
    uint32_t instance = InstanceRide(rideType, 0x3fa, params, 0, 0, nullptr, 0, 0, 0xffffffff, 0);
    tag_sRideAnimSet* instanceSet = gRideAnimSets[instance];
    sAnimTimeControl* control = static_cast<sAnimTimeControl*>(instanceSet->mTimeControls);
    for (uint32_t i = 0; i < 5; i++) {
        if ((pylon->mFlags & (0x20u << i)) != 0) {
            uint32_t kind = kPylonAnimKinds[i];
            if ((instanceSet->mGroups[kind].mFlags & 3) != 0) {
                SetNewAnim(instanceSet, kind, kPylonAnimIndexes[i], control, 0, 1.0f);
            }
        }
        control++;   // 0x38 bytes
    }
    instanceSet->mFlags |= 0x1000;
    // Time 0 and time 1 of the pylon's range: k * mFD0[2] + mFD0[3] + 0, clamped to mFD0[0] .. mFD0[1], times the control's length.
    auto animate = [&](float k) {
        float f2 = k * pylon->mFD0[2];
        f2 = f2 + pylon->mFD0[3];
        f2 = f2 + 0.0f;
        if (f2 < pylon->mFD0[0]) f2 = pylon->mFD0[0];
        if (f2 > pylon->mFD0[1]) f2 = pylon->mFD0[1];
        sAnimTimeControl* first = static_cast<sAnimTimeControl*>(gRideAnimSets[instance]->mTimeControls);
        first->mF20 = f2 * first->mF1C;
        AnimateRideSet(gRideAnimSets[instance], 1, 0);
    };
    animate(0.0f);
    pylon->mFBC = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF2C);
    pylon->mF68 = (pylon->mF68 < pylon->mFBC) ? pylon->mFBC : pylon->mF68;
    if (pylon->mF18 != -1) {
        pylon->mFC4 = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF18);
        pylon->mFC4 = pylon->mFC4 - pylon->mFBC;
    } else {
        pylon->mFC4 = 0.0f;
    }
    animate(1.0f);
    pylon->mFC0 = EmbeddedBlockFloat(gRideAnimSets[instance], pylon->mF2C);
    DestroyRideInstance(instance);
}

// 0x1003a9a8: the two pylon types' ride types (the .sam names), their set-up and the type's flags 1 (first) and 2 (second); the
// flags are set whether or not the mesh loaded.
void LoadCoasterPylons(tag_sCoasterType* type, sPylonControl* first, sPylonControl* second, char* folder) {
    sPylonControl* pylons[2] = {first, second};
    for (int k = 0; k < 2; k++) {
        sPylonControl* pylon = pylons[k];
        if (pylon->mName != nullptr) {
            int32_t size[2] = {pylon->mFB4, pylon->mFB8};
            pylon->mRideType = LoadRideType(folder, pylon->mName, size, nullptr, 0x1013405, pylon->mF0C, 5, nullptr, nullptr, 0, 0);
            if (pylon->mRideType != 0) SetupPylonData(type, pylon, pylon->mRideType);
            pylon->mFlags |= 1;
            type->mFlags |= (k == 0) ? 1 : 2;
        } else {
            pylon->mRideType = 0;
        }
    }
}

sFixedRideParams* GetTempFixedRideParams(int32_t a, int32_t b, int32_t x, int32_t z);

// 0x100369d8: `count` selects (zeroed block, MReleaseAllocF flag 1), each with +-FLT_MAX limits and no first record.
void SetCrossSectionSelectDefaults(tag_sCoasterType* type, uint32_t count, tag_sCrossSectionSelect** out, sCoasterRecord* a,
                                   sCoasterRecord* b, int32_t flag) {
    tag_sCrossSectionSelect* block =
        static_cast<tag_sCrossSectionSelect*>(MReleaseAllocF(count * static_cast<uint32_t>(sizeof(tag_sCrossSectionSelect)), 1));
    if (flag != 0) type->mSelectFlags |= 1;
    type->mSelectCount = count;
    type->mF78 = block;
    type->mSelectA = a;
    type->mSelectB = b;
    uint32_t lowBits = 0xff7fffff;    // 0x10264728: -FLT_MAX
    uint32_t highBits = 0x7f7fffff;   // 0x10264724: FLT_MAX
    float low, high;
    std::memcpy(&low, &lowBits, sizeof low);
    std::memcpy(&high, &highBits, sizeof high);
    for (uint32_t i = 0; i < count; i++) {
        tag_sCrossSectionSelect* select = block + i;
        select->mF00[0] = low;
        select->mF00[1] = high;
        select->mF00[2] = low;
        select->mF00[3] = high;
        select->mF00[4] = low;
        select->mF00[5] = high;
        select->mF00[6] = low;
        select->mF00[7] = high;
        select->mF20 = nullptr;
    }
    if (out != nullptr) *out = block;
}

// 0x1003851c: the selects of the .sam (count at balance 0x38a4, four pairs of degrees per select at 0x3534 + 0x2c * i, three section
// numbers per select at 0x3554, 0x3558, 0x355c). A section number (1 up) above `count` ends the walk with 0; the first of the three
// must also be nonzero. The flag word of the type (+0) gets 0x40 when the .sam names a record (balance 0x3b04).
int32_t CreateCrossSectionSelectsFromSamFile(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count,
                                             CCoasterBalance* balance, int32_t flag) {
    sCoasterRecord* named = nullptr;
    uint32_t number = static_cast<uint32_t>(SlotInt(balance, 0x3b04));
    if (static_cast<int32_t>(number) != 0) {
        type->mFlags |= 0x40;
        named = records + (number - 1);
    }
    uint32_t selectCount = static_cast<uint32_t>(SlotInt(balance, 0x38a4));
    if (selectCount == 0) return 0;
    tag_sCrossSectionSelect* select = nullptr;
    SetCrossSectionSelectDefaults(type, selectCount, &select, records + count, named, flag);
    const float radians = 0.017453292f;   // 0x10264720
    for (uint32_t i = 0; i < selectCount; i++, select++) {
        for (int pair = 0; pair < 4; pair++) {
            float f1 = SlotFloat(balance, 0x3534 + 0x2c * i + 8 * pair);
            float f2 = SlotFloat(balance, 0x3538 + 0x2c * i + 8 * pair);
            if (!(0.0f == f1 && 0.0f == f2)) {
                select->mF00[2 * pair] = radians * f1;
                select->mF00[2 * pair + 1] = radians * f2;
            }
        }
        uint32_t first = static_cast<uint32_t>(SlotInt(balance, 0x3554 + 0x2c * i));
        if (first == 0 || first > count) return 0;
        select->mF20 = records + (first - 1);
        uint32_t second = static_cast<uint32_t>(SlotInt(balance, 0x3558 + 0x2c * i));
        if (second != 0) {
            if (second > count) return 0;
            select->mF24 = records + (second - 1);
        }
        uint32_t third = static_cast<uint32_t>(SlotInt(balance, 0x355c + 0x2c * i));
        if (third != 0) {
            if (third > count) return 0;
            select->mF28 = records + (third - 1);
        }
    }
    return 1;
}

// 0x100387b4: moves the points of `record` whose `used` entry is nonzero down to `dst` (in order), writes each moved point's new
// number into `used`, and (once, flag 1 of the first edge's word) renumbers the edges' two point halfwords.
void RepackCrossSection(uint32_t* used, sCoasterRecord* record, sCrossSectionPoint* dst) {
    sCrossSectionPoint* source = record->mPoints;
    uint32_t next = 0;
    for (uint32_t i = 0; i < record->mF00; i++) {
        if (used[i] != 0) {
            if (dst != source) std::memmove(dst, source, sizeof(sCrossSectionPoint));
            used[i] = next;
            dst++;
            next++;
        }
        source++;
    }
    sCrossSectionEdge* edge = static_cast<sCrossSectionEdge*>(record->mBlock);
    if ((edge->mF00 & 1) != 0) return;
    for (uint32_t j = 0; j < record->mPointCount; j++, edge++) {
        edge->mF06 = static_cast<uint16_t>(used[edge->mF06]);
        edge->mF08 = static_cast<uint16_t>(used[edge->mF08]);
        edge->mF00 |= 1;
    }
}

// 0x100388cc: drops the points no edge uses from each section (two passes: the first counts, the second moves what is left into one
// block at the type's mF98). A section whose points are shared with another section, or that has a second point row, stays as it is
// (but pass 1 still repacks a section in place when no other section shares its points and it has unused points).
// The original's `used` is a 21-word local; here it is a persistent block of 0x10000 words (an edge or section of more than 21 points
// overflows the Mac's stack buffer).
void OptimiseCrossSections(tag_sCoasterType* type, sCoasterRecord* records, uint32_t count) {
    static std::vector<uint32_t> sUsed(0x10000, 0);
    uint32_t total = 0;
    sCrossSectionPoint* out = nullptr;
    for (uint32_t pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            type->mF98 = MReleaseAllocF(total * 0x14, 1);
            out = static_cast<sCrossSectionPoint*>(type->mF98);
        }
        sCoasterRecord* record = records;
        for (uint32_t s = 0; s < count; s++, record++) {
            if (sUsed.size() < record->mF00) sUsed.resize(record->mF00, 0);
            uint32_t* used = sUsed.data();
            for (uint32_t i = 0; i < record->mF00; i++) used[i] = 0;
            sCrossSectionEdge* edge = static_cast<sCrossSectionEdge*>(record->mBlock);
            for (uint32_t j = 0; j < record->mPointCount; j++, edge++) {
                used[edge->mF06] = 1;
                used[edge->mF08] = 1;
            }
            uint32_t n = 0;
            for (uint32_t i = 0; i < record->mF00; i++) n += used[i];
            if (n < record->mF00 && record->mPointsB == nullptr) {
                bool shared = false;
                for (uint32_t t = 0; t < count; t++) {
                    if (s != t && record->mPoints == records[t].mPoints) {
                        shared = true;
                        break;
                    }
                }
                if (!shared) {
                    RepackCrossSection(used, record, record->mPoints);
                } else if (pass == 0) {
                    total += n;
                } else {
                    RepackCrossSection(used, record, out);
                    record->mPoints = out;
                    record->mF00 = n;
                    out += n;
                }
            }
        }
        if (total == 0) {
            type->mF98 = nullptr;
            break;
        }
    }
}

// TPCoaster.cpp, main app: LoadCoasterType 0x1003ab7c (232 bytes).

void LoadCoasterType(uint32_t type, char* folder, CCoasterBalance& balance) {
    char texturePath[0x400];
    std::sprintf(texturePath, "%s\\GTexture", folder);
    auto* coaster = LoadCoasterTypeDataFile(type, folder, texturePath, balance);
    gRideTypes[type]->mFlags |= 0x40000;
    if (static_cast<int32_t>(gExtrudeFaceCountA) < coaster->mF3C) {
        gExtrudeFaceCountA = static_cast<uint32_t>(coaster->mF3C);
    }
    if (gExtrudeFaceCountB < coaster->mTextures->mCount) gExtrudeFaceCountB = coaster->mTextures->mCount;
    CreateTrackSectionMesh(coaster);
    SetupTrainType(coaster, folder);
    LoadCoasterPylons(coaster, &coaster->mPylonsA, &coaster->mPylonsB, folder);
    coaster->mF318 = static_cast<int32_t>(static_cast<uint32_t>(coaster->mF2F8) * static_cast<uint32_t>(coaster->mF2E8));
}

// Coaster instances (TPCoaster.cpp on the Mac, unit w186): SetCurrentEditCoaster 0x1003aca0, the pylon interval three 0x10040e10, 0x10040e64 and
// 0x10040eb8, FlagGenerateTrack 0x10040f08, FlagCollisionUpdateTrackAroundBrace 0x10040fa4, XZAngleBetweenPylons 0x10041018,
// GetPotentialPylonPositionArea 0x10041170, DestroyEndPylons 0x10042728, EndTrack 0x100427d0, SetCoasterState 0x10042bb8, IsCoasterType 0x100432dc,
// IsCoaster 0x10043330, FindCoasterInstanceFromEntranceID 0x10043380, SetCoasterCObject 0x10043480, InstanceCoaster 0x100434ec,
// SetCoasterScriptID 0x1004373c and ui_GetNumCoasterTracks 0x100458ac. The globals' places are in include/TPCoaster.h (names invented); every
// callee outside the unit is untranslated.

// 0x1003aca0 (12 bytes).
void SetCurrentEditCoaster(tag_sCoasterInstance* instance) { gCoasterEditInstance = instance; }

// The park's second-stage systems that LevelInit2 and LevelDeInit call (main application, unit w111): the attribute map, the poly
// search table, the heightmap system's words, the level lights' teardown, the highlight colour buffers, the coaster types and the
// data error handler. Each function keeps the original's order of calls and stores.

uint32_t gCoasterTypeCount;
tag_sCoasterType* gCoasterTypes[kMaxCoasterTypes];
uint32_t gCoasterTextureBase = 3;     // initialised data
// 0x1003acf0 (472 bytes, TPCoaster.cpp). Per type: the track sets' blocks (the one at +0x300 first, the others when they are
// not that set; +0x2fc and +0x304 may be the same set, whose block is then freed twice, the second time as NULL), +0x308, the
// texture set's first (count - gCoasterTextureBase) entries (the dynamic ones: CreateCoasterFromSamFile, src/TPCoaster_sam.cpp,
// puts the gCoasterTextureBase static entries LAST, so theirs are never freed; unit w138), the textures and the mesh set, then
// the texture set's entry array and itself, +0x18, +0x14, the record block (not cleared), +0x90 to +0x98, the record table and
// +0x78. The table entry is cleared last. The count of types is read again at every step and cleared at the end.
void DestroyCoasterTypes() {
    for (uint32_t index = 0; index < gCoasterTypeCount; ++index) {
        tag_sCoasterType* type = gCoasterTypes[index];
        sCarType* setB = type->mCarTypeB;
        sCarType* setA = type->mCarTypeA;
        sCarType* setC = type->mCarTypeC;
        MReleaseFreeF(setB->mBlock);
        setB->mBlock = nullptr;
        if (setA != setB) {
            MReleaseFreeF(setA->mBlock);
            setA->mBlock = nullptr;
        }
        if (setC != setB) {
            MReleaseFreeF(setC->mBlock);
            setC->mBlock = nullptr;
        }
        MReleaseFreeF(type->mCarTypes);
        type->mCarTypes = nullptr;
        const uint32_t base = gCoasterTextureBase;
        const uint32_t count = static_cast<uint32_t>(type->mTextures->mCount) - base;
        sDTextureEntry* entry = type->mTextures->mEntries;
        for (uint32_t i = 0; i < count; ++i) {
            MReleaseFreeF(entry->mBlock);
            entry->mBlock = nullptr;
            ++entry;
        }
        DMFreeTextures(type->mTextures);
        DMFreeMesh(type->mMeshSet);
        MReleaseFreeF(type->mTextures->mEntries);
        type->mTextures->mEntries = nullptr;
        MReleaseFreeF(type->mTextures);
        type->mTextures = nullptr;
        MReleaseFreeF(type->mMeshSet);
        type->mMeshSet = nullptr;
        MReleaseFreeF(type->mF14);
        type->mF14 = nullptr;
        MReleaseFreeF(type->mRecords[type->mRecordIndex].mBlock);
        MReleaseFreeF(type->mF90);
        MReleaseFreeF(type->mF94);
        MReleaseFreeF(type->mF98);
        type->mF90 = nullptr;
        type->mF94 = nullptr;
        type->mF98 = nullptr;
        MReleaseFreeF(type->mRecords);
        type->mRecords = nullptr;
        MReleaseFreeF(type->mF78);
        type->mF78 = nullptr;
        gCoasterTypes[index] = nullptr;
    }
    gCoasterTypeCount = 0;
}
