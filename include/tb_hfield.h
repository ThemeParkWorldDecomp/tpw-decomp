// tb_hfield.h: the inventory's tb_hfield.cpp range of the engine library: the view
// volume (CGamut) and the height field (CHField). PARTIAL: construction and shutdown
// only; the fields named mF<offset> are the ones those touch, the rest is padding until
// the owning code is translated. Mac offsets in the comments (CHField's pointers make
// the host object larger).
#pragma once
#include <cstddef>
#include <cstdint>

#include "platform/d3d_types.h"   // _D3DVECTOR
#include "tb_proctexture.h"   // PrepareEntry, sTileOverlay

class CProcTextureHandler;
class CTextureBase;
class CTextureEntry;
struct SVertex;
struct sHFieldQuad;
struct SPrimitive;

// The map cells in view, row by row (0x108 bytes; names invented): rows mFirstRow to
// mEndRow - 1, row z covering cells mLow[z] to mHigh[z] - 1 (signed bytes, indexed by the row
// itself). CGamut keeps one at +0xc4 (GetQuads), CHField one at +0x128 (DivideAndConquer
// fills it); CMapWho::PrepareWorldspace walks the one RenderWindow picked.
struct SGamutQuads {
    int32_t mFirstRow;   // 0x00
    int32_t mEndRow;     // 0x04
    int8_t mLow[128];    // 0x08
    int8_t mHigh[128];   // 0x88
};
static_assert(sizeof(SGamutQuads) == 0x108, "SGamutQuads layout");

// A vertex of the engine's strided vertex arrays that the math unit transforms in place
// (0x10 bytes; the type's name is the one CHField::CalcLine's mangled name gives, the member
// names are invented): the position, then the word Transform writes (w of type 0).
struct SVertex {
    float x, y, z;   // 0x00
    float w;         // 0x0c  not written by whoever builds the vertex
};
static_assert(sizeof(SVertex) == 0x10, "SVertex layout");

class CGamut {
public:
    CGamut();                                  // 0x10009a44
    void Init(float fov);                      // 0x10009abc
    void SetYClipper(float low, float high);   // 0x10009b80
    void SetX(float low, float high, float cell); // 0x10009c44
    void SetZ(float low, float high, float cell); // 0x10009cb4
    void CalcFurthestDistance();              // 0x10009bb8
    // Finds the cells and grid points in view: the footprint of the view volume on the height
    // field, as rows (src/tb_hfield_gamut.cpp). Always 1.
    int32_t CalculateGamut();                 // 0x10009f04
    // Where the ray through the view volume's corner `corner` (one of mCorners) meets the
    // height field, halfway between its top and bottom clip planes; the Mac returns the vector
    // through a hidden pointer (src/tb_hfield_gamut.cpp).
    _D3DVECTOR GetHFieldIntersection(_D3DVECTOR* corner);   // 0x10009d60
    // The cells in view: the SGamutQuads at +0xc4 (mFC4 onwards). Out of line at 0x1000bc40
    // (src/tb_map_render.cpp).
    SGamutQuads* GetQuads();
    // The grid points in view, in the same shape (+0x1cc, mF1CC onwards): CHField::Prepare
    // copies them. Out of line at 0x1000bc70 (src/tb_hfield_frame.cpp).
    SGamutQuads* GetVertices();

    float mCorners[4][3];
    float mYClipLow;       // 0x30
    float mYClipHigh;      // 0x34
    float mFurthestDistance; // 0x38
    float mF3C[6];         // 0x3c..0x50  0.0
    int32_t mF54[4];       // 0x54..0x60  0
    uint8_t mPad64[8];
    int32_t mF6C;          // 0x6c  0
    uint8_t mPad70[0xc0 - 0x70];
    int32_t mFC0;          // 0xc0  0
    int32_t mFC4;          // 0xc4  0
    int32_t mFC8;          // 0xc8  0
    uint8_t mPadCC[0x1cc - 0xcc];
    int32_t mF1CC;         // 0x1cc  0
    int32_t mF1D0;         // 0x1d0  0
    uint8_t mPad1D4[0x2d8 - 0x1d4];
};
static_assert(offsetof(CGamut, mF6C) == 0x6c && offsetof(CGamut, mFC0) == 0xc0 &&
                  offsetof(CGamut, mF1CC) == 0x1cc && sizeof(CGamut) == 0x2d8,
              "CGamut layout");
extern CGamut* gei_Gamut;   // engine export

int32_t Gamut_Init();    // 0x10009904
int32_t Gamut_Close();   // 0x100099d0

struct tag_sM3D2Header;
struct sHField;
struct sTexture;

