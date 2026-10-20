// The UI's draw objects (main app): what a control of the interface draws itself with, and
// the library that keeps the shared ones by name. The base class and the library are in
// the range the inventory files under ui_cityLobbyController.cpp (0x101cccc0..0x101cd42c),
// the engine mesh objects under ui_utils.cpp (0x10187a28..0x10188fb0).
// PARTIAL: the engine mesh object's Change, CalculateFrame and InstanceMe, the gradients' Change and the matrix helpers they use (GetUIMatrix,
// UpdateMeshLocalMatrix) are in src/ui_DrawObjectEngineMesh.cpp and src/ui_matrix.cpp (unit w42); the frame and slider objects' own Change and
// InstanceMe are in src/ui_DrawObjectResizable.cpp (unit w44); the gradients' other members are untranslated. The sprite draw object
// (InterfaceDrawObjectSprite, 0x1018c978..0x1018d0bc) is in src/ui_DrawObjectSprite.cpp, the text draw objects built on it
// (InterfaceTextObjectSprite, InterfaceTitleTextObject, InterfaceCursorPosTextObjectSprite, 0x1018d11c..0x1018ffb0) in
// src/ui_DrawObjectText.cpp.
#pragma once
#include <cstdint>

#include "TPCoaster.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "rsse.h"
#include "tb_starfield.h"
#include "TbTextFormatter.h"

// A mouse cursor of the UI: the base class is visible and draws nothing. Mac 0x14 bytes,
// the vtable pointer last (0x10), vtable 0x102afa08 (0x101cc6a0..0x101cc8bc, 0x101ccc8c).
class InterfaceCursor {
public:
    InterfaceCursor();                               // 0x101cc6a0
    virtual ~InterfaceCursor();                      // 0x101cc6f0
    virtual uint8_t GetVisible();                    // 0x101cc84c
    virtual void SetData(void* data, char kind);     // 0x101cc768: nothing
    virtual void Show();                             // 0x101cc7dc
    virtual void Hide();                             // 0x101cc814
    virtual void Use(int16_t a);                     // 0x101cc8bc: nothing
    virtual void Draw(int16_t x, int16_t y);         // 0x101cc888: nothing
    virtual void Animate(int32_t time);              // 0x101ccc8c: nothing
    void SetHot(char x, char y);                     // 0x101cc7a0

    uint8_t mVisible;   // 0x00
    int8_t mHotX;       // 0x01
    int8_t mHotY;       // 0x02
    uint8_t mF03[9];    //       not set by the constructor
    uint32_t mF0C;      // 0x0c  0
};

namespace SamsUtilities { class UCursor; }

// A cursor made from a picture file (src/ui_cursors.cpp): the platform layer's cursor does the work. Mac 0x18
// bytes (the base's 0x14 and the pointer), vtable 0x102b1580; the virtual members other than the constructor
// are in src/ui_cursor_classes.cpp (they call the platform's UCursor).
class InterfaceBFCursor : public InterfaceCursor {
public:
    InterfaceBFCursor();                              // 0x101fe208
    ~InterfaceBFCursor() override;                    // 0x101fe27c: deletes the platform's cursor
    uint8_t GetVisible() override;                    // 0x101fe338: the library's IsVisible, whatever the cursor
    void Show() override;                             // 0x101fe390: the library's Show, if there is a cursor; its count goes to mVisible
    void Hide() override;                             // 0x101fe404: the same with Hide
    void Use(int16_t a) override;                     // 0x101fe478: 1: hot spot to the cursor, then Set; 0: Reset
    // Reads a TGA file made of 32 x 32 frames side by side and makes the platform's cursor from them (a frame
    // every 300 milliseconds). 1 when the file was read (even if no cursor came of it), 0 when it was not.
    int32_t LoadData(const char* path);               // 0x101fe52c

    SamsUtilities::UCursor* mCursor;                  // 0x14  NULL until LoadData has made it
};

