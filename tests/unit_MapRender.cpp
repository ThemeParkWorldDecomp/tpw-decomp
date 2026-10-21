// Fakes for the engine's frame (src/engine/Gamut.cpp, src/engine/MapWho.cpp, src/engine/Mesh.cpp, src/engine/EngineUtil.cpp,
// src/engine/CsTexture.cpp; tests/test_MapRender.py). Every callee outside the unit records
// its arguments (objects by label, floats by bit pattern) and answers from the case's queues
// (calllog.h); some also record the state the call order matters for (the flag words, the
// list being prepared, the view matrix). The PowerPC side records the same events.
#include "MapWho.h"
#include "Wavelet.h"
#include "calllog.h"
#include "platform/indirectx.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_fpu.h"
#include "tb_hfield.h"
#include "tb_matrix.h"
#include "tb_mesh.h"
#include "tb_meshdelay.h"
#include "tb_particle.h"
#include "tb_rain.h"
#include "tb_rendermemory.h"
#include "tb_resource_display.h"
#include "tb_resource_system.h"
#include "tb_shadow.h"
#include "tb_sky.h"
#include "tb_splash.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"

#include <cstring>
#include <new>

using namespace calllog;

namespace {

constexpr int32_t kNodes = 96;
constexpr int32_t kHeads = 7 + 36;
constexpr int32_t kLinks = 8;
constexpr int32_t kMeshes = 8;

// Fixed labels (the same numbers as LABELS in test_MapRender.py).
enum : int32_t {
    kMap = 1, kOtherMap, kDisplay, kSystem, kHField, kGamut, kTCache, kRenderList, kSpriteManager,
    kRenderMemory, kDrawPrimitive, kParticle, kRain, kSky, kMapZoom, kStarfield, kShadow, kSplash,
    kErrorHandler, kCapture, kDDraw, kRenderSystem, kLoadSurface, kSurface, kBackBuffer, kMeshDelay,
    kLightManager, kFirstMesh = 30,
};

}  // namespace

extern "C" {
// One window of the case: flags, then x, y, width, height as float bits, then which matrix
// (0, 1: the case's window matrices; 2, 3: gei_InternalViewMatrix[0] and [1]).
struct MrWindow {
    uint32_t flags;
    uint32_t x, y, width, height;
    int32_t matrix;
};
// A link of the case: GetLinkType's answer and GetData's vector (or NULL).
struct MrLink {
    int32_t type;
    int32_t hasData;
    uint32_t data[3];
};
// Everything a case sets up; test_MapRender.py fills the same values on the PowerPC side.
struct MrCase {
    uint32_t displayFlags;
    int32_t width, height, frameCount, f84;
    uint32_t separation, fogColor, fogNear, fogFar, fade, f938;
    uint32_t systemFlags;
    int32_t renderer;
    int32_t windowCount;      // mF70
    int32_t hasLoadSurface;   // mF34
    int32_t f40, f74;
    int32_t hasCallback;      // mFinalCallback
    int32_t separateGlobal;   // gei_MapWho is another map than the one under test
    int32_t mapWidth, mapHeight;
    int32_t f30, globalF30;   // node indexes or -1
    int32_t frameMode, lastViewMade, lightMask;
    uint32_t entryFlags;
    int32_t gamut;            // mGamut: 0 NULL, 1 the gamut's quads, 2 the height field's
    uint32_t gameWindowFlags;
    int32_t ddResult;
    uint32_t meshFlags[kMeshes];
    int32_t delayCount;
    int32_t delay[kMeshes];   // mesh indexes in the delay list
    MrWindow windows[4];
    uint32_t view[16];
    uint32_t internal[32];
    uint32_t lastView[16];
    uint32_t windowMatrices[32];
    int32_t heads[kHeads];    // global lists 0..6, then the map cells row by row
    int32_t nodeLink[kNodes]; // link id 1..8, 0 for none
    int32_t nodeNext[kNodes]; // node index, -1 for none
    MrLink links[kLinks];
    int32_t rows[2][2];       // first and end row: the gamut's quads, the height field's
    int8_t low[2][128];
    int8_t high[2][128];
};
}

