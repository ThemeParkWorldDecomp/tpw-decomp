// tb_starfield.h: the engine's sprite classes (engine library; the inventory files them
// under tb_starfield.cpp at medium confidence: SSpriteEngineData 0x1002e0e0..,
// SSpriteGameData 0x1002f144.., CSpriteList 0x1003108c..). Their asserts name
// tb_sprite.cpp (SSpriteEngineData, SSpriteGameData) and tb_spritelist.cpp (next to
// CSpriteList), so this header should be split into those files when they are translated.
//
// PARTIAL. Translated (src/engine/SpriteData.cpp, src/engine/SpriteManager.cpp, with the sprite file reader of
// include/sprite_engine.h): SSpriteEngineData's constructor, destructor, CreateTextureEntry,
// Load_WCT, Load_RLE, Init(int, int), RefreshPartialTexture and Close; SSpriteGameData's
// constructor, SetUsedPosition and SetUsedSize; CSpriteList's constructor, destructor and Init.
// src/engine/SpriteData.cpp (unit w41b): SSpriteGameData's DrawPartial,
// CreateShadow, DrawRegularShadow and AddRegularShadow.
// src/engine/SpriteData.cpp, src/engine/SpriteManager.cpp (unit w38): SSpriteEngineData's Init(char*), SetUsedPart and the partial
// texture trio, CSpriteList's Close, Prepare, GetData and GetLinkType. The rest is declared for what its
// callers use (tb_ezsprite.cpp, the sprite manager) and is not translated: the sprite drawing
// (SSpriteGameData's frame members; CMatrix::operator*, emitted in this range, is in tb_matrix.h).
// Offsets are the Mac's; unknown fields are padding.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "MapWho.h"
#include "platform/d3d_types.h"

class CCompBase;
class CMatrix;
class CTextureEntry;
struct SSpriteDrawParameters;   // tb_drawprimitive.h
struct SVertex;                 // tb_hfield.h
struct CSpriteFileIO;
struct SVertex;

// A loaded sprite (0x18 bytes on the Mac). The destructor is not virtual. Three things make one:
// a file (CSpriteFileIO::Load fills an array of them and an array of texture entries they point
// into, src/engine/SpriteData.cpp), a TGA file (Init(char*)) or a size (Init(int, int): a pixel
// block the partial textures point into).
struct SSpriteEngineData {
    SSpriteEngineData();             // 0x1002e0e0: leaves mAspect alone
    ~SSpriteEngineData();            // 0x1002e138: Close()
    int32_t Init(char* name);        // 0x1002e810: 0 when the file could not be loaded
    // A blank block of width x height pixels in the current texture format (kept as 16-bit
    // sizes); asserts, pushes an out-of-memory error and answers 0 without memory.
    int32_t Init(int32_t width, int32_t height);   // 0x1002ea18
    int32_t Close();                 // 0x1002f00c
    // Makes the sprite's texture entry (a new array of one unless `entry` is given) holding
    // the image or, when `source` is given, that object to unpack it from; `alpha` is a second
    // image. The flags are `flags` | 0x1000140 (0x1000540 with an alpha image); the entry's
    // pitch is the width. Answers 0 when there was no memory for the entry.
    int32_t CreateTextureEntry(void* image, void* alpha, CCompBase* source, int32_t width, int32_t height,
                               CTextureEntry* entry, int32_t flags);   // 0x1002e1c4
    // Read the sprite's header and image from an open sprite file (kind 2: wavelet, kind 3:
    // RLE with the file's palette) and make its texture entry in `entry`, an element of the
    // file's array. 0 only when the loader object cannot be made.
    int32_t Load_WCT(void* file, CTextureEntry* entry);                       // 0x1002e33c
    int32_t Load_RLE(void* file, void* palette, CTextureEntry* entry);        // 0x1002e514
    // Takes the partial textures (the entries mTexture points at, mPartCount of them) off the cache's
    // pages and frees them, leaving the pixel block. Always 1.
    int32_t RefreshPartialTexture(); // 0x1002ef10
    // Moves the part of the picture in use; a sprite with a texture whose part changes first loses its partial
    // textures. (The stored halfword is compared as unsigned against the argument as signed.)
    void SetUsedPart(int16_t width, int16_t height);                 // 0x1002eb00
    // Cuts the used part of a sprite with a pixel block into entries of the cache's UI texture size that look
    // into the block from pixel (x, y); mPartCount is the number made. 0 only without memory (dead here).
    int32_t GeneratePartialTexture(int32_t x, int32_t y);            // 0x1002ebb8
    // Takes the partial textures off the cache and destroys them (nothing without a count). Always 1.
    int32_t DestroyPartialTexture();                                 // 0x1002ee28

