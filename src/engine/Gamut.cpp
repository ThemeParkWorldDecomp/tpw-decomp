// Gamut.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "m3d2.h"
#include "MapWho.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_fpu.h"
#include "tb_hfield.h"
#include "tb_loader_dependencies.h"   // CTextureEntry
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_meshdelay.h"
#include "tb_particle.h"
#include "tb_polygon.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_rain.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_shadow.h"
#include "tb_sky.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

// The texture cache's start-up and shutdown (engine library of SimTheme Park 2.04b3;
// 0x10032bd0, 0x10032d64, 0x1003377c..0x10034b48 and 0x100385a8, which the inventory files
// under Wavelet.cpp, and InitFrequencyTable 0x1000967c from its tb_hfield.cpp range).
// PARTIAL: placing entries on pages (Prepare, Remove, ...) is still to do.

// ---- Start-up and shutdown ------------------------------------------------------------
int32_t gFrequencyTable[16];

void InitFrequencyTable() {
    for (int32_t i = 0; i < 16; ++i) gFrequencyTable[i] = 16 - i;
}

// tb_texturecache_balance.cpp (engine library of SimTheme Park 2.04b3; the inventory files the
// texture cache under Wavelet.cpp and CycleFrequencyTable under tb_hfield.cpp): the last pieces of
// the texture cache's frame that the height field's callers wait for.
//
//   0x10009770 CycleFrequencyTable                          (360 bytes)
//   0x10038670 CTextureLink::CalcUsage                      (544)
//   0x10035244 CTextureCache::AutoMipmap                    (120)
//   0x1003514c CTextureCache::Entry_Compress                (176)
//   0x1003780c CTextureCache::BalanceCache_FreeLockedTextureSpace   (392)
//   0x100379e0 CTextureCache::BalanceCache_LevelSurfaceUsage        (2,024)
//
// CTextureCache::Execute runs the two balancing steps between sorting the pages and sorting the
// queue; SortEntries calls the two shrinkers; SortTextures and PackTextures call CalcUsage.
// The functions they call and that are not translated yet (Entry_Specific_Compress 0x10034e94 and
// BalanceCache_ClearTexture 0x1003770c) are declared in tb_texturecache.h.

// Every frame CTextureCache::Refresh calls this: the index of this frame's use count in each
// entry's place record is the display's frame count modulo 16, and the table of weights
// (16 for this frame, 15 for the one that was current a frame ago ... 1) turns round with it.
void CycleFrequencyTable() {
    gFrequencyIndex = static_cast<uint32_t>(gei_Display.mFrameCount) & 0xf;
    for (int32_t age = 0; age < 16; ++age) gFrequencyTable[(gFrequencyIndex + static_cast<uint32_t>(age)) & 0xf] = 16 - age;
}

// tb_hfield.cpp range of the engine library (SimTheme Park 2.04b3): the view volume and
// the height field. PARTIAL: only what engine start-up and shutdown run; the rest of the
// inventory's range (48 functions) is still to do.

// ---- CGamut ---------------------------------------------------------------------------
CGamut* gei_Gamut = nullptr;

int32_t Gamut_Init() {
    if (gei_Gamut != nullptr) return 0;
    CGamut* gamut = new (std::nothrow) CGamut;
    gei_Gamut = gamut;
    if (gamut == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    gamut->SetYClipper(0.0f, 0.0f);
    return 1;
}

int32_t Gamut_Close() {
    if (gei_Gamut == nullptr) return 0;
    delete gei_Gamut;
    gei_Gamut = nullptr;
    return 1;
}

CGamut::CGamut() {
    mF6C = 0;
    mFC8 = 0;
    mFC4 = 0;
    mF1D0 = 0;
    mF1CC = 0;
    mYClipHigh = 0.0f;
    mYClipLow = 0.0f;
    for (float& value : mF3C) value = 0.0f;
    for (int32_t& value : mF54) value = 0;
    mFC0 = 0;
}

// The original view-volume corners, with a single-precision value at each PPC step.

void CGamut::Init(float fov) {
    const float angle = 0.5f * ((3.1415927410125732421875f * fov) / 180.0f);
    const float y = 0.5f * static_cast<float>(std::tan(static_cast<double>(angle)));
    const float x = y / gei_Display.mF04;
    mCorners[0][0] = -x; mCorners[0][1] = y;  mCorners[0][2] = 1.0f;
    mCorners[1][0] = x;  mCorners[1][1] = y;  mCorners[1][2] = 1.0f;
    mCorners[2][0] = x;  mCorners[2][1] = -y; mCorners[2][2] = 1.0f;
    mCorners[3][0] = -x; mCorners[3][1] = -y; mCorners[3][2] = 1.0f;
}

void CGamut::SetYClipper(float low, float high) {
    mYClipLow = low;
    mYClipHigh = high;
}

void CGamut::CalcFurthestDistance() {
    const float dx = mF3C[1] - mF3C[0], dz = mF3C[3] - mF3C[2];
    mFurthestDistance = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(dx, dx, dz * dz))));
}
void CGamut::SetX(float low, float high, float cell) {
    mF3C[0] = low; mF3C[1] = high; mF3C[4] = cell;
    if (cell != 0.0f) { mF54[0] = ppc_fctiwz((high - low) / cell); mF54[2] = mF54[0] - 1; }
}
void CGamut::SetZ(float low, float high, float cell) {
    mF3C[2] = low; mF3C[3] = high; mF3C[5] = cell;
    if (cell != 0.0f) { mF54[1] = ppc_fctiwz((high - low) / cell); mF54[3] = mF54[1] - 1; }
}

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

