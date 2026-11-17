// host_free_view.cpp: the ground the free camera sees when it looks toward the horizon. CHField::DivideAndConquer finds the
// map's cells in view: the original tests only the cells within half the view distance of the camera (350 with the
// first person's 700, the fog hiding the edge) against a far plane 1000 out. Tilted low over the Land of Invention (96 by
// 85 cells of 10) the free camera showed plain fog colour past that (0.5.6 test build). With the port's fixes (freecam,
// src/engine/tb_hfield.cpp) and the Free camera on, every cell of the map is tested, against a far plane that reaches the
// map's farthest corner:
//   - the normal camera, fog on and off: only the cells near the camera, against 1000, as in the original (both builds);
//   - the free camera low over Land of Invention: the cells out to the map's far corner are in view, the far plane 1300;
//   - the free camera zoomed all the way out (500), the eye outside the map: the far plane reaches the far corner (1800);
//   - the free camera on a small map: the original's 1000; far from the map: never past the fog's start (1e6).
// The program runs the real CHField::DivideAndConquer and TestConQuad, and the real clipper's outside codes; the math
// unit is a stand-in that projects with the camera of the view matrix and the far plane of the last AdjustProjection.
// The park view (the free camera above 30 degrees) finds its cells with CGamut::CalculateGamut instead: the original's
// corner rays reach at most half the view distance (230 with fog off), and the far park showed as holes of sky colour.
// With the port's fix (freecam, src/engine/Gamut.cpp) and the Free camera on, they reach the ground or the map's edge:
//   - the normal camera tilted 35 degrees over Land of Invention: the footprint stops near 230, as in the original;
//   - the free camera there: the far corner's cell is in view, and the far plane reaches past 1000;
//   - the free camera from 96 places, headings and tilts: every cell the normal camera draws is drawn too.
// The island screen's own camera (gPortExternalView) is not the park's: with the Free camera on it finds the normal
// camera's cells and far planes, bit for bit, in first person (after an island was clicked; the sea went flat there in
// a 0.5.6 test build) and in the park view's way (before), and back in the park the free camera's are found again.
// The launcher's Draw distance (drawdist, include/TPCamera.h PortDrawSpaceFor, src/engine/tb_hfield.cpp): Original by
// default, the original's view distances and fog spaces bit for bit and the far plane 1000; Farther half as far again
// and Whole world out to the map's far corner, with the far plane past it.
// The program runs the real CalculateGamut, GetHFieldIntersection and the footprint polygon; the view matrix's turn
// of a corner ray is a stand-in.
// Made in both builds: without the port's fixes it checks only the normal camera. Exits nonzero on a failure.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "m3d2.h"
#include "tb_clipping.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_resource_display.h"
#ifdef TPW_TEST_PORT_FIX
#include "PortFreeCamera.h"   // kPortFreeZoomOut
#include "TPCamera.h"
#endif