    // The first words of the object in two readings of the same bytes (merged from units w30b and w33b; offsets are the Mac's):
    // the interface's sprite draw objects read the pixels, aspect and size through mWords (unit w30b); the sprite file reader names
    // the same bytes mData, mAspect, mWidth and mHeight (unit w33b). The pixels word is a native pointer on the host, so the
    // four words take 0x10 bytes there and the object 0x20.
    struct SurfaceWords {
        void* mPixels;               // Mac 0x04
        float mAspect;               // Mac 0x08
        uint16_t mWidth;             // Mac 0x0c
        uint16_t mHeight;            // Mac 0x0e
    };

    CTextureEntry* mTexture;         // 0x00  the sprite's texture (the frame's lists read its page and blending mode)
    union {
        SurfaceWords mWords;         // 0x04  (unit w30b's name for the four words below)
        struct {
            void* mData;             // 0x04  the pixel block of Init(int, int) (an engine allocation, freed by Close);
                                     //       none for a sprite loaded from a file
            float mAspect;           // 0x08  width / height of the image (0.0 without a height); the constructor leaves it
            uint16_t mWidth;         // 0x0c  the image's size in pixels
            uint16_t mHeight;        // 0x0e
        };
    };
    uint16_t mUsedWidth;             // 0x10  the part of it in use (the whole image from Init(char*))
    uint16_t mUsedHeight;            // 0x12
    int16_t mOffsetX;                // 0x14  where the sprite's corner is from its position (negative: half the size for Init(char*))
    union {                          // 0x16  one field, two uses
        int16_t mOffsetY;            //       the same for y (sprites from files and TGA files)
        uint16_t mPartCount;         //       how many partial textures mTexture holds, with a pixel block (GeneratePartialTexture)
    };
};
static_assert(sizeof(SSpriteEngineData) == 0x20, "the host sprite: two pointers, a float and six halfwords");
static_assert(sizeof(SSpriteEngineData::SurfaceWords) == 0x10, "mWords covers the same four words as mData..mHeight");

// One instance of a sprite (0x44 bytes on the Mac; trivial destructor: the engine frees
// it with operator delete only).
struct SSpriteGameData {
    SSpriteGameData();               // 0x1002f144
    // Move the part of the sprite's image in use. A change (SetUsedSize tests only the width, twice, as
    // written) takes the sprite's partial textures off the cache and frees them, as
    // SSpriteEngineData::RefreshPartialTexture does, when it has a texture and some; the new values
    // are kept as 16-bit halfwords either way.
    void SetUsedPosition(int32_t x, int32_t y);   // 0x1002f19c
    void SetUsedSize(int32_t width, int32_t height);   // 0x1002f2c0
    // Mac 0x30 is ONE FIELD WITH TWO USES, like SSpriteEngineData's offset: a partial sprite keeps the part of its pixel block
    // in use there (the halfwords mUsedWidth and mUsedHeight), a regular sprite keeps its rotation there, a float that
    // SetupVertices turns the quad by when flag 0x80 is set. The two halfwords are the first four bytes after mF2C, so the
    // angle is read from and written to their bytes.
    float Angle() const {
        float angle;
        std::memcpy(&angle, &mUsedWidth, sizeof angle);
        return angle;
    }
    void SetAngle(float angle) { std::memcpy(&mUsedWidth, &angle, sizeof angle); }

