// The mesh file ("M3D2", .md2) as the engine keeps it in memory.
//
// LoadM3D2Header (src/engine/M3D2Load.cpp) reads the whole file into one block and uses it in
// place: the file is little-endian, with offsets from its start where these structures
// have pointers. The structures below therefore keep the FILE's layout exactly (the sizes
// and offsets are the Mac's and the file's; static_assert on each), and every pointer
// field is a Ptr32 (include/Ptr32.h): whatever one of them points to lives in the
// platform's arena. Fields are named where the loader or another translated function
// shows what they are, and by offset (mF0C) otherwise.
//
// Record sizes are part of the format: the engine steps through nodes by 0xa0 and mini
// nodes by 0x58, and a mini node is the first 0x58 bytes of a node.
#pragma once
#include <cstddef>
#include <cstdint>

#include "Ptr32.h"
#include "tb_matrix.h"   // sMatrix, sVertexPosnSet

class CsTexture;
struct tag_sNode;
struct tag_sM3D2Header;
struct tag_sRideAnimSet;

struct s3DPoint {
    float x, y, z;

    void Byteswap();   // engine 0x1004b8d4
};
static_assert(sizeof(s3DPoint) == 0xc, "s3DPoint layout");
// Unit v51023: divides all three by the length of (x, z) (y not in the length).
void NormaliseVectorXZ(s3DPoint& point);   // 0x10045a38 (108 bytes)

// An axis-aligned box: minimum corner, then maximum corner.
struct sAlignedBox {
    float mMin[3];
    float mMax[3];

    void Byteswap();   // engine 0x10048f70
};
static_assert(sizeof(sAlignedBox) == 0x18, "sAlignedBox layout");

// Eight words of texture coordinates (one set).
struct sUVSet {
    float mF00[8];
};
static_assert(sizeof(sUVSet) == 0x20, "sUVSet layout");

// A texture slot of the mesh. The file has 0 in the pointer; CMesh::Init puts the loaded
// texture object there.
struct sTexture {
    uint16_t mF00;                   // 0x00
    uint16_t mF02;                   // 0x02
    Ptr32<CsTexture> mTexture;       // 0x04
};
static_assert(sizeof(sTexture) == 8, "sTexture layout");

struct sTextureInfo {
    uint32_t mFlags;                 // 0x00  bit 0 is cleared by load flag 4
    Ptr32<char> mFolder;             // 0x04  cleared by the loader; the game points it at one of
                                     //       its texture folders (OverrideTexturePaths)
    uint8_t mF08[2];                 // 0x08
    uint16_t mFrames;                // 0x0a  set to 1 by load flag 4
    Ptr32<char> mName;               // 0x0c  one name of 0x14 bytes per frame
};
static_assert(sizeof(sTextureInfo) == 0x10, "sTextureInfo layout");

// A face: three 16-bit words after a word that packs a 15-bit index and a flag. The file
// has the index in the low 15 bits; the Mac's compiler lays the same bit field out the
// other way round, and the engine's code reads it that way, so the loader rewrites the
// word on every host: index in the high 15 bits (word >> 1), the flag in bit 0.
struct sFace {
    uint16_t mIndexAndFlag;          // 0x00
    uint16_t mF02[3];                // 0x02
};
static_assert(sizeof(sFace) == 8, "sFace layout");

struct sFaceSet {
    Ptr32<sTexture> mTexture;        // 0x00
    uint16_t mFirstFace;             // 0x04  index into the node's faces
    uint16_t mFaceCount;             // 0x06
    uint16_t mF08;                   // 0x08
    uint16_t mF0A;                   // 0x0a
    uint8_t mF0C[4];                 // 0x0c  byte flags; CMesh::Init sets bit 1 of the first
};
static_assert(sizeof(sFaceSet) == 0x10, "sFaceSet layout");