namespace {

int gFailures = 0;
int gChecks = 0;

void Check(bool ok, const char* what, double value = -1) {
    ++gChecks;
    if (ok) return;
    ++gFailures;
    std::printf("FAIL %s (%g)\n", what, value);
}

alignas(CHField) unsigned char gFieldStorage[sizeof(CHField)];
CHField& Field() { return *reinterpret_cast<CHField*>(gFieldStorage); }
sHField gRecord;
CMatrix gView;
CMatrix gFinal;
std::vector<float> gFarPlanes;   // what each AdjustProjection was given

constexpr float kNear = 1.0f;

// The stand-in math unit: the camera's axes are the view matrix's rows 0 (right), 1 (up) and 2 (forward), its position
// row 3; a point d ahead comes out with w = d and z = far (d - near) / (far - near), so the clipper finds it inside
// from the near plane out to the far one, and x and y inside a quarter turn either way.
class StandInMath : public CTransformBase {
public:
    void Transform(sMatrix*, void*, sVertexPosnSet*, int32_t, int32_t, int32_t) override {}
    void Transform(sMatrix*, void* out, void* in, int32_t outStride, int32_t inStride, int32_t count, int32_t) override {
        const float far = gFarPlanes.empty() ? 1000.0f : gFarPlanes.back();
        for (int32_t i = 0; i < count; i++) {
            const float* p = reinterpret_cast<const float*>(static_cast<const uint8_t*>(in) + i * inStride);
            float* o = reinterpret_cast<float*>(static_cast<uint8_t*>(out) + i * outStride);
            const float rx = p[0] - gView.m[3][0];
            const float ry = p[1] - gView.m[3][1];
            const float rz = p[2] - gView.m[3][2];
            const float d = rx * gView.m[2][0] + ry * gView.m[2][1] + rz * gView.m[2][2];
            o[0] = rx * gView.m[0][0] + ry * gView.m[0][1] + rz * gView.m[0][2];
            o[1] = rx * gView.m[1][0] + ry * gView.m[1][1] + rz * gView.m[1][2];
            o[2] = far * (d - kNear) / (far - kNear);
            o[3] = d;
        }
    }
    float Project(void*, int32_t, int32_t, int32_t) override { return 0.0f; }
    void Light(SFVF_TLCTEX*, tag_sNode*, sFaceSet*, int32_t) override {}
    void TransformUV(STMCoordinateSet*, sUVSet*, int32_t, int32_t, int32_t) override {}
    void MultMatrix(const float*, const float*, float*) override {}
};
StandInMath gMath;

}  // namespace

// ---- the engine objects the unit reads
CDisplay gei_Display;
CMatrix gei_ProjectionMatrix;
CMatrix* gei_ViewMatrix = &gView;
CMatrix* gei_FinalMatrix = &gFinal;
CTransformBase* gei_MathUnit = &gMath;
CClipping* gei_Clipping = nullptr;
CDisplay::CDisplay() {}
CTransformBase::~CTransformBase() {}

float gProjectionFOV = 60.0f;

// ---- the stand-ins
void CMatrix::AdjustProjection(float farPlane) { gFarPlanes.push_back(farPlane); }
// The view matrix's turn (type 3) of a corner ray: by rows 0, 1 and 2, as the stand-in math unit's camera.
void CMatrix::TransformVertex(void* out, void* in, int32_t) {
    const float* v = static_cast<const float*>(in);
    float* o = static_cast<float*>(out);
    for (int32_t i = 0; i < 3; i++) o[i] = v[0] * m[0][i] + v[1] * m[1][i] + v[2] * m[2][i];
}
int32_t PreprocessMatrices() { return 1; }
void msl_assertion_failed(const char*, const char*, int32_t) {}

