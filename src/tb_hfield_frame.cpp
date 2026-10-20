#include "tpw_probe.h"
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
#include "tb_hfield.h"

#include <cstddef>
#include <cstring>

#include "m3d2.h"
#include "ppc_math.h"
#include "tb_loadmanager.h"
#include "tb_matrix.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "tb_texturecache.h"
#include "tb_textureentry.h"

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

SGamutQuads* CGamut::GetVertices() { return reinterpret_cast<SGamutQuads*>(&mF1CC); }

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