struct sHFieldQuad {
    uint16_t mF00, mF02;
};
struct sHField {
    uint32_t mF00;                   // 0x00
    uint16_t mHeightCount;           // 0x04
    uint16_t mQuadCount;             // 0x06
    float mF08[4];                   // 0x08
    uint32_t mF18[2];                // 0x18
    float mF20[2];                   // 0x20
    Ptr32<float> mHeights;           // 0x28
    Ptr32<sHFieldQuad> mQuads;       // 0x2c
};
static_assert(sizeof(sHField) == 0x30, "sHField layout");

// A list of points and a list of 16-bit words (mWordPairs pairs of them).
struct tag_sShape {
    uint32_t mF00;                   // 0x00
    uint16_t mPointCount;            // 0x04
    uint16_t mWordPairs;             // 0x06
    Ptr32<s3DPoint> mPoints;         // 0x08
    Ptr32<uint16_t> mWords;          // 0x0c
};
static_assert(sizeof(tag_sShape) == 0x10, "tag_sShape layout");

// A node without geometry; also the first 0x58 bytes of every node, so the three links
// may point at either kind.
struct tag_sMiniNode {
    uint32_t mFlags;                 // 0x00
    Ptr32<tag_sNode> mF04;           // 0x04  the node's three links in the hierarchy
    Ptr32<tag_sNode> mF08;           // 0x08
    Ptr32<tag_sNode> mF0C;           // 0x0c
    sMatrix mMatrix;                 // 0x10
    uint16_t mF50;                   // 0x50
    uint16_t mF52;                   // 0x52
    Ptr32<char> mName;               // 0x54
};
static_assert(sizeof(tag_sMiniNode) == 0x58, "tag_sMiniNode layout");

struct tag_sNode : tag_sMiniNode {
    uint16_t mVertexCount;           // 0x58  positions, in sets of four (mVertices)
    uint16_t mFaceSetCount;          // 0x5a
    uint16_t mF5C;                   // 0x5c
    uint16_t mF5E;                   // 0x5e  words at mF94; mPoints has at least this many
    Ptr32<sVertexPosnSet> mVertices; // 0x60
    Ptr32<s3DPoint> mPoints;         // 0x64  as many as the largest face index, plus one
    Ptr32<sUVSet> mUVs;              // 0x68
    Ptr32<sFaceSet> mFaceSets;       // 0x6c
    Ptr32<sFace> mFaces;             // 0x70
    Ptr32<sUVSet> mF74;              // 0x74  not an offset in the file: left as read. A block from
                                     //       the engine allocator that DestroyBumpmappingNormals frees
    sAlignedBox mBox;                // 0x78
    uint8_t mF90[4];                 // 0x90
    Ptr32<uint16_t> mF94;            // 0x94
    Ptr32<sTexture> mF98;            // 0x98  not an offset in the file: left as read
    Ptr32<uint32_t> mMeshWords;      // 0x9c  not swapped by the loader; CMesh::Init points it at the
                                     //       node's words in the mesh object's buffer
};
static_assert(sizeof(tag_sNode) == 0xa0, "tag_sNode layout");

// What an embedded record's first pointer leads to. With flag 0x40000 in the record, four
// more words lie mBack bytes BEFORE this structure.
struct sTrackMorphingParent {
    uint16_t mF00;                   // 0x00
    uint16_t mBack;                  // 0x02
    uint32_t mF04[3];                // 0x04
};
static_assert(sizeof(sTrackMorphingParent) == 0x10, "sTrackMorphingParent layout");

struct sNodeLightSource {
    float mF00[6];
};
static_assert(sizeof(sNodeLightSource) == 0x18, "sNodeLightSource layout");

struct sEmbeddedData {
    uint32_t mFlags;                           // 0x00
    uint32_t mF04;                             // 0x04
    uint32_t mF08;                             // 0x08
    Ptr32<sTrackMorphingParent> mMorphParent;  // 0x0c
    Ptr32<sNodeLightSource> mLight;            // 0x10
};
static_assert(sizeof(sEmbeddedData) == 0x14, "sEmbeddedData layout");

// ---- animation -------------------------------------------------------------------------
struct sScaleKey {
    uint16_t mF00, mF02;             // 0x00
    float mF04[3];                   // 0x04
};
static_assert(sizeof(sScaleKey) == 0x10, "sScaleKey layout");

