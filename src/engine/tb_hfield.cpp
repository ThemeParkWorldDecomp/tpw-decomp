// tb_hfield.cpp: name proven by __sinit_tb_hfield_cpp in the engine library.

#include "helpers/tb_hfieldHelpers.h"
#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/d3d_types.h"
#include "platform/indirectx.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_hfield.h"
#include "tb_init.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_printf_opengl.h"
#include "tb_proctexture.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_sky.h"
#include "tb_switch.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <utility>

// tb_hfield.cpp range of the engine library (SimTheme Park 2.04b3): the view volume and
// the height field. PARTIAL: only what engine start-up and shutdown run; the rest of the
// inventory's range (48 functions) is still to do.

// ---- CHField --------------------------------------------------------------------------
CHField* gei_HField = nullptr;

int32_t HField_Init() {
    if (gei_HField != nullptr) return 0;
    gei_HField = new (std::nothrow) CHField;
    if (gei_HField == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t HField_Close() {
    if (gei_HField != nullptr) {
        gei_HField->Close();
        delete gei_HField;
        gei_HField = nullptr;
    }
    return 1;
}

// The engine's procedural texture range (engine symbols shared, tb_hfield.cpp range; unit w111).

// 0x1000bde4 (80 bytes): hands the range to the height field, when there is one.
void PT_SetRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    if (gei_HField != nullptr) gei_HField->ProcTextureRange(x0, y0, x1, y1);
}

float gei_HFieldOne;                 // 0x100af120

// 0x1000be5c
void PT_Enable() {
    uint32_t flags = gei_TextureSettings.mFlags;
    if (flags & CTextureSettings::kProcTexturesBlocked) return;
    gei_TextureSettings.mFlags = flags | CTextureSettings::kProcTexturesOn;
}
// 0x1000be98
void PT_Disable() { gei_TextureSettings.mFlags &= ~CTextureSettings::kProcTexturesOn; }

// Unit v5953: ConvertSquareFrom32 (0x10003454), PT_Load (0x1000bed0), CSwitch::Do (0x100328ac),
// COpenGLPrintf::Close (0x10046c64).

uint32_t PT_Load(char* path, int32_t mode) {
    if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesBlocked) == 0) {
        if (gei_HField == nullptr) return 1;
        return static_cast<uint32_t>(gei_HField->LoadProcTextures(path, mode));
    }
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    SamsUtilities::gLogger->SetContext("engine", "tb_hfield.cpp", 0x86, 0x80000000u);
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new SamsUtilities::ULogManager;
    SamsUtilities::gLogger->Log("Procedural textures not allowed, refer to enginedebug.txt for possible explaination");
    return 1;
}

CHField::CHField() {
    mF24 = 0;
    mF20 = 0;
    mF12C = 0;
    mF128 = 0;
    mF00 = 0;
    mF04 = 0;
    mProcTextures = nullptr;
    mF238 = 0;
    mF234 = 0;
    mF240 = 0x7f;
    mF23C = 0x7f;
    mF2958 = 0;
    mF12968 = 0;
    mF12970 = nullptr;
}

CHField::~CHField() { Close(); }

// CHField::AdjustHField (engine 0x1000c0e8, 5,676 bytes; the inventory's tb_hfield.cpp
// range). Init calls it with display flag 0x10000 on the copy of the heights it has just
// made (mF12970): 27 fixed rectangles of grid points, in columns 28..68 and rows 0..16,
// are raised by 4.0, 1.0 or 1.4. The file's own heights (sHField::mHeights) stay as read.
//
// The original inlines one bounds test and add per grid point (no out-of-line copy exists)
// and unrolls the loops in several ways; the order of the rectangles is kept because some
// overlap with different amounts, and float additions in another order round differently.

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

void CHField::Init(tag_sM3D2Header* header) {
    mF00 = header->mHField;
    mF04 = header->mTextures;
    mGridWidth = mF00->mF18[0] + 1;
    mGridHeight = mF00->mF18[1] + 1;
    mInvCellX = 1.0f / mF00->mF08[2];
    mInvCellZ = 1.0f / mF00->mF08[3];
    const float range = mF00->mF20[1] - mF00->mF20[0];
    mInvHeightRange = range == 0.0f ? 0.0625f : 1.0f / range;
    mF12968 = 0;
    if ((gei_Display.mFlags & 0x10000) != 0) {
        if (mF12970 != nullptr) gFree(mF12970);
        const uint32_t bytes = mF00->mHeightCount * sizeof(float);
        mF12970 = gMalloc(bytes);
        if (mF12970 != nullptr) {
            std::memcpy(mF12970, mF00->mHeights.get(), bytes);
            AdjustHField();
        }
    }
    InitGamut();
}

int32_t CHField::Close() {
    if (mProcTextures != nullptr) {
        mProcTextures->Close();
        delete mProcTextures;
        mProcTextures = nullptr;
    }
    if (mF12970 != nullptr) gFree(mF12970);
    mF00 = 0;
    mF04 = 0;
    return CloseGamut();
}

// CHField::LoadProcTextures (engine 0x1000d96c, 480 bytes).

int32_t CHField::LoadProcTextures(char* name, int32_t size) {
    if (gei_System.mRenderer != 1 && gei_System.mRenderer != 3) return 1;
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new (std::nothrow) SamsUtilities::ULogManager;
    SamsUtilities::gLogger->SetContext("engine", "tb_hfield.cpp", 0x139, 0x80000000);
    if (SamsUtilities::gLogger == nullptr) SamsUtilities::gLogger = new (std::nothrow) SamsUtilities::ULogManager;
    SamsUtilities::gLogger->Log("Attempting to initialize procedural textures");
    if (name == nullptr) return 1;
    if (mProcTextures != nullptr) {
        delete mProcTextures;
        mProcTextures = nullptr;
    }
    if (mProcTextures == nullptr) {
        mProcTextures = new (std::nothrow) CProcTextureHandler;
        if (mProcTextures == nullptr) {
            gei_ErrorHandler.Push(2, 0xb0754000u);
            return 0;
        }
    }
    if (mProcTextures->Init(gei_TextureSettings.mProcTextureParam, size) == 0) return 0;
    if (!mProcTextures->Load(name)) {
        delete mProcTextures;
        mProcTextures = nullptr;
        return 0;
    }
    return 1;
}

// The engine height field's procedural texture window setter (engine symbols shared, tb_hfield.cpp range; unit w112a).

// 0x1000db80 (20 bytes): four stores, columns x0..x1 and rows y0..y1 of the window (PT_SetRange's callee).
void CHField::ProcTextureRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    mF234 = x0;
    mF238 = y0;
    mF23C = x1;
    mF240 = y1;
}

int32_t CHField::InitGamut() {
    const sHField& field = *mF00;
    gei_Gamut->SetX(field.mF08[0], ppc_fmadds(static_cast<float>(field.mF18[0]), field.mF08[2], field.mF08[0]), field.mF08[2]);
    gei_Gamut->SetZ(field.mF08[1], ppc_fmadds(static_cast<float>(field.mF18[1]), field.mF08[3], field.mF08[1]), field.mF08[3]);
    gei_Gamut->SetYClipper(field.mF20[0], field.mF20[1]);
    gei_Gamut->CalcFurthestDistance();
    gei_MapWho->Init(field.mF18[0], field.mF18[1]);
    return 1;
}

int32_t CHField::CloseGamut() { return 1; }

