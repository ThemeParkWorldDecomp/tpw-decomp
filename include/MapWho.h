// MapWho.h: the engine's map "who is where" lists (engine library; CMapWho's methods are
// at 0x10017808..0x10018d70+, filed under tb_mapzoom.cpp by the inventory at medium
// confidence; CMapWho::Render asserts with "tb_map.cpp", so the class is from tb_map.cpp).
//
// PARTIAL: construction, Close and SetFinalCallback are translated (src/tb_mapzoom.cpp);
// the other methods are the ones RenderSystem_* (tb_resource_system.cpp) and CEZSprite
// (tb_ezsprite.cpp) call. Link and Unlink return nonzero on success (CEZSprite tests it);
// the other results are not used by those callers.
#pragma once
#include <cstdint>

#include "tb_mapzoom.h"

class CMatrix;
class CLoadSurface;
class IDirectDrawSurface4;
struct tag_sM3D2Header;
struct SFVF_TLCTEX;
struct sAlignedBox;
struct tag_sNode;
class CLight;
struct SWindow {
    uint32_t mFlags;            // 0x00
    float mX, mY, mWidth, mHeight; // 0x04..0x10, clip-space rectangle
    CMatrix* mMatrix;           // 0x14

    // What CMapWho::RenderWindow does for the window (names invented from the calls each
    // bit makes). The game window has 0x1f31.
    enum : uint32_t {
        kClear = 0x0001,        // clear the viewport first
        kWorld = 0x0010,        // PrepareWorldspace; the sky, the height field, the particles
        kGlobalLists = 0x0020,  // prepare global lists 0..5
        kRain = 0x0100,         // the rain's Prepare and Render
        kParticles = 0x0200,    // the particles' Prepare
        kSky = 0x0400,          // the sky's Prepare
        kShadows = 0x0800,      // shadows from the lights of global list 6
        kLoadSurface = 0x1000,  // the loading backdrop (CMapWho +0x34)
        kMapZoom = 0x2000,      // the map zoom's Prepare and Execute
        kStarfield = 0x4000,    // the starfield's Prepare and Execute
    };
};

// Something that can be linked into a CMapWho list (a virtual base of CSpriteList).
// Its vtable is the destructor, then three slots only the derived classes fill.
class CMapLinkBase {
public:
    CMapLinkBase() {}                                                         // 0x10007b7c (12 bytes): only stores the vtable
    virtual ~CMapLinkBase();                                                  // 0x1001980c
    virtual int32_t Prepare() = 0;
    virtual int32_t GetLinkType() = 0;
    virtual void* GetData() = 0;
};

// Every link base that exists, in no particular order (the engine prepares them all each
// frame). Register: 1 always, but the 0x201st and later are not kept. Free: 1 when the
// object was in the table (the last entry takes its place), 0 when not.
int32_t RegisterLinkBase(CMapLinkBase* link);                                 // 0x10019700
int32_t FreeLinkBase(CMapLinkBase* link);                                     // 0x10019768
extern CMapLinkBase* gLinkBases[0x200];   // engine data 0x100af620 (not exported; names invented)
extern int32_t gLinkBaseCount;            // 0x100af61c

// One entry of a list: 8 bytes on the Mac. The class keeps 0x4800 of them in one block and
// hands them out from a free list.
struct SMapLinkType {
    CMapLinkBase* mLink;     // 0x00
    SMapLinkType* mNext;     // 0x04

    // Prepares the links from this one on while each says it is a light (GetLinkType 1).
    // Called on the list head, which may be NULL (the loop tests `this`;
    // src/tb_lightmanager_frame.cpp).
    void PrepareLights();    // 0x10017454
};

// Mac offsets in the comments; the pointers make the host object larger (0x7c bytes on
// the Mac). Fields named mF<offset> are only set by the constructor so far.
class CMapWho {
public:
    enum : int32_t { kNodeCount = 0x4800 };

    CMapWho();                                                                // 0x100174f8
    ~CMapWho();                                                               // 0x10017720
    // The function the map calls after culling each frame (GAMESYSTEM_Tick installs
    // PostCullOperations for a park and removes it when the park closes).
    void SetFinalCallback(void (*callback)());                                // 0x100177cc
    int32_t Close();                                                          // 0x10017a74
    int32_t Init(int32_t width, int32_t height);                               // 0x10017984
    void QSetup_Game();                                                       // 0x10019640
    int32_t Link_Add(SMapLinkType** head, CMapLinkBase* link);                  // 0x10017808
    // The engine's frame: every registered window, through RenderWindow (src/tb_map_render.cpp).
    // The original returns 1, 0 when the alt-tab recovery fails, or GetAltTabStatus()'s
    // answer when a window fails; both callers (RenderSystem_Render, the main app's
    // EngineTick2) ignore it, and the declaration predates the translation, so it stays void.
    void Render();                                                            // 0x10018d70
    // The game window's flags (mGameWindow.mFlags).
    void SetRenderOptions(int32_t options);                                   // 0x10017ad8
    int32_t GetRenderOptions();                                               // 0x10017b10
    // Prepares the links of global list 6, then those of every map cell in view (mGamut);
    // 0 at the first Prepare that fails.
    int32_t PrepareWorldspace();                                              // 0x100182bc
    // Draws one window; 0 when a step fails (the window stops there).
    int32_t RenderWindow(SWindow* window);                                    // 0x10018470
    // After a lost surface: closes and reopens the texture cache, the renderer and
    // DirectDraw; 0 (everything closed, error 0xb0754014 pushed) when a step fails.
    int32_t ALTTAB_RestoreState();                                            // 0x100194a8
    int32_t Link(CMapLinkBase* link, int32_t list);                           // 0x10017b48
    int32_t Unlink(CMapLinkBase* link, int32_t list);                         // 0x10017c58
    int32_t Reset(int32_t list);                                              // 0x10017d38
    int32_t Link(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link, int32_t list); // 0x10017dd4
    int32_t Unlink(int32_t a, int32_t b, int32_t c, int32_t d, CMapLinkBase* link); // 0x10017f10
    int32_t Reset(int32_t a, int32_t b, int32_t c, int32_t d);                 // 0x100180b4
    int32_t ResetAll();                                                       // 0x100181e8