// The cursors by number. Mac 8 bytes (0x101cc994..0x101ccc34).
class CursorTable {
public:
    CursorTable();                                              // 0x101cc994
    ~CursorTable();                                             // 0x101cc9cc: nothing
    void DeAllocate();                                          // 0x101cca38
    int32_t Allocate(uint16_t count);                           // 0x101ccafc
    int32_t SetEntry(uint16_t index, InterfaceCursor* cursor);  // 0x101ccbb0
    InterfaceCursor* GetEntry(uint16_t index);                  // 0x101ccc34

    InterfaceCursor** mEntries;   // 0x00
    uint16_t mCount;              // 0x04
};

// The text part of a change (the name is invented). InitGeneralUI builds one on its stack
// and sets only the fields named here.
struct DOTextStyle {
    uint8_t mF00[4];
    uint8_t mF04;        // 0x04
    uint8_t mF05;        // 0x05
    uint8_t mF06[3];     // 0x06  (SetDrawObject sets the last one, +8, to the low byte of the window font's id)
    uint8_t mF09;        // 0x09
    uint8_t mF0A[6];
    uint8_t mColour[3];  // 0x10
    uint8_t mF13;
    // The word at 0x00 (mF00 above) is the text's address on the Mac: the text object's Change, given flag 0x100, draws that text
    // (UpdateTextures). The host's pointer is larger than the four bytes, so it has a member of its own, last (added by unit w35c).
    char16_t* mText = nullptr;
};

// What Change is told, one field per bit of its flags (the fields past the rectangle are
// unknown; the size is at most 0x28 on the Mac).
struct DOChange {
    uint8_t mF00[4];
    int16_t mFrame;       // 0x04  flag 1
    int16_t mDepth;       // 0x06  flag 2
    int16_t mF08;         // 0x08  flag 4
    int16_t mFrameBase;   // 0x0a  flag 0x80000
    int16_t mF0C;         // 0x0c  flag 0x40, with the next
    int16_t mF0E;         // 0x0e
    DOTextStyle* mText;   // 0x10  (InitGeneralUI: flags 0xc00)
    CoolRect* mRect;      // 0x14  flag 0x10
    uint8_t mF18[0x10];
};

// Mac 0x3c bytes; the vtable pointer is the last field (0x38), vtable 0x102afa38.
class InterfaceDrawObject {
public:
    explicit InterfaceDrawObject(void* data);                       // 0x101cccc0
    virtual ~InterfaceDrawObject();                                 // 0x101ccd9c
    virtual void Draw(int16_t a, CoolRect* rect, int16_t b);        // 0x101cce44: nothing
    virtual void Draw(int16_t a, CoolRect* rect, int16_t b, char16_t* text);   // 0x101cce84: nothing
    virtual void Change(uint32_t flags, DOChange* change);          // 0x101ccec8
    // The Mac's int answers and takes pointers as well (the cursor position text object takes the address of a text and answers the
    // address of a rectangle), so the host's are as wide as a pointer (retyped by unit w35c from int32_t).
    virtual intptr_t Query(uint32_t what, intptr_t value);          // 0x101ccfac: 0
    virtual void Show(int16_t shown);                               // 0x101ccfe8
    virtual void Refresh();                                         // 0x1018a1b0: nothing
    virtual int32_t GetWritableDataArea(int32_t* a, int32_t* b);    // 0x1018a1e8: 0
    virtual InterfaceDrawObject* InstanceMe();                      // 0x1018a234: returns its argument
    void SetFrameBase(int16_t frame);                               // 0x101cd020
    void SetFrame(int16_t frame);                                   // 0x101cd070

    void* mData;           // 0x00  for an engine mesh object: its tag_sMesh
    int16_t mShown;        // 0x04
    int16_t mFrameBase;    // 0x06
    int16_t mFrame;        // 0x08
    int16_t mFrameCount;   // 0x0a  frames below this can be set
    int16_t mInLibrary;    // 0x0c  1 once the library has it (the object then owns its mesh)
    int16_t mResizable;    // 0x0e  1 for the frame and slider meshes. Also the number of windows using the object, as
                           //       InterfaceWindow::SetDrawObject keeps it: SET to 1 when a window takes the object, one less when
                           //       a window gives it up; the object is deleted when mResizable + mInLibrary reaches 0 (so 0 for
                           //       an object outside the library, -1 for one in it)
    CoolRect mRect;        // 0x10
    CoolRect mRect2;       // 0x1c
    int16_t mF28;          // 0x28
    int16_t mF2A;          // 0x2a
    int16_t mF2C;          // 0x2c  change flag 4
    int16_t mF2E;          // 0x2e
    int16_t mF30;          // 0x30  change flag 0x40, with the next
    int16_t mF32;          // 0x32
    int16_t mDepth;        // 0x34  change flag 2 (not set by the constructor)
};

