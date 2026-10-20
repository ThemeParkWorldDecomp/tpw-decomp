// tb_hfield_gamut.cpp (engine library of SimTheme Park 2.04b3, the part of the inventory's
// tb_hfield.cpp range that the assertion strings call tb_gamut.cpp): CGamut::CalculateGamut
// 0x10009f04 (6,168 bytes) and CGamut::GetHFieldIntersection 0x10009d60 (356 bytes).
//
// CalculateGamut finds the cells and grid points of the height field that a first-person view
// can see. The view volume's four corner rays (CGamut::mCorners, set by CGamut::Init) are cut
// with the height field's top and bottom (mYClipLow and mYClipHigh) by GetHFieldIntersection;
// the four points and the camera make the polygon of the footprint on the ground (CPolygon
// keeps it convex; the point where a ray meets the ground is clamped to half the view
// distance). The polygon is clipped to the map's z and then x extent (GenerateClipZVector and
// GenerateClipXVector), turned into cell units and walked edge by edge: an edge that rises in z
// lowers the row's low column (mLow) and one that falls raises its high column (mHigh); the
// rows of grid points are the rows of cells widened by the next row. At the end the projection
// matrix's far plane is set to the farthest footprint point.
//
// Quirks kept: the rays and the polygon work in single precision with the fused multiply-adds
// of the helpers; the byte columns are compared and stored through (int8_t) while the new
// value is a 32-bit int (a column outside -128..127 is stored truncated and compared again
// the next time); a row outside 0..127 would write outside the rows (the Mac's bytes before
// and after them: mFirstRow, mEndRow and, past the end, the other rows); the end row of the
// grid points keeps the old value when it is above the map's z count (mF54[1]); the first
// part of the function writes the rows' initial values twice, the second time over the first.
#include <cmath>
#include <cstring>

#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_polygon.h"
#include "tb_resource_display.h"

namespace {

// Constants (engine data 0x1004fbf0..0x1004fc0c).
constexpr float kRayStep = 0.001f;                   // 0x3a83126f  replaces a ray with no y (0x1004fbf8)
constexpr float kAlmostOne = 0.99999898672103881835937500f;   // 0x3f7fffef  rounds a row up (0x1004fbf0)
constexpr float kFarthestStart = 10.0f;              // 0x41200000  (0x1004fbf4)

using ClipFunction = _D3DVECTOR (*)(_D3DVECTOR, _D3DVECTOR, float);

// The clip of one coordinate (axis 0 is x, axis 2 is z) of the polygon `in` to the slab
// low..high that CalculateGamut runs twice. Each edge goes from the previous vertex to the
// current one; the code is 3 * (where the current vertex lies) + (where the previous one lies),
// a place being 0 below the low plane, 1 between and 2 above the high one (a compare that is
// false for a NaN counts as not above). Returns the number of vertices written to `out`.
int32_t ClipToSlab(const float (*in)[3], int32_t count, float (*out)[3], int32_t axis, float low, float high,
                   ClipFunction clip) {
    int32_t written = 0;
    auto emit = [&](float x, float y, float z) {
        out[written][0] = x;
        out[written][1] = y;
        out[written][2] = z;
        ++written;
    };
    auto place = [&](float coordinate) { return (low < coordinate ? 1 : 0) + (high < coordinate ? 1 : 0); };
    int32_t previous = count - 1;
    for (int32_t i = 0; i < count; ++i) {
        const float* current = in[i];
        const float* before = in[previous];
        const _D3DVECTOR a = {current[0], current[1], current[2]};
        const _D3DVECTOR b = {before[0], before[1], before[2]};
        switch (place(current[axis]) * 3 + place(before[axis])) {
        case 1: {
            emit(b.x, b.y, b.z);
            const _D3DVECTOR cut = clip(a, b, low);
            emit(cut.x, cut.y, cut.z);
            break;
        }
        case 2: {
            const _D3DVECTOR cutHigh = clip(a, b, high);
            emit(cutHigh.x, cutHigh.y, cutHigh.z);
            const _D3DVECTOR cutLow = clip(a, b, low);
            emit(cutLow.x, cutLow.y, cutLow.z);
            break;
        }
        case 3: {
            const _D3DVECTOR cut = clip(a, b, low);
            emit(cut.x, cut.y, cut.z);
            break;
        }
        case 4:
            emit(b.x, b.y, b.z);
            break;
        case 5: {
            const _D3DVECTOR cut = clip(a, b, high);
            emit(cut.x, cut.y, cut.z);
            break;
        }
        case 6: {
            const _D3DVECTOR cutLow = clip(a, b, low);
            emit(cutLow.x, cutLow.y, cutLow.z);
            const _D3DVECTOR cutHigh = clip(a, b, high);
            emit(cutHigh.x, cutHigh.y, cutHigh.z);
            break;
        }
        case 7: {
            emit(b.x, b.y, b.z);
            const _D3DVECTOR cut = clip(a, b, high);
            emit(cut.x, cut.y, cut.z);
            break;
        }
        default:   // 0 and 8: both ends on the same outer side
            break;
        }
        previous = i;
    }
    return written;
}

// fctiwz of a single-precision value.
int32_t Trunc(float value) { return ppc_fctiwz(static_cast<double>(value)); }

}  // namespace