namespace {

// A link of the map lists: records its calls with its id (1..8).
struct FakeLink : CMapLinkBase {
    int32_t id = 0;
    MrLink spec{};
    int32_t Prepare() override;
    int32_t GetLinkType() override;
    void* GetData() override;
};

// The renderer behind gei_System.mRenderSystem: the three slots the unit calls record.
struct FakeRenderSystem : CRenderSystem {
    int32_t ClearViewport() override;
    int32_t FlipScreen() override { return 0; }
    int32_t TempShutdown() override;
    int32_t ReInitialize() override;
    int32_t GetTextureMemory() override { return 0; }
    int32_t GetVRAMSize() override { return 0; }
    void* CreateTextureBase() override { return nullptr; }
};

// The draw primitive behind gei_DrawPrimitive: the slots the unit calls record.
struct FakeDraw : CDrawPrimitive {
    int32_t Init() override;
    int32_t SetBlendingMode(int32_t) override { return 0; }
    int32_t SetTexture(int32_t, void*) override { return 0; }
    int32_t Execute(SPrimitive*) override { return 0; }
    int32_t ExecuteShadow(SPrimitive*) override { return 0; }
    int32_t SetFogParameters(float start, float end, uint32_t colour) override;
    int32_t BeginDraw() override;
    int32_t EndDraw() override;
    int32_t DrawTestPoly() override { return 0; }
    int32_t DrawFilledBox(float x1, float y1, float x2, float y2, int32_t colour, CTextureBase* texture) override;
    int32_t DrawEdgeBox() override;
};

alignas(CMapWho) unsigned char mapStorage[2][sizeof(CMapWho)];
CMapWho* map;
SMapLinkType nodes[kNodes];
SMapLinkType* cells[36];
FakeLink links[kLinks];
SWindow windows[4];
CMatrix windowMatrices[2];
CHField hfield;
CGamut gamut;
CTextureCache tcache;
CRenderList renderList;
CSpriteManager spriteManager;
CRenderMemory renderMemory;
FakeDraw draw;
CParticle particle;
CRain rain;
CDirectDraw ddraw;
FakeRenderSystem renderSystem;
CLoadSurface loadSurface;
alignas(8) unsigned char surfaces[2][16];
CMesh meshes[kMeshes];
CLightManager lightManager;
GrafPort ports[2];
int32_t delayAdds[kMeshes];   // MeshPrepare: the mesh index it appends to the delay list (once), or -1

// The node index of a list pointer (-1 for NULL, -2 outside the pool).
int32_t Node(const SMapLinkType* p) {
    if (p == nullptr) return -1;
    if (p >= nodes && p < nodes + kNodes) return static_cast<int32_t>(p - nodes);
    return -2;
}

// A node index of the case as a list pointer.
SMapLinkType* NodeAt(int32_t index) { return index < 0 ? nullptr : &nodes[index]; }

// A float from its bit pattern.
float Float(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

// The bit pattern of each word of a matrix.
void Words(const CMatrix& m, uint32_t* out) { std::memcpy(out, &m, sizeof(CMatrix)); }

// Fills a matrix from bit patterns.
void SetWords(CMatrix& m, const uint32_t* words) { std::memcpy(&m, words, sizeof(CMatrix)); }

// The window matrix a case's selector names.
CMatrix* WindowMatrix(int32_t which) {
    if (which < 2) return &windowMatrices[which];
    return &gei_InternalViewMatrix[which - 2];
}

// Which quads a map's mGamut points at: 0 NULL, 1 the gamut's, 2 the height field's, -1 other.
int32_t GamutCode(const void* p) {
    if (p == nullptr) return 0;
    if (p == static_cast<const void*>(&gamut.mFC4)) return 1;
    if (p == static_cast<const void*>(&hfield.mF128)) return 2;
    return -1;
}

// The final callback: records the map's mF74 at the time (it is updated after the call).
void FinalCallback() { ev("FinalCallback", {map->mF74}); }

// The engine's hooks.
int32_t IsFirstPerson() {
    ev("IsFirstPerson");
    return static_cast<int32_t>(ret("IsFirstPerson", 0));
}
void UpdateMouse() { ev("UpdateMouse"); }

}  // namespace

// ---- Globals the unit reads (defined by untranslated or other files in the game) ----------
CSystem gei_System;
CDisplay gei_Display;
CMatrix gei_InternalViewMatrix[2];
CMatrix* gei_ViewMatrix;
CMapWho* gei_MapWho;
CHField* gei_HField = &hfield;
CGamut* gei_Gamut = &gamut;
CTextureCache* gei_TCache = &tcache;
CRenderList* gei_RenderList = &renderList;
CSpriteManager* gei_SpriteManager = &spriteManager;
CRenderMemory* gei_RenderMemory = &renderMemory;
CDrawPrimitive* gei_DrawPrimitive = &draw;
CParticle* gei_Particle = &particle;
CRain* gei_Rain = &rain;
CSky gei_Sky;
CMapZoom gei_MapZoom;
CStarfield gStarfield;
CShadow gShadow;
CSplash gei_Splash;
CErrorHandler gei_ErrorHandler;
CCapture gei_Capture;
CDirectDraw* gei_DDraw = &ddraw;
int32_t gDDResult;
CLightManager* gei_LightManager = &lightManager;
TbIsFirstPersonFn gIsFirstPerson = IsFirstPerson;
TbUpdateMouseFn gUpdateMouse = UpdateMouse;

// ---- Constructors and destructors of the objects above (not part of the unit) -------------
CSystem::CSystem() {}
CDisplay::CDisplay() {}
CMapLinkBase::~CMapLinkBase() {}
CMapWho::CMapWho() {}
CMapWho::~CMapWho() {}
CHField::CHField() {}
CHField::~CHField() {}
CGamut::CGamut() {}
CTextureCache::CTextureCache() {}
CSpriteManager::CSpriteManager() {}
CSpriteManager::~CSpriteManager() {}
CRenderMemory::CRenderMemory() {}
CDrawPrimitive::CDrawPrimitive() {}
CDrawPrimitive::~CDrawPrimitive() {}
CParticle::CParticle() {}
CParticle::~CParticle() {}
int32_t CParticle::GetLinkType() { return 4; }
void* CParticle::GetData() { return nullptr; }
CRain::CRain() {}
CDirectDraw::CDirectDraw() {}
CDirectDraw::~CDirectDraw() {}
CSky::CSky() {}
CSky::~CSky() {}
CShadow::~CShadow() {}
CSplash::~CSplash() {}
CMesh::CMesh() {}
CMesh::~CMesh() {}
int32_t CMesh::GetLinkType() { return 2; }
void* CMesh::GetData() { return nullptr; }
CLightManager::CLightManager() {}

// ---- The fakes ----------------------------------------------------------------------------
// The links: Prepare records the list being prepared (gei_MapWho's mF30) too.
int32_t FakeLink::Prepare() {
    ev("LinkPrepare", {id, Node(gei_MapWho->mF30)});
    return static_cast<int32_t>(ret("LinkPrepare", id, 1));
}
int32_t FakeLink::GetLinkType() {
    ev("LinkType", {id});
    return spec.type;
}
void* FakeLink::GetData() {
    ev("LinkData", {id});
    return spec.hasData ? spec.data : nullptr;
}

// The renderer's slots.
int32_t FakeRenderSystem::ClearViewport() {
    ev("ClearViewport", {lab(this)});
    return static_cast<int32_t>(ret("ClearViewport", 1));
}
int32_t FakeRenderSystem::TempShutdown() {
    ev("RSTempShutdown", {lab(this)});
    return static_cast<int32_t>(ret("RSTempShutdown", 1));
}
int32_t FakeRenderSystem::ReInitialize() {
    ev("RSReInitialize", {lab(this)});
    return static_cast<int32_t>(ret("RSReInitialize", 1));
}

// The draw primitive's slots.
int32_t FakeDraw::Init() {
    ev("DPInit", {lab(this)});
    return static_cast<int32_t>(ret("DPInit", 1));
}
int32_t FakeDraw::SetFogParameters(float start, float end, uint32_t colour) {
    ev("Fog", {lab(this), fbits(start), fbits(end), colour});
    return static_cast<int32_t>(ret("Fog", 1));
}
int32_t FakeDraw::BeginDraw() {
    ev("BeginDraw", {lab(this)});
    return static_cast<int32_t>(ret("BeginDraw", 1));
}
int32_t FakeDraw::EndDraw() {
    ev("EndDraw", {lab(this)});
    return static_cast<int32_t>(ret("EndDraw", 1));
}
int32_t FakeDraw::DrawFilledBox(float x1, float y1, float x2, float y2, int32_t colour, CTextureBase* texture) {
    ev("FilledBox", {lab(this), fbits(x1), fbits(y1), fbits(x2), fbits(y2), colour, lab(texture)});
    return static_cast<int32_t>(ret("FilledBox", 1));
}
int32_t FakeDraw::DrawEdgeBox() {
    ev("EdgeBox", {lab(this)});
    return static_cast<int32_t>(ret("EdgeBox", 1));
}

// PreprocessMatrices records the view the window installed.
int32_t PreprocessMatrices() {
    uint32_t w[16];
    Words(gei_InternalViewMatrix[0], w);
    ev("ViewA", {w[0], w[1], w[2], w[3], w[4], w[5], w[6], w[7]});
    ev("ViewB", {w[8], w[9], w[10], w[11], w[12], w[13], w[14], w[15]});
    return 1;
}

// SetWindow records the two flag words and the light colour mask at the time too.
void CDisplay::SetWindow(float x, float y, float width, float height) {
    ev("SetWindow", {lab(this), fbits(x), fbits(y), fbits(width), fbits(height), mFlags, gei_System.mFlags,
                     gei_LightManager->mF04});
}
void CDisplay::ResetWindow() { ev("ResetWindow", {lab(this)}); }

// The height field and the gamut.
int32_t CHField::DivideAndConquer() {
    ev("DivideAndConquer", {lab(this)});
    return 1;
}
int32_t CHField::Prepare() {
    ev("HFPrepare", {lab(this), gRenderEntryFlags});
    return static_cast<int32_t>(ret("HFPrepare", 1));
}
int32_t CHField::Execute() {
    ev("HFExecute", {lab(this)});
    return static_cast<int32_t>(ret("HFExecute", 1));
}
int32_t CHField::DrawSkyShadow() {
    ev("SkyShadow", {lab(this)});
    return static_cast<int32_t>(ret("SkyShadow", 1));
}
int32_t CGamut::CalculateGamut() {
    ev("CalculateGamut", {lab(this)});
    return 1;
}

// The texture cache.
int32_t CTextureCache::Refresh() {
    ev("TCRefresh", {lab(this)});
    return 1;
}
int32_t CTextureCache::Execute() {
    ev("TCExecute", {lab(this)});
    return static_cast<int32_t>(ret("TCExecute", 1));
}
int32_t CTextureCache::Close() {
    ev("TCClose", {lab(this)});
    return static_cast<int32_t>(ret("TCClose", 1));
}
int32_t CTextureCache::ReInitialize(int32_t format) {
    ev("TCReInitialize", {lab(this), format});
    return static_cast<int32_t>(ret("TCReInitialize", 1));
}

// The render list, the sprite manager and the render memory.
void CRenderList::Reset() { ev("RLReset", {lab(this)}); }
int32_t CRenderList::Render() {
    ev("RLRender", {lab(this)});
    return static_cast<int32_t>(ret("RLRender", 1));
}
void CSpriteManager::Reset() { ev("SMReset", {lab(this)}); }
int32_t CSpriteManager::Prepare() {
    ev("SMPrepare", {lab(this)});
    return 1;
}
int32_t CSpriteManager::Render() {
    ev("SMRender", {lab(this)});
    return static_cast<int32_t>(ret("SMRender", 1));
}
void CRenderMemory::Reset(int32_t pools) { ev("RMReset", {lab(this), pools}); }

// The particles, the rain, the sky, the map zoom and the starfield.
int32_t CParticle::Prepare() {
    ev("ParticlePrepare", {lab(this)});
    return 1;
}
int32_t CParticle::PlotParticles() {
    ev("PlotParticles", {lab(this)});
    return static_cast<int32_t>(ret("PlotParticles", 1));
}
int32_t CRain::Prepare() {
    ev("RainPrepare", {lab(this)});
    return 1;
}
int32_t CRain::Render() {
    ev("RainRender", {lab(this)});
    return static_cast<int32_t>(ret("RainRender", 1));
}
int32_t CSky::Prepare() {
    ev("SkyPrepare", {lab(this)});
    return 1;
}
int32_t CSky::Render() {
    ev("SkyRender", {lab(this)});
    return static_cast<int32_t>(ret("SkyRender", 1));
}
CMapZoom::CMapZoom() {}    // the real one (src/engine/tb_mapzoom.cpp) is outside this unit
CMapZoom::~CMapZoom() {}
int32_t CMapZoom::Prepare() {
    ev("ZoomPrepare", {lab(this)});
    return 1;
}
int32_t CMapZoom::Execute() {
    ev("ZoomExecute", {lab(this)});
    return static_cast<int32_t>(ret("ZoomExecute", 1));
}
CStarfield::~CStarfield() {}   // the real one (src/engine/tb_starfield.cpp) is outside this unit
int32_t CStarfield::Prepare() {
    ev("StarPrepare", {lab(this)});
    return 1;
}
int32_t CStarfield::Execute() {
    ev("StarExecute", {lab(this)});
    return static_cast<int32_t>(ret("StarExecute", 1));
}

// The shadows: the light direction's three words.
void CShadow::ResetLightSources() { ev("ResetLights", {lab(this)}); }
int32_t CShadow::AddLightSource(D3DVECTOR direction) {
    uint32_t w[3];
    std::memcpy(w, &direction, sizeof w);
    ev("AddLight", {lab(this), w[0], w[1], w[2]});
    return 1;
}

// A delayed mesh's Prepare: records its flags and the delay count, and may append a mesh
// (the first time only).
int32_t CMesh::Prepare() {
    const int32_t index = static_cast<int32_t>(this - meshes);
    ev("MeshPrepare", {lab(this), mFlags, gMeshDelay.mCount});
    if (delayAdds[index] >= 0) {
        gMeshDelay.mMeshes[gMeshDelay.mCount] = &meshes[delayAdds[index]];
        gMeshDelay.mCount++;
        delayAdds[index] = -1;
    }
    return 1;
}

// The blit of the loading backdrop.
int32_t IDirectDrawSurface4::Blt(SamsUtilities::URectangle::LongRect* dest, IDirectDrawSurface4* source,
                                 SamsUtilities::URectangle::LongRect* sourceRect, uint32_t flags, DDBLTFX* fx) {
    ev("Blt", {lab(this), lab(dest), lab(source), lab(sourceRect), flags, lab(fx)});
    return static_cast<int32_t>(ret("Blt", 0));
}

// DirectDraw's recovery.
int32_t CDirectDraw::TempShutdown() {
    ev("DDTempShutdown", {lab(this)});
    return static_cast<int32_t>(ret("DDTempShutdown", 1));
}
int32_t CDirectDraw::ReInitialize() {
    ev("DDReInitialize", {lab(this)});
    return static_cast<int32_t>(ret("DDReInitialize", 1));
}
int32_t CDirectDraw::Close() {
    ev("DDClose", {lab(this)});
    return 1;
}

// The window manager, the assertion, the splash, the errors, the system and the capture.
GrafPort* FrontWindow() {
    ev("FrontWindow");
    return ret("FrontWindow", 1) != 0 ? gei_System.mWindow : &ports[1];
}
void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("Assert", {line}, {condition, file});
}
int32_t CSplash::Close() {
    ev("SplashClose", {lab(this)});
    return 1;
}
void CErrorHandler::Push(int32_t level, int32_t code) { ev("Push", {lab(this), level, code}); }
int32_t CErrorHandler::GetAltTabStatus() {
    ev("AltTabStatus", {lab(this)});
    return static_cast<int32_t>(ret("AltTabStatus", 0));
}
void CSystem::UpdateLights() { ev("UpdateLights", {lab(this), mFlags}); }
int32_t RenderSystem_Flush() {
    ev("Flush");
    return 1;
}
int32_t RenderSystem_Close() {
    ev("RSClose");
    return 1;
}
int32_t CCapture::Capture() {
    ev("Capture", {lab(this), gei_System.mFlags, gei_Display.mFlags});
    return 1;
}