// tb_hfield_conquad.cpp (engine library of SimTheme Park 2.04b3; the inventory's tb_hfield.cpp
// range): CHField::TestConQuad 0x1000dd14 (1,024 bytes).
//
// A first-person view (the lobby's) finds the cells it sees with CHField::DivideAndConquer, which
// hands boxes of grid cells to TestConQuad. A box is the cells x0..x1 by z0..z1; TestConQuad
// builds the eight corners of the block of space above it (the height field's lowest and highest
// heights over the box's four corners), transforms them by the final matrix and asks the clipper
// for their outside codes. When all eight lie outside one plane the box is out of view and
// nothing happens. When all eight are inside, or the box is a single cell, the rows of grid
// points at +0x20 are widened to take the box and mF1C (the far plane the projection will use)
// is raised to the farthest depth the corners reached. Otherwise the box is cut in four at the
// middle and each part is tested in turn.

void CHField::TestConQuad(int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    const sHField& field = *mF00;
    // The cell corners in world space: the position is fused (cell * size + origin), the cell
    // number is converted to single precision first. mF08 is the origin x, z (0, 1) and the
    // cell size x, z (2, 3).
    const float worldX0 = ppc_fmadds(static_cast<float>(x0), field.mF08[2], field.mF08[0]);
    const float worldZ0 = ppc_fmadds(static_cast<float>(z0), field.mF08[3], field.mF08[1]);
    const float worldX1 = ppc_fmadds(static_cast<float>(x1), field.mF08[2], field.mF08[0]);
    const float worldZ1 = ppc_fmadds(static_cast<float>(z1), field.mF08[3], field.mF08[1]);
    const float low = field.mF20[0];
    const float high = field.mF20[1];
    // The eight corners, x, y, z each: the lowest height first (z0 then z1 rows), then the highest.
    float corners[8][3] = {
        {worldX0, low, worldZ0},  {worldX1, low, worldZ0},  {worldX0, low, worldZ1},  {worldX1, low, worldZ1},
        {worldX0, high, worldZ0}, {worldX1, high, worldZ0}, {worldX0, high, worldZ1}, {worldX1, high, worldZ1},
    };
    // The product of the box's two extents, 32 bits: 1 is a single cell.
    const int32_t extent = static_cast<int32_t>(static_cast<uint32_t>(x1 - x0) * static_cast<uint32_t>(z1 - z0));

    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    SVertex transformed[8];
    gei_MathUnit->Transform(gei_FinalMatrix, transformed, corners, 0x10, 0xc, 8, 0);
    // The clipper's answer: [0] is the AND of the eight outside codes, [1] their OR.
    uint8_t answer[4] = {0xff, 0, 0, 0};
    uint8_t codes[8];
    CClipping::CPP_TestData(answer, gei_Clipping, codes, transformed, 8, 0x10, 0xff);
    if (answer[0] != 0) return;

    if (answer[1] == 0 || extent == 1) {
        // In view whole, or too small to cut: the rows z0..z1 reach out to the box (the columns
        // are bytes in the object, compared as ints against the full column number).
        SGamutQuads* rows = VertexRows();
        if (mF20 > z0) mF20 = z0;
        if (mF24 < z1) mF24 = z1;
        for (int32_t z = z0; z <= z1; ++z) {
            if (rows->mLow[z] > x0) rows->mLow[z] = static_cast<int8_t>(x0);
            if (rows->mHigh[z] < x1) rows->mHigh[z] = static_cast<int8_t>(x1);
        }
        // mF1C follows the depth (the third word) of each corner, farthest wins; a NaN never does.
        for (const SVertex& corner : transformed) {
            if (mF1C < corner.z) mF1C = corner.z;
        }
        return;
    }

    // Partly in view: the four quarters, each only when it has some width and depth.
    const int32_t middleX = static_cast<int32_t>(static_cast<uint32_t>(x0) + static_cast<uint32_t>(x1)) >> 1;
    const int32_t middleZ = static_cast<int32_t>(static_cast<uint32_t>(z0) + static_cast<uint32_t>(z1)) >> 1;
    if (middleX > x0) {
        if (middleZ > z0) TestConQuad(x0, z0, middleX, middleZ);
        if (z1 > middleZ) TestConQuad(x0, middleZ, middleX, z1);
    }
    if (x1 > middleX) {
        if (middleZ > z0) TestConQuad(middleX, z0, x1, middleZ);
        if (z1 > middleZ) TestConQuad(middleX, middleZ, x1, z1);
    }
}

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

sTileOverlay gTileOverlays[CHField::kMaxOverlays];

namespace {

// A cell's first word. Bit 0 hides the cell; 0x80 marks a procedural cell that carries a
// picture to put over the procedural texture; 0x100 asks the handler to make the tile again
// (Prepare takes it off and hands the old flags on).
constexpr uint16_t kCellHidden = 0x0001;
constexpr uint16_t kCellOverlay = 0x0080;
constexpr uint16_t kCellRegenerate = 0x0100;

// Bytes per pixel by texture format, this file's own copy of the table (engine data
// 0x1005179c; tb_loaders.cpp has another, with the same values).
constexpr int32_t kBytesPerPixel[6] = {2, 2, 2, 4, 2, 4};

// What a picture's priority is raised to when Prepare asks for it.
constexpr float kPicturePriority = 0.5f;

// DivideAndConquer's constants (engine data 0x1004fc14..0x1004fc28).
constexpr float kViewDistanceNoFog = 460.0f;   // 0x43e60000
constexpr float kProjectionFar = 1000.0f;      // 0x447a0000
constexpr float kFarStart = 100.0f;            // 0x42c80000
constexpr float kHalf = 0.5f;                  // 0x3f000000
constexpr float kAlmostOne = 0.999f;           // 0x3f7fbe77
constexpr float kFive = 5.0f;                  // 0x40a00000

// The rows of grid points start out empty: low columns 0x7f, high columns -1, and the row
// range 0x7f .. -1.
constexpr int8_t kEmptyLow = 0x7f;
constexpr int8_t kEmptyHigh = -1;

// A float as the int the processor's fctiwz makes of it.
int32_t Truncate(float value) { return ppc_fctiwz(static_cast<double>(value)); }

// A cell number as the 8-bit range the display keeps.
int32_t ClampToByte(int32_t value) {
    if (value < -0x80) return -0x80;
    if (value > 0x7f) return 0x7f;
    return value;
}

// A cell number as an index into the map: the original's clamp for its upper bound is
// unsigned, so a negative number never gets there (it is made 0 first).
int32_t ClampToMap(int32_t value, uint32_t count) {
    if (value < 0) return 0;
    if (static_cast<uint32_t>(value) >= count) return static_cast<int32_t>(count) - 1;
    return value;
}

// The low and high columns of a block of rows, indexed as the original does it: from the
// block's bytes, the low columns from byte 8 and the high columns 128 bytes after, so a row
// one past the end of the low columns is the first of the high ones and row -1 of the high
// columns is the last of the low ones. (An index into the arrays themselves would leave their
// bounds.)
class Columns {
public:
    explicit Columns(SGamutQuads& rows) : mBytes(reinterpret_cast<int8_t*>(&rows)) {}
    int8_t& Low(int32_t row) { return mBytes[offsetof(SGamutQuads, mLow) + row]; }
    int8_t& High(int32_t row) { return mBytes[offsetof(SGamutQuads, mHigh) + row]; }

private:
    int8_t* mBytes;
};

}  // namespace

