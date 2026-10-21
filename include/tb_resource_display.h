// tb_resource_display.h: the engine's display object and render list (engine library,
// original tb_resource_display.cpp; CDisplay::CDisplay at 0x10027de0).
//
// CDisplay holds no pointers, so its layout is the original's on every host (0x93c bytes;
// the fields named mF<offset> are written by the constructor and not yet seen in use). The
// object is 0x94c bytes on the Mac: the last 0x10 are not set by the constructor (see
// gHotSprite below).
#pragma once
#include <cstddef>
#include <cstdint>

#include "platform/d3d_types.h"

struct SSpriteGameData;

// One colour channel of the display's pixel format, as CDirectDraw::Init passes it to
// CDisplay::SetBitPattern: the channel mask, ShiftFactor(mask) (the lowest set bit) and
// BitsSet(mask) (the number of bits).
struct SBitData {
    uint32_t mMask;     // 0x00
    int32_t mShift;     // 0x04
    int32_t mBits;      // 0x08
};

class CDisplay {
public:
    enum : uint32_t {
        kFlag200 = 0x00000200,   // CMatrix::Projection: set -> m[3][3] = 1, clear -> m[2][3] = sin(fov/2)
        kFlag10000 = 0x00010000, // cleared by SetDisplay when the renderer is 4
        // The names in CMapWho::Render's assertion that the two are not both set.
        kSpecularEnable = 0x00000400,   // DISPLAY__SPECULARENABLE
        kEnableFog = 0x00002000,        // DISPLAY__ENABLEFOG
        kTestWholeMap = 0x00000020,     // CHField::DivideAndConquer tests every cell, not those near the camera
        // CHField::Execute: with it set, cells with quad flag 0x400 keep their full colour
        // (clear: their vertex colours are halved and lose the low byte).
        kFlag800 = 0x00000800,
    };

    CDisplay();                                                   // 0x10027de0
    // The screen size and depth; the window becomes the whole screen.
    void SetDisplay(int32_t width, int32_t height, int32_t depth);   // 0x10027f2c
    // The part of the screen that is drawn into, in fractions of the screen: x and y of
    // the corner (y from the bottom), then the size.
    void SetWindow(float x, float y, float width, float height);     // 0x1002806c
    void ResetWindow();                                              // 0x10028200
    // Keeps the four channels; for a 16-bit display also builds the two tables that
    // convert a byte to its place in a pixel.
    void SetBitPattern(SBitData alpha, SBitData red, SBitData green, SBitData blue);   // 0x1002827c
    void SetZSpace(float scale);                                     // 0x10028470
    // The first argument is not used (ZSetup reads only b and c).
    void ZSetup(float a, float b, float c);                          // 0x100284c4
    // Where the fog starts and ends.
    void SetFogSpace(float nearDistance, float farDistance);         // 0x10028558
    void SetFogColor(uint32_t argb);                                 // 0x100285c8
    // The top bit of each colour mask, and the three together.
    void SetMaxBits();                                               // 0x10028664
    void OutputDebugInfo();                                          // 0x10028718 (empty)