// tb_polygon.cpp (engine library of SimTheme Park 2.04b3; the original is tb_gamut.cpp, the
// file name the assertion in the clip vectors reports): the footprint polygon of CGamut's
// view volume and the two functions that cut an edge at a plane. CPolygon::DumpData
// 0x1000b748, GenerateClipXVector 0x1000b7dc, GenerateClipZVector 0x1000b930,
// CPolygon::GetNewVertexOutsideStatus 0x1000ba74, CPolygon::IsOutsideOfPlane 0x1000bb9c.
//
// Quirks kept: the clip vectors report a parameter outside -1..1 (a NaN too) through the MSL
// assertion and then use it all the same; the x and z a clip vector does not take from its
// plane come from a fused multiply-add of the parameter and the edge's extent.

namespace {

// Which side of the edge from vertex index - 1 (the last vertex for index 0) to vertex `index`
// the point lies on, as the listing does it in both IsOutsideOfPlane and the loop of
// GetNewVertexOutsideStatus: the edge's x extent times the point's z offset, plus the negated z
// extent times the point's x offset, the second product fused.
float EdgeSide(const CPolygon& polygon, _D3DVECTOR v, int32_t index) {
    const int32_t before = index - 1 < 0 ? polygon.mCount - 1 : index - 1;
    const float* previous = polygon.mVertices[before];
    const float* current = polygon.mVertices[index];
    const float zOffset = v.z - previous[2];
    const float xExtent = current[0] - previous[0];
    const float zExtent = current[2] - previous[2];
    const float xOffset = v.x - previous[0];
    const float product = xExtent * zOffset;
    return ppc_fmadds(-zExtent, xOffset, product);
}

// The assertion every clip vector makes about the edge's parameter.
void CheckParameter(float t, int32_t line) {
    if (!(t <= 1.0f && t >= -1.0f)) msl_assertion_failed("(f<=1) && (f>=-1)", "tb_gamut.cpp", line);
}

}  // namespace

void CPolygon::DumpData(_D3DVECTOR* data) {
    // The Mac's body returns at once for a pointer; for a null one it counts mCount down in
    // two loops with empty bodies (a trace that was compiled out). Nothing is written.
    (void)data;
}

_D3DVECTOR GenerateClipXVector(_D3DVECTOR a, _D3DVECTOR b, float x) {
    const float t = (x - b.x) / (a.x - b.x);
    CheckParameter(t, 0xf6);
    _D3DVECTOR result;
    result.x = x;
    result.y = ppc_fmadds(t, a.y - b.y, b.y);
    result.z = ppc_fmadds(t, a.z - b.z, b.z);
    return result;
}

_D3DVECTOR GenerateClipZVector(_D3DVECTOR a, _D3DVECTOR b, float z) {
    const float t = (z - b.z) / (a.z - b.z);
    CheckParameter(t, 0x103);
    _D3DVECTOR result;
    result.x = ppc_fmadds(t, a.x - b.x, b.x);
    result.y = ppc_fmadds(t, a.y - b.y, b.y);
    result.z = z;
    return result;
}
int32_t CPolygon::GetNewVertexOutsideStatus(_D3DVECTOR v) {
    int32_t any = 0;
    mOutsideCount = 0;
    for (int32_t index = mCount - 1; index >= 0; --index) {
        mOutside[index] = EdgeSide(*this, v, index) > 0.0f ? 1 : 0;
        mOutsideCount += mOutside[index];
        any |= mOutside[index];
    }
    return any;
}

float CPolygon::IsOutsideOfPlane(_D3DVECTOR v, int32_t index) {
    return EdgeSide(*this, v, index);
}

// tb_map_render.cpp: the engine's frame (engine library, the inventory's tb_mapzoom.cpp
// range; CMapWho::Render asserts with "tb_map.cpp"). CMapWho::Render 0x10018d70 draws every
// registered window (twice or three times each in the stereo modes), RenderWindow 0x10018470
// one window, PrepareWorldspace 0x100182bc the links in view, ALTTAB_RestoreState
// 0x100194a8 the recovery after a lost surface, SetRenderOptions 0x10017ad8 and
// GetRenderOptions 0x10017b10 the game window's flags; CGamut::GetQuads 0x1000bc40 (the
// tb_hfield.cpp range) is here because only the frame uses it.

SGamutQuads* CGamut::GetQuads() { return reinterpret_cast<SGamutQuads*>(&mFC4); }

// tb_hfield_frame.cpp (engine library of SimTheme Park 2.04b3, the height field's per-frame
// members of tb_hfield.cpp): CHField::Prepare 0x1000ed18, CHField::DivideAndConquer
// 0x1000e144 and CGamut::GetVertices 0x1000bc70.
//
// DivideAndConquer is what a first-person view uses (the lobby's) to find the map cells in
// view: it tests the box of cells within half the view distance of the camera (TestConQuad,
// untranslated, widens the rows of grid points that are in the view volume), adds a block of
// cells ahead of the camera, turns the rows of grid points into rows of cells (a cell needs
// both its grid points' rows) and leaves the cells around the camera in the display for the
// procedural textures. Prepare runs inside PrepareWorldspace: it has
// the texture cache place the picture of every cell in view and, with procedural textures
// on, collects the cells inside the procedural window for the handler.
//
// Quirks kept: DivideAndConquer's upper row bound divides the camera's z by the cell WIDTH
// (the lower bound divides by the cell depth); a row of grid points no cell touched takes the
// row before it (for row 0 the original reads the low byte of the end row, which sits just
// before the low columns on the big-endian Mac); the column bounds are stored as bytes but
// compared as ints; Prepare calls the cache with a null entry when a cell's texture has none,
// and when the list for the procedural textures is full it skips the cell without clearing
// its flag.

SGamutQuads* CGamut::GetVertices() { return reinterpret_cast<SGamutQuads*>(&mF1CC); }