int32_t CHField::DivideAndConquer() {
    const float viewDistance =
        (gei_Display.mFlags & CDisplay::kEnableFog) != 0 ? gei_Display.mFirstPersonViewDistance : kViewDistanceNoFog;
    gei_ProjectionMatrix.AdjustProjection(kProjectionFar);
    PreprocessMatrices();
    mF1C = kFarStart;
    SGamutQuads& vertices = *VertexRows();
    Columns vertexColumns(vertices);
    vertices.mFirstRow = 0x7f;
    vertices.mEndRow = -1;
    std::memset(vertices.mLow, kEmptyLow, sizeof vertices.mLow);
    std::memset(vertices.mHigh, static_cast<uint8_t>(kEmptyHigh), sizeof vertices.mHigh);
    if ((gei_Display.mFlags & CDisplay::kTestWholeMap) == 0) {
        // The cells within half the view distance (plus a cell) of the camera.
        const float half = viewDistance * kHalf;
        const float cameraX = gei_ViewMatrix->m[3][0];
        const float cameraZ = gei_ViewMatrix->m[3][2];
        const int32_t width = static_cast<int32_t>(mF00->mF18[0]);
        const int32_t depth = static_cast<int32_t>(mF00->mF18[1]);
        int32_t x0 = Truncate(mInvCellX * (cameraX - half));
        int32_t x1 = Truncate(mInvCellX * (kAlmostOne + (cameraX + half)));
        int32_t z0 = Truncate(mInvCellZ * (cameraZ - half));
        int32_t z1 = Truncate(mInvCellZ * (kAlmostOne + (cameraZ + half)));
        tpw_probe::Set(tpw_probe::kDacX0, x0);
        tpw_probe::Set(tpw_probe::kDacX1, x1);
        tpw_probe::Set(tpw_probe::kDacZ0, z0);
        tpw_probe::Set(tpw_probe::kDacZ1, z1);
        tpw_probe::Set(tpw_probe::kDacWidth, width);
        tpw_probe::Set(tpw_probe::kDacDepth, depth);
        // Wholly outside the map: nothing is in view and the cells in view stay as they were.
        if (x0 >= width || z0 >= depth || x1 < 0 || z1 < 0) {
            tpw_probe::Hit(tpw_probe::kDacEarlyOut);
            return 1;
        }
        if (x0 < 0) x0 = 0;
        if (z0 < 0) z0 = 0;
        if (x1 >= width) x1 = width;
        if (z1 >= depth) z1 = depth;
        TestConQuad(x0, z0, x1, z1);

        // A block of cells about eleven wide is always added: centred on the camera moved by
        // five times the view matrix's x and z parts of its third column (added to a position
        // in cells, as the original does), five cells either way.
        const sHField& field = *mF00;
        const CMatrix& view = *gei_ViewMatrix;
        const float centreColumn = kFive * view.m[0][2] + cameraX / field.mF08[2];
        const float centreRow = kFive * view.m[2][2] + cameraZ / field.mF08[3];
        // The row of the block's far edge divides the camera's z by the cell width (kept).
        const float centreRowFar = kFive * view.m[2][2] + cameraZ / field.mF08[2];
        const int32_t columnLow = ClampToMap(Truncate(centreColumn - kFive), field.mF18[0]);
        const int32_t columnHigh = ClampToMap(Truncate(kFive + centreColumn), field.mF18[0]);
        const int32_t rowLow = ClampToMap(Truncate(centreRow - kFive), field.mF18[1]);
        const int32_t rowHigh = ClampToMap(Truncate(kFive + centreRowFar), field.mF18[1]);
        if (vertices.mFirstRow > rowLow) vertices.mFirstRow = rowLow;
        if (vertices.mEndRow < rowHigh + 1) vertices.mEndRow = rowHigh + 1;
        // Rows rowLow .. rowHigh + 1 take the columns, stored as bytes and compared as ints.
        for (int32_t row = rowLow; row <= rowHigh + 1; row++) {
            if (vertexColumns.Low(row) > columnLow) vertexColumns.Low(row) = static_cast<int8_t>(columnLow);
            if (vertexColumns.High(row) < columnHigh + 1) vertexColumns.High(row) = static_cast<int8_t>(columnHigh + 1);
        }
    } else {
        TestConQuad(0, 0, static_cast<int32_t>(mF00->mF18[0]), static_cast<int32_t>(mF00->mF18[1]));
    }

    // Rows of grid points no box touched take the row before them. (For row 0 that is the byte
    // before the low columns, the low byte of the end row on the big-endian Mac.)
    SGamutQuads& quads = *QuadRows();
    Columns quadColumns(quads);
    quads.mFirstRow = vertices.mFirstRow;
    quads.mEndRow = vertices.mEndRow;
    tpw_probe::Set(tpw_probe::kRowsFirst, quads.mFirstRow);
    tpw_probe::Set(tpw_probe::kRowsEnd, quads.mEndRow);
    for (int32_t row = vertices.mFirstRow; row <= vertices.mEndRow; row++) {
        if (static_cast<uint8_t>(vertexColumns.Low(row)) == 0x7f)
            vertexColumns.Low(row) = row != 0 ? vertexColumns.Low(row - 1) : static_cast<int8_t>(vertices.mEndRow);
        if (vertexColumns.High(row) == -1) vertexColumns.High(row) = vertexColumns.High(row - 1);
    }
    // A cell needs both of its grid points' rows: the narrower columns of the two.
    for (int32_t row = quads.mFirstRow; row <= quads.mEndRow; row++) {
        quadColumns.Low(row) = vertexColumns.Low(row);
        if (vertexColumns.Low(row) < vertexColumns.Low(row + 1)) quadColumns.Low(row) = vertexColumns.Low(row + 1);
        quadColumns.High(row) = vertexColumns.High(row);
        if (vertexColumns.High(row) > vertexColumns.High(row + 1)) quadColumns.High(row) = vertexColumns.High(row + 1);
    }

    // The projection gets the far plane the boxes in view needed.
    gei_ProjectionMatrix.AdjustProjection(mF1C);
    // The cells around the camera for the procedural textures, relative to the map's origin.
    const float half = viewDistance * kHalf;
    const float cameraX = gei_ViewMatrix->m[3][0];
    const float cameraZ = gei_ViewMatrix->m[3][2];
    const float originX = mF00->mF08[0];
    const float originZ = mF00->mF08[1];
    gei_Display.mF928 = ClampToByte(Truncate(mInvCellX * ((cameraX - half) - originX)));
    gei_Display.mF92C = ClampToByte(Truncate(mInvCellX * ((half + cameraX) - originX)));
    gei_Display.mF930 = ClampToByte(Truncate(mInvCellZ * ((cameraZ - half) - originZ)));
    gei_Display.mF934 = ClampToByte(Truncate(mInvCellZ * ((half + cameraZ) - originZ)));
    return 1;
}

// tb_hfield_helpers.cpp: the height field's helpers that CHField::Execute and the culling
// call (engine tb_hfield.cpp range, 0x1000e760..0x1000f658), and the face sorter of the
// draw primitive (QSortFaces_float, 0x10006d40, which the inventory files under tb_error.cpp).
//
//   0x1000e760 CHField::CheckGamutArea     (148 bytes)
//   0x1000e824 CHField::CalcFullLighting   (492)
//   0x1000ea40 CHField::CalcLine           (672)
//   0x1000f060 CHField::QSort_textures     (284)
//   0x1000f1b0 CHField::GetTexturesUsed    (516)
//   0x1000f3f8 CHField::ProcessTextureParameters (172)
//   0x1000f4ec CHField::qSortTextures_lpTex (364)
//   0x10006d40 QSortFaces_float            (324)