struct sQuatKey {
    uint16_t mF00, mF02;             // 0x00
    float mF04[4];                   // 0x04

    void Byteswap();   // engine 0x1004af68
};
static_assert(sizeof(sQuatKey) == 0x14, "sQuatKey layout");

// A rotation as four floats (the Mac's debug name; x, y, z and w in that order, as
// RAQuaternionToLAxis reads them). A sQuatKey's mF04 is one.
struct sQuaternion {
    float x, y, z, w;
};
static_assert(sizeof(sQuaternion) == 0x10, "sQuaternion layout");

struct sAnimatedTexture {
    uint16_t mF00;                   // 0x00
    uint16_t mPairCount;             // 0x02
    Ptr32<uint16_t> mPairs;          // 0x04  mPairCount pairs of words
};
static_assert(sizeof(sAnimatedTexture) == 8, "sAnimatedTexture layout");

struct sPathController {
    uint32_t mF00;                   // 0x00
    int32_t mWordCount;              // 0x04
    uint32_t mF08;                   // 0x08
    Ptr32<uint32_t> mWords;          // 0x0c
};
static_assert(sizeof(sPathController) == 0x10, "sPathController layout");

struct sUVTrackControl {
    uint16_t mF00, mF02;             // 0x00  the track's first place in the animation's mF0C and mF10 arrays, and
                                     // its number of keys (RAAnimateTextureCoords)
};
struct sUVAnim {
    uint32_t mTrackCount;            // 0x00
    Ptr32<sUVTrackControl> mTrack;   // 0x04
    int32_t mCount;                  // 0x08
    Ptr32<uint16_t> mF0C;            // 0x0c  mCount words
    Ptr32<float> mF10;               // 0x10  mCount pairs
};
static_assert(sizeof(sUVAnim) == 0x14, "sUVAnim layout");

// A compressed vertex: three 10-bit fields in one word, which the loader rewrites as it
// does a face's first word (see src/engine/M3D2Load.cpp, src/engine/EngineUtil.cpp, src/game/park/TPHField.cpp for the layout the engine reads).
struct sCVertex {
    uint32_t mPacked;
};
static_assert(sizeof(sCVertex) == 4, "sCVertex layout");

struct sCMDupedVert {
    uint16_t mF00, mF02;
};
struct sCMKeyData {
    uint16_t mVertexCount;           // 0x00
    uint16_t mKeyCount;              // 0x02
    Ptr32<uint16_t> mF04;            // 0x04  mKeyCount words
    Ptr32<uint16_t> mF08;            // 0x08  mVertexCount words
    Ptr32<sCVertex> mVertices;       // 0x0c  mVertexCount * mKeyCount of them
    uint32_t mF10;                   // 0x10
};
static_assert(sizeof(sCMKeyData) == 0x14, "sCMKeyData layout");

struct sMorphSet {
    uint16_t mF00;                   // 0x00
    uint16_t mKeyDataCount;          // 0x02
    uint16_t mF04;                   // 0x04
    uint16_t mDupedCount;            // 0x06
    uint16_t mF08;                   // 0x08
    uint8_t mF0A[2];                 // 0x0a
    Ptr32<sCMKeyData> mKeyData;      // 0x0c
    Ptr32<sCMDupedVert> mDuped;      // 0x10
    s3DPoint mF14;                   // 0x14
    s3DPoint mF20;                   // 0x20
};
static_assert(sizeof(sMorphSet) == 0x2c, "sMorphSet layout");

// What a node animation's morph pointer leads to when the animation has flag 0x4000.
struct sMorphPoints {
    Ptr32<void> mF00;                // 0x00
    uint32_t mPointCount;            // 0x04
    Ptr32<s3DPoint> mPoints;         // 0x08
};
static_assert(sizeof(sMorphPoints) == 0xc, "sMorphPoints layout");

