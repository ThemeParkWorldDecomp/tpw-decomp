// The height field's node gamuts (main application, the rsse.cpp range of the inventory):
// for every node of the terrain mesh, the rectangle of height field cells its box covers,
// kept in four signed bytes of the node (mF90: Z high, Z low, X high, X low) so that the
// map can tell which nodes to draw. SetupHFieldNodeGamuts 0x100b971c starts at the mesh's
// root node with the identity matrix; DoCalcNodeHFieldGamut 0x100b9304 does one node and
// its children.
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"
#include "rsse.h"
#include <algorithm>

// 0x1029eb78..0x1029eb84 (names invented): the limits the four indices are held to.
int32_t gNodeGamutMinX = -128;   // 0x1029eb78
int32_t gNodeGamutMaxX = 127;    // 0x1029eb7c
int32_t gNodeGamutMinZ = -128;   // 0x1029eb80
int32_t gNodeGamutMaxZ = 127;    // 0x1029eb84

namespace {
constexpr uint32_t kNodeNoBox = 0x200;            // the node has no box of its own (children only)
constexpr uint32_t kNodeSkipped = 0x40000000;     // the node and its children are left out
constexpr uint32_t kNodeGamutSet = 0x2000;
constexpr uint32_t kNodeOutsideField = 0x4000000;

// fctiwz of a single-precision quotient, as the original's (int) cast of a float.
int32_t Cell(float quotient) {
    return ppc_fctiwz(static_cast<double>(quotient));
}

// The first cell: the quotient truncated toward zero, less one when the POSITION (not the
// quotient) is below zero, as the original tests it. With an origin other than zero the
// two tests differ, and that is kept.
int32_t LowCell(float position, float origin, float size) {
    const float quotient = (position - origin) / size;
    const int32_t cell = Cell(quotient);
    return position < 0.0f ? static_cast<int32_t>(static_cast<uint32_t>(cell) - 1u) : cell;
}

// The last cell: 0.999 is added to the quotient before it is truncated.
int32_t HighCell(float position, float origin, float size) {
    const float kAlmostOne = 0.999f;   // 0x10264df8
    const float quotient = (position - origin) / size;
    return Cell(kAlmostOne + quotient);
}
}  // namespace

// The node's matrix goes into `matrix` (the destination of CombineMatricesNoWDest2nd, the
// parameter's own copy), the node's box corners go through it, and each axis is put in
// order. The cells are limited by the four globals and stored as bytes (truncated, so a
// value outside -128..127 wraps) unless the node has flag 0x200. Flag 0x4000000 is set
// before the limits apply, when the unlimited box reaches below cell 0 or past the last
// cell. The children, except those with flag 0x40000000, follow with the combined matrix.
void DoCalcNodeHFieldGamut(tag_sNode* node, sHField* field, sMatrix matrix) {
    CombineMatricesNoWDest2nd(&node->mMatrix, &matrix);
    if ((node->mFlags & kNodeNoBox) == 0) {
        s3DPoint low = {node->mBox.mMin[0], node->mBox.mMin[1], node->mBox.mMin[2]};
        s3DPoint high = {node->mBox.mMax[0], node->mBox.mMax[1], node->mBox.mMax[2]};
        VectorThroughLMatrixNoW(&matrix, &low);
        VectorThroughLMatrixNoW(&matrix, &high);
        // fcmpo and ble: a NaN on either side leaves the pair as it is. The y pair is ordered too,
        // though nothing reads it afterwards.
        if (low.x > high.x) std::swap(low.x, high.x);
        if (low.y > high.y) std::swap(low.y, high.y);
        if (low.z > high.z) std::swap(low.z, high.z);
        // sHField: mF08 is the origin (x, z) then the cell size (x, z); mF18 the cell counts.
        int32_t firstX = LowCell(low.x, field->mF08[0], field->mF08[2]);
        int32_t firstZ = LowCell(low.z, field->mF08[1], field->mF08[3]);
        int32_t lastX = HighCell(high.x, field->mF08[0], field->mF08[2]);
        int32_t lastZ = HighCell(high.z, field->mF08[1], field->mF08[3]);
        if (firstX < 0 || firstZ < 0 || lastX >= static_cast<int32_t>(field->mF18[0]) ||
            lastZ >= static_cast<int32_t>(field->mF18[1]))
            node->mFlags |= kNodeOutsideField;
        firstX = std::max(gNodeGamutMinX, firstX);
        lastX = std::min(lastX, gNodeGamutMaxX);
        firstZ = std::max(gNodeGamutMinZ, firstZ);
        lastZ = std::min(lastZ, gNodeGamutMaxZ);
        node->mFlags |= kNodeGamutSet;
        node->mF90[3] = static_cast<uint8_t>(firstX);
        node->mF90[2] = static_cast<uint8_t>(lastX);
        node->mF90[1] = static_cast<uint8_t>(firstZ);
        node->mF90[0] = static_cast<uint8_t>(lastZ);
    }
    // mF0C is the first child and mF08 the next sibling of the same parent.
    for (tag_sNode* child = node->mF0C; child != nullptr; child = child->mF08)
        if ((child->mFlags & kNodeSkipped) == 0) DoCalcNodeHFieldGamut(child, field, matrix);
}

void SetupHFieldNodeGamuts(tag_sM3D2Header* header) {
    sMatrix identity;
    SetMatrixIdentity(&identity);
    DoCalcNodeHFieldGamut(header->mF78, header->mHField, identity);
}