    // The first four words are what CMatrix::TransformVertex leaves when Prepare projects the sprite's position
    // (mPosition): x and y on the screen, the depth (the order key, below) and w.
    union {
        uint8_t mF00[8];
        struct {
            float mScreenX;          // 0x00
            float mScreenY;          // 0x04
        };
    };
    float mDepth;                    // 0x08  the order the sprites of one texture are drawn in, largest first
    union {
        uint8_t mF0C[4];
        float mScreenW;              // 0x0c  w of the projected position
    };
    int32_t mF10;                    // 0x10  0x58000 from CEZSprite::CreateSpriteInstance (the flags)
    D3DVECTOR mPosition;             // 0x14
    union {                          // 0x20
        uint8_t mF20[4];
        float mShadowLift;           // with flag 0x800: added to the height the ground gives under each corner of the sprite's shadow
    };
    int32_t mF24;                    // 0x24  -1 from CreateSpriteInstance (the colour of the sprite's quads; DrawPartial's draw record takes it)
    SSpriteEngineData* mSprite;      // 0x28
    float mF2C;                      // 0x2c  10.0 from CreateSpriteInstance

    // The frame's members. Each answers 0 when the renderer fails, except Prepare, which answers 1.
    // Prepare is in src/engine/SpriteData.cpp: it lists the sprite for the frame and has the texture cache prepare its texture.
    int32_t Prepare();                           // 0x1002f3d0
    // The drawing of a regular sprite: works out the quad (four corners of SVertex, the picture's corners, the blending mode
    // and the flags) in `params` for DrawSprite and AddSprite. Asserts (tb_sprite.cpp line 0x4e4) and goes on when the
    // sprite is not listed this frame (flag 0x10000000); always 1.
    int32_t CalcRegular(SSpriteDrawParameters* params);   // 0x1003042c (src/engine/SpriteData.cpp)
    // Spreads the four corners (copies of one point) into the sprite's quad: the used rectangle of the sprite's image,
    // scaled by xScale and yScale (0.0 means "20 times the sprite's size", the x one also times the display's aspect),
    // turned by the angle and flipped by the flags; sets gHotSprite and the hot sprite test on the way. Answers 1 when
    // the quad's winding has to be reversed (an odd number of flips), else 0.
    int32_t SetupVertices(SVertex* vertices, float xScale, float yScale);   // 0x1002f688 (src/engine/SpriteData.cpp)
    // Two kinds of sprite use the bytes from 0x30 to 0x3f in different ways. A partial sprite (a pixel block cut into
    // texture entries, drawn by DrawPartial) keeps the part of the picture in use; a regular sprite (drawn by CalcRegular
    // and the shadow members) keeps its turn and the frames of its sprite file. The constructor makes the partial words zero.
    union {                          // 0x30
        struct {
            int16_t mUsedWidth;      // the constructor makes both pairs zero. DrawPartial reads these two as the picture's size on
            int16_t mUsedHeight;     //   the screen: the hang flags move it by them, and the height always lifts it by its own size
                                     //   (little-endian host: the float view below has the halves the other way round)
        };
        float mRotation;             // a regular sprite with flag 0x80: the angle the shadow's corners are turned by (RotateVertex_Y)
    };
    union {                          // 0x34..0x3f (the host's pointer is wider: 0x38..0x47)
        struct {
            int16_t mUsedX;          // 0x34
            int16_t mUsedY;          // 0x36
            uint8_t mF38[8];
        };
        struct {
            CSpriteFileIO* mFrameFile;   // 0x34  the sprite file whose sprites are this sprite's frames (count, then the array)
            uint16_t mFrameBase;         // 0x38  the first frame of the sprite in it
            uint8_t mLightFrames[4];     // 0x3c  how far from it the frame of each shadow (light source) is
        };
    };
    SSpriteGameData* mNext;          // 0x40  the next sprite of the same group while the frame sorts (0x48 on the host)

