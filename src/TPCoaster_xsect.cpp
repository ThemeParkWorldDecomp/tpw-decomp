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
#include <cstdint>

#include "CoasterBalance.h"
#include "TPCoaster.h"
#include "TPEngine.h"

namespace {

// The balance slot at Mac byte offset `offset`.
inline const BalanceValue& Slot(const CCoasterBalance* balance, uint32_t offset) {
    return balance->mValues[(offset - 8) / 4];
}
inline float SlotFloat(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mFloat; }
inline int32_t SlotInt(const CCoasterBalance* balance, uint32_t offset) { return Slot(balance, offset).mInteger; }

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