// A draw object that is a mesh of the engine. Mac 0x84 bytes, vtable 0x102a7c30.
class InterfaceDrawEngineMesh : public InterfaceDrawObject {
public:
    explicit InterfaceDrawEngineMesh(void* mesh);                   // 0x10187a28
    ~InterfaceDrawEngineMesh() override;                            // 0x10187b28
    void Draw(int16_t a, CoolRect* rect, int16_t b) override;       // 0x101884f4: nothing
    void Draw(int16_t a, CoolRect* rect, int16_t b, char16_t* text) override;   // 0x10188538: nothing
    // Places the mesh: flags 1 and 0x80000 (the frame, the frame base) choose the node; 0x10 (the rectangle), 4 (the z rotation), 0x40 (the
    // pivot) and 1 place it from the node's box through GetUIMatrix; 2 sets its depth. Nothing happens before the mesh has a node chosen.
    void Change(uint32_t flags, DOChange* change) override;         // 0x10188168
    void Show(int16_t shown) override;                              // 0x10187bfc
    // An object of the library that is also in use by windows (mResizable) answers a new object with an instance of its mesh (InstanceMesh in
    // the UI's heap) and its three frame words; any other answers itself.
    InterfaceDrawObject* InstanceMe() override;                     // 0x10188580
    // Chooses the node the frame (mFrame + mFrameBase) names in the mesh's chain: the old node's flags get 0x8010 (hidden), the new one's lose it.
    // Answers the node (that of the last choice when neither flag 1 nor 0x80000 is given, NULL before any).
    tag_sNode* CalculateFrame(uint32_t flags, DOChange* change);    // 0x1018806c

    sMatrix mMatrix;       // 0x3c  not set by the constructor
    uint32_t* mNodeFlags;  // 0x7c  when set, Show hides and shows through these flags too
    int32_t mLocalised;    // 0x80  1 for the meshes of the language's folder
};

// The frames and the sliders, which stretch: their own Change and InstanceMe (src/ui_DrawObjectResizable.cpp, unit w44).
// Vtables 0x102a7c04 and 0x102a7bbc. Their destructors (0x10188704, 0x10188c04) are the
// base's.
class InterfaceDrawEngineResizableFrame : public InterfaceDrawEngineMesh {
public:
    explicit InterfaceDrawEngineResizableFrame(void* mesh);         // 0x1018868c
    ~InterfaceDrawEngineResizableFrame() override;                  // 0x10188704
    void Change(uint32_t flags, DOChange* change) override;         // 0x101887f0
    InterfaceDrawObject* InstanceMe() override;                     // 0x10188a68
};
class InterfaceDrawEngineResizableSlider : public InterfaceDrawEngineMesh {
public:
    explicit InterfaceDrawEngineResizableSlider(void* mesh);        // 0x10188b8c
    ~InterfaceDrawEngineResizableSlider() override;                 // 0x10188c04
    void Change(uint32_t flags, DOChange* change) override;         // 0x10188cf0
    InterfaceDrawObject* InstanceMe() override;                     // 0x10188ed0
};

// ---- the gradients and the matrix helpers (src/ui_DrawObjectEngineMesh.cpp and src/ui_matrix.cpp, unit w42)

// The matrix of an object placed in the UI's 2048 x 1536 space (src/ui_matrix.cpp, 0x10187ce4, 848 bytes). The object's box (lower left corner
// x, y and size w, h in the object's own units) is put on the rectangle whose left and BOTTOM edge are `left` and `bottom` and whose size is
// `width` x `height` (UI units), then scaled to the engine's -1 to 1 screen. A nonzero `angle` (4096 to a turn) first turns the object about the
// pivot (`pivotX`, `pivotY`, UI units) in the 4:3 screen, with TPGlobals' sine table. The fourth column of the result is the identity's.
void GetUIMatrix(sMatrix* matrix, int16_t left, int16_t bottom, int16_t width, int16_t height, float x, float y, float w, float h,
                 int16_t pivotX, int16_t pivotY, int16_t angle);