    SMapLinkType* mNodes;        // 0x00  kNodeCount entries, one engine allocation
    SMapLinkType* mFree;         // 0x04  the entries not in a list, chained through mNext
    int32_t mNodeCount;          // 0x08
    int32_t mNodesUsed;          // 0x0c
    SMapLinkType** mMap;         // 0x10  Init: one list head per map square; freed by Close
    SMapLinkType* mF14[7];       // 0x14..0x2c, global lists (lights precede meshes)
    SMapLinkType* mF30;          // 0x30, list being prepared
    CLoadSurface* mF34;          // 0x34, loading backdrop
    int32_t mWidth;              // 0x38  Init's arguments
    int32_t mHeight;             // 0x3c
    int32_t mF40;                // 0x40  0x30
    void* mGamut;               // 0x44
    SWindow mGameWindow;        // 0x48
    SWindow* mWindows[4];       // 0x60
    int32_t mF70;                // 0x70  0
    int32_t mF74;                // 0x74  -1
    void (*mFinalCallback)();    // 0x78
};

extern CMapWho* gei_MapWho;   // engine data 0x100af618

// Ored into the render list entries made while PrepareWorldspace prepares global list 6
// (0x01000000 then, 0 for the map cells); CRenderList::AddEntry reads it. Engine data
// 0x100b0504 (not exported; name invented).
extern uint32_t gRenderEntryFlags;

// CMapWho::Render's statics (engine data, not exported; names invented): the kind of frame
// it set up last (0x10051c0c, -1 at first: 1 one view, 2 side-by-side stereo, 3 red and blue
// stereo), and the view matrix of the previous one-view frame with one window (0x100af5d4,
// a local static CMatrix made identity on first use; 0x100af614 is its guard byte).
extern int32_t gRenderFrameMode;
extern CMatrix gRenderLastView;
extern int8_t gRenderLastViewMade;

// The loading backdrop a map can keep (CMapWho +0x34; the inventory's tb_mapzoom.cpp range).
// Only Display is known.
class CLoadSurface {
public:
    // Copies the surface onto the back buffer (gei_DDraw->mBack); 0, with the DirectDraw
    // error pushed, when the blit fails.
    int32_t Display();                 // 0x10017270 (src/tb_loadsurface.cpp)

    IDirectDrawSurface4* mSurface;     // 0x00
};

// The map zoom, CMapZoom and gei_MapZoom, is in tb_mapzoom.h (src/tb_mapzoom_class.cpp).

int32_t MapWho_Init();    // 0x10017318
int32_t MapWho_Close();   // 0x100173d0

// The light manager (the inventory's tb_mapzoom.cpp range, 0x100143b0..0x10014e68; the
// construction is in src/tb_mapzoom.cpp, the lighting in src/tb_lightmanager_frame.cpp).
struct SVertex;
struct _D3DVECTOR;
struct tag_sNode;
class CLight;
class CLightManager {
public:
    CLightManager();      // 0x100143b0
    // Works out the colour of each of `count` points from the prepared lights, one word each
    // into `colours`: the points' normals (12 bytes each) and positions (the first three words
    // of 16 bytes each) are read (CHField::CalcFullLighting).
    void CalcIntensities(uint32_t* colours, _D3DVECTOR* normals, SVertex* vertices, int32_t count);   // 0x10014938

    // Works out how much fog each of `count` vertices `stride` bytes apart gets; returns
    // nonzero (a byte) when any vertex is fogged.
    uint8_t CalcVertexFogIntensities(SFVF_TLCTEX* vertices, int32_t count, int32_t stride);   // 0x10014d38
    // Brings the prepared lights (gev_LightChain) into the space of `world` (its inverse, kept
    // in gLightInverseWorld); the box is not read. Returns 1.
    int32_t PrepareLighting(CMatrix* world, sAlignedBox* box);   // 0x100146d4
    // The colour of a point with the given normal: GlobalDiffuse plus each prepared light's
    // share, left in gDiffuseResult (tb_lightmanager.h).
    void CalcDiffuseColor(_D3DVECTOR* normal, float x, float y, float z);   // 0x10014418
    // CalcIntensities for the vertices a mesh node lists (its index words, positions in the
    // node's four-wide position sets, normals in order).
    void CalcNodeVertexColors(uint32_t* colours, tag_sNode* node);   // 0x10014b28
    // Chains the lights of a NULL-terminated array into gev_LightChain, unless a light has flag
    // 0x10000000. Returns 1.
    int32_t AddLights(CLight** lights);   // 0x100148c4

    int32_t mF00;         // 0x00  0
    int32_t mF04;         // 0x04  -1
};
extern CLightManager* gei_LightManager;   // engine data 0x100af1b8 (not exported; name invented)
// The ambient colour the light manager starts from: {1.0, 0.15, 0.15, 0.15} (engine
// export, 0x100af1a8).
extern float GlobalDiffuse[4];

int32_t LightManager_Init();    // 0x10014278
int32_t LightManager_Close();   // 0x10014334