// ---- The test's entry points ----------------------------------------------------------------
extern "C" {

// Sets every object up as the case says and labels them.
void tpw_mr_begin(const MrCase* c) {
    std::memset(mapStorage, 0xa5, sizeof mapStorage);
    map = new (mapStorage[0]) CMapWho;
    CMapWho* global = c->separateGlobal ? new (mapStorage[1]) CMapWho : map;
    gei_MapWho = global;
    for (int32_t i = 0; i < kNodes; i++) {
        nodes[i].mLink = c->nodeLink[i] > 0 ? &links[c->nodeLink[i] - 1] : nullptr;
        nodes[i].mNext = NodeAt(c->nodeNext[i]);
    }
    for (int32_t i = 0; i < 7; i++) map->mF14[i] = NodeAt(c->heads[i]);
    for (int32_t i = 0; i < 36; i++) cells[i] = NodeAt(c->heads[7 + i]);
    map->mMap = cells;
    map->mWidth = c->mapWidth;
    map->mHeight = c->mapHeight;
    map->mF30 = NodeAt(c->f30);
    if (global != map) global->mF30 = NodeAt(c->globalF30);
    map->mF34 = c->hasLoadSurface ? &loadSurface : nullptr;
    map->mF40 = c->f40;
    map->mF70 = c->windowCount;
    map->mF74 = c->f74;
    map->mFinalCallback = c->hasCallback ? FinalCallback : nullptr;
    map->mGameWindow.mFlags = c->gameWindowFlags;
    map->mGamut = c->gamut == 1 ? static_cast<void*>(gamut.GetQuads()) : c->gamut == 2 ? &hfield.mF128 : nullptr;
    for (int32_t i = 0; i < 4; i++) {
        windows[i].mFlags = c->windows[i].flags;
        windows[i].mX = Float(c->windows[i].x);
        windows[i].mY = Float(c->windows[i].y);
        windows[i].mWidth = Float(c->windows[i].width);
        windows[i].mHeight = Float(c->windows[i].height);
        windows[i].mMatrix = WindowMatrix(c->windows[i].matrix);
        map->mWindows[i] = &windows[i];
    }
    for (int32_t i = 0; i < kLinks; i++) {
        links[i].id = i + 1;
        links[i].spec = c->links[i];
    }
    SGamutQuads* quads[2] = {gamut.GetQuads(), reinterpret_cast<SGamutQuads*>(&hfield.mF128)};
    for (int32_t q = 0; q < 2; q++) {
        quads[q]->mFirstRow = c->rows[q][0];
        quads[q]->mEndRow = c->rows[q][1];
        std::memcpy(quads[q]->mLow, c->low[q], 128);
        std::memcpy(quads[q]->mHigh, c->high[q], 128);
    }
    gei_Display.mFlags = c->displayFlags;
    gei_Display.mWidth = c->width;
    gei_Display.mHeight = c->height;
    gei_Display.mFrameCount = c->frameCount;
    gei_Display.mF84 = c->f84;
    gei_Display.mF8B8 = Float(c->separation);
    gei_Display.mFogColor = c->fogColor;
    gei_Display.mFogNear = Float(c->fogNear);
    gei_Display.mFogFar = Float(c->fogFar);
    gei_Display.mFade = c->fade;
    gei_Display.mF938 = Float(c->f938);
    gei_System.mFlags = c->systemFlags;
    gei_System.mRenderer = c->renderer;
    gei_System.mWindow = &ports[0];
    gei_System.mRenderSystem = &renderSystem;
    SetWords(gei_InternalViewMatrix[0], c->internal);
    SetWords(gei_InternalViewMatrix[1], c->internal + 16);
    SetWords(windowMatrices[0], c->windowMatrices);
    SetWords(windowMatrices[1], c->windowMatrices + 16);
    static CMatrix view;
    SetWords(view, c->view);
    gei_ViewMatrix = &view;
    gRenderFrameMode = c->frameMode;
    gRenderLastViewMade = static_cast<int8_t>(c->lastViewMade);
    SetWords(gRenderLastView, c->lastView);
    lightManager.mF04 = c->lightMask;
    gRenderEntryFlags = c->entryFlags;
    gDDResult = c->ddResult;
    for (int32_t i = 0; i < kMeshes; i++) {
        meshes[i].mFlags = c->meshFlags[i];
        delayAdds[i] = static_cast<int32_t>(ret("MeshAdd", i, -1));
    }
    gMeshDelay.mCount = c->delayCount;
    for (int32_t i = 0; i < 8; i++) gMeshDelay.mMeshes[i] = &meshes[c->delay[i]];
    loadSurface.mSurface = reinterpret_cast<IDirectDrawSurface4*>(surfaces[0]);
    ddraw.mBack = reinterpret_cast<IDirectDrawSurface4*>(surfaces[1]);
    const void* labelled[] = {map, global != map ? global : nullptr, &gei_Display, &gei_System, &hfield, &gamut,
                              &tcache, &renderList, &spriteManager, &renderMemory, &draw, &particle, &rain,
                              &gei_Sky, &gei_MapZoom, &gStarfield, &gShadow, &gei_Splash, &gei_ErrorHandler,
                              &gei_Capture, &ddraw, &renderSystem, &loadSurface, surfaces[0], surfaces[1],
                              &gMeshDelay, &lightManager};
    for (int32_t i = 0; i < static_cast<int32_t>(sizeof labelled / sizeof labelled[0]); i++)
        tpw_cl_label(i + 1, labelled[i]);
    for (int32_t i = 0; i < kMeshes; i++) tpw_cl_label(kFirstMesh + i, &meshes[i]);
}

// Runs one operation (the numbers of OPS in test_MapRender.py); its result, or 0.
int32_t tpw_mr_run(int32_t op, int32_t arg) {
    switch (op) {
    case 1:
        map->Render();
        return 0;
    case 2:
        return map->RenderWindow(&windows[0]);
    case 3:
        return map->PrepareWorldspace();
    case 4:
        return map->ALTTAB_RestoreState();
    case 5:
        map->SetRenderOptions(arg);
        return 0;
    case 6:
        return map->GetRenderOptions();
    case 7:
        return GamutCode(gamut.GetQuads());
    case 8:
        gMeshDelay.Reset();
        return 0;
    case 9:
        return gMeshDelay.Prepare();
    case 10:
        return loadSurface.Display();
    case 11: {
        const int16_t previous = FPU_SetSinglePrecision();
        FPU_RestorePrecision(static_cast<int16_t>(arg));
        FPU_SetFullPrecision(static_cast<int16_t>(arg));
        return previous;
    }
    }
    return -99;
}

// Everything the unit may have changed, in the order test_MapRender.py reads it.
int32_t tpw_mr_state(uint32_t* out) {
    int32_t n = 0;
    for (int32_t m = 0; m < 2; m++) {
        Words(gei_InternalViewMatrix[m], out + n);
        n += 16;
    }
    out[n++] = gei_Display.mFlags;
    out[n++] = static_cast<uint32_t>(gei_Display.mF84);
    out[n++] = fbits(gei_Display.mF938);
    out[n++] = gei_System.mFlags;
    out[n++] = static_cast<uint32_t>(GamutCode(map->mGamut));
    out[n++] = static_cast<uint32_t>(Node(map->mF30));
    out[n++] = static_cast<uint32_t>(Node(gei_MapWho->mF30));
    out[n++] = static_cast<uint32_t>(map->mF74);
    out[n++] = map->mGameWindow.mFlags;
    out[n++] = static_cast<uint32_t>(gMeshDelay.mCount);
    for (int32_t i = 0; i < kMeshes; i++) out[n++] = meshes[i].mFlags;
    out[n++] = gRenderEntryFlags;
    out[n++] = static_cast<uint32_t>(gRenderFrameMode);
    out[n++] = static_cast<uint32_t>(gRenderLastViewMade);
    Words(gRenderLastView, out + n);
    n += 16;
    out[n++] = static_cast<uint32_t>(lightManager.mF04);
    out[n++] = static_cast<uint32_t>(gDDResult);
    for (const SWindow& w : windows) {
        out[n++] = w.mFlags;
        out[n++] = fbits(w.mX);
        out[n++] = fbits(w.mY);
        out[n++] = fbits(w.mWidth);
        out[n++] = fbits(w.mHeight);
    }
    return n;
}

}  // extern "C"