// 0x100652f4 (48 bytes, TPRide.cpp; here because the gradients are its callers): the mesh's engine mesh gets the matrix as its local matrix.
void UpdateMeshLocalMatrix(tag_sMesh* mesh, sMatrix* matrix);

// A gradient is a quad of the interface (a dynamic mesh set of its own: two halves of four vertices each, in a box of 100 x 100 units) whose
// middle edge moves with a fraction from 0 to 100: flag 8 of Change sets it from a value (1024 is 100) and has the vertices and the texture
// coordinates redone. The horizontal one moves the edge along x, the vertical one along y. Mac 0x124 bytes: the base's 0x3c, the mesh set's one
// entry at 0x3c (its box at 0x4c), the set at 0x6c, the texture set at 0xbc and the matrix at 0xdc (translation z, 0x114, is the object's depth),
// the fraction at 0x120; vtables 0x102a7b18 (horizontal) and 0x102a7b58 (vertical). The members below are those Change and CalcVerts read (the
// host keeps its pointers native, so the Mac offsets are in the comments only). The constructors set `mData` (word 0) to 0 and to 1 once the mesh
// is made: Change does nothing before.
class InterfaceDrawEngineHGradient : public InterfaceDrawObject {
public:
    explicit InterfaceDrawEngineHGradient(void* mesh);              // 0x101898c8 (untranslated)
    ~InterfaceDrawEngineHGradient() override;                       // 0x10189c0c (untranslated)
    void Change(uint32_t flags, DOChange* change) override;         // 0x10189ea0
    void Show(int16_t shown) override;                              // 0x10189df8 (untranslated)
    // The four vertices on the edge between the quad's halves (set 0 lanes 1 and 2, set 1 lanes 0 and 1) go to x = the fraction, y 100 for the
    // upper ones and 0 for the lower, z 0. Not virtual here (the vertical one's is).
    void CalcVerts();                                               // 0x10189cd4
    void CalcUVs();                                                 // 0x10189d70 (untranslated; Change does the same inline)

    float mBox[6];            // 0x4c  the entry's box: minimum x, y, z, then maximum x, y, z (0, 0, 0, 100, 100, 0 from the constructor)
    tag_sDMeshSet mMeshSet;   // 0x6c  mVertexSets (0x7c), mUVSets (0x84) and mMesh (0x90) are used
    sMatrix mMatrix;          // 0xdc
    float mFraction;          // 0x120  0 to 100
    // Appended for the constructor (native layout; the Mac's are in the comments): the mesh set's one entry, the texture set's one entry and
    // the texture set.
    sDMeshEntry mEntry;       // 0x3c  (its box is mBox's, 0x4c)
    sDTextureEntry mTexEntry; // 0xac  mFlags 2, mName the mesh's name
    sDTextureSet mTextures;   // 0xbc
};
class InterfaceDrawEngineVGradient : public InterfaceDrawObject {
public:
    // `mesh` is the texture's name ("meter.wct"): the texture entry and DMCreateMesh both take it. The constructor (src/ui_DrawObjectGradient.cpp,
    // unit w118) fills the mesh set and its one entry, loads the textures, makes the mesh and the quad (CalcVerts and CalcUVs through the vtable
    // with the fraction 0); mData stays 0 when the textures or the mesh could not be made.
    explicit InterfaceDrawEngineVGradient(void* mesh);              // 0x10188ff8
    ~InterfaceDrawEngineVGradient() override;                       // 0x1018934c: frees the textures and the mesh when mData is set
    void Change(uint32_t flags, DOChange* change) override;         // 0x101895e0
    void Show(int16_t shown) override;                              // 0x10189538: enables or disables the mesh render when the state changes
    virtual void CalcVerts();                                       // 0x10189414
    virtual void CalcUVs();                                         // 0x101894b0: the fraction / 100 on the edge between the halves