_D3DVECTOR CGamut::GetHFieldIntersection(_D3DVECTOR* corner) {
    // The ray's direction: the corner turned by the view matrix's rotation (type 3).
    float direction[3];
    gei_ViewMatrix->TransformVertex(direction, corner, 3);
    if (0.0f == direction[1]) direction[1] = kRayStep;

    // Where the ray meets the height field's lowest and highest planes, as multiples of the
    // direction, each outside 0..half the view distance replaced by that half.
    const float cameraY = gei_ViewMatrix->m[3][1];
    const float half = 0.5f * gei_Display.mFirstPersonViewDistance;
    float low = (mYClipLow - cameraY) / direction[1];
    if (low < 0.0f || low > half) low = half;
    float high = (mYClipHigh - cameraY) / direction[1];
    if (high < 0.0f || high > half) high = half;

    float t = high;
    if (low != high) t = 0.5f * (low + high);

    _D3DVECTOR hit;
    hit.x = ppc_fmadds(t, direction[0], gei_ViewMatrix->m[3][0]);
    hit.y = ppc_fmadds(t, direction[1], gei_ViewMatrix->m[3][1]);
    hit.z = ppc_fmadds(t, direction[2], gei_ViewMatrix->m[3][2]);
    return hit;
}

int32_t CGamut::CalculateGamut() {
    float farthest = kFarthestStart;
    SGamutQuads* cells = reinterpret_cast<SGamutQuads*>(&mFC4);     // GetQuads
    SGamutQuads* points = reinterpret_cast<SGamutQuads*>(&mF1CC);   // GetVertices

    // The rows' first values: overwritten before anything reads them.
    cells->mFirstRow = 0x7f;
    cells->mEndRow = -1;
    std::memset(cells->mLow, 0x7f, sizeof cells->mLow);
    std::memset(cells->mHigh, 0xff, sizeof cells->mHigh);

    // The four corner rays of the view volume: half the field of view's tangent, widened by
    // the display's aspect ratio, at depth 1.
    const float angle = 0.5f * ((3.1415927410125732421875f * gProjectionFOV) / 180.0f);
    const float y = static_cast<float>(std::tan(static_cast<double>(angle)));
    const float x = y / gei_Display.mF04;
    const float corners[4][3] = {{-x, y, 1.0f}, {x, y, 1.0f}, {x, -y, 1.0f}, {-x, -y, 1.0f}};
    std::memcpy(mCorners, corners, sizeof corners);

    // The footprint polygon starts at the camera and gets the four points where the rays meet
    // the height field.
    const float cameraX = gei_ViewMatrix->m[3][0];
    const float cameraY = gei_ViewMatrix->m[3][1];
    const float cameraZ = gei_ViewMatrix->m[3][2];
    CPolygon polygon{};
    polygon.AddVertex({cameraX, cameraY, cameraZ});
    for (int32_t i = 0; i < 4; ++i) {
        const _D3DVECTOR hit = GetHFieldIntersection(reinterpret_cast<_D3DVECTOR*>(mCorners[i]));
        polygon.AddVertex(hit);
        const float dy = hit.y - cameraY;
        const float dx = hit.x - cameraX;
        const float dz = hit.z - cameraZ;
        const float dy2 = dy * dy;
        const float dx2 = dx * dx;
        const float dz2 = dz * dz;
        const float distance = static_cast<float>(std::sqrt(static_cast<double>(dz2 + (dx2 + dy2))));
        if (distance > farthest) farthest = distance;
    }

    // Clip to the map: z first (into a scratch list), then x (back into the polygon). The
    // planes are the map's low edge and its high edge less one cell.
    const float zLow = mF3C[2];
    const float zHigh = mF3C[3] - mF3C[5];
    const float xLow = mF3C[0];
    const float xHigh = mF3C[1] - mF3C[4];
    float clipped[2 * CPolygon::kMaxVertices][3];
    const int32_t zCount = ClipToSlab(polygon.mVertices, polygon.mCount, clipped, 2, zLow, zHigh, GenerateClipZVector);
    polygon.mCount = zCount;
    polygon.mCount = ClipToSlab(clipped, zCount, polygon.mVertices, 0, xLow, xHigh, GenerateClipXVector);

    // Into cell units.
    const float inverseZ = 1.0f / mF3C[5];
    const float inverseX = 1.0f / mF3C[4];
    for (int32_t i = 0; i < polygon.mCount; ++i) {
        polygon.mVertices[i][0] = polygon.mVertices[i][0] * inverseX;
        polygon.mVertices[i][2] = polygon.mVertices[i][2] * inverseZ;
    }

    const int32_t mapRows = mF54[1];
    std::memset(cells->mLow, 0x7f, sizeof cells->mLow);
    std::memset(cells->mHigh, 0, sizeof cells->mHigh);
    cells->mFirstRow = 0x7f;
    cells->mEndRow = 0;

    // Walk the polygon's edges (previous vertex to current one). An edge that rises in z
    // lowers the low column of the rows it crosses, a falling one raises the high column; the
    // column in a row is the edge's x there, truncated. Rows run from the edge's first row to
    // the row of its end rounded up.
    int32_t previous = polygon.mCount - 1;
    for (int32_t i = 0; i < polygon.mCount; ++i) {
        const float* current = polygon.mVertices[i];
        const float* before = polygon.mVertices[previous];
        const float currentZ = current[2];
        const float beforeZ = before[2];
        if (currentZ > beforeZ) {
            const float beforeX = before[0];
            const float currentX = current[0];
            const float slope = (currentX - beforeX) / (currentZ - beforeZ);
            const int32_t firstRow = Trunc(beforeZ);
            const int32_t lastRow = Trunc(kAlmostOne + currentZ);
            const int32_t beforeColumn = Trunc(beforeX);
            const int32_t currentColumn = Trunc(currentX);
            float column = beforeX;
            if (Trunc(currentZ) > firstRow) {
                const float roundedRow = static_cast<float>(Trunc(kAlmostOne + beforeZ));
                column = ppc_fmadds(slope, roundedRow - beforeZ, beforeX);
            }
            if (firstRow < cells->mFirstRow) cells->mFirstRow = firstRow;
            if (lastRow > cells->mEndRow) cells->mEndRow = lastRow;
            int8_t* first = &cells->mLow[firstRow];
            int8_t* last = &cells->mLow[lastRow];
            if (*first > beforeColumn) *first = static_cast<int8_t>(beforeColumn);
            if (*last > currentColumn) *last = static_cast<int8_t>(currentColumn);
            int32_t value = Trunc(column);
            if (*first > value) *first = static_cast<int8_t>(value);
            for (int32_t row = firstRow + 1; row < lastRow; ++row) {
                int8_t* cell = &cells->mLow[row];
                value = Trunc(column);
                if (*cell > value) *cell = static_cast<int8_t>(value);
                if (row < lastRow - 1) {
                    column = column + slope;
                    value = Trunc(column);
                    if (*cell > value) *cell = static_cast<int8_t>(value);
                }
            }
            value = Trunc(column);
            if (*last > value) *last = static_cast<int8_t>(value);
        } else if (currentZ < beforeZ) {
            const float currentX = current[0];
            const float beforeX = before[0];
            const float slope = (beforeX - currentX) / (beforeZ - currentZ);
            float column = kAlmostOne + currentX;
            const int32_t firstRow = Trunc(currentZ);
            const int32_t lastRow = Trunc(kAlmostOne + beforeZ);
            const int32_t beforeColumn = Trunc(kAlmostOne + beforeX);
            const int32_t currentColumn = Trunc(column);
            if (firstRow < Trunc(beforeZ)) {
                const float roundedRow = static_cast<float>(Trunc(kAlmostOne + currentZ));
                column = ppc_fmadds(slope, roundedRow - currentZ, column);
            }
            if (firstRow < cells->mFirstRow) cells->mFirstRow = firstRow;
            if (lastRow > cells->mEndRow) cells->mEndRow = lastRow;
            int8_t* last = &cells->mHigh[lastRow];
            int8_t* first = &cells->mHigh[firstRow];
            if (*last < beforeColumn) *last = static_cast<int8_t>(beforeColumn);
            if (*first < currentColumn) *first = static_cast<int8_t>(currentColumn);
            int32_t value = Trunc(column);
            if (*first < value) *first = static_cast<int8_t>(value);
            for (int32_t row = firstRow + 1; row < lastRow; ++row) {
                int8_t* cell = &cells->mHigh[row];
                value = Trunc(column);
                if (*cell < value) *cell = static_cast<int8_t>(value);
                if (row < lastRow - 1) {
                    column = column + slope;
                    value = Trunc(column);
                    if (*cell < value) *cell = static_cast<int8_t>(value);
                }
            }
            value = Trunc(column);
            if (*last < value) *last = static_cast<int8_t>(value);
        }
        previous = i;
    }

    // A row's high column is at least its low column.
    for (int32_t row = cells->mFirstRow; row <= cells->mEndRow; ++row) {
        if (cells->mHigh[row] < cells->mLow[row]) cells->mHigh[row] = cells->mLow[row];
    }

    polygon.DumpData(nullptr);

    // The grid points of a row are those of the cells of this row and the next one.
    for (int32_t row = cells->mFirstRow; row <= cells->mEndRow; ++row) {
        points->mLow[row] = cells->mLow[row];
        if (cells->mLow[row] > cells->mLow[row + 1]) points->mLow[row] = cells->mLow[row + 1];
        points->mHigh[row] = cells->mHigh[row];
        if (cells->mHigh[row] < cells->mHigh[row + 1]) points->mHigh[row] = cells->mHigh[row + 1];
    }
    points->mFirstRow = cells->mFirstRow;
    points->mEndRow = points->mEndRow <= mapRows ? cells->mEndRow : mapRows;

    gei_ProjectionMatrix.AdjustProjection(farthest);
    return 1;
}