struct tag_sNodeAnim {
    uint32_t mF00;                   // 0x00
    uint32_t mFlags;                 // 0x04  0x4000: mMorph is an sMorphPoints
    uint32_t mF08;                   // 0x08
    uint32_t mF0C;                   // 0x0c
    uint16_t mF10, mF12, mF14;       // 0x10
    uint16_t mEnableCount;           // 0x16
    Ptr32<tag_sShape> mShapes;       // 0x18
    Ptr32<sQuatKey> mQuatKeys;       // 0x1c
    Ptr32<sScaleKey> mScaleKeys;     // 0x20
    Ptr32<sPathController> mPath;    // 0x24
    Ptr32<sMorphSet> mMorph;         // 0x28
    Ptr32<sUVAnim> mUVAnim;          // 0x2c
    Ptr32<int16_t> mEnables;         // 0x30
    Ptr32<uint8_t> mF34;             // 0x34  records of eight bytes
    uint8_t mF38[4];                 // 0x38
    Ptr32<tag_sNode> mNode;          // 0x3c  not an offset in the file: left as read
};
static_assert(sizeof(tag_sNodeAnim) == 0x40, "tag_sNodeAnim layout");

struct tag_sAnimHeader {
    uint32_t mF00, mF04, mF08;                  // 0x00
    uint16_t mShapeCount;                       // 0x0c
    uint16_t mScaleKeyCount;                    // 0x0e
    uint16_t mQuatKeyCount;                     // 0x10
    uint16_t mNodeAnimCount;                    // 0x12
    uint16_t mF14, mF16;                        // 0x14
    uint16_t mAnimatedTextureCount;             // 0x18
    uint16_t mDisableCount;                     // 0x1a
    uint16_t mF1C;                              // 0x1c
    uint8_t mF1E[2];                            // 0x1e
    Ptr32<tag_sShape> mShapes;                  // 0x20
    Ptr32<sScaleKey> mScaleKeys;                // 0x24
    Ptr32<sQuatKey> mQuatKeys;                  // 0x28
    Ptr32<tag_sNodeAnim> mNodeAnims;            // 0x2c
    Ptr32<sAnimatedTexture> mAnimatedTextures;  // 0x30
    Ptr32<sUVAnim> mUVAnims;                    // 0x34
    Ptr32<uint16_t> mNodesToDisable;            // 0x38
    Ptr32<uint8_t> mF3C;                        // 0x3c  records of eight bytes
    Ptr32<void> mF40;                           // 0x40  not offsets in the file: left as read
    Ptr32<void> mF44;                           // 0x44
};
static_assert(sizeof(tag_sAnimHeader) == 0x48, "tag_sAnimHeader layout");

// ---- the header ------------------------------------------------------------------------
constexpr uint32_t kM3D2Magic = 0x1cd15d46;
constexpr uint32_t kM3D2Version = 0xdd;        // the newest the loader takes
constexpr uint32_t kM3D2AnimVersion = 0xcb;    // the one animation version it takes

