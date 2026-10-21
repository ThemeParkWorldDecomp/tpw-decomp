// tb_drawprimitive.h: the engine's primitive renderer (engine library, original
// tb_drawprimitive.cpp: its asserts name the file; functions from 0x10006954, filed under
// tb_error.cpp by the inventory).
//
// PARTIAL: the virtual interface as far as translated callers use it. The constructor and
// destructor are in src/engine/DrawPrimitive.cpp, the primitive buffer (GetNextPrimitive to
// RoundOffPrimitive, below) in src/engine/DrawPrimitive.cpp, the polygon adder and the
// sprite (AddPoly_TLCTEX, DrawSprite) in src/engine/DrawPrimitive.cpp and the batching
// sprite (AddSprite, with the vertex record's four small members) in
// src/engine/tb_error.cpp, src/engine/DrawPrimitive.cpp; QSortFaces_float is still to do. CDrawPrimitive is a virtual base of
// COpenGLDrawPrimitive; on the Mac
// its vtable pointer is at +0x2c, and gei_DrawPrimitive points at that subobject. The
// slots, in order: the destructor, Init, SetBlendingMode, SetTexture, Execute,
// ExecuteShadow, SetFogParameters, BeginDraw, EndDraw, DrawTestPoly, DrawFilledBox, then
// DrawPoly, DrawEdgeBox, DrawProfileBox, GetPixelFillRate, DrawWiredframedBoundingBox,
// GetPixel, DrawDetectPoly (not declared yet).
#pragma once
#include <cstddef>
#include <cstdint>

#include "platform/msl.h"
#include "ppc_math.h"

struct SPrimitive;
struct SSpriteDrawParameters;
struct XZType;
struct SVertex;
class CTextureBase;

// A transformed and lit vertex with one set of texture coordinates (0x20 bytes: format
// 0x1c4, D3DFVF_XYZRHW | DIFFUSE | SPECULAR | TEX1).
struct SFVF_TLCTEX {
    float x, y, z, rhw;              // 0x00
    uint32_t colour;                 // 0x10
    uint32_t specular;               // 0x14
    float u, v;                      // 0x18

    // The vertex record's small members, out of line in the original (src/engine/tb_error.cpp). Their names
    // are the mangled ones; the parameters' names are invented.
    // The record `index` records on, `size` bytes apart (the product is taken in 32 bits, as the Mac does).
    SFVF_TLCTEX* ReturnPtr(int32_t size, int32_t index);   // 0x100087e8
    // The texture coordinates of set `set`: eight bytes a set from the first one at 0x18 (QUIRK: the set number moves the
    // WRITE, so a set above 0 lands inside the following fields or records; AddSprite only writes set 0).
    void SetUV(int32_t set, float u, float v);             // 0x10008824
    void SetDiffuse(uint32_t colour);                      // 0x10008864
    // Copies the position (x, y, z, w) of an SVertex in, taken by value (four words of the call).
    void SetVertex(SVertex vertex);                        // 0x1000889c
};
static_assert(sizeof(SFVF_TLCTEX) == 0x20, "SFVF_TLCTEX layout");

// What CClipping::CPP_TestData leaves for a polygon (the first two bytes of the four it writes):
// mAll is the AND of the vertices' outside codes (nonzero: the whole polygon is outside one plane
// and is not drawn), mAny the OR (the planes ClipPoly has to clip against; zero: nothing to clip).
struct ClipCodeType {
    uint8_t mAll;
    uint8_t mAny;
};

// Whether the triangle a, b, c of transformed vertices turns its front to the viewer: the
// determinant of the rows (x, y, rhw) of its corners is above zero. Inline in the original (no
// out-of-line copy; CHField::Execute and DrawSkyShadow and SRenderListEntry::CalcRenderEntry
// use the same nine globals), which keeps the nine values in globals first and compares two
// fused multiply-subtracts. Name invented; the globals are engine data 0x1017319c..0x101731bc
// (laid out in the reverse order of the declaration below: gFaceX0 at 0x101731bc, gFaceW2 at
// 0x1017319c; names invented, defined in src/engine/tb_hfield.cpp).
extern float gFaceX0, gFaceX1, gFaceX2, gFaceY0, gFaceY1, gFaceY2, gFaceW0, gFaceW1, gFaceW2;
inline bool SFVF_FacesViewer(const SFVF_TLCTEX& a, const SFVF_TLCTEX& b, const SFVF_TLCTEX& c) {
    gFaceX0 = a.x;
    gFaceY0 = a.y;
    gFaceW0 = a.rhw;
    gFaceX1 = b.x;
    gFaceY1 = b.y;
    gFaceW1 = b.rhw;
    gFaceX2 = c.x;
    gFaceY2 = c.y;
    gFaceW2 = c.rhw;
    const float left = ppc_fmsubs(gFaceX1, ppc_fmsubs(gFaceY2, gFaceW0, gFaceY0 * gFaceW2), gFaceW1 * (gFaceX0 * gFaceY2));
    const float right = ppc_fmsubs(gFaceX2, ppc_fmsubs(gFaceY1, gFaceW0, gFaceY0 * gFaceW1), gFaceW2 * (gFaceX0 * gFaceY1));
    return left > right;
}