// One quad CHField::Execute drew, in the order it drew them (0x10 bytes on the Mac; the type's
// name is the one qSortTextures_lpTex's mangled name gives, the member names are invented).
// Execute has them sorted by page and makes one primitive per run of quads on the same page.
struct QuadSaveInfo {
    CTextureBase* mTexture;   // 0x00  the page of the quad's texture entry (NULL without one, or off the cache)
    int32_t mFirstVertex;     // 0x04  the quad's first vertex in the primitive's vertex memory
    int32_t mF08;             // 0x08  the texture entry's mF40 (-1 without an entry); goes to the primitive's mF34
    int16_t mCount1;          // 0x0c  vertices of the first triangle's polygon (4 for an unclipped quad kept whole)
    int16_t mCount2;          // 0x0e  vertices of the second triangle's polygon (0 unless clipped)
};

class CHField {
public:
    // The cells Prepare can list for the procedural textures (engine data 0x100a2dcc holds
    // their overlays, gTileOverlays).
    enum : int32_t { kMaxOverlays = 2500 };

    CHField();                // 0x1000bffc
    ~CHField();               // 0x1000c068
    void Init(tag_sM3D2Header* header);   // 0x1000d740
    int32_t InitGamut();                 // 0x1000dbc8
    void AdjustHField();                // 0x1000c0e8 (src/tb_hfield_adjust.cpp)
    int32_t Close();          // 0x1000d8b8
    int32_t CloseGamut();     // 0x1000dce0: 1
    int32_t LoadProcTextures(char* name, int32_t size);   // 0x1000d96c (src/tb_hfield_proctex.cpp)
    void ProcTextureRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1);   // 0x1000db80 (20 bytes, src/tb_hfield_texrange.cpp): PT_SetRange's callee
    // The frame's members. DivideAndConquer fills the SGamutQuads at +0x128 (mF128 onwards) in
    // first-person views; Prepare, Execute and DrawSkyShadow return 0 on failure.
    int32_t DivideAndConquer();   // 0x1000e144 (src/tb_hfield_frame.cpp)
    // Has the texture cache place the pictures of the cells in view; with procedural textures
    // on it collects the cells inside the procedural window and hands them to the handler.
    int32_t Prepare();            // 0x1000ed18 (src/tb_hfield_frame.cpp)
    // Tests the box of grid cells x0..x1, z0..z1 against the view volume and, when it is in
    // view, widens the rows of the grid points at +0x20 and raises mF1C; a box that is only
    // partly in view is cut in four and each quarter tested (src/tb_hfield_conquad.cpp).
    void TestConQuad(int32_t x0, int32_t z0, int32_t x1, int32_t z1);   // 0x1000dd14

    // The grid points in view (the SGamutQuads at +0x20: mF20 and mF24 are its rows, then the
    // low and high columns of each row) and the cells in view (the SGamutQuads at +0x128).
    // The members are contiguous, so one pointer reaches each block.
    SGamutQuads* VertexRows() { return reinterpret_cast<SGamutQuads*>(&mF20); }
    SGamutQuads* QuadRows() { return reinterpret_cast<SGamutQuads*>(&mF128); }
    // Whether the cells in view (the SGamutQuads at +0x128) reach the area of cells a node
    // covers: `area` is the node's bytes mF90 (last row, first row, last column, first column;
    // src/tb_hfield_helpers.cpp). QSort_textures sorts pages by address, with the quirk that
    // its second scan stops on the first index, not the last one.
    int32_t CheckGamutArea(int8_t* area);                              // 0x1000e760
    void QSort_textures(void** textures, int32_t low, int32_t high);   // 0x1000f060
    // Draws the cells in view (src/tb_hfield_execute.cpp): when the view or the lights changed,
    // one static primitive of their vertices, then one execution per run of cells on the same
    // texture page. 1 also when nothing is drawn; 0 when an execution fails.
    int32_t Execute();            // 0x1000f69c
    int32_t DrawSkyShadow();      // 0x10011180 (src/tb_hfield_shadow.cpp)

    // Execute's helpers (src/tb_hfield_helpers.cpp). CalcFullLighting works out mVertexColours for the grid
    // points in view. CalcLine fills, for grid row `row` and columns low..high, the outside
    // codes (a byte a point) and the transformed points (x, y, z, rhw) of an array of 128; it
    // returns at once when low > 126 and never reads `spare` (128 words).
    // GetTexturesUsed puts the texture entry of every cell in view whose flag 1 is clear into
    // `entries`, in Execute's order, and the different pages they are on into `textures`.
    // ProcessTextureParameters permutes the four corner numbers in `corners` (one byte each,
    // in memory order) by the quad's flags 0x78. qSortTextures_lpTex sorts quads[low..high]
    // by page.
    void CalcFullLighting();      // 0x1000e824
    void CalcLine(uint8_t* codes, SVertex* vertices, uint32_t* spare, int32_t low, int32_t high,
                  int32_t row);   // 0x1000ea40
    void CalcShadowLine(uint8_t* codes, SVertex* vertices, int32_t low, int32_t high,
                        int32_t row);   // 0x10010e7c
    void GetTexturesUsed(void** textures, int32_t& count, CTextureEntry** entries);   // 0x1000f1b0
    void ProcessTextureParameters(uint32_t* corners, sHFieldQuad* quad);              // 0x1000f3f8
    void qSortTextures_lpTex(QuadSaveInfo* quads, int32_t low, int32_t high);         // 0x1000f4ec

    sHField* mF00;            // 0x00, heightfield file record
    sTexture* mF04;           // 0x04, mesh textures
    int32_t mGridWidth;       // 0x08, number of height samples
    int32_t mGridHeight;      // 0x0c
    float mInvCellX;          // 0x10
    float mInvCellZ;          // 0x14
    float mInvHeightRange;    // 0x18
    float mF1C;               // 0x1c  DivideAndConquer: 100.0, which TestConQuad raises to the farthest
                              //       corner it sees; the projection's far plane at the end
    int32_t mF20;             // 0x20  0
    int32_t mF24;             // 0x24  0
    uint8_t mPad28[0x128 - 0x28];
    int32_t mF128;            // 0x128  0
    int32_t mF12C;            // 0x12c  0
    uint8_t mPad130[0x230 - 0x130];
    CProcTextureHandler* mProcTextures;   // 0x230  closed and destroyed by Close
    int32_t mF234;            // 0x234  0     the window of cells with procedural textures:
    int32_t mF238;            // 0x238  0     columns mF234..mF23C, rows mF238..mF240
    int32_t mF23C;            // 0x23c  0x7f
    int32_t mF240;            // 0x240  0x7f
    uint8_t mPad244[4];
    // 0x248  Prepare's list for the procedural textures: one tile (row * 256 + column, relative
    //        to the window) per cell, which the handler's PrepareList turns in place into the
    //        cell's texture entry (mTexture); GetTexturesUsed then takes those entries, in the
    //        same order, in place of the mesh's textures.
    PrepareEntry mPrepareEntries[kMaxOverlays];
    int32_t mF2958;           // 0x2958  0
    uint8_t mPad295C[0x2968 - 0x295c];
    // 0x2968  the colour of each grid point, row z at [z], 128 columns a row: CalcFullLighting
    //         works them out, Execute puts them into the vertices.
    uint32_t mVertexColours[128][128];
    int32_t mF12968;          // 0x12968  0
    int32_t mF1296C;          // 0x1296c  Prepare: the number of cells it listed (not set when it ends early)
    void* mF12970;            // 0x12970  an engine allocation, freed by Close: with display flag
                              //          0x10000 Init copies the heights here (floats, rows of
                              //          mF00->mF18[0] + 1) and AdjustHField raises parts of them
    // Native pointer for Mac +0x2958; the legacy word above stays for existing callers.
    SPrimitive* mShadowPrimitive = nullptr;
};