struct tag_sM3D2Header {
    uint32_t mMagic;                            // 0x00
    uint32_t mVersion;                          // 0x04
    uint32_t mAnimVersion;                      // 0x08
    uint32_t mF0C, mF10, mF14;                  // 0x0c
    char mName[0x14];                           // 0x18
    uint32_t mF2C;                              // 0x2c
    uint32_t mFlags;                            // 0x30  1 geometry, 2 height field, 0x20 animation
    uint16_t mF34;                              // 0x34
    uint16_t mTextureCount;                     // 0x36  textures and texture infos
    uint16_t mVertexSetCount;                   // 0x38
    uint16_t mUVSetCount;                       // 0x3a
    uint16_t mFaceSetCount;                     // 0x3c
    uint16_t mFaceCount;                        // 0x3e
    uint16_t mShapeCount;                       // 0x40
    uint16_t mAllNodeCount;                     // 0x42  nodes and mini nodes
    uint16_t mNodeCount;                        // 0x44
    uint16_t mF46;                              // 0x46
    uint16_t mEmbeddedCount;                    // 0x48
    uint16_t mPointCount;                       // 0x4a
    Ptr32<char*> mPaths;                        // 0x4c  an offset like the others in the file; the game
                                                //       replaces it with its own table of folder names
    Ptr32<sTexture> mTextures;                  // 0x50
    Ptr32<sTextureInfo> mTextureInfos;          // 0x54
    Ptr32<sVertexPosnSet> mVertexSets;          // 0x58
    Ptr32<s3DPoint> mPoints;                    // 0x5c
    Ptr32<sUVSet> mUVSets;                      // 0x60
    Ptr32<sFaceSet> mFaceSets;                  // 0x64
    Ptr32<sFace> mFaces;                        // 0x68
    Ptr32<sHField> mHField;                     // 0x6c
    Ptr32<tag_sNode> mNodes;                    // 0x70
    Ptr32<tag_sMiniNode> mMiniNodes;            // 0x74
    Ptr32<tag_sNode> mF78;                      // 0x78
    Ptr32<sEmbeddedData> mEmbedded;             // 0x7c
    sAlignedBox mBox;                           // 0x80
    Ptr32<tag_sAnimHeader> mAnim;               // 0x98
    Ptr32<tag_sM3D2Header> mSelf;               // 0x9c  set by the loader
    float mFA0;                                 // 0xa0
    Ptr32<tag_sNode> mFA4;                      // 0xa4  not an offset in the file: left as read
    uint8_t mFA8[4];                            // 0xa8
    Ptr32<tag_sShape> mShapes;                  // 0xac
    Ptr32<void> mBlock;                         // 0xb0  the allocation the header lies in
    union {
        uint8_t mFB4[4];                        // 0xb4
        Ptr32<tag_sRideAnimSet> mRideSet;       // 0xb4  InstanceRide: the ride instance this mesh belongs to
    };
};
static_assert(sizeof(tag_sM3D2Header) == 0xb8, "tag_sM3D2Header layout");
static_assert(offsetof(tag_sM3D2Header, mPaths) == 0x4c && offsetof(tag_sM3D2Header, mAnim) == 0x98,
              "tag_sM3D2Header fields");

// The node (a full node or a mini node) of the header's embedded entry `entry` (RideInstance.cpp and TickRideHeads
// work it out the same way): entry i belongs to node mF46 + i, and the nodes after the full ones are mini nodes.
inline tag_sMiniNode* EmbeddedNode(tag_sM3D2Header* header, uint32_t entry) {
    const uint32_t index = entry + header->mF46;
    if (index < header->mNodeCount) return header->mNodes.get() + index;
    return header->mMiniNodes.get() + (index - header->mNodeCount);
}

// Load flags.
constexpr uint32_t kM3D2OldMeshesToo = 1;   // take versions before kM3D2Version
constexpr uint32_t kM3D2NoAnimation = 2;    // the file must have no animation (without it: must have one)
constexpr uint32_t kM3D2OneFrame = 4;       // see sTextureInfo

// Reads a mesh file and makes it usable in place. nullptr (with a line in the debug log)
// when the file cannot be opened, is not a mesh, is too old, or has or lacks animation
// against the flags. Free the result with gFree on its mBlock.
tag_sM3D2Header* LoadM3D2Header(char* name, uint32_t flags);   // engine 0x100483f4
void FreeM3D2Header(tag_sM3D2Header* header);                  // engine 0x10049574