namespace {

constexpr float kPi = 3.14159265f;

// A map of `columns` by `rows` cells of `cell`, heights low..high, origin at 0, 0.
void MakeMap(uint32_t columns, uint32_t rows, float cell, float low, float high) {
    std::memset(&gRecord, 0, sizeof gRecord);
    gRecord.mF08[0] = 0.0f;
    gRecord.mF08[1] = 0.0f;
    gRecord.mF08[2] = cell;
    gRecord.mF08[3] = cell;
    gRecord.mF18[0] = columns;
    gRecord.mF18[1] = rows;
    gRecord.mF20[0] = low;
    gRecord.mF20[1] = high;
}

// The camera at x, y, z, turned `heading` (radians from +z toward +x) and tilted `pitch` (radians, below 0 down).
void SetCamera(float x, float y, float z, float heading, float pitch) {
    std::memset(&gView, 0, sizeof gView);
    const float fx = std::sin(heading) * std::cos(pitch);
    const float fy = std::sin(pitch);
    const float fz = std::cos(heading) * std::cos(pitch);
    const float rx = std::cos(heading);
    const float rz = -std::sin(heading);
    gView.m[0][0] = rx;
    gView.m[0][2] = rz;
    gView.m[1][0] = fy * rz;   // up = forward x right, with right horizontal
    gView.m[1][1] = fz * rx - fx * rz;
    gView.m[1][2] = -fy * rx;
    gView.m[2][0] = fx;
    gView.m[2][1] = fy;
    gView.m[2][2] = fz;
    gView.m[3][0] = x;
    gView.m[3][1] = y;
    gView.m[3][2] = z;
    gView.m[3][3] = 1.0f;
}

struct Result {
    int32_t first, end;          // the rows of cells in view
    int32_t farthestRow;         // the farthest row that has a cell in view
    bool cornerInView;           // the map's far corner cell (last column, last row)
    float testFar;               // the far plane the boxes were tested against
    float drawFar;               // the far plane the projection got at the end
};

Result Run(uint32_t displayFlags, float viewDistance) {
    CHField& field = Field();
    std::memset(static_cast<void*>(&field), 0, sizeof field);
    field.mF00 = &gRecord;
    field.mInvCellX = 1.0f / gRecord.mF08[2];
    field.mInvCellZ = 1.0f / gRecord.mF08[3];
    std::memset(static_cast<void*>(&gei_Display), 0, sizeof gei_Display);
    gei_Display.mFlags = displayFlags;
    gei_Display.mFirstPersonViewDistance = viewDistance;
    gFarPlanes.clear();
    Check(field.DivideAndConquer() == 1, "DivideAndConquer's answer");
    const SGamutQuads& cells = *field.QuadRows();
    Result r{cells.mFirstRow, cells.mEndRow, -1, false, 0.0f, 0.0f};
    const int32_t lastColumn = static_cast<int32_t>(gRecord.mF18[0]) - 1;
    const int32_t lastRow = static_cast<int32_t>(gRecord.mF18[1]) - 1;
    for (int32_t row = cells.mFirstRow; row < cells.mEndRow && row < 128; row++) {
        if (row < 0 || cells.mLow[row] >= cells.mHigh[row]) continue;
        r.farthestRow = row;
        if (row == lastRow && cells.mLow[row] <= lastColumn && cells.mHigh[row] > lastColumn) r.cornerInView = true;
    }
    Check(gFarPlanes.size() == 2, "AdjustProjection calls", static_cast<double>(gFarPlanes.size()));
    if (gFarPlanes.size() == 2) {
        r.testFar = gFarPlanes[0];
        r.drawFar = gFarPlanes[1];
    }
    return r;
}

struct GamutResult {
    int32_t farthestRow;         // the farthest row that has a cell in view
    bool cornerInView;           // the map's far corner cell
    float drawFar;               // the far plane the projection got
    SGamutQuads cells;           // the cells in view
};

// Every cell of `inner` is a cell of `outer`.
bool Covers(const SGamutQuads& outer, const SGamutQuads& inner) {
    for (int32_t row = inner.mFirstRow; row <= inner.mEndRow && row < 128; row++) {
        if (row < 0 || inner.mHigh[row] < inner.mLow[row]) continue;
        if (row < outer.mFirstRow || row > outer.mEndRow) return false;
        if (inner.mLow[row] < outer.mLow[row] || inner.mHigh[row] > outer.mHigh[row]) return false;
    }
    return true;
}

alignas(CGamut) unsigned char gGamutStorage[sizeof(CGamut)];

// CGamut::CalculateGamut over the map of MakeMap (its bounds as CHField::Prepare gives them), the display 4:3.
GamutResult RunGamut() {
    std::memset(gGamutStorage, 0, sizeof gGamutStorage);
    CGamut& gamut = *reinterpret_cast<CGamut*>(gGamutStorage);
    const float cell = gRecord.mF08[2];
    const int32_t columns = static_cast<int32_t>(gRecord.mF18[0]);
    const int32_t rows = static_cast<int32_t>(gRecord.mF18[1]);
    gamut.mF3C[0] = 0.0f;
    gamut.mF3C[1] = static_cast<float>(columns) * cell;
    gamut.mF3C[2] = 0.0f;
    gamut.mF3C[3] = static_cast<float>(rows) * cell;
    gamut.mF3C[4] = cell;
    gamut.mF3C[5] = cell;
    gamut.mF54[0] = columns;
    gamut.mF54[1] = rows;
    gamut.mF54[2] = columns - 1;
    gamut.mF54[3] = rows - 1;
    gamut.mYClipLow = gRecord.mF20[0];
    gamut.mYClipHigh = gRecord.mF20[1];
    std::memset(static_cast<void*>(&gei_Display), 0, sizeof gei_Display);
    gei_Display.mF04 = 0.75f;
    gei_Display.mFirstPersonViewDistance = 460.0f;
    gFarPlanes.clear();
    Check(gamut.CalculateGamut() == 1, "CalculateGamut's answer");
    const SGamutQuads& cells = *gamut.GetQuads();
    GamutResult r{-1, false, gFarPlanes.empty() ? 0.0f : gFarPlanes.back(), cells};
    for (int32_t row = cells.mFirstRow; row <= cells.mEndRow && row < 128; row++) {
        if (row < 0 || cells.mHigh[row] < cells.mLow[row]) continue;
        r.farthestRow = row;
        if (row == rows - 1 && cells.mLow[row] <= columns - 1 && cells.mHigh[row] >= columns - 1) r.cornerInView = true;
    }
    Check(gFarPlanes.size() == 1, "CalculateGamut's AdjustProjection calls", static_cast<double>(gFarPlanes.size()));
    return r;
}

void SetFree(bool on) {
#ifdef TPW_TEST_PORT_FIX
    gPortFreeCamera = on;
    gPortFreeLowView = on;
#else
    (void)on;
#endif
}

}  // namespace