    uint32_t mFlags;           // 0x00  0x410100 after construction
    float mF04;                // 0x04  height / width; Matrix_Init scales gScreenMatrix x by 2 * this
    int32_t mWidth;            // 0x08  screen width in pixels
    int32_t mHeight;           // 0x0c  screen height
    float mHalfWidth;          // 0x10  (width - 0.999) / 2
    float mHalfHeight;         // 0x14
    float mTwoOverWidth;       // 0x18  2 / width
    float mTwoOverHeight;      // 0x1c
    int32_t mF20;              // 0x20  0
    int32_t mBitsPerPixel;     // 0x24  display mode dwRGBBitCount (16, 24 or 32)
    int32_t mWindowWidth;      // 0x28  the window in pixels
    int32_t mWindowHeight;     // 0x2c
    float mWindowCentreXBiased;   // 0x30  mWindowCentreX + 0.24975
    float mWindowCentreYBiased;   // 0x34
    float mWindowCentreX;      // 0x38  window origin + half its size
    float mWindowCentreY;      // 0x3c
    float mWindowHalfWidth;    // 0x40  (window width - 0.999) / 2
    float mWindowHalfHeight;   // 0x44
    float mZ48;                // 0x48  depth scale (ZSetup multiplies its results by it)
    float mZ4C;                // 0x4c  0.003; AdjustProjection's clamp distance factor
    float mZ50;                // 0x50  0.0
    float mZ54;                // 0x54  0.1
    float mZ58;                // 0x58  0.2
    float mZ5C;                // 0x5c  1.0
    float mZ60;                // 0x60  mZ48 * (mZ54 - mZ50); CMatrixPowerPC::Project type 2 depth scale
    float mZ64;                // 0x64  mZ48 * mZ54
    float mZ68;                // 0x68  ZSetup: z/w split point (Project default type)
    float mZ6C;                // 0x6c  ZSetup outputs, used by CMatrixPowerPC::Project
    float mZ70;                // 0x70
    float mZ74;                // 0x74
    float mZ78;                // 0x78
    float mZ7C;                // 0x7c  CMatrixPowerPC::Project type 1 depth scale
    int32_t mFrameCount;       // 0x80  incremented by every flip (RenderSystem_FlipScreen)
    int32_t mF84;              // 0x84  zeroed by every flip
    SBitData mAlphaBits;       // 0x88  SetBitPattern(alpha, red, green, blue)
    SBitData mRedBits;         // 0x94
    SBitData mGreenBits;       // 0xa0
    SBitData mBlueBits;        // 0xac
    uint32_t mTable16A[256];   // 0xb8   16-bit displays only (SetBitPattern)
    uint32_t mTable16B[256];   // 0x4b8
    float mF8B8;               // 0x8b8  1.5
    uint32_t mFogColor;        // 0x8bc  0x40008000 after construction
    float mFogBlue;            // 0x8c0  the colour's bytes as floats, 0..255
    float mFogAlpha;           // 0x8c4
    float mFogRed;             // 0x8c8
    float mFogGreen;           // 0x8cc
    float mFogNear;            // 0x8d0  100
    float mFogFar;             // 0x8d4  130
    float mFogScale;           // 0x8d8  1 / (far - near), 0 when far is not beyond near
    float mFogNearSquared;     // 0x8dc
    float mFogFarSquared;      // 0x8e0
    float mFogScaleSquared;    // 0x8e4
    float mF8E8;               // 0x8e8  0.3
    int32_t mF8EC;             // 0x8ec  0x80
    int32_t mF8F0;             // 0x8f0  0x20
    int32_t mF8F4;             // 0x8f4  1
    int32_t mF8F8;             // 0x8f8  0
    int32_t mF8FC;             // 0x8fc  0x4f
    float mFirstPersonViewDistance;   // 0x900  400; EngineStartup_SetOptions: 200, 325, 450 or 700
    uint32_t mRedMask, mRedMaskBits;   // 0x904, 0x908  (SetMaxBits: the mask's top bit)
    uint32_t mGreenMask, mGreenMaskBits; // 0x90c, 0x910
    uint32_t mBlueMask, mBlueMaskBits;  // 0x914, 0x918
    uint32_t mRGBMask;                // 0x91c
    uint32_t mTopBitsMask;            // 0x920  SetMaxBits: the three top bits together
    uint32_t mFade;                   // 0x924  fade amount, 0..255
    // 0x928..0x934  CHField::DivideAndConquer leaves the camera's surroundings here, in cells
    //               relative to the map's origin and clamped to -128..127: the columns from
    //               mF928 to mF92C and the rows from mF930 to mF934 (half the view distance
    //               each way).
    int32_t mF928;
    int32_t mF92C;
    int32_t mF930;
    int32_t mF934;
    float mF938;                      // 0x938  0.5
};

static_assert(offsetof(CDisplay, mWidth) == 0x08 && offsetof(CDisplay, mBitsPerPixel) == 0x24 &&
                  offsetof(CDisplay, mZ48) == 0x48 && offsetof(CDisplay, mZ54) == 0x54 &&
                  offsetof(CDisplay, mZ60) == 0x60 && offsetof(CDisplay, mZ68) == 0x68 &&
                  offsetof(CDisplay, mZ7C) == 0x7c && offsetof(CDisplay, mFrameCount) == 0x80 &&
                  offsetof(CDisplay, mRedBits) == 0x94 && offsetof(CDisplay, mBlueBits) == 0xac &&
                  offsetof(CDisplay, mTable16B) == 0x4b8 && offsetof(CDisplay, mFogColor) == 0x8bc &&
                  offsetof(CDisplay, mFirstPersonViewDistance) == 0x900 &&
                  offsetof(CDisplay, mTopBitsMask) == 0x920 && sizeof(CDisplay) == 0x93c,
              "CDisplay field offsets");

extern CDisplay gei_Display;   // engine data 0x10165634

// The Mac's gei_Display continues for 0x10 bytes the constructor does not set: +0x93c and
// +0x940 are floats SSpriteGameData::SetupVertices (0x1002f688) compares a sprite's screen
// position with, +0x944 is not known, and +0x948 is a pointer to the sprite nearest to that
// point, set by SetupVertices and cleared at the start of CSpriteManager::RenderSprites. A
// pointer does not fit the display's fixed layout on a 64-bit host, so it is a global here.
extern SSpriteGameData* gHotSprite;   // gei_Display + 0x948
// gei_Display + 0x93c and + 0x940 (the same declarations as include/sprite_engine.h, which says where they are defined): the
// screen point SSpriteGameData::SetupVertices looks for the sprite nearest to.
extern float gSpriteHotX;
extern float gSpriteHotY;

