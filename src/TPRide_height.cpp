// TPRide.cpp (main app of SimTheme Park 2.04b3): the two terrain-height lookups, unit w140: DetermineHFieldHeight (0x1005ea70, 776 bytes)
// and DetermineHFieldHeightQuick (0x1005eda8, 676 bytes).
//
// The height field is gHeightfieldHeader->mHField (include/m3d2.h sHField): the cell sizes mF08[2] (x) and mF08[3] (z), the origin
// mF08[0] and mF08[1], the counts mF18[0] (width) and mF18[1] (depth), (width + 1) * (depth + 1) heights and one sHFieldQuad per cell.
// Both lookups pick the cell, the fraction inside it and the diagonal the cell is cut along, then interpolate the corner heights.
// Every float is a single in the listing's order; the fused multiply-adds are ppc_fmadds (4 in the full lookup, 10 in the quick one).
// The Mac reads the quad's flags at quads + index * 4, which is sHFieldQuad::mF00.
#include <cfloat>
#include <cmath>

#include "TPCamera.h"
#include "TPEngine.h"
#include "m3d2.h"
#include "ppc_math.h"

namespace {
// An unsigned count as the Mac converts it (the magic 2^52 trick): the exact value rounded to a single.
float CountToFloat(uint32_t n) { return static_cast<float>(n); }

// x and z must lie inside the field: 0 <= x < 10 * width and 0 <= z < 10 * depth (the cells are 10 units wide in the field's own scale).
bool InsideField(const sHField* hf, float x, float z) {
    const float zero = 0.0f;
    if (x < zero) return false;
    if (z < zero) return false;
    const float ten = 10.0f;
    if (x >= ten * CountToFloat(hf->mF18[0])) return false;
    if (z >= ten * CountToFloat(hf->mF18[1])) return false;
    return true;
}
}  // namespace

// The terrain height at (x, z): 0.0f outside the field (and where the cell fraction falls outside it); with flag 0 a cell whose flag
// word has bit 1 answers FLT_MAX (the largest float: no ground there). The fraction (x - origin) / cell size picks the cell with the
// unsigned truncation __cvt_fp2unsigned. Bit 4 of the cell's flags is the cell's other diagonal.
float DetermineHFieldHeight(float x, float z, int32_t flag) {
    const sHField* const hf = gHeightfieldHeader->mHField.get();
    if (!InsideField(hf, x, z)) return 0.0f;
    const float width = CountToFloat(hf->mF18[0]);
    const float depth = CountToFloat(hf->mF18[1]);
    const float fx = (x - hf->mF08[0]) / hf->mF08[2];
    if (fx < 0.0f || !(fx <= width)) return 0.0f;
    double whole = 0.0;
    const float fracX = static_cast<float>(std::modf(static_cast<double>(fx), &whole));
    const uint32_t ix = cw_cvt_fp2unsigned(static_cast<double>(fx));
    const float fz = (z - hf->mF08[1]) / hf->mF08[3];
    if (fz < 0.0f || !(fz <= depth)) return 0.0f;
    const float fracZ = static_cast<float>(std::modf(static_cast<double>(fz), &whole));
    const float wholeZ = static_cast<float>(whole);
    const uint32_t iz = cw_cvt_fp2unsigned(static_cast<double>(wholeZ));
    const uint32_t cell = ix + iz * hf->mF18[0];
    const uint16_t quad = hf->mQuads.get()[static_cast<int32_t>(cell)].mF00;
    if (flag == 0 && (quad & 1) != 0) return FLT_MAX;
    const uint32_t row = hf->mF18[0] + 1;
    const int32_t v00 = static_cast<int32_t>(ix + iz * row);
    const int32_t v10 = static_cast<int32_t>(ix + 1 + iz * row);
    const int32_t v01 = static_cast<int32_t>(ix + (iz + 1) * row);
    const int32_t v11 = static_cast<int32_t>(ix + 1 + (iz + 1) * row);
    // slopeFar: the slope is taken along the far edge (v11 - v10), else the near one (v01 - v00); upper: the far corner's form.
    bool slopeFar;
    bool upper;
    if ((quad & 4) != 0) {
        if (fracX >= 1.0f - fracZ) {
            slopeFar = true;
            upper = true;
        } else {
            slopeFar = false;
            upper = false;
        }
    } else if (fracX >= fracZ) {
        slopeFar = true;
        upper = false;
    } else {
        slopeFar = false;
        upper = true;
    }
    const float* const h = hf->mHeights.get();
    float slope;
    if (slopeFar) slope = h[v11] - h[v10];
    else slope = h[v01] - h[v00];
    if (upper) {
        const float base = fracX * h[v11];
        const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
        return ppc_fmadds(1.0f - fracZ, -slope, lower);
    }
    const float base = fracX * h[v10];
    const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
    return ppc_fmadds(fracZ, slope, lower);
}

// The quick lookup: no origin, no flag word test for "no ground", the cell index is the fctiwz truncation (a signed word), and a cell
// without bit 0x800 is flat along its first diagonal whatever its bit 4 says (the full lookup never reads 0x800).
float DetermineHFieldHeightQuick(float x, float z) {
    const sHField* const hf = gHeightfieldHeader->mHField.get();
    if (!InsideField(hf, x, z)) return 0.0f;
    double wholeX = 0.0;
    double wholeZ = 0.0;
    const float fracX = static_cast<float>(std::modf(static_cast<double>(x / hf->mF08[2]), &wholeX));
    const int32_t ix = ppc_fctiwz(wholeX);
    const float fracZ = static_cast<float>(std::modf(static_cast<double>(z / hf->mF08[3]), &wholeZ));
    const int32_t iz = ppc_fctiwz(wholeZ);
    const int32_t width = static_cast<int32_t>(hf->mF18[0]);
    const int32_t cell = static_cast<int32_t>(static_cast<uint32_t>(ix) + static_cast<uint32_t>(iz) * static_cast<uint32_t>(width));
    const uint16_t quad = hf->mQuads.get()[cell].mF00;
    const int32_t v00 = static_cast<int32_t>(static_cast<uint32_t>(ix) + static_cast<uint32_t>(iz) * static_cast<uint32_t>(width + 1));
    const int32_t v10 = v00 + 1;
    const int32_t v01 = v00 + width + 1;
    const int32_t v11 = v00 + width + 2;
    const float* const h = hf->mHeights.get();
    if ((quad & 0x800) == 0) {
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v01] - h[v00], lower);
    }
    if ((quad & 4) != 0) {
        const float limit = 1.0f - fracZ;
        if (fracX >= limit) {
            const float base = fracX * h[v11];
            const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
            return ppc_fmadds(limit, -(h[v11] - h[v10]), lower);
        }
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v01] - h[v00], lower);
    }
    if (fracX >= fracZ) {
        const float base = fracX * h[v10];
        const float lower = ppc_fmadds(1.0f - fracX, h[v00], base);
        return ppc_fmadds(fracZ, h[v11] - h[v10], lower);
    }
    const float base = fracX * h[v11];
    const float lower = ppc_fmadds(1.0f - fracX, h[v01], base);
    return ppc_fmadds(1.0f - fracZ, -(h[v01] - h[v00]), lower);
}