namespace {

// A page (or any pointer) as the Mac compares it: unsigned.
uintptr_t Address(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

}  // namespace

// Whether any row of the area's rows (area[1] to area[0]) has cells in view that reach the
// area's columns (area[3] to area[2]); `area` is a node's mF90. The rows in view are only
// compared with the area's first and last row once; every row the area names is then looked
// at, in view or not, so a row outside 0..127 reads whatever lies next to the row bytes.
int32_t CHField::CheckGamutArea(int8_t* area) {
    const int32_t lastRow = area[0];
    if (mF128 > lastRow) return 0;
    const int32_t firstRow = area[1];
    if (mF12C < firstRow) return 0;
    const SGamutQuads* cells = QuadRows();
    for (int32_t row = firstRow; row <= lastRow; ++row) {
        if (cells->mLow[row] <= area[2] && cells->mHigh[row] >= area[3]) return 1;
    }
    return 0;
}

// The colour of every grid point of the rows 0..mGridHeight - 1 (the whole grid, not only
// the points in view), once for each change of the lights. A row is built on the stack as
// 128 positions and normals, then CLightManager::CalcIntensities fills the row's colours.
// Quirks kept: the position steps by the origin's x (sHField mF08[0]) and not by the cell
// size, while z steps by the cell size; the last row's z slope reads the row 128 floats
// back, not mGridWidth floats; the arrays hold 128 points, as the stack frame does.
void CHField::CalcFullLighting() {
    if (mF12968 == gei_System.mLightFrame && (gei_System.mFlags & CSystem::kLightsChanged) == 0) return;
    const sHField& field = *mF00;
    const float* heights = field.mHeights;
    SVertex vertices[128];
    D3DVECTOR normals[128];
    float z = field.mF08[1];
    for (int32_t row = 0; row < mGridHeight; ++row) {
        float x = field.mF08[0];
        for (int32_t column = 0; column < mGridWidth; ++column) {
            vertices[column].x = x;
            vertices[column].y = heights[0];
            vertices[column].z = z;
            float slopeX;
            if (column <= 0) {
                slopeX = heights[0] - heights[1];
            } else if (column < mGridWidth - 1) {
                slopeX = heights[-1] - heights[1];
            } else {
                slopeX = heights[-1] - heights[0];
            }
            normals[column].x = slopeX * mInvHeightRange;
            normals[column].y = 1.0f;
            float slopeZ;
            if (row <= 0) {
                slopeZ = heights[0] - heights[mGridWidth];
            } else if (row < mGridHeight - 1) {
                slopeZ = heights[-mGridWidth] - heights[mGridWidth];
            } else {
                slopeZ = heights[-0x80] - heights[0];
            }
            normals[column].z = slopeZ * mInvHeightRange;
            ++heights;
            x += field.mF08[0];
        }
        gei_LightManager->CalcIntensities(mVertexColours[row], normals, vertices, mGridWidth);
        z += field.mF08[3];
    }
    mF12968 = gei_System.mLightFrame;
}

// Grid row `row`, columns low..high: the world position of each point (x = column * cell + origin,
// z = row * cell + origin, both fused; y is the height added to 0.0, which turns -0.0 into
// +0.0), transformed in place by the final matrix, then the outside code of each point (the
// clipper's test, mask 0xff) into codes[low..high]. Nothing happens when low is 127 or more.
// The third array is never read; the w word of a point is whatever Transform makes of it. An
// empty range (low above high) still transforms and tests with a count of 0 or less.
void CHField::CalcLine(uint8_t* codes, SVertex* vertices, uint32_t* spare, int32_t low, int32_t high, int32_t row) {
    if (low >= 127) return;
    const sHField& field = *mF00;
    SVertex* first = vertices + low;
    const float* heights = field.mHeights;
    const int32_t rowStart = mGridWidth * row;
    const float z = ppc_fmadds(static_cast<float>(row), field.mF08[3], field.mF08[1]);
    for (int32_t column = low; column <= high; ++column) {
        SVertex& point = vertices[column];
        point.x = ppc_fmadds(static_cast<float>(column), field.mF08[2], field.mF08[0]);
        point.y = 0.0f + heights[column + rowStart];
        point.z = z;
    }
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    const int32_t count = high - low + 1;
    gei_MathUnit->Transform(gei_FinalMatrix, first, first, 0x10, 0x10, count, 0);
    uint8_t unused[4];   // the clipper's answer (the AND and the OR of the codes): not used
    CClipping::CPP_TestData(unused, gei_Clipping, codes + low, first, count, 0x10, 0xff);
}

int32_t CHField::Prepare() {
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) == 0) return 1;
    // Not first person: the cells and grid points in view are the gamut's.
    if ((gei_System.mFlags & CSystem::kFirstPerson) == 0) {
        std::memcpy(QuadRows(), gei_Gamut->GetQuads(), sizeof(SGamutQuads));
        std::memcpy(VertexRows(), gei_Gamut->GetVertices(), sizeof(SGamutQuads));
    }
    SGamutQuads& quads = *QuadRows();
    Columns quadColumns(quads);
    if (quads.mFirstRow > quads.mEndRow) return 1;
    // Procedural textures need their handler.
    if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesOn) != 0 && mProcTextures == nullptr)
        gei_TextureSettings.mFlags &= ~CTextureSettings::kProcTexturesOn;
    int32_t listed = 0;
    for (int32_t row = quads.mFirstRow; row < quads.mEndRow; row++) {
        for (int32_t column = quadColumns.Low(row); column < quadColumns.High(row); column++) {
            // No bounds check, as in the original.
            sHFieldQuad* cell =
                &mF00->mQuads[static_cast<int32_t>(static_cast<uint32_t>(column) +
                                                   static_cast<uint32_t>(row) * mF00->mF18[0])];
            tpw_probe::Hit(tpw_probe::kPrepareCells);
            if ((cell->mF00 & kCellHidden) != 0) {
                tpw_probe::Hit(tpw_probe::kPrepareHidden);
                continue;
            }
            bool procedural = false;
            if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesOn) != 0)
                procedural = column >= mF234 && column <= mF23C && row >= mF238 && row <= mF240;
            if (procedural) {
                if (listed >= kMaxOverlays) continue;
                mPrepareEntries[listed].mTile =
                    static_cast<uint32_t>((row - mF238) * 0x100 + (column - mF234));
                sTileOverlay& overlay = gTileOverlays[listed];
                overlay.mFlags = cell->mF00;
                cell->mF00 &= static_cast<uint16_t>(~kCellRegenerate);
                if ((cell->mF00 & kCellOverlay) != 0) {
                    CTextureEntry* entry = mF04[cell->mF02].mTexture->GetTextureEntry(0);
                    overlay.mData = entry;
                    overlay.mWidth = entry->mWidth;
                    overlay.mHeight = entry->mHeight;
                    overlay.mFormat = kBytesPerPixel[gei_TextureSettings.mFormat];
                } else {
                    overlay.mData = nullptr;
                }
                listed++;
            } else {
                CsTexture* texture = mF04[cell->mF02].mTexture;
                if (texture == nullptr) {
                    tpw_probe::Hit(tpw_probe::kPrepareNoTexture);
                    continue;
                }
                CTextureEntry* entry = texture->GetTextureEntry(0);
                if (entry != nullptr) entry->Lock();
                gei_TCache->Prepare(entry, kPicturePriority);
            }
        }
    }
    if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesOn) != 0 && listed > 0)
        mProcTextures->PrepareList(mPrepareEntries, gTileOverlays, listed);
    mF1296C = listed;
    return 1;
}