int main() {
    // Land of Invention: 96 by 85 cells of 10, heights -10 to 10. The camera near the corner at 0, 0, 25 up, looking along
    // the diagonal toward the far corner, 5 degrees down: the horizon of the maintainer's picture.
    MakeMap(96, 85, 10.0f, -10.0f, 10.0f);
    const float heading = std::atan2(945.0f, 835.0f);
    SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);

    // The normal camera, fog on with the first person's 700: the cells within 350 (and the block ahead of the camera),
    // tested against 1000, as in the original.
    SetFree(false);
    {
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == 1000.0f, "normal camera, fog: far plane", r.testFar);
        Check(r.farthestRow >= 30 && r.farthestRow <= 37, "normal camera, fog: farthest row", r.farthestRow);
        Check(!r.cornerInView, "normal camera, fog: the far corner is not tested");
    }
    // Fog off: the original's 460, the cells within 230.
    {
        const Result r = Run(0, 700.0f);
        Check(r.testFar == 1000.0f, "normal camera, no fog: far plane", r.testFar);
        Check(r.farthestRow >= 20 && r.farthestRow <= 25, "normal camera, no fog: farthest row", r.farthestRow);
    }
    // The park view, tilted 35 degrees from 120 up over the same corner: the footprint stops near half the view distance
    // (230 deep), as in the original.
    SetCamera(15.0f, 120.0f, 15.0f, heading, -35.0f * kPi / 180.0f);
    const GamutResult normalLow = RunGamut();
    Check(normalLow.farthestRow >= 28 && normalLow.farthestRow <= 36, "normal camera, park view: farthest row",
          normalLow.farthestRow);
    Check(!normalLow.cornerInView, "normal camera, park view: the far corner is not in view");
    Check(normalLow.drawFar > 250.0f && normalLow.drawFar < 400.0f, "normal camera, park view: far plane",
          normalLow.drawFar);
    // Steeper (60 degrees from 200 up, over the middle of the map): kept for the free camera's comparison.
    SetCamera(480.0f, 200.0f, 200.0f, 0.3f, -60.0f * kPi / 180.0f);
    const GamutResult normalSteep = RunGamut();
    SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);