struct SFVF_TLCTEX;
class CTextureBase;

// One primitive: a record of the render memory's table (0x38 bytes on the Mac). It is the
// record CRenderMemory::LoadInfo hands out (SRenderLoadInfo in tb_rendermemory.h keeps its
// bytes opaque and has the same size and pointer places), which GetNextPrimitive fills in
// and the renderer's Execute reads. Mac offsets in the comments. The host's pointers are
// wider: the texture takes 8 bytes (so the filler after it is 4 bytes shorter and every
// field up to mVertexCount keeps its Mac offset), and the next two fields move up with the
// vertex and index pointers.
struct SPrimitive {
    enum : uint32_t {
        kTypeMask = 0x0000000f,      // ExecuteCurrentPrimitive passes the low nibble to Project
        kDrawn = 0x00000010,         // set once the renderer has executed it
        kProject = 0x00000020,       // project the vertices before executing
        kCheckFog = 0x00000040,      // work out the fog intensities before executing
        kExecuteNow = 0x00000200,    // execute at once whatever the blending mode says
        kRoundOff = 0x00000400,      // RoundOffPrimitive (an empty function)
    };
    enum : uint32_t {
        kFogged = 0x00080000,        // blending mode: the vertices got a fog intensity
        kFogCleared = 0x00100000,    // blending mode: cleared before the fog is worked out
        kNoDraw = 0x00000800,        // blending mode: a primitive that is never sorted or drawn
        kSortSkipped = 0x00001000,   // blending mode: with renderer 4 it is not sorted either
    };

    uint32_t mFlags;             // 0x00
    uint32_t mBlendingMode;      // 0x04  SetBlendingMode's argument
    void* mTexture;              // 0x08  SetTexture's argument
    uint8_t mF0C[8];             // 0x0c  three words on the Mac, not used by this unit
    int32_t mPrimitiveType;      // 0x18  a _D3DPRIMITIVETYPE
    uint32_t mVertexFormat;      // 0x1c  0xc4, 0x1c4, 0x2c4 or 0x3c4
    int32_t mVertexCount;        // 0x20
    int32_t mIndexCount;         // 0x24
    uint8_t* mVertexData;        // 0x28
    uint16_t* mIndexData;        // 0x2c  (0x30 on the host)
    float mDepth;                // 0x30  the sum of the vertices' depths, divided by their count when executed
    int32_t mF34;                // 0x34  -1 when made
    // Mac 0x0c and 0x10, inside the words mF0C stands for: the pages of a second and third
    // texture stage, which SRenderListEntry::CalcRenderEntry sets for a mesh with a second set
    // of texture coordinates (the OpenGL renderer draws one stage and never reads them). Two
    // pointers do not fit the host's eight bytes of mF0C, so they are kept here, after the
    // Mac's fields; the render memory's record (SRenderLoadInfo) grew by the same 0x10 bytes.
    void* mTexture1;             // Mac 0x0c
    void* mTexture2;             // Mac 0x10
};
static_assert(sizeof(void*) != 8 || (offsetof(SPrimitive, mPrimitiveType) == 0x18 &&
                                      offsetof(SPrimitive, mVertexCount) == 0x20 &&
                                      offsetof(SPrimitive, mVertexData) == 0x28 &&
                                      sizeof(SPrimitive) == 0x50),
              "SPrimitive layout on a 64-bit host");

// Bytes in one vertex of the format GetNextPrimitive was given. Inline in tb_typedef.h
// (its assertion names line 427 of that file) and copied into GetNextPrimitive and
// DuplicateCurrentPrimitive; the name is invented. Any other format asserts and gives 0.
inline int32_t SFVF_VertexSize(uint32_t format) {
    switch (format) {
    case 0xc4: return 0x18;
    case 0x1c4: return 0x20;
    case 0x2c4: return 0x28;
    case 0x3c4: return 0x30;
    }
    msl_assertion_failed("0", "tb_typedef.h", 0x1ab);
    return 0;
}

// The one list of vertex numbers that CClipping::ClipPoly clips in place: a polygon of
// `count` vertices whose numbers are at `list` (8 bytes on the Mac).
struct VertexClipList {
    int32_t mCount;              // 0x00
    uint16_t* mList;             // 0x04
};