// DrawSkyShadow's lazily initialized coordinate factors (engine data, names invented).
extern uint8_t gShadowUInitialized, gShadowVInitialized;   // 0x10052dc4, 0x10052dbc
extern float gShadowUScale, gShadowVScale;                 // 0x10052dc8, 0x10052dc0
// 0x12974 bytes on the Mac.
extern CHField* gei_HField;   // engine data 0x100af11c (not exported)
// The overlay of each cell Prepare listed (0x14 bytes each on the Mac, 50,000 in all: they
// end where gei_HField begins). Name invented; defined in src/tb_hfield_frame.cpp.
extern sTileOverlay gTileOverlays[CHField::kMaxOverlays];   // engine data 0x100a2dcc

// Execute's work arrays (engine data, names invented; defined in src/tb_hfield_execute.cpp):
// the texture entry of each cell it may draw, which GetTexturesUsed fills, and the quads it drew.
extern CTextureEntry* gQuadTextures[0x4000];   // 0x10052dcc
extern QuadSaveInfo gQuadSaveInfo[0x4000];     // 0x10062dcc

// Execute's work arrays (engine data, names invented; defined in src/tb_hfield_execute.cpp):
// the texture entry of each cell it may draw, which GetTexturesUsed fills, and the quads it drew.
extern CTextureEntry* gQuadTextures[0x4000];   // 0x10052dcc
extern QuadSaveInfo gQuadSaveInfo[0x4000];     // 0x10062dcc

int32_t HField_Init();    // 0x1000bca4
int32_t HField_Close();   // 0x1000bd60: always 1

// Procedural textures on and off (0x1000be5c, 0x1000be98).
void PT_Enable();
void PT_Disable();
// 0x1000bed0 (engine, 268 bytes, untranslated): loads the procedural texture set of the file `path` (LoadProceduralTextureSet makes it
// "<mesh folder>base.lnd") in mode `mode`; answers 1 or the result of CHField::LoadProcTextures. LoadProceduralTextureSet drops the answer.
uint32_t PT_Load(char* path, int32_t mode);
// 0x1000bde4 (engine): the cell range the procedural textures cover (x0, y0, x1, y1); nothing without a height field.
void PT_SetRange(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