// Sorts the pages by address (unsigned), the quicksort of the original: the pivot is the
// middle element, the first scan runs up while the element is below it. QUIRK: the second scan
// runs down while the element is above the pivot and the FIRST index is above `low` (the
// natural test is the second index), so when the first scan stops at `low` the second does
// not run and the swap takes the last element, and the result can come out unsorted.
// An empty range does nothing; the original reads textures[(low + high) >> 1] first.
void CHField::QSort_textures(void** textures, int32_t low, int32_t high) {
    if (high < low) return;
    int32_t i = low;
    int32_t j = high;
    const uintptr_t pivot = Address(textures[Middle(low, high)]);
    do {
        while (i <= high && Address(textures[i]) < pivot) ++i;
        while (low < i && Address(textures[j]) > pivot) --j;
        if (i > j) break;
        std::swap(textures[i], textures[j]);
        --j;
        ++i;
    } while (i <= j);
    if (low < j) QSort_textures(textures, low, j);
    if (i < high) QSort_textures(textures, i, high);
}

// For every cell in view (rows mF128 .. mF12C - 1 of the SGamutQuads at +0x128, columns mLow ..
// mHigh - 1 of each) whose quad does not have flag 1: its texture entry goes to entries[],
// in the order of the cells, and its page (the entry's texture while it is on a cache page,
// else NULL) to textures[] unless that is in the list already; count is the length of the
// list, which is sorted at the end. The entry is the next of the procedural texture entries
// (mPrepareEntries, which PrepareList filled) when texture setting 4 is on and the cell is inside the rectangle
// mF234..mF23C by mF238..mF240, else the first entry of the mesh texture the quad names
// (NULL without a texture object).
void CHField::GetTexturesUsed(void** textures, int32_t& count, CTextureEntry** entries) {
    int32_t entryCount = 0;
    int32_t procCount = 0;
    count = 0;
    const SGamutQuads* cells = QuadRows();
    const sHField& field = *mF00;
    for (int32_t z = mF128; z < mF12C; ++z) {
        for (int32_t x = cells->mLow[z]; x < cells->mHigh[z]; ++x) {
            const sHFieldQuad& quad = field.mQuads[x + z * static_cast<int32_t>(field.mF18[0])];
            if ((quad.mF00 & 1) != 0) continue;
            bool procedural = false;
            if ((gei_TextureSettings.mFlags & CTextureSettings::kProcTexturesOn) != 0) {
                procedural = x >= mF234 && x <= mF23C && z >= mF238 && z <= mF240;
            }
            CTextureEntry* entry;
            if (procedural) {
                entry = mPrepareEntries[procCount++].mTexture;
            } else {
                CsTexture* texture = mF04[quad.mF02].mTexture;
                entry = texture != nullptr ? texture->GetTextureEntry(0) : nullptr;
            }
            entries[entryCount++] = entry;
            void* page = entry != nullptr ? entry->GetTexture() : nullptr;
            int32_t found = 0;
            while (found < count && textures[found] != page) ++found;
            if (found == count) {
                textures[found] = page;
                ++count;
            }
        }
    }
    QSort_textures(textures, 0, count - 1);
}

// Turns the four corner numbers of a cell (one byte each, in memory order) by the quad's
// flags: 0x40 first swaps the second and fourth; then 0x08 rotates the four one place right
// (the last comes first), else 0x10 swaps the first with the third and the second with the
// fourth, else 0x20 rotates one place left (the first goes last). No flag among 0x78: no change.
void CHField::ProcessTextureParameters(uint32_t* corners, sHFieldQuad* quad) {
    if ((quad->mF00 & 0x78) == 0) return;
    uint8_t* c = reinterpret_cast<uint8_t*>(corners);
    if ((quad->mF00 & 0x40) != 0) std::swap(c[1], c[3]);
    if ((quad->mF00 & 0x08) != 0) {
        const uint8_t last = c[3];
        c[3] = c[2];
        c[2] = c[1];
        c[1] = c[0];
        c[0] = last;
        return;
    }
    if ((quad->mF00 & 0x10) != 0) {
        std::swap(c[0], c[2]);
        std::swap(c[1], c[3]);
        return;
    }
    if ((quad->mF00 & 0x20) != 0) {
        const uint8_t first = c[0];
        c[0] = c[1];
        c[1] = c[2];
        c[2] = c[3];
        c[3] = first;
    }
}

// Sorts the quads Execute drew by their page (the first word, unsigned): QSort_textures'
// quicksort with the natural second scan (the second index above `low`), moving whole records.
// An empty range does nothing (the original reads quads[(low + high) >> 1] first).
void CHField::qSortTextures_lpTex(QuadSaveInfo* quads, int32_t low, int32_t high) {
    if (high < low) return;
    int32_t i = low;
    int32_t j = high;
    const uintptr_t pivot = Address(quads[Middle(low, high)].mTexture);
    do {
        while (i <= high && Address(quads[i].mTexture) < pivot) ++i;
        while (j > low && Address(quads[j].mTexture) > pivot) --j;
        if (i > j) break;
        std::swap(quads[i], quads[j]);
        --j;
        ++i;
    } while (i <= j);
    if (low < j) qSortTextures_lpTex(quads, low, j);
    if (i < high) qSortTextures_lpTex(quads, i, high);
}

// CHField::Execute (engine 0x1000f69c, 6,072 bytes; the inventory's tb_hfield.cpp range): the
// height field's drawing, called by CMapWho::RenderWindow after the sky.
//
// Only when the view or the lights changed (system flags 0xc0000000; otherwise the static
// primitive of the last such frame is still in the render memory): the lights are prepared,
// every grid point lit (CalcFullLighting), the texture entry of every cell in view found
// (GetTexturesUsed), then the cells in view are walked row by row with two rows of transformed
// grid points (CalcLine) at hand. A cell with quad flag 1 is skipped; one whose four corners
// are all outside one plane is not drawn; the others become four vertices in one primitive
// (format 0x1c4): kept whole when both triangles face the viewer, one triangle when only one
// does, or, when a corner is outside, each facing triangle clipped by CClipping::ClipPoly.
// Each drawn cell leaves a QuadSaveInfo; they are sorted by page, and each run of cells on the
// same page gets the primitive (the first run) or a duplicate of it, its triangles as index
// fans and an execution with depth vertices * 2,000,000.
//
// Every a*b+c written with ppc_fmadds (the texture coordinates) and the facing test's
// ppc_fmsubs (tb_drawprimitive.h) are fused in the original.
//
// Quirks kept: the cell's corner 1 and 2 colours and points come from the next grid row, read
// through mVertexColours[row + 1] and the next CalcLine row with no bound check (row 127 would
// read past both); the specular word of every vertex is never written (the primitive keeps
// what its memory held; the clipped corners are copied to ClipPoly with stack garbage there);
// CalcLine's third array is copied along with the other two every row although neither CalcLine
// nor Execute reads it (the vertices take mVertexColours); when only the second triangle faces the viewer the four
// vertices are moved down by one with an overlapping memcpy (memmove here, which gives the
// forward copy's result); every primitive gets the vertex count of the whole height field.