class CDrawPrimitive {
public:
    CDrawPrimitive(); // 0x10006954
    virtual ~CDrawPrimitive();
    virtual int32_t Init() = 0;
    virtual int32_t SetBlendingMode(int32_t mode) = 0;
    virtual int32_t SetTexture(int32_t stage, void* texture) = 0;
    virtual int32_t Execute(SPrimitive* primitive) = 0;
    virtual int32_t ExecuteShadow(SPrimitive* primitive) = 0;
    virtual int32_t SetFogParameters(float start, float end, uint32_t colour) = 0;
    virtual int32_t BeginDraw() = 0;
    virtual int32_t EndDraw() = 0;
    virtual int32_t DrawTestPoly() = 0;
    // Screen-space box from (x1, y1) to (x2, y2); 0 on failure.
    virtual int32_t DrawFilledBox(float x1, float y1, float x2, float y2, int32_t colour, CTextureBase* texture) = 0;
    // Vtable slot 0x38 (DrawPoly, slot 0x34, is not declared yet); CMapWho::RenderWindow
    // calls it for renderers 1 and 3 and ignores the result. Pure in the original (a null
    // slot in CDrawPrimitive's own vtable 0x100516f8); it has a body here only so that the
    // classes that do not override it yet (COpenGLDrawPrimitive, whose DrawEdgeBox is
    // 0x10044420, and the tests' fakes) stay concrete. Make it pure when they do.
    virtual int32_t DrawEdgeBox() { return 0; }
    // The other drawing members, in the original's order after DrawEdgeBox. Pure in the
    // original too, with bodies here for the same reason. COpenGLDrawPrimitive's are in
    // src/engine/DrawPrimitiveOpenGL.cpp.
    virtual int32_t DrawPoly(XZType* points, int32_t count, int32_t colour, float scale) { return 0; }
    virtual int32_t DrawProfileBox() { return 0; }
    virtual float GetPixelFillRate() { return 0.0f; }
    virtual int32_t DrawWiredframedBoundingBox(SVertex* corners, int32_t unused, int32_t faces) { return 0; }
    virtual int32_t GetPixel(int32_t x, int32_t y, uint32_t* pixel) { return 0; }
    virtual int32_t DrawDetectPoly(float size, int32_t colour, int32_t unused, float u0, float v0, float u1,
                                   float v1) { return 0; }
    // Mac 0x20 (mFields[8]): the emboss mode; texture loading chooses the extra image planes
    // from it (tb_loaders.cpp).
    int32_t EmbossMode() const { return static_cast<int32_t>(mFields[8]); }
    uint32_t mFields[11]; // original 0x00..0x28; vptr at 0x2c