    // The frame's members. The shadows and DrawPartial are translated (src/engine/SpriteData.cpp,
    // unit w41b); Prepare, DrawRegular and AddRegular are not. Each answers 0 when the renderer fails, except Prepare,
    // which answers 1 (and DrawPartial, which answers 1 whatever the renderer does).
    // The sprite's shadows drawn one by one (a primitive of their own each, DrawSprite) / added to the primitive being
    // built (AddSprite): one quad for each light source of the shadow object (gShadow), lying on the ground the
    // height field gives, with the sprite's picture (the manager's blob shadow `shadowTexture` instead when it is
    // not NULL). Nothing is drawn, and 1 answered, unless flags 0x200 and 0x10000000 are both set. 0 when the
    // renderer fails.
    int32_t DrawRegularShadow();                 // 0x1002ff20
    int32_t AddRegularShadow(CTextureEntry* shadowTexture);   // 0x10030210
    int32_t DrawRegular();                       // 0x10030a38
    int32_t AddRegular();                        // 0x10030ac0
    // Draws a sprite with a pixel block (a text object's picture): one DrawSprite for each of the partial
    // textures, cut at the cache's UI texture size, placed on the screen by the sprite's position and alignment
    // flags. Always 1, even when DrawSprite fails (the failure only ends the drawing).
    int32_t DrawPartial();                       // 0x10030b48
    // Works out the four corners of this sprite's shadow for the light source gShadow is at and lays them on the
    // ground (the height field under each corner): `vertices` is four SVertex (0x10 bytes each, the second
    // transform leaves them as x, y, z, w), `shadowMatrix` the light's matrix, the scales stretch the shadow along
    // its two axes. Answers the light's shade (gShadow.mLightShades), the colour of the shadow.
    int32_t CreateShadow(SVertex* vertices, CMatrix* shadowMatrix, float scaleX, float scaleZ);   // 0x1002fa14
};
static_assert(offsetof(SSpriteGameData, mUsedHeight) == offsetof(SSpriteGameData, mUsedWidth) + 2,
              "Angle() reads the two halfwords as one float");

// The instances of one sprite set, linked into gei_MapWho through its virtual base
// (0x28 bytes on the Mac: +0 pointer to the CMapLinkBase at +0x20, +0x1c vtable).
class CSpriteList : public virtual CMapLinkBase {
public:
    CSpriteList();                   // 0x1003108c (also registers the link base)
    virtual ~CSpriteList();          // 0x10031160: Close(), then forgets the link base
    int32_t Prepare() override;      // 0x10031488
    int32_t GetLinkType() override;  // 0x10031fe4
    void* GetData() override;        // 0x10031fb0
    // Allocates the instance array (gMalloc, count pointers), keeps it at +0x14 and the count at +8,
    // and returns it; NULL on failure.
    SSpriteGameData** Init(int32_t count);   // 0x10031378
    // Frees the instance array (gFree) and zeroes the capacity.
    int32_t Close();                 // 0x10031408

    int32_t mCount;                  // 0x04  instances in use (CEZSprite keeps it current)
    SSpriteGameData** mInstances;    // 0x14  what Init made (on the host the offset differs)
    int32_t mCapacity;               // 0x08  the count Init was given (0 after Close)
    int32_t mFrameA;                 // 0x0c  -1; Prepare keeps the display's frame words (+0x80, +0x84) here
    int32_t mFrameB;                 // 0x10  0
    int32_t mF18;                    // 0x18  0
};

// One kind of sprite for RenderSprites (texture page and blending key) in its tree of kinds
// (0x14 bytes on the Mac, made with every byte 0xff). A link of -1, or a page of -1,
// means "not made yet".
struct SSpriteGroup {
    SSpriteGroup* mAbove;          // 0x00  the group of the kinds that compare higher (texture page first)
    SSpriteGroup* mBelow;          // 0x04  and lower
    uintptr_t mPage;               // 0x08  the texture page (0 for a texture not on a page)
    SSpriteGameData* mFirst;       // 0x0c  the sprites of this kind, the largest mDepth first
    int32_t mBlend;                // 0x10  the blending key
};
extern SSpriteGroup gSpriteGroups[0x800];   // engine data 0x10169154 (not exported; name invented)

// The sprite manager (engine). 0x2410 bytes on the Mac; the pointer tables make the host's
// longer. Construction and shutdown are in src/engine/SpriteManager.cpp, the rest in
// src/engine/SpriteManager.cpp. Every frame the game resets it, adds the sprites and sprite lists to
// draw, prepares and renders them.
class CSpriteManager {
public:
    enum : int32_t { kMaxSprites = 0x800, kMaxLists = 0x80 };