CTextureEntry* gQuadTextures[0x4000];
QuadSaveInfo gQuadSaveInfo[0x4000];
float gFaceX0, gFaceX1, gFaceX2, gFaceY0, gFaceY1, gFaceY2, gFaceW0, gFaceW1, gFaceW2;

namespace {

// The vertex size of format 0x1c4 (engine data 0x100517b8, the 0x1c4 entry of the vertex size
// table at 0x100517b4).
constexpr int32_t kVertexSize = 0x20;
static_assert(kVertexSize == sizeof(SFVF_TLCTEX), "one SFVF_TLCTEX a vertex");

// The texture coordinates of a cell's corners 0..3 (a local array in the original, copied
// from engine data 0x100517c4); ProcessTextureParameters chooses which corner gets which.
constexpr float kCornerUV[4][2] = {{0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}};

// The part of a texture entry the vertices use: its place on its page.
struct TexturePlace {
    float mUSize, mVSize, mUOffset, mVOffset;
};

// One corner of a cell: its transformed point (four words from CalcLine), its colour and the
// corner number ProcessTextureParameters left for it.
struct Corner {
    const float* mPoint;
    uint32_t mColour;
    uint8_t mUV;
};

// A vertex of the cell: the point copied as four words, the colour (halved and without its
// low byte for a cell with quad flag 0x400, unless the display has flag 0x800), the texture
// coordinates of the corner placed on the page. The specular word stays as it was.
void PutVertex(SFVF_TLCTEX& out, const Corner& corner, bool shaded, const TexturePlace& place) {
    std::memcpy(&out.x, corner.mPoint, 4 * sizeof(float));
    out.colour = shaded ? (corner.mColour >> 1) & 0x7f7f7f00u : corner.mColour;
    out.u = ppc_fmadds(place.mUSize, kCornerUV[corner.mUV][0], place.mUOffset);
    out.v = ppc_fmadds(place.mVSize, kCornerUV[corner.mUV][1], place.mVOffset);
}

// The cell's four vertices from vertices[0] on: corners 0, 1, 2, 3 with quad flag 2, else
// 1, 2, 3, 0 (the diagonal between the two triangles changes).
void PutCell(SFVF_TLCTEX* vertices, const Corner (&corners)[4], const sHFieldQuad& quad, const TexturePlace& place) {
    const int32_t start = (quad.mF00 & 2) != 0 ? 0 : 1;
    for (int32_t k = 0; k < 4; ++k) {
        const bool shaded = (gei_Display.mFlags & CDisplay::kFlag800) == 0 && (quad.mF00 & 0x400) != 0;
        PutVertex(vertices[k], corners[(start + k) & 3], shaded, place);
    }
}

// The triangles of a polygon of `count` vertices from vertex `first` as a fan, written from
// the last triangle back: (first, first + k + 1, first + k + 2) for k = count - 3 down to 0.
void PutFan(uint16_t*& out, int32_t first, int32_t count) {
    for (int32_t k = count - 2; k > 0;) {
        --k;
        out[0] = static_cast<uint16_t>(first);
        out[1] = static_cast<uint16_t>(first + k + 1);
        out[2] = static_cast<uint16_t>(first + k + 2);
        out += 3;
    }
}

// Notes a drawn cell.
void SaveQuad(int32_t index, CTextureBase* page, int32_t first, const CTextureEntry* entry, int32_t count1,
              int32_t count2) {
    QuadSaveInfo& info = gQuadSaveInfo[index];
    info.mTexture = page;
    info.mF08 = entry != nullptr ? entry->mF40 : -1;
    info.mFirstVertex = first;
    info.mCount1 = static_cast<int16_t>(count1);
    info.mCount2 = static_cast<int16_t>(count2);
}

}  // namespace