#ifdef TPW_TEST_PORT_FIX
    // The free camera low (the fog flag kept): the whole map is tested, against a far plane past the far corner (1261.5
    // and a cell, rounded up to 1300), and the corner is in view.
    SetFree(true);
    {
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == 1300.0f, "free camera, low: far plane", r.testFar);
        Check(r.farthestRow == 84, "free camera, low: farthest row", r.farthestRow);
        Check(r.cornerInView, "free camera, low: the far corner is in view");
        Check(r.drawFar > 1000.0f && r.drawFar <= 1300.0f, "free camera, low: the drawing's far plane", r.drawFar);
        Check(r.first == 0 && r.end >= 84, "free camera, low: the rows", r.end);
    }
    // Fog off (looking higher): the same.
    {
        const Result r = Run(0, 700.0f);
        Check(r.testFar == 1300.0f, "free camera, no fog: far plane", r.testFar);
        Check(r.cornerInView, "free camera, no fog: the far corner is in view");
    }
    // The original's test of the whole map (kTestWholeMap): the same far plane.
    {
        const Result r = Run(CDisplay::kTestWholeMap, 700.0f);
        Check(r.testFar == 1300.0f, "free camera, whole map: far plane", r.testFar);
        Check(r.cornerInView, "free camera, whole map: the far corner is in view");
    }
    // The free camera zoomed all the way out (kPortFreeZoomOut, 500) from a target over the near corner, 10 degrees down:
    // the eye is outside the map, at about -354, 86, -311, and the far plane still reaches the far corner (1756 and a
    // cell, rounded up to 1800), which is in view.
    {
        const float pitch = 10.0f * kPi / 180.0f;
        const float back = kPortFreeZoomOut * std::cos(pitch);
        SetCamera(15.0f - back * std::sin(heading), kPortFreeZoomOut * std::sin(pitch), 15.0f - back * std::cos(heading),
                  heading, -pitch);
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == 1800.0f, "free camera, farthest zoom: far plane", r.testFar);
        Check(r.cornerInView, "free camera, farthest zoom: the far corner is in view");
    }
    // A small map all within 1000: the original's far plane.
    MakeMap(40, 30, 10.0f, -10.0f, 10.0f);
    SetCamera(15.0f, 25.0f, 15.0f, std::atan2(385.0f, 285.0f), -5.0f * kPi / 180.0f);
    {
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == 1000.0f, "free camera, small map: far plane", r.testFar);
        Check(r.cornerInView, "free camera, small map: the far corner is in view");
    }
    // The camera a million away: never past the fog's start.
    MakeMap(96, 85, 10.0f, -10.0f, 10.0f);
    SetCamera(1.0e6f, 25.0f, 0.0f, -kPi / 2.0f, 0.0f);
    {
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == kPortFreeFogNear, "free camera, far away: far plane", r.testFar);
    }
    // The park view tilted 35 degrees: the footprint reaches the far corner (the map's last row), the far plane past
    // 1000, and every cell the normal camera draws is drawn.
    SetCamera(15.0f, 120.0f, 15.0f, heading, -35.0f * kPi / 180.0f);
    {
        const GamutResult r = RunGamut();
        Check(r.cornerInView, "free camera, park view: the far corner is in view");
        Check(r.farthestRow >= 84, "free camera, park view: farthest row", r.farthestRow);
        Check(r.drawFar > 1000.0f && r.drawFar < 2000.0f, "free camera, park view: far plane", r.drawFar);
        Check(Covers(r.cells, normalLow.cells), "free camera, park view: the normal camera's cells are drawn");
    }
    // Steeper: still every cell the normal camera draws.
    SetCamera(480.0f, 200.0f, 200.0f, 0.3f, -60.0f * kPi / 180.0f);
    {
        const GamutResult r = RunGamut();
        Check(Covers(r.cells, normalSteep.cells), "free camera, steep park view: the normal camera's cells are drawn");
        Check(r.drawFar < 2000.0f, "free camera, steep park view: far plane", r.drawFar);
    }
    // From the middle, a corner and outside the map, eight headings, tilted 31 to 80 degrees: every cell the normal
    // camera draws is drawn.
    {
        const float places[3][3] = {{480.0f, 200.0f, 425.0f}, {15.0f, 120.0f, 15.0f}, {-100.0f, 80.0f, 400.0f}};
        const float pitches[4] = {-31.0f, -45.0f, -60.0f, -80.0f};
        int32_t missed = 0;
        for (const auto& place : places) {
            for (int32_t step = 0; step < 8; step++) {
                for (const float pitch : pitches) {
                    SetCamera(place[0], place[1], place[2], static_cast<float>(step) * kPi / 4.0f, pitch * kPi / 180.0f);
                    SetFree(false);
                    const GamutResult normal = RunGamut();
                    SetFree(true);
                    const GamutResult free = RunGamut();
                    if (!Covers(free.cells, normal.cells) && missed++ == 0) {
                        std::printf("first missed: %g %g %g, heading %d eighths, pitch %g\n", place[0], place[1], place[2],
                                    step, pitch);
                    }
                }
            }
        }
        Check(missed == 0, "free camera, park views: the normal camera's cells are drawn", missed);
    }
    // The island screen's camera (gPortExternalView, set by HandleCamera, which also ends the low view there) with the
    // Free camera on: the normal camera's cells and far planes, bit for bit, in first person and in the park view's way.
    {
        SetFree(false);
        SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);
        const Result normal = Run(CDisplay::kEnableFog, 700.0f);
        const SGamutQuads normalCells = *Field().QuadRows();
        SetCamera(15.0f, 120.0f, 15.0f, heading, -35.0f * kPi / 180.0f);
        const GamutResult normalView = RunGamut();
        SetFree(true);
        gPortFreeLowView = false;
        gPortExternalView = true;
        Check(!PortFreeCameraDraws(), "island screen: the Free camera's drawing changes are off");
        SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == normal.testFar && r.drawFar == normal.drawFar && !r.cornerInView &&
                  std::memcmp(Field().QuadRows(), &normalCells, sizeof normalCells) == 0,
              "island screen, first person: the normal camera's cells and far planes", r.drawFar);
        SetCamera(15.0f, 120.0f, 15.0f, heading, -35.0f * kPi / 180.0f);
        const GamutResult view = RunGamut();
        Check(view.drawFar == normalView.drawFar && std::memcmp(&view.cells, &normalView.cells, sizeof view.cells) == 0,
              "island screen, park view: the normal camera's cells and far plane", view.drawFar);
        // Back in the park (HandleCamera clears it): the free camera's footprint again.
        gPortExternalView = false;
        Check(PortFreeCameraDraws(), "back in the park: the Free camera's drawing changes are on");
        const GamutResult back = RunGamut();
        Check(back.cornerInView, "back in the park, park view: the far corner is in view");
    }
    // The normal camera again after the free one: the original's.
    SetFree(false);
    SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);
    {
        const Result r = Run(CDisplay::kEnableFog, 700.0f);
        Check(r.testFar == 1000.0f, "normal camera again: far plane", r.testFar);
        Check(!r.cornerInView, "normal camera again: the far corner is not tested");
    }
    SetCamera(15.0f, 120.0f, 15.0f, heading, -35.0f * kPi / 180.0f);
    {
        const GamutResult r = RunGamut();
        Check(r.farthestRow == normalLow.farthestRow && r.drawFar == normalLow.drawFar &&
                  std::memcmp(&r.cells, &normalLow.cells, sizeof r.cells) == 0,
              "normal camera again, park view: the original's cells and far plane");
    }

    // Draw distance (drawdist, include/TPCamera.h): Original unless the launcher sets it (the launcher's own default is
    // Farther), and at Original every View distance's view distance and fog space are the original's, bit for bit.
    Check(gPortDrawDistance == kPortDrawOriginal, "draw distance: Original unless the launcher sets it");
    const float viewSettings[5][3] = {{200.0f, 50.0f, 60.0f}, {325.0f, 100.0f, 110.0f}, {450.0f, 135.0f, 150.0f},
                                      {700.0f, 230.0f, 250.0f}, {400.0f, 100.0f, 130.0f}};
    for (const auto& v : viewSettings) {
        for (const float reach : {700.0f, 1300.0f, 1e6f}) {
            const PortDrawSpace o = PortDrawSpaceFor(kPortDrawOriginal, v[0], v[1], v[2], reach);
            Check(std::memcmp(&o.viewDistance, &v[0], 4) == 0 && std::memcmp(&o.fogNear, &v[1], 4) == 0 &&
                      std::memcmp(&o.fogFar, &v[2], 4) == 0,
                  "draw distance, Original: the View distance's own", v[0]);
            const PortDrawSpace f = PortDrawSpaceFor(kPortDrawFarther, v[0], v[1], v[2], reach);
            Check(f.viewDistance == v[0] * 1.5f && f.fogNear == v[1] * 1.5f && f.fogFar == v[2] * 1.5f,
                  "draw distance, Farther: half as far again", v[0]);
            const PortDrawSpace w = PortDrawSpaceFor(kPortDrawWhole, v[0], v[1], v[2], reach);
            Check(w.viewDistance >= 2.0f * reach && w.fogFar == reach && w.fogNear < reach && w.fogNear >= v[1],
                  "draw distance, Whole world: the fog only at the reach", v[0]);
        }
    }
    {   // nothing nearer than the original's: a fog already past the reach, or none, stays
        const PortDrawSpace w = PortDrawSpaceFor(kPortDrawWhole, 700.0f, 900.0f, 1000.0f, 800.0f);
        Check(w.viewDistance == 700.0f && w.fogNear == 900.0f && w.fogFar == 1000.0f, "draw distance, Whole world: no nearer");
        const PortDrawSpace z = PortDrawSpaceFor(kPortDrawWhole, 700.0f, 0.0f, 0.0f, 1300.0f);
        Check(z.viewDistance == 700.0f && z.fogFar == 0.0f, "draw distance, Whole world: no fog space, kept");
    }
    // DivideAndConquer in first person: at Original even a view distance past the original's longest keeps the far
    // plane 1000; above Original, the far plane reaches the map (1300 from this corner), Farther at Max (700 * 1.5) tests
    // further than Max, and Whole world (the map's diagonal 1281, rounded up to 1300, both ways) reaches the far corner.
    SetFree(false);
    SetCamera(15.0f, 25.0f, 15.0f, heading, -5.0f * kPi / 180.0f);
    const Result maxRun = Run(CDisplay::kEnableFog, 700.0f);
    {
        const Result r = Run(CDisplay::kEnableFog, 1050.0f);
        Check(r.testFar == 1000.0f, "draw distance, Original: the original's far plane",
              r.testFar);
    }
    gPortDrawDistance = kPortDrawFarther;
    {
        const Result same = Run(CDisplay::kEnableFog, 700.0f);
        Check(same.testFar == 1000.0f && same.farthestRow == maxRun.farthestRow,
              "draw distance, Farther: a view distance the original has keeps its far plane", same.testFar);
        const Result r = Run(CDisplay::kEnableFog, 700.0f * kPortDrawFartherScale);
        Check(r.testFar == 1300.0f, "draw distance, Farther at Max: far plane", r.testFar);
        Check(r.farthestRow > maxRun.farthestRow, "draw distance, Farther at Max: further than Max", r.farthestRow);
    }
    gPortDrawDistance = kPortDrawWhole;
    {
        const PortDrawSpace w = PortDrawSpaceFor(kPortDrawWhole, 700.0f, 230.0f, 250.0f, 1300.0f);
        const Result r = Run(CDisplay::kEnableFog, w.viewDistance);
        Check(r.testFar == 1300.0f && r.cornerInView && r.farthestRow == 84, "draw distance, Whole world: the far corner",
              r.farthestRow);
    }
    gPortDrawDistance = kPortDrawOriginal;
#else
    std::printf("without the port's fixes only the normal camera is checked\n");
#endif

    std::printf("%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
