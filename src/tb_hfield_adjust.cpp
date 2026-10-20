// CHField::AdjustHField (engine 0x1000c0e8, 5,676 bytes; the inventory's tb_hfield.cpp
// range). Init calls it with display flag 0x10000 on the copy of the heights it has just
// made (mF12970): 27 fixed rectangles of grid points, in columns 28..68 and rows 0..16,
// are raised by 4.0, 1.0 or 1.4. The file's own heights (sHField::mHeights) stay as read.
//
// The original inlines one bounds test and add per grid point (no out-of-line copy exists)
// and unrolls the loops in several ways; the order of the rectangles is kept because some
// overlap with different amounts, and float additions in another order round differently.
#include "tb_hfield.h"
#include "m3d2.h"

namespace {

// The amounts (engine constants 0x1004fc50, 0x1004fc4c and 0x1004fc48: 0x40800000,
// 0x3f800000, 0x3fb33333).
constexpr float kRaise4 = 4.0f;
constexpr float kRaise1 = 1.0f;
constexpr float kRaise1_4 = 1.4f;

// Adds delta to the copied height at column x, row z when the point is on the grid
// (0..mF18[0], 0..mF18[1]; the counts compare unsigned). A row holds mF18[0] + 1 heights.
inline void AddHeight(CHField& field, int32_t x, int32_t z, float delta) {
    const sHField& record = *field.mF00;
    if (z < 0 || static_cast<uint32_t>(z) > record.mF18[1]) return;
    if (x < 0 || static_cast<uint32_t>(x) > record.mF18[0]) return;
    float* heights = static_cast<float*>(field.mF12970);
    heights[static_cast<uint32_t>(z) * (record.mF18[0] + 1) + static_cast<uint32_t>(x)] += delta;
}

// AddHeight over columns x0..x1 and rows z0..z1 (inclusive), a row at a time.
void AddHeights(CHField& field, int32_t x0, int32_t x1, int32_t z0, int32_t z1, float delta) {
    for (int32_t z = z0; z <= z1; ++z) {
        for (int32_t x = x0; x <= x1; ++x) AddHeight(field, x, z, delta);
    }
}

}  // namespace

void CHField::AdjustHField() {
    // Columns 28..47 (listing address of each rectangle's loop in the comments).
    AddHeights(*this, 28, 30, 0, 12, kRaise4);     // 0x1000c0e8
    AddHeights(*this, 31, 43, 11, 13, kRaise4);    // 0x1000c1cc
    AddHeights(*this, 44, 46, 11, 16, kRaise4);    // 0x1000c248
    AddHeights(*this, 29, 29, 0, 12, kRaise4);     // 0x1000c32c
    AddHeights(*this, 30, 44, 11, 12, kRaise4);    // 0x1000c398
    AddHeights(*this, 45, 45, 11, 16, kRaise4);    // 0x1000c4f4
    AddHeights(*this, 31, 47, 10, 10, kRaise1);    // 0x1000c600
    AddHeights(*this, 32, 34, 8, 8, kRaise1);      // 0x1000c670
    AddHeights(*this, 31, 46, 9, 9, kRaise1);      // 0x1000c744
    AddHeights(*this, 31, 32, 0, 8, kRaise1);      // 0x1000c85c
    AddHeights(*this, 33, 34, 6, 8, kRaise1);      // 0x1000c904
    AddHeights(*this, 35, 39, 7, 8, kRaise1);      // 0x1000c9ac
    // Columns 48..68.
    AddHeights(*this, 66, 68, 0, 12, kRaise4);     // 0x1000cb08
    AddHeights(*this, 53, 65, 11, 13, kRaise4);    // 0x1000cbec
    AddHeights(*this, 50, 52, 11, 16, kRaise4);    // 0x1000cc68
    AddHeights(*this, 67, 67, 0, 12, kRaise4);     // 0x1000cd4c
    AddHeights(*this, 52, 67, 11, 12, kRaise4);    // 0x1000cdb8
    AddHeights(*this, 51, 51, 11, 16, kRaise4);    // 0x1000cedc
    AddHeights(*this, 49, 65, 10, 10, kRaise1);    // 0x1000cfe8
    AddHeights(*this, 48, 50, 8, 8, kRaise1);      // 0x1000d058
    AddHeights(*this, 49, 65, 9, 9, kRaise1);      // 0x1000d12c
    AddHeights(*this, 63, 65, 0, 8, kRaise1);      // 0x1000d19c
    AddHeights(*this, 63, 65, 6, 8, kRaise1);      // 0x1000d280
    AddHeights(*this, 58, 62, 7, 8, kRaise1);      // 0x1000d364
    // Across both halves, rows 0..6.
    AddHeights(*this, 33, 46, 0, 6, kRaise1_4);    // 0x1000d4c0
    AddHeights(*this, 50, 60, 0, 6, kRaise1_4);    // 0x1000d574
    AddHeights(*this, 46, 49, 0, 4, kRaise1_4);    // 0x1000d5f0
}
