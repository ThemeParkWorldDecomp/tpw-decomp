// CDynamicMesh (analyser.cpp range of the main app, 0x100e1610..): the meshes the park draws cell by cell (the blueprint,
// the water and the flags). Unit w112b translates the constructor, the destructor, SetBoundingBox, DC_CreateSquareFace and
// DC_SetVertex (src/game/park/DynamicCell.cpp); unit w120 translates DrawAll (0x100e1a04) and DC_CreatePolyFace (0x100e2778) with the per-frame
// tick that calls them (src/game/park/DynamicCell.cpp).
//
// The Mac object is 0xa4 bytes with no vtable. The host object is native: the three embedded records (the texture set, the mesh
// entry and the mesh set) hold native pointers, the Mac offsets are in the comments, and the member names are invented. Members
// have no default initialisers on purpose: the constructor leaves a few bytes as the heap had them (item +2 and +0xe, set +6).
#pragma once
#include <cstddef>
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"

// The kinds of dynamic mesh (the enum's name is the Mac's; the values are the ones DC_SetupDynamicCellMeshes passes).
enum DYN_Type : int32_t {
    kDynBlueprint = 0,
    kDynWater = 1,
    kDynFlags = 3,
};

class CDynamicMesh {
public:
    // 0x100e1610 (716 bytes): `ok` is a four-word buffer whose first word the constructor sets to 1 and then to DMCreateMesh's answer
    // (always 1 on the Mac, so the else path is dead code). sizeA is the number of squares, sizeB the number of face sets (texture
    // entries), the six floats the mesh's box.
    CDynamicMesh(uint16_t sizeA, uint16_t sizeB, uint32_t* ok, const char* name, DYN_Type type, float minX, float minY, float minZ,
                 float maxX, float maxY, float maxZ);
    // 0x100e191c (104 bytes): disables and frees the mesh (DMDisableMeshRender and DMFreeMesh on the set at +0x54),
    // then the object itself when the Mac's flag is positive (the host's `delete`).
    ~CDynamicMesh();