struct SMapLinkType;
struct tag_sNode;
struct sFaceSet;
struct SPrimitive;
class CMatrix;
class CLight;
class CTextureEntry;

// One thing to draw: a mesh node, added by CRenderList::AddEntry (0x14 bytes on the Mac; the
// pointers make the host's longer). Its members turn it into a primitive for the renderer.
struct SRenderListEntry {
    SMapLinkType* mLink;           // 0x00  gei_MapWho->mF30, the map list being prepared
    tag_sNode* mNode;              // 0x04
    uint32_t mFlags;               // 0x08  AddEntry's argument ored with gRenderEntryFlags
    uint8_t* mVertices;            // 0x0c  the node's vertices, transformed (render memory scratch space)
    void* mData;                   // 0x10  the node's points (node flag 0x1000000) or, four bytes in, its mesh words

    // The blending mode of a face set drawn with a texture (src/engine/RenderList.cpp).
    int32_t GetFaceSetBlendMode(sFaceSet* faceSet, CTextureEntry* texture);   // 0x10025e8c
    // Starts the primitive for one face set; *extra is a value CalcRenderEntry takes back.
    // NULL when there is nothing to draw (src/engine/RenderList.cpp).
    SPrimitive* SetupPrimitive(int32_t faceSet, int32_t* extra);   // 0x10026044
    // Adds one face set's vertices and the faces that turn to the viewer to the primitive;
    // `extra` is the vertex size SetupPrimitive gave back (0x20, or 0x28 with a second set of
    // texture coordinates). Always 1 (src/engine/RenderList.cpp).
    int32_t CalcRenderEntry(SPrimitive* primitive, int32_t faceSet, int32_t extra);   // 0x1002477c
};

// One face set waiting to be drawn, in RenderByTexture's work array (0x14 bytes on the Mac).
// Sorted by texture page in a tree of groups; inside a group the blended ones are kept
// sorted by blending mode and texture entry, the others in the order they come.
struct SRenderFace {
    SRenderListEntry* mEntry;      // 0x00
    CTextureEntry* mTexture;       // 0x04  the face set's texture entry, NULL when it has none
    int32_t mBlendMode;            // 0x08  0x1256 without a texture
    uint8_t mSorted;               // 0x0c  1: the blending mode asks for sorted drawing
    uint8_t mFaceSet;              // 0x0d  index in the node's face sets
    SRenderFace* mNext;            // 0x10  the next of the same group and kind
};

// One texture page of RenderByTexture's tree (0x14 bytes on the Mac).
struct SRenderGroup {
    uintptr_t mPage;               // 0x00  the texture page, 0 for the faces that have none
    SRenderGroup* mAbove;          // 0x04  the group of the pages that compare higher
    SRenderGroup* mBelow;          // 0x08  and lower (unsigned)
    SRenderFace* mSorted;          // 0x0c  the faces whose blending mode asks to be sorted
    SRenderFace* mUnsorted;        // 0x10  the others, last added first
};

extern SRenderGroup gRenderGroups[0x1000];   // engine data 0x100b162c (not exported; name invented)
extern SRenderFace gRenderFaces[0x8000];     // engine data 0x100c562c (up to the pointer to the list)

// The list of things to draw this frame (0x14004 bytes on the Mac).
class CRenderList {
public:
    CRenderList() : mCount(0) {}   // inline in RenderList_Init

    enum : int32_t { kCapacity = 0x1000 };

    // Empties the list.
    void Reset();                  // 0x10026478
    // Adds a node unless it has no vertices or the render memory is out of scratch space;
    // the vertices go through the math unit at once. 0 (and an error pushed) when the list is
    // full, else 1. The two middle arguments are not used.
    int32_t AddEntry(CMatrix& view, CMatrix& unused, CLight** lights, tag_sNode* node, int32_t flags);   // 0x100262e0
    // Draws everything added, face set by face set, texture page by texture page; 0 when the
    // renderer fails.
    int32_t RenderByTexture();     // 0x100264b0
    // RenderByTexture, then the render memory's unrendered primitives; 0 on failure.
    int32_t Render();              // 0x10026954

    int32_t mCount;                // 0x00
    SRenderListEntry mEntries[kCapacity];   // 0x04
};
extern CRenderList* gei_RenderList;   // engine data 0x1016562c (not exported; name invented)

// The dot product of two vectors passed by value (the file's range, between the entries and the
// list); y times y first, then the x and z terms as fused multiply-adds. src/engine/RenderList.cpp.
float DotProduct(D3DVECTOR a, D3DVECTOR b);   // 0x10025cac

// 1 when the list was made, 0 when there already is one (0x10024650); Close: whether
// there was one to free (0x10024704).
int32_t RenderList_Init();
int32_t RenderList_Close();

#include "tb_rain.h"