    float mBox[6];            // 0x4c  as the horizontal one's
    tag_sDMeshSet mMeshSet;   // 0x6c
    sMatrix mMatrix;          // 0xdc
    float mFraction;          // 0x120
    // What the constructor builds in place (unit w118), after the members above so that nothing that reads them moves. The host keeps the box
    // twice, in mBox (which Change reads) and in mItem.mBox (which DMCreateMesh copies into the node); the constructor writes both the same.
    sDMeshEntry mItem;               // 0x3c  the mesh set's one entry (0x30 bytes): flags 3, 4 faces, 8 vertices, 8 UVs, 1 face set, 8 points
    sDTextureEntry mTextureEntry;    // 0xac  flags 2, the texture's name
    sDTextureSet mTextures;          // 0xbc  count 1, the folder "data:ui:textures", the entry above
};

struct TbRenderArea;

// A draw object that is a surface of the engine's sprite system: the interface's pictures and texts are drawn by writing into the
// surface and letting the sprite manager blit it. The object keeps one sprite instance of its own (a one-entry instance array and a
// one-entry sprite list) and, while shown, links the list into the map's list 0x30. Mac 0xdc bytes (the base's 0x3c, then the
// members below; the sprite list at 0xb4 is 0x28 bytes), vtable 0x102a826c. Its subclasses (the text objects, from
// InterfaceTextObjectSprite on) add their members after it. The fields the host keeps natively (the pointers) are larger, so the
// Mac offsets are in the comments only.
//
// Quirks: Change does not chain to the base's, so the frame, the pivot and the frame base are ignored; a Change without a rectangle
// does nothing until the first one has made the surface (CreateTextures, through the vtable); Show does nothing until then either
// (and does not record the state); Draw draws nothing (the sprite manager does).
class InterfaceDrawObjectSprite : public InterfaceDrawObject {
public:
    explicit InterfaceDrawObjectSprite(void* data);                 // 0x1018c978
    ~InterfaceDrawObjectSprite() override;                          // 0x1018ca88
    using InterfaceDrawObject::Draw;
    void Draw(int16_t a, CoolRect* rect, int16_t b) override;       // 0x1018cff0: nothing
    // Flags 0x10 (the rectangle, in the UI's 2048 x 1536 units), 2 (the depth, at least 1) and 4 (mF2C) place the sprite: its
    // position in the engine's units is (2 * left / 2048 - 1, 1 - 2 * BOTTOM / 1536, 1 - depth / 40) (the bottom, not the top).
    // The first rectangle also sizes the surface in the real screen's pixels and makes it.
    void Change(uint32_t flags, DOChange* change) override;         // 0x1018ccc0
    void Show(int16_t shown) override;                              // 0x1018cf10
    void Refresh() override;                                        // 0x1018cc5c
    // The surface's size in *a and *b and its pixels as the answer (only the low 32 bits of the pointer on the host, which cannot
    // be dereferenced: a caller on the host reads mEngineData.mWords.mPixels); 0 before the surface exists or for a NULL argument.
    int32_t GetWritableDataArea(int32_t* a, int32_t* b) override;   // 0x1018d038
    virtual void UpdateTextures(char16_t* text);                    // 0x1018cb48: RefreshPartialTexture
    virtual void CreateTextures(char16_t* text);                    // 0x1018cbb4: makes the surface once
    void SetPreDrawCallback(int32_t (*callback)(TbRenderArea&));    // 0x1018d0bc

    int16_t mPixelWidth;                // 0x3c  the rectangle's size in the real screen's pixels (gUIRealScreenWidth / 2048 of it)
    int16_t mPixelHeight;               // 0x3e  (gUIRealScreenHeight / 1536 of it)
    uint8_t mF40[4];                    // 0x40  not set by the constructor
    int16_t mCreated;                   // 0x44  0 until CreateTextures has made the surface
    uint8_t mF46[6];                    // 0x46  not set by the constructor
    int32_t (*mPreDraw)(TbRenderArea&); // 0x4c  NULL (SetPreDrawCallback); the text objects call it before they draw
    int32_t mF50;                       // 0x50  0
    SSpriteGameData mSprite;            // 0x54  the one sprite: flags 0x30000, depth scale 1.0, its position set by Change
    SSpriteGameData** mInstances;       // 0x98  what the list's Init made: one entry, &mSprite
    SSpriteEngineData mEngineData;      // 0x9c  the surface
    CSpriteList mList;                  // 0xb4  one instance; linked into list 0x30 of the map while the object is shown
    // The word at 0x50 (mF50 above, always 0) as the pointer it is on the Mac: the text objects call it after they have drawn, with
    // the render area they drew into (added by unit w35c; nothing in the translated code sets it).
    int32_t (*mPostDraw)(TbRenderArea&) = nullptr;
};