    // 0x100e19b0 (28 bytes): the six floats go to the mesh entry's box.
    void SetBoundingBox(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    // 0x100e210c (1,576 bytes): a square (side: 0 floor, 1/4/0x10/0x40 the four walls; any other value makes no vertices) of four
    // vertices and two faces at the end of the buffers. The float after `texture` and the byte after `large` are not used.
    void DC_CreateSquareFace(float x, float y, float z, uint8_t side, uint8_t rotation, int32_t texture, float unused1, int32_t large,
                             uint8_t unused2, int32_t keepPosition);
    // 0x100e1f10 (444 bytes): one vertex (position, point and texture coordinates) at the vertex counter, which it advances.
    void DC_SetVertex(float x, float y, float z, float u, float v, int32_t followTerrain, s3DPoint* point);
    // 0x100e1a04 (1,248 bytes, src/game/park/DynamicCell.cpp, unit w120): rebuilds the mesh from the sorted list of dynamic faces (gDynamicFaceKeys over
    // gDynamicFaces): clears the counters and the 20 face sets, then walks the list, drawing every face whose mesh type is this one (squares
    // through DC_CreateSquareFace, poly faces through DC_CreatePolyFace) up to mMaxSquares, and sets the entry's counts. The walk stops at the
    // first face of another type after it has seen one of its own. The entry's box is the last drawn face's first corner plus and minus 10,
    // clamped to +-1024 (the original's running box is never updated).
    void DrawAll();
    // 0x100e2778 (232 bytes, src/game/park/DynamicCell.cpp, unit w120): a quadrilateral as four vertices (DC_SetVertex, no terrain following) and two
    // faces at the cursor. `texture` is never read. The vertex counter is read once, before the vertices are made.
    void DC_CreatePolyFace(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points);

    DYN_Type mType;                  // 0x00  0 blueprint, 1 water, 3 flags
    sDTextureSet mTextures;          // 0x04  the mesh's own texture set (0x20 bytes on the Mac)
    sDMeshEntry mItem;               // 0x24  the one mesh entry (0x30 bytes): flags 3, 2N faces, 4N vertices, 4N UVs, sizeB face sets
    tag_sDMeshSet mMeshSet;          // 0x54  the set DMCreateMesh, DMEnableMeshRender, DMDisableMeshRender and DMFreeMesh work on
    sFace* mFaceCursor;              // 0x94  the next face to write (starts at the set's faces)
    uint32_t mFaceCount;             // 0x98  faces written so far
    uint32_t mVertexCount;           // 0x9c  vertices written so far (the next vertex index)
    int32_t mMaxSquares;             // 0xa0  sizeA; DrawAll refuses to draw more squares ("Max faces exceeded in Drawall")
};

// The class's globals (all zero at start, like the Mac's bss).
extern float gDynamicCellUV[10][3];            // 0x102f9b48  texture coordinates (u, v, unused) of the square corners; DC_TickDynamicCellMeshes writes them
extern float gDynamicCellF9BC0;                // 0x102f9bc0  the map origin's lag in z (DC_TickDynamicCellMeshes)
extern float gDynamicCellF9BC4;                // 0x102f9bc4  the same in x
extern char gDynamicCellTexturePath[0x400];    // 0x102f7cc8  "Data:Levels:<theme>:Dynamic:Textures" (the buffer's size is unknown)
extern int32_t gLLCCDB50;                      // 0x102ddb50  a cell system word (the map origin in z, inferred from DC_CreateSquareFace's use of it)
extern int32_t gLLCCDB54;                      // 0x102ddb54  the same in x (the int that scales into gDynamicCellF9BC4)
extern sDTextureEntry gDynamicCellTextureEntries[20];   // 0x102a01bc  initialised data, defined by the unit that translates DMLoadTextures

// ---- the dynamic face list (unit w120, src/game/park/DynamicCell.cpp). Names invented; Mac addresses given. The producers (DC_AddSquareFaceToList
// 0x100e02a0, DC_AddPolyFaceToList 0x100e03f0) are in src/game/park/DynamicCell.cpp (unit w127); the layout is what DrawAll and the producers agree on. Plain data, so the
// Mac layout holds on the host (the byte fields are single bytes).
struct sDynamicFace {                   // 0xa0 bytes
    union {
        struct {                        // a square (mIsSquare nonzero): DC_CreateSquareFace's arguments
            float mX, mY, mZ;           // 0x00
            uint8_t mSide;              // 0x0c  0 floor, 1/4/0x10/0x40 the walls
            uint8_t mPad0D[3];          // 0x0d
            int32_t mRotation;          // 0x10  DrawAll passes the low byte
            float mF14;                 // 0x14  DC_CreateSquareFace's unused float
            int32_t mLarge;             // 0x18
            uint8_t mF1C;               // 0x1c  its unused byte
            uint8_t mPad1D[3];          // 0x1d
        };
        struct {                        // a poly face (mIsSquare 0): DC_CreatePolyFace's three arrays of four points
            s3DPoint mPositions[4];     // 0x00
            s3DPoint mUVs[4];           // 0x30  u and v are used, z is not
            s3DPoint mPoints[4];        // 0x60  the vertices' "point" arguments
        };
        uint8_t mBytes[0x90];
    };
    DYN_Type mType;                     // 0x90  the mesh the face belongs to: 0 blueprint, 1 water, 3 flags
    int32_t mIsSquare;                  // 0x94  0 a poly face, nonzero a square
    int32_t mKeepPosition;              // 0x98  squares only (DC_CreateSquareFace's last argument)
    uint8_t mTexture;                   // 0x9c  0 to 19, the face set the face goes to
    uint8_t mPad9D[3];                  // 0x9d
};
static_assert(sizeof(sDynamicFace) == 0xa0, "sDynamicFace layout");
static_assert(offsetof(sDynamicFace, mType) == 0x90, "mType at 0x90");
static_assert(offsetof(sDynamicFace, mKeepPosition) == 0x98, "mKeepPosition at 0x98");
static_assert(offsetof(sDynamicFace, mTexture) == 0x9c, "mTexture at 0x9c");
constexpr int32_t kDynamicFaceMax = 0x806;   // both AddToList functions refuse at this many ("Too many (square|poly) dynamic faces !")
extern sDynamicFace gDynamicFaces[kDynamicFaceMax];   // 0x102fbdc0 (TOC 0x102611f8), 0x50240 bytes, BSS
// 0x102f9da4 (TOC 0x102611fc), 0x2018 bytes, BSS: (record index << 5) | (texture & 0x1f) for each of the gDynamicCellFBDBC (analyser.h,
// 0x102fbdbc, right after the array) records; the tick sorts it by the low five bits.
extern uint32_t gDynamicFaceKeys[kDynamicFaceMax];
// The eight places the tick puts a flag (initialised data of the image, 0x102a0320, TOC 0x10261200).
struct sDynamicFlagSpot {
    int32_t mF00;                       // 0x00  -1 in the image, never read here
    float mX, mY, mZ;                   // 0x04
};
static_assert(sizeof(sDynamicFlagSpot) == 0x10, "sDynamicFlagSpot layout");
extern sDynamicFlagSpot gDynamicFlagSpots[8];   // 0x102a0320

// The tick's callees that are still stand-ins (analyser.cpp range, unit w120 declares them):
void HDZ_RotatePointZ(s3DPoint* points, float angle);                  // 0x100e0544 (148 bytes): rotates four consecutive points (x, y) about z
void DC_AnimateWater(int32_t a, int32_t b, float c, float d);           // 0x100e10f4 (560 bytes)
void DC_DrawWater(float x, float y, float z, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f);   // 0x100e060c (580 bytes)
void DC_AnimateFlag(int32_t a, int32_t b, float c, float d);            // 0x100e1350 (664 bytes)
void DC_DrawFlag(float x, float y, float z, float w, int32_t a, int32_t b, int32_t texture, float angle);   // 0x100e087c (2,124 bytes)
void DC_ResetSquareFaceListPtr();                                       // 0x100e0260 (16 bytes): the record count to 0
// 0x100dfb68 (512 bytes): a 1-based quicksort of `count` keys on their low five bits; the pointer is ONE WORD BELOW the first key.
void sort(uint32_t* keysMinusOne, uint32_t count);

// ---- the face list's producers and the water (unit w127, src/game/park/DynamicCell.cpp)
// 0x100e02a0 (268 bytes): appends a square at the end of the list. `texture` goes to the byte at 0x9c and to the key; a face whose texture is 1
// is dropped (nothing stored, the count unchanged) while gDynamicCellFlag is 0; mKeepPosition is `keepPosition`, or 1 while gDynamicCellF9BD4
// is not 0. `rotation` (0x10), `unused1` (0x14), `large` (0x18) and `unused2` (0x1c) are stored as DC_CreateSquareFace takes them. A full list
// (0x806 records) CTRACEs "Too many (square) dynamic faces !" at level 6 and stores nothing.
void DC_AddSquareFaceToList(float x, float y, float z, uint8_t side, int32_t texture, int32_t rotation, float unused1, int32_t large,
                            uint8_t unused2, int32_t keepPosition, DYN_Type type);
// 0x100e03f0 (256 bytes): appends a poly face: the four positions (x, y, z), the four texture coordinates (u, v only: the record's z words
// are left alone) and the four points (x, y, z); mIsSquare and mKeepPosition 0. The same limit and key as the square's; its CTRACE text is
// "Too many (poly) dynamic faces !".
void DC_AddPolyFaceToList(s3DPoint* positions, int32_t texture, s3DPoint* uvs, s3DPoint* points, DYN_Type type);

// The two grids DC_AnimateWater and DC_AnimateFlag write and DC_DrawWater and DC_DrawFlag read (BSS, 0xe40 bytes each): 16 columns of 19
// points, the point of column c and row r at [c][r]. The animators' first count is the columns, the second the rows (the tick passes 0x10 and 0x13).
// A holds the surface normals (normalised), B the positions: the water puts x = column * step, z = row * step and y = 1.7 * the wave's cosine.
extern s3DPoint gDynamicGridNormals[16][19];     // 0x102f7ec8 (TOC 0x102611f0)
extern s3DPoint gDynamicGridPositions[16][19];   // 0x102f8d08 (TOC 0x102611f4)

// ---- the flags (unit w128, src/game/park/DynamicCell.cpp)
// 0x102a03a0 (TOC 0x10262c90): initialised data, 1.0f. DC_AnimateFlag limits the ripple's stretch (the point's column coordinate over the
// flag's width, 0 at the pole) to it.
extern float gDynamicFlagClamp;
