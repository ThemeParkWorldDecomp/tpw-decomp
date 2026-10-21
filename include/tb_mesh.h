// tb_mesh.h: the engine's mesh object (engine library, 0x1001cb44..0x1001f1ec; the
// inventory files the range under tb_profiler.cpp, the original name is not in the binary).
//
// A CMesh wraps one loaded mesh file (include/m3d2.h): Init gives every texture record
// its texture object, loads the textures and makes a buffer of words for the nodes; the
// engine then draws the object through the map lists (its virtual base). 0x15c bytes on
// the Mac: +0 the pointer to the CMapLinkBase at +0x154, +0x150 the vtable; the host's
// compiler lays the class out its own way.
#pragma once
#include <cstdint>

#include "MapWho.h"
#include "Ptr32.h"
#include "m3d2.h"
#include "tb_matrix.h"

class CsTexture;
class CTextureEntry;
class CLight;
struct SFVF_TLCTEX;

// What the ride base and ride sign code remembers of a texture record it has given
// another texture: the texture it had, and the record's pointer field.
struct SOldCsTexture {
    SOldCsTexture() : mOld(nullptr), mWhere(nullptr) {}   // 0x1001eb94
    CsTexture* mOld;                 // 0x00
    Ptr32<CsTexture>* mWhere;        // 0x04
};

class CMesh : public virtual CMapLinkBase {
public:
    enum : uint32_t {
        kInitialised = 0x10000000,
        kRideBase = 0x20000000,
        kRideSign = 0x40000000,
    };

    CMesh();                                 // 0x1001cb44: also registers the link base
    ~CMesh() override;                       // 0x1001cd68
    int32_t Init(tag_sM3D2Header* header);   // 0x1001ce38
    int32_t Close();                         // 0x1001d244
    void SetLocalMatrix(int32_t flags, CMatrix& matrix); // 0x1001d3d0
    void SetLocalMatrix(int32_t flags, sMatrix& matrix); // 0x1001d654
    int32_t Prepare() override;              // 0x1001df78 (src/engine/Mesh.cpp)
    // The per-frame members (src/engine/Mesh.cpp). Preprocess walks a node, its siblings (mF08)
    // and their children (mF0C) with two matrices per level; a node in view is lit, added to the
    // render list and has its textures asked for. 0 when the render list refuses a node.
    int32_t Preprocess(tag_sNode* node, CMatrix* matrix, CMatrix* matrix2);   // 0x1001dccc
    // Asks the texture cache for the textures of a node's face sets (and the node's extra one),
    // with the depth CClipping::CheckBoundingBox just worked out as their priority.
    void PreprocessTextures(tag_sNode* node);                                // 0x1001db1c
    // Brings the lights up to date for a node and works out its vertex colours (into the mesh
    // words, after the frame stamp). Always 1.
    int32_t StaticCalculateLights(tag_sNode* node, CMatrix& matrix, CLight** lights, int32_t flags);   // 0x1001e26c
    int32_t DestroyBumpmappingNormals();     // 0x1001e468
    void* GetData() override;                // 0x1001e598
    int32_t GetLinkType() override;          // 0x1001e5c4: 2
    int32_t DeleteRideBase();                // 0x1001ebd0
    int32_t DeleteRideSign();                // 0x1001f020
    int32_t GenerateRideSign(uint32_t* indices, int32_t count); // 0x1001ed9c: indices become arena offsets of pixel data
    uintptr_t GetTexturePointer(int32_t index);                // 0x1001f120: native pixel pointer
    int32_t GenerateRideBase(int32_t a, int32_t b, int32_t c, int32_t d);   // 0x10239a90 (glue import; GetBlendedBaseTextures)
    // Unit v5752 (src/engine/Mesh.cpp). The cells x0..x1, y0..y1 (the arguments a..d
    // above are x0, y0, x1, y1) of the height field with both overlay flags get generated textures.
    int32_t GenerateRideBase(CTextureEntry* target, CTextureEntry* source, int32_t x, int32_t y, int32_t flags);   // 0x1001e644
    void CenterMatrix(int32_t flags, float scale);   // 0x1001d97c

    uint32_t mFlags;                    // 0x004
    tag_sM3D2Header* mHeader;           // 0x008
    uint32_t mF0C;                      // 0x00c  not set by the constructor
    CMatrix mMatrix;                    // 0x010
    CMatrix mMatrix2;                   // 0x050
    int32_t mF90;                       // 0x090  -1
    int32_t mF94;                       // 0x094  -1
    uint32_t mF98[33];                  // 0x098
    uint32_t mF11C;                     // 0x11c
    uint32_t mF120;                     // 0x120  not set by the constructor
    SOldCsTexture* mRideBaseOld;        // 0x124
    CsTexture* mRideBaseTextures;       // 0x128
    int32_t mRideBaseCount;             // 0x12c
    SOldCsTexture* mRideSignOld;        // 0x130
    CsTexture* mRideSignTextures;       // 0x134
    int32_t mRideSignCount;             // 0x138
    uint32_t mF13C;                     // 0x13c
    uint32_t mF140;                     // 0x140  the callback's slot (see mCallback)
    uint32_t* mNodeWords;               // 0x144  from the engine allocator: for each node one word
                                        //        and one for each of its mF5E
    uint32_t* mNodeWordCursor;          // 0x148
    uint32_t mF14C;                     // 0x14c  not set by the constructor
    // The word at 0x140 is a pointer to the function Prepare calls (with the header) when a
    // new frame starts: a code pointer, which a uint32_t cannot hold on the host. The member
    // above is kept as it was and this one takes its place; it starts null, as the
    // constructor leaves 0x140. Nothing in the engine sets it.
    void (*mCallback)(tag_sM3D2Header*) = nullptr;
};

// Multiplies the colours of `count` vertices `stride` bytes apart by `colour` (each byte
// channel: (a * b) >> 8, packed back). The original reads the colour of the FIRST vertex on
// every round (its pointer is never advanced; only the one it writes through is) and stores
// the product at each vertex in turn. With a stride that does not reach the first colour again,
// the first vertex ends with P(first) and every other one with P(P(first)), P being the
// product. Kept.
void ColorVertices(SFVF_TLCTEX* vertices, uint32_t colour, int32_t count, int32_t stride);   // 0x10003570