// ---- the text draw objects (src/ui_DrawObjectText.cpp, unit w35c; the inventory files them under ui_textmanager.cpp)

// What the text draw objects call that is not theirs (the main app's helpers of unit w35b and the Bullfrog drawing library). The unit's
// tests fake all of them; the types are those of the mangled names.
void AdjustJustificationForHebrew(int32_t& justification);                                  // 0x1018b2c8: swaps left and right (0 and 2) in Hebrew
void MakeRenderArea(uint8_t* pixels, TbRenderArea* area, int32_t width, int32_t height);   // 0x1018b360: an area over the pixels, in the screen's format
unsigned char* UIMain_ConvertString(char16_t* text);                                        // 0x10181a7c: the text in the language's code page, in a buffer of the library's
void LbDraw_SetCurrent(TbRenderArea* area);                                                 // bullfrog 0x1002b588: the surface the formatter draws into

// A draw object that is a text: the sprite object's surface holds the picture of the text, redrawn by UpdateTextures whenever the text,
// the style or the size changes. Mac 0x19c bytes (the operator new argument of every construction site), vtable 0x102a8238: the sprite's
// with ~, Change and UpdateTextures replaced. The members past the sprite object (0xdc) are the text style a Change (flags 0x400 to
// 0x40000, from a DOTextStyle) sets, then the formatter that lays the text out.
//
// Quirks: UpdateTextures does not call UI_DrawText: it does the same job inline (sets the font and colour, makes a render area over the
// surface, clears the surface unless the pre-draw callback has drawn the background, draws with the formatter, then takes the rectangle the
// text took as the object's second rectangle). A justification code (after the Hebrew swap) outside 0 to 2 leaves the text box as
// whatever registers the caller had: the translation makes it empty. A font id outside 0 to 12 makes the original read the word at address 0x14
// as the font; the translation passes NULL.
class InterfaceTextObjectSprite : public InterfaceDrawObjectSprite {
public:
    explicit InterfaceTextObjectSprite(void* data);                 // 0x1018d11c
    ~InterfaceTextObjectSprite() override;                          // 0x1018d1dc
    // The sprite object's Change (rectangle, depth and the word at 0x2c), then the style: flag 0x400 horizontal justification, 0x800
    // vertical, 0x1000 wrap, 0x2000 the byte at 0xdf, 0x4000 and 0x8000 the offsets, 0x10000 the font, 0x20000 the colour mode,
    // 0x40000 the colour (and the sprite's colour word); 0x100 then draws the style's text.
    void Change(uint32_t flags, DOChange* change) override;         // 0x1018d798
    void UpdateTextures(char16_t* text) override;                   // 0x1018d2c4

    int8_t mJustify;          // 0xdc  horizontal: 0 left, 1 centre, 2 right (the Hebrew swap exchanges 0 and 2)
    int8_t mVJustify;         // 0xdd  vertical: 0 top, 1 centre, 2 bottom
    uint8_t mWrap;            // 0xde  0: the text box is 0x4000 wide, placed by the justification (no wrapping); else the object's width
    uint8_t mF0DF;            // 0xdf  set by flag 0x2000, never read
    int8_t mFontId;           // 0xe0  0 to 12: the language's font (gText.mFonts); 0xff until a Change sets it
    uint8_t mColourMode;      // 0xe1  0: one colour (the text colour; white for UpdateTextures, which ignores the colour bytes); 1: the font's own
    int16_t mOffsetX;         // 0xe2  in the UI's 2048 x 1536 units
    int16_t mOffsetY;         // 0xe4
    TbTextFormatter mFormatter;   // 0xe8 (0xb4 bytes)