int32_t CHField::Execute() {
    SGamutQuads& cells = *QuadRows();
    SGamutQuads& points = *VertexRows();
    if (cells.mFirstRow > cells.mEndRow) return 1;
    if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) == 0) {
        tpw_probe::Hit(tpw_probe::kHFieldExecuteSkipped);
        return 1;
    }
    tpw_probe::Hit(tpw_probe::kHFieldExecute);

    int32_t saved = 0;   // QuadSaveInfo records written
    gev_LightChain = nullptr;
    gei_MapWho->mF14[6]->PrepareLights();
    sAlignedBox box;     // not initialised (PrepareLighting does not read it)
    gei_LightManager->PrepareLighting(&gei_LightMatrix, &box);
    CalcFullLighting();
    void* textures[1024];
    textures[0] = nullptr;   // the original passes whatever is there when GetTexturesUsed lists no page
    int32_t textureCount;
    GetTexturesUsed(textures, textureCount, gQuadTextures);
    tpw_probe::Hit(tpw_probe::kHFieldTextures, static_cast<uint32_t>(textureCount));
    int32_t mode = 0x1256;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) mode |= 0x100000;
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(1, 4, 0x1c4, mode, textures[0]);
    if (primitive == nullptr) {
        tpw_probe::Hit(tpw_probe::kHFieldPrimitiveNull);
        return 1;
    }

    // Two grid rows at a time: [0] the cells' top row, [1] the row below (contiguous in the
    // original, which reads [1] as [0] + 0x80 and so on). `spare` is CalcLine's unused array.
    uint8_t codes[2][128];
    float vertices[2][128][4];
    uint32_t spare[2][128];
    int32_t used = 0;    // vertices written
    int32_t row = cells.mFirstRow;
    CalcLine(codes[1], reinterpret_cast<SVertex*>(vertices[1]), spare[1], points.mLow[row], points.mHigh[row], row);
    CTextureEntry** texture = gQuadTextures;   // one entry for each cell whose flag 1 is clear
    for (; row < cells.mEndRow; ++row) {
        std::memcpy(vertices[0], vertices[1], sizeof vertices[1]);
        std::memcpy(codes[0], codes[1], sizeof codes[1]);
        std::memcpy(spare[0], spare[1], sizeof spare[1]);
        CalcLine(codes[1], reinterpret_cast<SVertex*>(vertices[1]), spare[1], points.mLow[row + 1],
                 points.mHigh[row + 1], row + 1);
        for (int32_t cell = cells.mLow[row]; cell < cells.mHigh[row]; ++cell) {
            sHFieldQuad* quad = &mF00->mQuads.get()[cell + row * static_cast<int32_t>(mF00->mF18[0])];
            if ((quad->mF00 & 1) != 0) continue;
            if ((codes[0][cell] & codes[0][cell + 1] & codes[1][cell + 1] & codes[1][cell]) == 0) {
                // The corner numbers, a byte each in memory order (the Mac's word 0x00010203).
                uint32_t cornerWord;
                uint8_t* order = reinterpret_cast<uint8_t*>(&cornerWord);
                order[0] = 0;
                order[1] = 1;
                order[2] = 2;
                order[3] = 3;
                ProcessTextureParameters(&cornerWord, quad);
                CTextureEntry* entry = *texture;
                CTextureBase* page;
                TexturePlace place;
                if (entry == nullptr) {
                    page = nullptr;
                    place = {0.0f, 0.0f, 0.0f, 0.0f};
                } else {
                    page = entry->GetTexture();
                    place = {entry->mUSize, entry->mVSize, entry->mUOffset, entry->mVOffset};
                }
                const int32_t outside = codes[0][cell] | codes[0][cell + 1] | codes[1][cell + 1] | codes[1][cell];
                const Corner corners[4] = {
                    {vertices[0][cell], mVertexColours[row][cell], order[0]},
                    {vertices[1][cell], mVertexColours[row + 1][cell], order[1]},
                    {vertices[1][cell + 1], mVertexColours[row + 1][cell + 1], order[2]},
                    {vertices[0][cell + 1], mVertexColours[row][cell + 1], order[3]},
                };
                if (outside == 0) {
                    // All in view: the four vertices go straight into the primitive.
                    SFVF_TLCTEX* cell4 = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used;
                    PutCell(cell4, corners, *quad, place);
                    const SFVF_TLCTEX* base = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData);
                    const bool front1 = SFVF_FacesViewer(base[used], base[used + 1], base[used + 2]);
                    const bool front2 = SFVF_FacesViewer(base[used], base[used + 2], base[used + 3]);
                    if (front1 && front2) {
                        SaveQuad(saved, page, used, entry, 4, 0);
                        used += 4;
                        ++saved;
                    } else if (front1 || front2) {
                        SaveQuad(saved, page, used, entry, 3, 0);
                        used += 3;
                        ++saved;
                        // Only the second triangle (0, 2, 3): its last two vertices move down.
                        if (front2) std::memmove(cell4 + 1, cell4 + 2, 2 * kVertexSize);
                    }
                } else {
                    // A corner outside: each facing triangle is clipped into the primitive.
                    SFVF_TLCTEX staged[4];
                    SFVF_TLCTEX clip[21];   // ClipPoly's second work buffer (0x2a0 bytes, as the original's)
                    int32_t count1 = 0;
                    int32_t count2 = 0;
                    PutCell(staged, corners, *quad, place);
                    if (SFVF_FacesViewer(staged[0], staged[1], staged[2])) {
                        std::memcpy(&clip[0], &staged[0], kVertexSize);
                        std::memcpy(&clip[1], &staged[1], kVertexSize);
                        std::memcpy(&clip[2], &staged[2], kVertexSize);
                        count1 = 3;
                        gei_Clipping->ClipPoly(outside, reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used,
                                               clip, count1, kVertexSize);
                    }
                    if (SFVF_FacesViewer(staged[0], staged[2], staged[3])) {
                        std::memcpy(&clip[0], &staged[0], kVertexSize);
                        std::memcpy(&clip[1], &staged[2], kVertexSize);
                        std::memcpy(&clip[2], &staged[3], kVertexSize);
                        count2 = 3;
                        gei_Clipping->ClipPoly(outside,
                                               reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + (used + count1),
                                               clip, count2, kVertexSize);
                    }
                    if (count1 + count2 > 0) {
                        SaveQuad(saved, page, used, entry, count1, count2);
                        used += count1 + count2;
                        ++saved;
                    }
                }
            }
            ++texture;
        }
    }

    primitive->mVertexCount = used;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) gei_DrawPrimitive->CheckFogOnCurrentPrimitive();
    const int32_t blending = static_cast<int32_t>(primitive->mBlendingMode);   // after the fog, before the projection
    gei_DrawPrimitive->ProjectCurrentPrimitive(0);
    tpw_probe::Hit(tpw_probe::kHFieldCells, static_cast<uint32_t>(saved));
    qSortTextures_lpTex(gQuadSaveInfo, 0, saved - 1);

    // One execution per run of cells on the same page.
    int32_t i = 0;
    const QuadSaveInfo* info = gQuadSaveInfo;
    while (i < saved) {
        CTextureBase* const page = info->mTexture;
        int32_t run = 1;
        while (i + run < saved && page == gQuadSaveInfo[i + run].mTexture) ++run;
        if (primitive != nullptr) {
            primitive->mTexture = page;
        } else {
            primitive = gei_DrawPrimitive->DuplicateCurrentPrimitive(1, 4, 0x1c4, blending, info->mTexture);
            if (primitive == nullptr) return 1;
        }
        primitive->mF34 = info->mF08;
        uint16_t* index = primitive->mIndexData;
        for (; run != 0; --run) {
            PutFan(index, info->mFirstVertex, info->mCount1);
            PutFan(index, info->mFirstVertex + info->mCount1, info->mCount2);
            ++info;
            ++i;
        }
        primitive->mFlags = (primitive->mFlags | 0x100) & ~SPrimitive::kDrawn;
        primitive->mVertexCount = used;
        primitive->mIndexCount = static_cast<int32_t>(index - primitive->mIndexData);
        if (gei_DrawPrimitive->ExecuteCurrentPrimitive(static_cast<float>(primitive->mVertexCount * 2000000)) == 0)
            return 0;
        primitive = nullptr;
    }
    return 1;
}

// The height field's cloud shadow pass (engine tb_hfield.cpp):
// CHField::CalcShadowLine 0x10010e7c (712 bytes), DrawSkyShadow 0x10011180 (5,488).

uint8_t gShadowUInitialized = 0, gShadowVInitialized = 0;
float gShadowUScale, gShadowVScale;

void CHField::CalcShadowLine(uint8_t* codes, SVertex* vertices, int32_t low, int32_t high, int32_t row) {
    if (low == 127 || high == -1) return;
    const sHField& field = *mF00;
    const float* heights = mF12970 != nullptr ? static_cast<float*>(mF12970) : field.mHeights.get();
    const float lift = gei_Display.mF938;
    SVertex* first = vertices + low;
    const int32_t rowStart = mGridWidth * row;
    const float z = ppc_fmadds(static_cast<float>(row), field.mF08[3], field.mF08[1]);
    for (int32_t column = low; column <= high; ++column) {
        vertices[column].x = ppc_fmadds(static_cast<float>(column), field.mF08[2], field.mF08[0]);
        vertices[column].y = lift + heights[column + rowStart];
        vertices[column].z = z;
    }
    if (gei_MathUnit == nullptr) msl_assertion_failed("gei_MathUnit", "tb_matrix.h", 0x98);
    const int32_t count = high - low + 1;
    gei_MathUnit->Transform(gei_FinalMatrix, first, first, 0x10, 0x10, count, 0);
    uint8_t unused[4];
    CClipping::CPP_TestData(unused, gei_Clipping, codes + low, first, count, 0x10, 0xff);
}

namespace {

constexpr int32_t kVertexSize_2 = sizeof(SFVF_TLCTEX);
// Engine constant 0x1004fc10, shared with the sky's scroll coordinates.
constexpr float kStep = 0x1.78d4fep-6f;

// Writes a reverse fan, with the clipped counts narrowed to signed halfwords first.
void PutFan_2(uint16_t*& out, int32_t first, int16_t count) {
    for (int32_t k = count - 2; k > 0;) {
        --k;
        out[0] = static_cast<uint16_t>(first);
        out[1] = static_cast<uint16_t>(first + k + 1);
        out[2] = static_cast<uint16_t>(first + k + 2);
        out += 3;
    }
}

// The specular word is untouched, including in the stack polygons sent to ClipPoly.
void PutCell_2(SFVF_TLCTEX* out, const float (&points)[2][128][4], int32_t cell, int32_t row,
             bool diagonal, uint32_t colour, const float (&uv)[4][2], const float (&place)[4]) {
    const float* corners[4] = {points[0][cell], points[1][cell], points[1][cell + 1], points[0][cell + 1]};
    const int32_t start = diagonal ? 0 : 1;
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t corner = (start + i) & 3;
        const bool edge = corner == 0 || corner == 3 ? row == 0 : row == -1;
        std::memcpy(&out[i].x, corners[corner], 16);
        out[i].colour = edge ? colour & 0xffffff00u : colour;
        out[i].u = ppc_fmadds(place[0], uv[corner][0], place[2]);
        out[i].v = ppc_fmadds(place[1], uv[corner][1], place[3]);
    }
}

}  // namespace