// The loader's steps (src/engine/EngineUtil.cpp). Each one puts a table's records into the Mac's
// byte order (nothing to do on a little-endian host) and turns the offsets in them into
// pointers into the block at `base`.
void AlignTo64Bit(uint32_t& value, uint32_t* moved);     // 0x10039078
void AlignTo64Bit(void*& address, uint32_t* moved);      // 0x100390cc
// 0x10039120 and 0x10039174 (src/engine/EngineUtil.cpp): the same for a multiple of four.
void AlignTo32Bit(uint32_t& value, uint32_t* moved);
void AlignTo32Bit(void*& address, uint32_t* moved);
void FixMainPointers(tag_sM3D2Header* header, const void* base);                                      // 0x10049fe4
void FixEmbeddedRecords(sEmbeddedData* records, uint16_t count, const void* base);                    // 0x10049d38
void FixTextureInfoRecords(sTextureInfo* infos, uint16_t count, int32_t oneFrame, const void* base);  // 0x10049c38
void FixHeightFieldRecords(sHField* field, const void* base);                                         // 0x100495dc
void FixUVRecords(sUVSet* sets, uint16_t count, const void* base);                                    // 0x10049884
void FixShapeRecords(tag_sShape* shapes, uint16_t count, const void* base);                           // 0x100498e4
void FixTextureRecords(sTexture* textures, uint16_t count, const void* base);                         // 0x10049af4
void FixFaceSetRecords(sFaceSet* sets, uint16_t count, const void* base);                             // 0x1004a150
void FixFaceRecords(sFace* faces, uint16_t count, const void* base);                                  // 0x1004a21c
void FixNodeRecords(tag_sNode* nodes, uint16_t count, const void* base);                              // 0x1004a2d4
void FixNodePointers(tag_sNode* node, const void* base);                                              // 0x1004a5cc
void FixMiniNodeRecords(tag_sMiniNode* nodes, uint16_t count, const void* base);                      // 0x1004a6cc
void FixAnimPointers(tag_sAnimHeader* anim, const void* base);                                        // 0x1004a7fc
void FixAnimTextureRecords(sAnimatedTexture* textures, uint16_t count, const void* base);             // 0x1004a8dc
void FixAnimNodeRecords(tag_sNodeAnim* anims, uint16_t count, const void* base);                      // 0x1004a9f4
void FixAnimNodePointers(tag_sNodeAnim* anim, const void* base);                                      // 0x1004acbc
void FixAnimScaleKeyRecords(sScaleKey* keys, uint16_t count);                                         // 0x1004ad88
void FixAnimQuatKeys(sQuatKey* keys, uint16_t count, const void* base);                               // 0x1004aedc
void FixAnimNodesToDisable(uint16_t* nodes, uint16_t count, const void* base);                        // 0x1004afe0
void FixAnimPathRecords(sPathController* path, const void* base);                                     // 0x1004b044
void FixAnimEnableRecords(int16_t* enables, uint16_t count);                                          // 0x1004b11c
void FixAnimUVRecords(sUVAnim* anims, uint16_t count, const void* base);                              // 0x1004b17c
void FixAnimMorphRecords(tag_sNodeAnim* anim, const void* base);                                      // 0x1004b3dc

// Unit v5061 (src/game/park/TPHField.cpp)
// The ride's height map: a byte per cell (stride gRideHeightStride) and the bounds of the vertices seen.
struct sRideHeightMap {
    uint32_t mF00, mF04, mF08;   // 0x00
    Ptr32<uint8_t> mCells;       // 0x0c  rows of gRideHeightStride bytes
    uint32_t mF10, mF14;         // 0x10
    float mMin[3];               // 0x18
    float mMax[3];               // 0x24
};
static_assert(sizeof(sRideHeightMap) == 0x30, "sRideHeightMap layout");

extern int32_t gRideHeightStride;    // 0x102c2144
extern float gRideHeightLimitZ;      // 0x102c2148
extern float gRideHeightLimitX;      // 0x102c214c
extern int32_t gRideHeightRows;      // 0x102c2140 (defined in src/game/park/TPHField.cpp)
extern int32_t gRideHeightScan[50][4];   // 0x102c1e20  per row: min cell, max cell, height there, height there

// 0x1005d304 (1984 bytes). Walks the node tree; the matrix is passed by value.
int32_t CalculateMaxVertexHeights(tag_sNode* node, tag_sM3D2Header* header, sMatrix matrix, sRideHeightMap* map,
                                  tag_sNode** badNode, tag_sAnimHeader* anim, int32_t unused, int32_t* flag);

// Unit v5287 (src/game/park/TPHField.cpp)
// 0x1005c050 (96 bytes). Copies the quad table of `copy` over the header's own height field (mQuadCount words).
void RestoreHFieldDataFromCopy(tag_sM3D2Header* header, sHField* copy);