    // The text colour: red, green, blue and alpha, the last four bytes of the sprite object's mF46 (Mac 0x48 to 0x4b). The constructor sets
    // the first three to 0xff and leaves the alpha.
    uint8_t* TextColour() { return &mF46[2]; }
};

// Unit v5074: the text object of a cash column (Mac 0x19c bytes too; constructor 0x1018e594, untranslated stand-in). Its base is a guess.
class InterfaceNullCashTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceNullCashTextObject(void* data);               // 0x1018e594
    ~InterfaceNullCashTextObject() override;                        // 0x1018e674 (unit v51115)
};

// The title: the same text drawn twice, first in grey, two pixels left and down, then in white (the Mac's vtable 0x102a7e8c).
class InterfaceTitleTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceTitleTextObject(void* data);                  // 0x1018fbe8
    ~InterfaceTitleTextObject() override;                           // 0x1018fcc4
    void UpdateTextures(char16_t* text) override;                   // 0x1018fdb8
};

// The text of an edit box, which can say where its characters are (vtable 0x102a7f3c). Its constructor asks the formatter to record the
// positions. Query: what 1 answers the position of character `value` (the end of the text when there is none), what 2 measures the
// text at the address `value`; both answer the address of the UI's one scratch rectangle (gUIQueryRect), in the UI's units; anything else 0.
class InterfaceCursorPosTextObjectSprite : public InterfaceTextObjectSprite {
public:
    explicit InterfaceCursorPosTextObjectSprite(void* data);        // 0x1018f1d0
    ~InterfaceCursorPosTextObjectSprite() override;                 // 0x1018f2b8
    intptr_t Query(uint32_t what, intptr_t value) override;         // 0x1018f3b4
};
extern CoolRect gUIQueryRect;   // 0x1038cd5c, original name unknown: the rectangle Query answers

// Unit v51012: the text object of a time (hours; vtable of the Mac's class not translated). Change 0x1018efcc.
class InterfaceVisitorTimeTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceVisitorTimeTextObject(void* data);            // 0x1018edf4 (unit v51115)
    ~InterfaceVisitorTimeTextObject() override;                     // 0x1018eed4
    void Change(uint32_t flags, DOChange* change) override;         // 0x1018efcc
};

// The text objects of the main panel's number windows (src/ui_DrawObjectNumber.cpp, unit w118). Mac 0x19c bytes like the text sprite; the Mac
// constructors are the text sprite's with its base constructor inlined and their own vtable stored last (0x102a8204 and 0x102a80f4), the
// destructors the sprite chain inlined (the host's are the language's). Change is the part that puts the digits and the cash in the window
// (flag 0x80, and for the number object flag 8); translated in src/ui_TextObjects.cpp (unit w125).
class InterfaceNumberTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceNumberTextObject(void* data);                 // 0x1018d930
    ~InterfaceNumberTextObject() override;                          // 0x1018da0c
    void Change(uint32_t flags, DOChange* change) override;        // 0x1018db00 (src/ui_TextObjects.cpp)
};
class InterfaceCashAndGTickTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceCashAndGTickTextObject(void* data);           // 0x1018e190
    ~InterfaceCashAndGTickTextObject() override;                    // 0x1018e274
    void Change(uint32_t flags, DOChange* change) override;        // 0x1018e36c (src/ui_TextObjects.cpp)
};

// The shared draw objects, by the hash of their name: a singly linked list in the order
// they were added. A node is 0x18 bytes on the Mac.
class DrawObjectLibrary {
public:
    struct Node {
        InterfaceDrawObject* mObject;   // 0x00
        char mName[12];                 // 0x04  the first twelve characters, not always ended
        uint32_t mHash;                 // 0x10
        Node* mNext;                    // 0x14
    };
    static int32_t AddDrawObject(InterfaceDrawObject* object, char* name);   // 0x101cd0bc
    static void RemoveAll(int16_t destroyObjects);                          // 0x101cd214
    static InterfaceDrawObject* FindDrawObject(char* name);                 // 0x101cd2d4
    static InterfaceDrawObject* FindDrawObject(uint32_t hash);              // 0x101cd3b0
};
extern DrawObjectLibrary::Node* gDrawObjectLibrary;   // 0x10395c30, original name unknown