int32_t CHField::DrawSkyShadow() {
    if (mF12970 == nullptr) return 1;
    if ((gei_System.mFlags & 0xc0000000u) == 0 && mShadowPrimitive != nullptr) {
        SFVF_TLCTEX* vertices = reinterpret_cast<SFVF_TLCTEX*>(mShadowPrimitive->mVertexData);
        const D3DVECTOR* position = gei_Sky.GetPosition();
        float previous[3];
        std::memcpy(previous, mPad295C, sizeof previous);
        const float dz = position->z - previous[2];
        const float dx = position->x - previous[0];
        std::memcpy(mPad295C, position, sizeof previous);
        for (uint32_t i = 0; i < static_cast<uint32_t>(mShadowPrimitive->mVertexCount); ++i) {
            vertices[i].u += dz;
            vertices[i].v += dx;
        }
        return 1;
    }
    CTextureEntry* entry = gei_Sky.GetSkyTEntry();
    if (entry == nullptr) return 1;
    const uint32_t flags = (gei_Display.mFlags & CDisplay::kEnableFog) != 0 ? 0x60 : 0x20;
    uint32_t mode = (entry->GetBlendingMode() & ~0x400u) | 0x800;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0) mode |= 0x100000;
    SPrimitive* primitive = gei_DrawPrimitive->GetNextPrimitive(1, 4, 0x1c4, static_cast<int32_t>(mode),
                                                              entry->GetTexture());
    mShadowPrimitive = primitive;
    if (primitive == nullptr) return 1;
    uint16_t* indices = primitive->mIndexData;
    const uintptr_t vertexLimit = reinterpret_cast<uintptr_t>(gei_DrawPrimitive->mVertexEnd) - 16 * kVertexSize_2;
    const uintptr_t indexLimit = reinterpret_cast<uintptr_t>(gei_DrawPrimitive->mIndexEnd) - 0x30;
    const D3DVECTOR* position = gei_Sky.GetPosition();
    const uint32_t colour = static_cast<uint32_t>(gei_Display.mF8FC);
    std::memcpy(mPad295C, position, 12);
    primitive->mFlags = flags;
    SGamutQuads& cells = *QuadRows();
    SGamutQuads& rows = *VertexRows();
    uint8_t codes[2][128];
    float points[2][128][4];
    int32_t used = 0;
    CalcShadowLine(codes[1], reinterpret_cast<SVertex*>(points[1]), rows.mLow[cells.mFirstRow],
                   rows.mHigh[cells.mFirstRow], cells.mFirstRow);
    const float place[4] = {entry->mUSize, entry->mVSize, entry->mUOffset, entry->mVOffset};
    for (int32_t row = cells.mFirstRow; row < cells.mEndRow; ++row) {
        std::memcpy(points[0], points[1], sizeof points[0]);
        std::memcpy(codes[0], codes[1], sizeof codes[0]);
        CalcShadowLine(codes[1], reinterpret_cast<SVertex*>(points[1]), rows.mLow[row + 1], rows.mHigh[row + 1], row + 1);
        const float rowUV = kStep * static_cast<float>(row);
        for (int32_t cell = cells.mLow[row]; cell < cells.mHigh[row]; ++cell) {
            SFVF_TLCTEX* out = reinterpret_cast<SFVF_TLCTEX*>(primitive->mVertexData) + used;
            if (vertexLimit < reinterpret_cast<uintptr_t>(out) || indexLimit < reinterpret_cast<uintptr_t>(indices))
                goto finished;
            const sHFieldQuad& quad = mF00->mQuads.get()[cell + row * static_cast<int32_t>(mF00->mF18[0])];
            // The original tests bit 0x80000000 of this zero-extended halfword, so no quad flags skip it.
            if ((codes[0][cell] & codes[0][cell + 1] & codes[1][cell] & codes[1][cell + 1]) != 0) continue;
            if (gShadowUInitialized == 0) {
                gShadowUScale = 1.0f;
                gShadowUInitialized = 1;
            }
            if (gShadowVInitialized == 0) {
                gShadowVScale = 1.0f;
                gShadowVInitialized = 1;
            }
            const float columnUV = kStep * static_cast<float>(cell);
            const float uv[4][2] = {
                {gShadowUScale * (position->x + columnUV), gShadowVScale * ((kStep + position->z) - rowUV)},
                {gShadowUScale * (position->x + columnUV), gShadowVScale * (position->z - rowUV)},
                {gShadowUScale * ((kStep + position->x) + columnUV), gShadowVScale * (position->z - rowUV)},
                {gShadowUScale * ((kStep + position->x) + columnUV), gShadowVScale * ((kStep + position->z) - rowUV)},
            };
            const int32_t outside = codes[0][cell] | codes[0][cell + 1] | codes[1][cell] | codes[1][cell + 1];
            const int32_t first = used;
            int32_t count1 = 0, count2 = 0;
            if (outside == 0) {
                PutCell_2(out, points, cell, row, (quad.mF00 & 2) != 0, colour, uv, place);
                const bool front1 = SFVF_FacesViewer(out[0], out[1], out[2]);
                const bool front2 = SFVF_FacesViewer(out[0], out[2], out[3]);
                if (front1 && front2) {
                    count1 = 4;
                } else if (front1 || front2) {
                    count1 = 3;
                    if (front2) std::memmove(out + 1, out + 2, 2 * kVertexSize_2);
                }
                used += count1;
            } else {
                SFVF_TLCTEX staged[4];
                SFVF_TLCTEX clip[21];
                PutCell_2(staged, points, cell, row, (quad.mF00 & 2) != 0, colour, uv, place);
                if (SFVF_FacesViewer(staged[0], staged[1], staged[2])) {
                    std::memcpy(clip, staged, 3 * kVertexSize_2);
                    count1 = 3;
                    gei_Clipping->ClipPoly(outside, out, clip, count1, kVertexSize_2);
                }
                if (SFVF_FacesViewer(staged[0], staged[2], staged[3])) {
                    std::memcpy(clip, staged, kVertexSize_2);
                    std::memcpy(clip + 1, staged + 2, 2 * kVertexSize_2);
                    count2 = 3;
                    gei_Clipping->ClipPoly(outside, out + count1, clip, count2, kVertexSize_2);
                }
                if (count1 + count2 > 0) {
                    used += count1 + count2;
                } else {
                    count1 = 0;
                    count2 = 0;
                }
            }
            const int16_t fan1 = static_cast<int16_t>(count1);
            const int16_t fan2 = static_cast<int16_t>(count2);
            PutFan_2(indices, first, fan1);
            PutFan_2(indices, first + fan1, fan2);
        }
    }
finished:
    primitive->mVertexCount = used;
    primitive->mIndexCount = static_cast<int32_t>(indices - primitive->mIndexData);
    if (primitive->mVertexCount == 0 || primitive->mIndexCount == 0) {
        mShadowPrimitive = nullptr;
        return 1;
    }
    const int32_t depth = static_cast<int32_t>(static_cast<uint32_t>(used) * 1900000u);
    return gei_DrawPrimitive->ExecuteCurrentPrimitive(static_cast<float>(depth)) == 0 ? 0 : 1;
}

// 0x1001271c
void __sinit_tb_hfield_cpp() { gei_HFieldOne = 1.0f; }