    CSpriteManager();                      // 0x10031538
    ~CSpriteManager();                     // 0x1003157c
    int32_t Close();                       // 0x100316f8
    // Replaces the blob shadow texture with the one in the file (flags 0x40040 set); 0 when
    // it could not be loaded. A NULL name asserts (tb_spritelist.cpp) and goes on.
    int32_t LoadBlobShadow(char* path);    // 0x10031604
    // Forgets the sprites and lists and tells the texture cache about the blob shadow.
    void Reset();                          // 0x10031770
    // Prepares every instance of every list added, last first.
    int32_t Prepare();                     // 0x100319c8
    // RenderSprites, then RenderPartialSprites; 0 on failure.
    int32_t Render();                      // 0x100317e4
    // Each answers 0, adding nothing, when its table (kMaxSprites, kMaxSprites, kMaxLists) is full.
    int32_t AddSprite(SSpriteGameData* sprite);          // 0x10031874
    int32_t AddPartialSprite(SSpriteGameData* sprite);   // 0x100318e4
    int32_t AddList(CSpriteList* list);                  // 0x1003195c
    // Draws the sprites added, last first: one by one when display flag 0x80 is set, else
    // grouped by texture and blending and sorted by depth. 0 on failure.
    int32_t RenderSprites();               // 0x10031a74
    int32_t RenderPartialSprites();        // 0x10031f08

    // The Mac's partial sprite table has room for 0x80 (the list table follows at 0x2200)
    // but AddPartialSprite lets 0x800 in, so the 129th and later overwrite the lists, the
    // blob shadow pointer and the counts. Here the table has its own room.
    SSpriteGameData* mSprites[kMaxSprites];          // 0x0000  mF2404 of them
    SSpriteGameData* mPartialSprites[kMaxSprites];   // 0x2000  mF2408 of them
    CSpriteList* mLists[kMaxLists];                  // 0x2200  mF240C of them
    CTextureEntry* mTextures;              // 0x2400  an array made with new[]: the blob shadow
    int32_t mF2404;                        // 0x2404  0  (sprites)
    int32_t mF2408;                        // 0x2408  0  (partial sprites)
    int32_t mF240C;                        // 0x240c  0  (lists)
};
extern CSpriteManager* gei_SpriteManager;   // engine export

int32_t SpriteManager_Init();    // 0x10031238
int32_t SpriteManager_Close();   // 0x100312f8

class CMatrix;

// The starfield: stars as points on a sphere, drawn as small triangles (src/
// tb_starfield_frame.cpp; the inventory's tb_starfield.cpp range, 0x1003206c..0x10032200).
// 0x1c bytes on the Mac. Another part of the engine fills it in (count, texture and the
// stars themselves); this class draws and frees it.
class CStarfield {
public:
    CStarfield()   // the static initialiser 0x1003266c
        : mCount(0), mF04(0), mTexture(nullptr), mStars(nullptr), mSize(1.0f), mF14(0), mMatrix(nullptr) {}
    ~CStarfield();                         // 0x1003206c: Close()
    // Tells the texture cache that the texture will be drawn (priority 0.5); always 1.
    int32_t Prepare();                     // 0x1003219c
    // Draws every star in front of the camera as a triangle; 0 on failure, 1 when the
    // renderer has no primitive to give.
    int32_t Execute();                     // 0x10032200
    // Frees the texture array and the stars; 1 when there were stars.
    int32_t Close();                       // 0x100320f0

    int32_t mCount;                        // 0x00  stars
    int32_t mF04;                          // 0x04  cleared by Close
    CTextureEntry* mTexture;               // 0x08  an array made with new[]
    float* mStars;                         // 0x0c  three floats a star (an engine allocation; 0x10 on the host)
    float mSize;                           // 0x10  half the height of a star (1.0 at start-up)
    int32_t mF14;                          // 0x14  0
    CMatrix* mMatrix;                      // 0x18  multiplied with the final matrix when set (0x20 on the host)
};
extern CStarfield gStarfield;   // engine data 0x10173158 (not exported; name invented)
// The static initialiser (src/engine/tb_starfield.cpp): constructs gStarfield, registers its destructor.
void __sinit_tb_starfield_cpp();                // 0x1003266c
void StarfieldDestructorThunk(void* object);    // stands for the TVector of ~CStarfield (0x10050750)
extern uint32_t gStarfieldAtExitNode[4];        // engine data 0x10173174 (name invented)