extern sHeapInfo* gUIHeap;        // 0x1038cc60, original name unknown: HNewHeap's in InitGeneralUI

// Unit v5088: the text object of a cash column (Mac 0x19c bytes like the text sprite; constructor 0x1018de60, unit v5L16: the text sprite's
// constructor inlined, then its own vtable 0x102a814c).
class InterfaceCashTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceCashTextObject(void* data);                   // 0x1018de60
    ~InterfaceCashTextObject() override;                            // 0x1018df3c (unit v51115, src/ui_TextObjects_InterfacePasswordTextObject_InterfacePasswordTex.cpp)
};

// Unit v51115: the number-window text objects the main panels make (Mac 0x19c bytes like the text sprite; the constructors are the text
// sprite's with its base constructor inlined and their own vtable stored last).
class InterfaceVisitorNumTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceVisitorNumTextObject(void* data);             // 0x1018eb38
    ~InterfaceVisitorNumTextObject() override;                      // 0x1018ec18
};
class InterfaceRankTextObject : public InterfaceTextObjectSprite {
public:
    explicit InterfaceRankTextObject(void* data);                   // 0x1018f93c
};

// Unit v5983 (src/ui_TextObjects_InterfaceCashTextObject_Change.cpp): the Change overrides of five text objects as free functions (their
// classes' vtables would need the whole draw object chain). Flag 0x80 (0x100 for the password) makes the text and draws it with flag 0x100;
// any other flags go straight to InterfaceTextObjectSprite::Change.
class InterfacePasswordTextObject;
void InterfaceCashTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change);       // 0x1018e02c
void InterfacePasswordTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change);   // 0x1018dd50
// 0x1018dcb0: the destructor with the hidden delete flag; answers self. (The Mac stores the vtable 0x102a81ac at +0x38: not kept.)
InterfacePasswordTextObject* InterfacePasswordTextObject_Dtor(InterfacePasswordTextObject* self, int16_t deleteFlag);
void InterfaceNullCashTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change);   // 0x1018e768
void InterfacePercentTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change);    // 0x1018ea44
void InterfaceVisitorNumTextObject_Change(InterfaceTextObjectSprite* self, uint32_t flags, DOChange* change); // 0x1018ed10
// Callees outside the unit (faked by the test library).
namespace textobj_callee {
int32_t Labs(int32_t value);                          // labs, glue 0x10239928
void WcsSet(char16_t* text, char16_t c);              // NS_MacDoze::_wcsset, glue 0x102397f0
void CursorDtor(void* self, int16_t deleteFlag);      // InterfaceCursorPosTextObjectSprite::~..., 0x1018f2b8
void Delete(void* self);                              // operator delete, glue 0x10238218
}  // namespace textobj_callee

// Unit v51114 (src/ui_StatusPanel_SaveTags_UIStatusPanel_GetNextObject_UIStatusPanel_EnableNameClick.cpp): InterfaceRankTextObject::~InterfaceRankTextObject
// 0x1018fa18 (188 bytes) with the hidden delete flag; answers self. The class is not declared (see InterfaceRankTextObject_Change); the Mac
// stores its vtables at +0x38 (not kept). Callees (faked by the test library): the sprite chain's destructors, inlined into this one.
class InterfaceRankTextObject;
namespace rank_callee {
void FormatterDtor(void* self, int16_t deleteFlag);    // TbTextFormatter::~TbTextFormatter, glue 0x10238e18 (self + 0xe8)
void SpriteListDtor(void* self, int16_t deleteFlag);   // CSpriteList::~CSpriteList, glue 0x10238cf8 (self + 0xb4)
void EngineDataDtor(void* self, int16_t deleteFlag);   // SSpriteEngineData::~SSpriteEngineData, glue 0x10238d10 (self + 0x9c)
void DrawObjectDtor(void* self, int16_t deleteFlag);   // InterfaceDrawObject::~InterfaceDrawObject, 0x101ccd9c
void Delete(void* self);                               // operator delete, glue 0x10238218
}  // namespace rank_callee
InterfaceRankTextObject* InterfaceRankTextObject_Dtor(InterfaceRankTextObject* self, int16_t deleteFlag);