    // The primitive buffer (src/engine/DrawPrimitive.cpp; the asserts name
    // tb_drawprimitive.cpp, the stride inline tb_typedef.h). A primitive is made in the
    // render memory's table, filled with vertices and indices by the caller (AddClipPolyList
    // and the callers' own writes, then counted in the record), and executed. `flags` is
    // CRenderMemory::LoadInfo's (kStatic, kDynamic, kKeepCurrent).
    //
    // Makes the next record and keeps it as the current one; 0 when the memory has none.
    // The blending mode `mode` is what the renderer's SetBlendingMode takes.
    SPrimitive* GetNextPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat, int32_t mode,
                                 void* texture);                   // 0x10006a58
    // The same from a copy of the remembered record (LoadInfo_CopyVDB).
    SPrimitive* DuplicateCurrentPrimitive(int32_t flags, int32_t primitiveType, uint32_t vertexFormat,
                                          int32_t mode, void* texture);   // 0x10006bec
    // Executes a primitive the caller built itself; 1 (and nothing done) when it is empty.
    int32_t ExternExecute(SPrimitive* primitive);                  // 0x10006eb4
    // Finishes the current primitive (fog, projection, depth), then executes it now or sorts
    // its faces for the sorted draw; the current primitive is cleared. 0 when the renderer fails.
    int32_t ExecuteCurrentPrimitive();                             // 0x10006f40
    // Counts the current primitive's memory in the render memory, optionally sets its depth
    // (-1.0 keeps the one it has) and executes it. 1 when there is nothing to execute.
    int32_t ExecuteCurrentPrimitive(float depth);                  // 0x10007360
    // Executes the primitive now if its blending mode is one of the instant ones.
    int32_t ExecutePrimitiveIfInstant(SPrimitive* primitive);      // 0x10007444
    // Clips a triangle list against the view and adds what is left to the current primitive's
    // indices; vertexCount is the vertices the caller wrote at its start (clipping adds
    // some). Returns the indices written, 0 if there is no room or all of it is outside.
    int32_t AddClipPolyList(int32_t& vertexCount, uint16_t* indices, int32_t indexCount, int32_t mask);   // 0x100074fc
    // Works out the fog intensities of `count` vertices when fog is on.
    void ApplyFog(SFVF_TLCTEX* vertices, int32_t count);           // 0x10007c4c
    void CheckFogOnCurrentPrimitive();                             // 0x10007d08
    // Adds the math unit's projection of the current primitive's vertices to its depth.
    void ProjectCurrentPrimitive(int32_t type);                    // 0x10007df4
    void RoundOffPrimitive();                                      // 0x10007ea0 (empty)

    // The blending mode bits the renderer executes at once (mFields[2]; 0x3000 for OpenGL).
    uint32_t InstantMask() const { return mFields[2]; }
    // Mac 0x18 (mFields[6]): bytes in a vertex of the current primitive.
    int32_t VertexSize() const { return static_cast<int32_t>(mFields[6]); }
    void SetVertexSize(int32_t size) { mFields[6] = static_cast<uint32_t>(size); }

    // Pointers on the Mac, so they do not fit mFields[7], [9] and [10] on the host (those
    // stay as the words the constructor leaves and nothing reads them):
    SPrimitive* mCurrent = nullptr;      // 0x1c  the primitive being built
    uint8_t* mVertexEnd = nullptr;       // 0x24  the end of the vertex memory (CRenderMemory::GetVertexEndPosition)
    uint16_t* mIndexEnd = nullptr;       // 0x28  the end of the index memory

    // One camera-facing sprite from four corners, as a primitive of its own (src/tb_drawprimitive_
    // sprite.cpp). `mode` is the mask of the clip planes whose outside codes count (-1 all, -3 all
    // but the far plane). 1 when nothing is drawn, else what ExecuteCurrentPrimitive answers (0
    // when the renderer fails).
    int32_t DrawSprite(SSpriteDrawParameters* sprite, int32_t mode);                // 0x10007edc
    // The same corners added to the primitive being built instead of a primitive of their own (the sprite manager
    // executes it once for a whole kind of sprite); `mode` as DrawSprite's. Not translated here yet: the shadows
    // (SSpriteGameData::AddRegularShadow) call it, 0 when the renderer fails.
    int32_t AddSprite(SSpriteDrawParameters* sprite, int32_t mode);                 // 0x10008230
    // Adds a polygon of `count` vertices `stride` bytes apart to the current primitive: clipped
    // against the planes `clip` names (worked out here when `clip` is NULL), its vertices
    // appended and a fan of triangles added to the indices, wound the other way with `flags` & 2.
    // Always answers 1, drawn or not (src/engine/DrawPrimitive.cpp).
    int32_t AddPoly_TLCTEX(SFVF_TLCTEX* vertices, int32_t count, int32_t stride, ClipCodeType* clip,
                           int32_t flags);                                          // 0x100078c0

};

// What DrawSprite takes (a stack record in its callers: CSky::RenderAdd, CParticle::PlotParticles,
// CRain::__LightningFlash; 0x2c bytes on the Mac, whose vertex pointer is a 32-bit address). Four
// corners of 0x10 bytes each: x, y, z, w (clip space, in the order the picture's corners (u0, v0),
// (u1, v0), (u1, v1), (u0, v1) go with). DrawSprite reads every field except mF14 and changes two:
// the blending mode gains 0x100000 when the display's fog is on, and the colour is ANDed with the
// light manager's.
struct SSpriteDrawParameters {
    uint32_t mFlags;           // 0x00  2 winds the polygon the other way, 0x400 marks the primitive for round-off;
                               //       the other bits (RenderAdd's 1 and 4) are not read
    void* mVertices;           // 0x04
    int32_t mF08;              // 0x08  the colour of all four corners (-1 white)
    uint32_t mBlendingMode;    // 0x0c
    CTextureBase* mTexture;    // 0x10  the page the picture is on, or null
    uint32_t mF14;             // 0x14  not read
    float mU0, mV0;            // 0x18  the picture's corner on the page
    float mU1, mV1;            // 0x20  and the opposite one
    int32_t mF28;              // 0x28  the projection's type (CTransformBase::Project's last argument)
};

// The face sorter of ExecuteCurrentPrimitive: orders the faces by their depth keys,
// `high` inclusive, permuting the triples of vertex numbers with them. Not translated.
void QSortFaces_float(float* keys, uint16_t* indices, int32_t low, int32_t high);   // 0x10006d40


extern CDrawPrimitive* gei_DrawPrimitive;   // engine data 0x10052b3c
