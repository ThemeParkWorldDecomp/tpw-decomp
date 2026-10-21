// MapWho.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "Light.h"
#include "m3d2.h"
#include "MapWho.h"
#include "platform/d3d_types.h"
#include "platform/interfacelib.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "tb_capture.h"
#include "tb_ddraw.h"
#include "tb_drawprimitive.h"
#include "tb_error.h"
#include "tb_fpu.h"
#include "tb_hfield.h"
#include "tb_light.h"
#include "tb_lightmanager.h"
#include "tb_matrix.h"
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
#include "tpw_probe.h"
#include "Wavelet.h"
#include <cmath>
#include <cstring>
#include <new>

// tb_lightmanager_frame.cpp (engine library of SimTheme Park 2.04b3): the light manager's
// per-frame lighting. The inventory files the range under tb_mapzoom.cpp (the construction and
// Init/Close are in src/tb_mapzoom.cpp); the function after the light manager in the binary,
// SMapLinkType::PrepareLights, is the map lists' and sits here because the lights are what it
// prepares.
//
//   0x10014418 CLightManager::CalcDiffuseColor      (628 bytes)
//   0x100146d4 CLightManager::PrepareLighting       (420)
//   0x100148c4 CLightManager::AddLights             (60)
//   0x10014938 CLightManager::CalcIntensities       (416)
//   0x10014b28 CLightManager::CalcNodeVertexColors  (456)
//   0x10014d38 CLightManager::CalcVertexFogIntensities (224)
//   0x10017454 SMapLinkType::PrepareLights          (112)

namespace {

// The loop of SMapLinkType::PrepareLights over a list that may be empty. Out of line so that
// the compiler cannot take the member function's `this` for non-null and drop the test.
__attribute__((noinline)) void PrepareLightList(SMapLinkType* node) {
    while (node != nullptr) {
        CMapLinkBase* link = node->mLink;
        if (link->GetLinkType() == 1) {
            link->Prepare();
            node = node->mNext;
        } else {
            node = nullptr;
        }
    }
}

}  // namespace

// Prepares the links of a map list from its head on, as long as each says it is a light.
void SMapLinkType::PrepareLights() {
    SMapLinkType* head = this;
    asm volatile("" : "+r"(head));
    PrepareLightList(head);
}

// Map setup and list operations from the original engine's tb_map.cpp.

int32_t CMapWho::Link_Add(SMapLinkType** head, CMapLinkBase* link) {
    tpw_probe::Hit(tpw_probe::kMapLinkAdd);
    SMapLinkType* node = mFree;
    if (node != nullptr) mFree = node->mNext;
    ++mNodesUsed;
    if (node == nullptr) { tpw_probe::Hit(tpw_probe::kMapLinkNoNode); return 0; }
    node->mLink = link;
    for (auto* existing = *head; existing != nullptr; existing = existing->mNext)
        if (existing->mLink == link) { tpw_probe::Hit(tpw_probe::kMapLinkDuplicate); return 1; }
    // Duplicate links consume a free node in the original too.
    SMapLinkType* previous = nullptr;
    SMapLinkType* next = *head;
    if (link->GetLinkType() != 1) {
        while (next != nullptr && next->mLink->GetLinkType() == 1) {
            previous = next;
            next = next->mNext;
        }
    }
    node->mNext = next;
    if (previous == nullptr) *head = node;
    else previous->mNext = node;
    gei_System.mFlags |= 0x80000000u;
    return 1;
}

int32_t CMapWho::Init(int32_t width, int32_t height) {
    if (mMap != nullptr) Close();
    mWidth = width;
    mHeight = height;
    const uint32_t cells = static_cast<uint32_t>(width * height);
    mMap = static_cast<SMapLinkType**>(gMalloc(cells * sizeof(SMapLinkType*)));
    if (mMap == nullptr) gei_ErrorHandler.Push(3, kErrOutOfMemory);
    else {
        std::memset(mMap, 0, cells * sizeof(SMapLinkType*));
        QSetup_Game();
    }
    return mMap != nullptr;
}

// tb_map_render.cpp: the engine's frame (engine library, the inventory's tb_mapzoom.cpp
// range; CMapWho::Render asserts with "tb_map.cpp"). CMapWho::Render 0x10018d70 draws every
// registered window (twice or three times each in the stereo modes), RenderWindow 0x10018470
// one window, PrepareWorldspace 0x100182bc the links in view, ALTTAB_RestoreState
// 0x100194a8 the recovery after a lost surface, SetRenderOptions 0x10017ad8 and
// GetRenderOptions 0x10017b10 the game window's flags; CGamut::GetQuads 0x1000bc40 (the
// tb_hfield.cpp range) is here because only the frame uses it.

uint32_t gRenderEntryFlags = 0;
int32_t gRenderFrameMode = -1;
CMatrix gRenderLastView;
int8_t gRenderLastViewMade = 0;

namespace {

// The errors Render and ALTTAB_RestoreState push (level 3).
constexpr int32_t kErrAltTabRender = static_cast<int32_t>(0xb0754013);
constexpr int32_t kErrAltTabRestore = static_cast<int32_t>(0xb0754014);

// Global list 6: the lights (CLight links there), prepared first by PrepareWorldspace.
constexpr int32_t kLightList = 6;

// Makes `head` the list being prepared (gei_MapWho's mF30, whichever map is drawing) and
// prepares its links; 0 at the first that fails. Inline in the original (three copies; the
// name is invented). The next link is read after each Prepare.
int32_t PrepareList(SMapLinkType* head) {
    gei_MapWho->mF30 = head;
    for (SMapLinkType* node = head; node != nullptr; node = node->mNext) {
        tpw_probe::Hit(tpw_probe::kPrepareListLinks);
        if (node->mLink->Prepare() == 0) return 0;
    }
    return 1;
}

// The renderers RenderWindow draws the edge box and the fade for (a switch in the original:
// 2 and anything outside 1..3 skip).
bool DrawsOverlays(int32_t renderer) { return renderer == 1 || renderer == 3; }

}  // namespace

void CMapWho::SetRenderOptions(int32_t options) { mGameWindow.mFlags = static_cast<uint32_t>(options); }

int32_t CMapWho::GetRenderOptions() { return static_cast<int32_t>(mGameWindow.mFlags); }

int32_t CMapWho::Link(CMapLinkBase* link, int32_t list) {
    if ((list & 0x70) != 0) return Link_Add(&mF14[(list >> 4) - 1], link);
    if ((list & 1) != 0 && link->GetLinkType() == 2) {
        auto* data = static_cast<tag_sM3D2Header*>(link->GetData());
        if (data != nullptr) return Link(static_cast<int8_t>(data->mFA8[3]), static_cast<int8_t>(data->mFA8[1]),
                                       static_cast<int8_t>(data->mFA8[2]), static_cast<int8_t>(data->mFA8[0]), link, 0);
    }
    return 0;
}

int32_t CMapWho::PrepareWorldspace() {
    gRenderEntryFlags = 0x01000000;
    for (SMapLinkType* n = mF14[kLightList]; n != nullptr; n = n->mNext) tpw_probe::Hit(tpw_probe::kGlobalList6);
    if (mF14[kLightList] != nullptr && PrepareList(mF14[kLightList]) == 0) return 0;
    if (gei_HField->Prepare() == 0) return 0;
    gRenderEntryFlags = 0;
    if (mGamut == nullptr) return 1;
    for (int32_t row = static_cast<SGamutQuads*>(mGamut)->mFirstRow; row < static_cast<SGamutQuads*>(mGamut)->mEndRow;
         row++) {
        const SGamutQuads* quads = static_cast<SGamutQuads*>(mGamut);
        const int32_t low = quads->mLow[row];
        int32_t count = quads->mHigh[row] - low;
        SMapLinkType** cell = &mMap[low + row * mWidth];   // no bounds check in the original
        while (count-- > 0) {
            if (PrepareList(*cell) == 0) return 0;
            cell++;
        }
    }
    return 1;
}

int32_t CMapWho::RenderWindow(SWindow* window) {
    // The window's view is the engine's for the window; the previous one comes back at the end.
    const CMatrix view = gei_InternalViewMatrix[0];
    gei_InternalViewMatrix[0] = *window->mMatrix;
    PreprocessMatrices();
    gei_Display.SetWindow(window->mX, window->mY, window->mWidth, window->mHeight);
    if ((window->mFlags & SWindow::kClear) != 0 && gei_System.mRenderSystem->ClearViewport() == 0) goto failed;
    gei_Display.mF938 =
        ((gei_Display.mFlags & CDisplay::kEnableFog) != 0 || gei_System.mRenderer == 4) ? 0.25f : 0.6f;
    if ((window->mFlags & SWindow::kLoadSurface) != 0 && mF34 != nullptr) mF34->Display();
    if (gIsFirstPerson() != 0) gei_System.mFlags |= CSystem::kFirstPerson;
    if ((gei_System.mFlags & CSystem::kFirstPerson) != 0) {
        CHField* field = gei_HField;
        field->DivideAndConquer();
        mGamut = &field->mF128;   // its SGamutQuads
    } else {
        gei_Gamut->CalculateGamut();
        mGamut = gei_Gamut->GetQuads();
    }
    gei_TCache->Refresh();
    gei_RenderList->Reset();
    gei_SpriteManager->Reset();
    gei_RenderMemory->Reset((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) != 0
                                ? CRenderMemory::kStatic | CRenderMemory::kDynamic
                                : CRenderMemory::kDynamic);
    mF30 = nullptr;
    gMeshDelay.Reset();
    gei_DrawPrimitive->SetFogParameters(gei_Display.mFogNear, gei_Display.mFogFar, gei_Display.mFogColor);
    if (gei_DrawPrimitive->BeginDraw() == 0) goto failed;
    if ((window->mFlags & SWindow::kWorld) != 0 && PrepareWorldspace() == 0) goto failed;
    if ((window->mFlags & SWindow::kGlobalLists) != 0) {
        // The original takes the number of lists from bit 0 of mF40, but both choices are 6.
        for (int32_t list = 0; list < 6; list++)
            if (PrepareList(mF14[list]) == 0) goto failed;
    }
    if ((window->mFlags & SWindow::kParticles) != 0) gei_Particle->Prepare();
    if ((window->mFlags & SWindow::kRain) != 0) gei_Rain->Prepare();
    if ((window->mFlags & SWindow::kSky) != 0) gei_Sky.Prepare();
    if ((window->mFlags & SWindow::kMapZoom) != 0) gei_MapZoom.Prepare();
    if ((window->mFlags & SWindow::kStarfield) != 0) gStarfield.Prepare();
    // Once a frame however many windows there are.
    if (mFinalCallback != nullptr && mF74 != gei_Display.mFrameCount) {
        mFinalCallback();
        mF74 = gei_Display.mFrameCount;
    }
    gei_SpriteManager->Prepare();
    gMeshDelay.Prepare();
    if (gei_TCache->Execute() == 0) goto failed;
    if ((window->mFlags & SWindow::kWorld) != 0) {
        if (gei_Sky.Render() == 0 || gei_HField->Execute() == 0) goto failed;
        if ((gei_Display.mFlags & CDisplay::kFlag10000) != 0 && gei_HField->DrawSkyShadow() == 0) goto failed;
        if (gei_Particle->PlotParticles() == 0) goto failed;
    }
    if ((window->mFlags & SWindow::kMapZoom) != 0 && gei_MapZoom.Execute() == 0) goto failed;
    if ((window->mFlags & SWindow::kStarfield) != 0 && gStarfield.Execute() == 0) goto failed;
    if ((window->mFlags & SWindow::kShadows) != 0) {
        gShadow.ResetLightSources();
        SMapLinkType* lights = mF14[kLightList];
        if ((gei_Display.mFlags & 0x20000) == 0 && lights != nullptr) {
            gei_MapWho->mF30 = lights;
            for (SMapLinkType* node = lights; node != nullptr; node = node->mNext) {
                if (node->mLink->GetLinkType() != 1) continue;   // a CLight
                const auto* direction = static_cast<const D3DVECTOR*>(node->mLink->GetData());
                if (direction != nullptr) gShadow.AddLightSource(*direction);
            }
        }
    }
    if (gei_SpriteManager->Render() == 0) goto failed;
    if ((window->mFlags & SWindow::kRain) != 0 && gei_Rain->Render() == 0) goto failed;
    if (gei_RenderList->Render() == 0) goto failed;
    if (DrawsOverlays(gei_System.mRenderer) && (gei_Display.mFlags & (0x10000000 | 0x08000000 | 0x2)) == 0)
        gei_DrawPrimitive->DrawEdgeBox();
    if (gei_DrawPrimitive->EndDraw() == 0) goto failed;
    // The fade (CLBLobbyView::UpdatedFade sets display flag 0x80000 and mFade): a box over
    // the whole screen in its own draw.
    if ((gei_Display.mFlags & 0x80000) != 0 && DrawsOverlays(gei_System.mRenderer)) {
        if (gei_DrawPrimitive->BeginDraw() == 0) goto failed;
        if (gei_DrawPrimitive->DrawFilledBox(0.0f, 0.0f, static_cast<float>(gei_Display.mWidth),
                                             static_cast<float>(gei_Display.mHeight),
                                             static_cast<int32_t>(gei_Display.mFade), nullptr) == 0)
            goto failed;
        if (gei_DrawPrimitive->EndDraw() == 0) goto failed;
    }
    gei_InternalViewMatrix[0] = view;
    gei_Display.mF84++;
    return 1;
failed:
    gei_DrawPrimitive->EndDraw();
    gei_InternalViewMatrix[0] = view;
    return 0;
}

void CMapWho::Render() {
    if ((gei_System.mFlags & 0x40) != 0) gei_System.mFlags |= CSystem::kViewChanged;
    if ((gei_Display.mFlags & CDisplay::kEnableFog) != 0 && (gei_Display.mFlags & CDisplay::kSpecularEnable) != 0)
        msl_assertion_failed(
            "!((gei_Display.md_Flags&DISPLAY__ENABLEFOG) && (gei_Display.md_Flags&DISPLAY__SPECULARENABLE))",
            "tb_map.cpp", 1106);
    const uint32_t displayFlags = gei_Display.mFlags;   // both flag words are put back at the end
    if (FrontWindow() != gei_System.mWindow) return;   // 1 in the original
    if ((gei_System.mFlags & CSystem::kFlag00000001) != 0) {   // the splash is up
        gei_System.mFlags &= ~CSystem::kFlag00000001;
        gei_Splash.Close();
    }
    if ((gei_System.mFlags & CSystem::kAltTab) != 0) {
        if (ALTTAB_RestoreState() == 0) {
            gei_ErrorHandler.Push(3, kErrAltTabRender);
            return;   // 0 in the original
        }
        gei_System.mFlags &= ~CSystem::kAltTab;
    }
    const uint32_t systemFlags = gei_System.mFlags;
    gei_Display.ResetWindow();
    const int16_t precision = FPU_SetSinglePrecision();
    if ((gei_Display.mFlags & 4) == 0) {
        // One view. With one window, the view moving since the last frame counts as a change.
        if (gRenderFrameMode != 1) {
            gRenderFrameMode = 1;
            gei_System.UpdateLights();
        } else if (mF70 == 1) {
            if (gRenderLastViewMade == 0) {   // the local static's construction
                gRenderLastView.SetIdentity();
                gRenderLastViewMade = 1;
            }
            if ((gei_System.mFlags & (CSystem::kViewChanged | CSystem::kLightsChanged)) == 0 &&
                std::memcmp(&gRenderLastView, gei_ViewMatrix, sizeof(CMatrix)) != 0)
                gei_System.mFlags |= CSystem::kViewChanged;
            std::memcpy(&gRenderLastView, gei_ViewMatrix, sizeof(CMatrix));
        } else {
            gei_System.mFlags |= CSystem::kViewChanged;
        }
        for (int32_t i = 0; i < mF70; i++)
            if (RenderWindow(mWindows[i]) == 0) goto failed;
    } else {
        // Stereo: display flag 8 red and blue in one picture, otherwise side by side.
        if ((gei_Display.mFlags & 8) != 0) {
            if (gRenderFrameMode != 3) gRenderFrameMode = 3;
        } else if (gRenderFrameMode != 2) {
            RenderSystem_Flush();
            gRenderFrameMode = 2;
        }
        gei_System.UpdateLights();
        for (int32_t i = 0; i < mF70; i++) {
            // The two eyes: the view moved by half the separation either way along its first
            // row (fused multiply-adds), the first eye by minus half.
            CMatrix firstEye;
            CMatrix secondEye;
            secondEye = *gei_ViewMatrix;
            firstEye = secondEye;
            const float separation = gei_Display.mF8B8;
            const float plusHalf = separation * 0.5f;
            const float minusHalf = -separation * 0.5f;
            for (int32_t c = 0; c < 4; c++) firstEye.m[3][c] = ppc_fmadds(minusHalf, firstEye.m[0][c], firstEye.m[3][c]);
            for (int32_t c = 0; c < 4; c++) secondEye.m[3][c] = ppc_fmadds(plusHalf, secondEye.m[0][c], secondEye.m[3][c]);
            SWindow eye = *mWindows[i];
            const uint32_t flags = gei_Display.mFlags;
            gei_Display.mFlags = flags & ~CDisplay::kFlag10000;
            if ((gei_Display.mFlags & 8) != 0) {
                // The first eye in red, the second in blue (the light manager's colour mask, the
                // second with display flag 0x80000000), then a pass with the sky and the global
                // lists only.
                eye.mFlags = mWindows[i]->mFlags & 0x1b11;
                eye.mMatrix = &firstEye;
                gei_LightManager->mF04 = static_cast<int32_t>(0xff0000ff);
                if (RenderWindow(&eye) == 0) goto failed;
                gei_Display.mFlags |= 0x80000000;
                eye.mMatrix = &secondEye;
                gei_LightManager->mF04 = 0x00ffffff;
                if (RenderWindow(&eye) == 0) goto failed;
                gei_Display.mFlags &= 0x7fffffff;
                gei_LightManager->mF04 = -1;
                eye.mFlags = mWindows[i]->mFlags & 0x420;
                if (RenderWindow(&eye) == 0) goto failed;
            } else {
                // The first eye in the right half of the window, the second in the left.
                eye.mMatrix = &firstEye;
                eye.mX = ppc_fmadds(0.48f, 1.0f + eye.mX, 0.02f);
                eye.mY = ppc_fmadds(0.48f, 1.0f + eye.mY, -0.52f);
                eye.mWidth = eye.mWidth * 0.48f;
                eye.mHeight = eye.mHeight * 0.48f;
                if (RenderWindow(&eye) == 0) goto failed;
                eye = *mWindows[i];
                eye.mMatrix = &secondEye;
                eye.mX = ppc_fmadds(0.48f, 1.0f + eye.mX, -0.98f);
                eye.mY = ppc_fmadds(0.48f, 1.0f + eye.mY, -0.52f);
                eye.mWidth = eye.mWidth * 0.48f;
                eye.mHeight = eye.mHeight * 0.48f;
                if (RenderWindow(&eye) == 0) goto failed;
            }
            gei_Display.mFlags = flags;
        }
    }
    gei_Capture.Capture();
    gei_System.mFlags &= ~(CSystem::kViewChanged | CSystem::kLightsChanged);
    if ((gei_System.mFlags & 2) != 0)
        FPU_SetFullPrecision(precision);
    else
        FPU_RestorePrecision(precision);
    gei_Display.mFlags = displayFlags;
    gei_System.mFlags = systemFlags;   // so the clearing above, and every flag the windows set, are undone
    return;   // 1 in the original
failed:
    gei_Display.mFlags = displayFlags;
    gei_System.mFlags = systemFlags;
    gei_ErrorHandler.GetAltTabStatus();   // the original returns its answer
}

int32_t CMapWho::ALTTAB_RestoreState() {
    const int32_t renderer = gei_System.mRenderer;
    if (renderer == 1 || renderer == 3) {
        if (gei_TCache->Close() == 0 || gei_System.mRenderSystem->TempShutdown() == 0 || gei_DDraw->TempShutdown() == 0 ||
            gei_DDraw->ReInitialize() == 0 || gei_System.mRenderSystem->ReInitialize() == 0 ||
            gei_TCache->ReInitialize(-1) == 0 || gei_DrawPrimitive->Init() == 0) {
            gei_TCache->Close();
            RenderSystem_Close();
            gei_DDraw->Close();
            gei_ErrorHandler.Push(3, kErrAltTabRestore);
            return 0;
        }
    }
    gUpdateMouse();
    return 1;
}

void CMapWho::QSetup_Game() {
    mGameWindow.mFlags = 0x1f31;
    mGameWindow.mX = mGameWindow.mY = -1.0f;
    mGameWindow.mWidth = mGameWindow.mHeight = 2.0f;
    mGameWindow.mMatrix = gei_ViewMatrix;
    if (mF70 < 4) {
        int32_t i = 0;
        while (i < mF70 && mWindows[i] != &mGameWindow) ++i;
        if (i == mF70) mWindows[mF70++] = &mGameWindow;
    }
    // The original keeps just one window even if it was already registered later.
    mF70 = 1;
}

namespace {
int32_t Remove(CMapWho& map, SMapLinkType*& head, CMapLinkBase* link) {
    SMapLinkType* previous = nullptr;
    SMapLinkType* node = head;
    while (node != nullptr && node->mLink != link) { previous = node; node = node->mNext; }
    if (node == nullptr) { gei_System.mFlags |= 0x80000000u; return 0; }
    if (previous == nullptr) head = node->mNext;
    else previous->mNext = node->mNext;
    node->mNext = map.mFree;
    map.mFree = node;
    --map.mNodesUsed;
    return 1;
}
void Clear(CMapWho& map, SMapLinkType*& head) {
    while (head != nullptr) {
        auto* next = head->mNext;
        head->mNext = map.mFree;
        map.mFree = head;
        --map.mNodesUsed;
        head = next;
    }
}
bool Clip(CMapWho& map, int32_t& x0, int32_t& z0, int32_t& x1, int32_t& z1) {
    if (x0 >= map.mWidth || x1 < 0 || z0 >= map.mHeight || z1 < 0) return false;
    if (x0 < 0) x0 = 0;
    if (map.mWidth <= x1) x1 = map.mWidth - 1;
    if (z0 < 0) z0 = 0;
    if (map.mHeight <= z1) z1 = map.mHeight - 1;
    return true;
}
}

int32_t CMapWho::Link(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* link, int32_t) {
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    int32_t result = 1;
    for (int32_t x = x0; x <= x1; ++x)
        for (int32_t z = z0; z <= z1; ++z) result &= Link_Add(&mMap[x + z * mWidth], link);
    return result;
}
int32_t CMapWho::Unlink(CMapLinkBase* link, int32_t list) {
    if ((list & 0x70) == 0) return 0;
    return Remove(*this, mF14[(list >> 4) - 1], link);
}
int32_t CMapWho::Unlink(int32_t x0, int32_t z0, int32_t x1, int32_t z1, CMapLinkBase* link) {
    if (mMap == nullptr) return 1;
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    int32_t result = 1;
    for (int32_t z = z0; z <= z1; ++z)
        for (int32_t x = x0; x <= x1; ++x) result &= Remove(*this, mMap[x + z * mWidth], link);
    return result;
}
int32_t CMapWho::Reset(int32_t list) {
    if ((list & 0x70) == 0) return 0;
    Clear(*this, mF14[(list >> 4) - 1]);
    gei_System.mFlags |= 0x80000000u;
    return 1;
}
int32_t CMapWho::Reset(int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    if (!Clip(*this, x0, z0, x1, z1)) return 0;
    for (int32_t z = z0; z <= z1; ++z)
        for (int32_t x = x0; x <= x1; ++x) {
            Clear(*this, mMap[x + z * mWidth]);
            gei_System.mFlags |= 0x80000000u;
        }
    return 1;
}
int32_t CMapWho::ResetAll() {
    Reset(0, 0, mWidth - 1, mHeight - 1);
    for (auto& head : mF14) { Clear(*this, head); gei_System.mFlags |= 0x80000000u; }
    return 1;
}

// tb_mapzoom.cpp range of the engine library (SimTheme Park 2.04b3; CMapWho is from
// tb_map.cpp by its own assertion): the light manager's and the map lists' start-up and
// shutdown. PARTIAL: the rest of the range (lighting, linking, rendering) is still to do.

// ---- Link bases -------------------------------------------------------------------------
CMapLinkBase* gLinkBases[0x200];
int32_t gLinkBaseCount = 0;

int32_t RegisterLinkBase(CMapLinkBase* link) {
    if (gLinkBaseCount < 0x200) {
        gLinkBases[gLinkBaseCount] = link;
        ++gLinkBaseCount;
    }
    return 1;
}

int32_t FreeLinkBase(CMapLinkBase* link) {
    int32_t i = gLinkBaseCount - 1;
    while (i >= 0 && gLinkBases[i] != link) --i;
    if (i < 0) return 0;
    const int32_t count = gLinkBaseCount;
    gLinkBaseCount = count - 1;
    gLinkBases[i] = gLinkBases[count - 1];
    return 1;
}

// ---- CMapWho ----------------------------------------------------------------------------
CMapWho* gei_MapWho = nullptr;

int32_t MapWho_Init() {
    if (gei_MapWho != nullptr) return 0;
    gei_MapWho = new (std::nothrow) CMapWho;
    if (gei_MapWho == nullptr) {
        gei_ErrorHandler.Push(3, kErrOutOfMemory);
        return 0;
    }
    return 1;
}

int32_t MapWho_Close() {
    CMapWho* map = gei_MapWho;
    if (map == nullptr) return 0;
    map->Close();
    delete gei_MapWho;
    gei_MapWho = nullptr;
    return 1;
}

CMapWho::CMapWho() {
    mNodes = nullptr;
    mFree = nullptr;
    mNodeCount = 0;
    mNodesUsed = 0;
    mF40 = 0x30;
    mHeight = 0;
    mWidth = 0;
    mF70 = 0;
    mF30 = 0;
    mMap = nullptr;
    mF34 = 0;
    mFinalCallback = nullptr;
    mF74 = -1;
    for (auto& value : mF14) value = nullptr;
    // Every entry goes on the free list, the last one first in line.
    bool ready = false;
    mNodes = static_cast<SMapLinkType*>(gMalloc(kNodeCount * sizeof(SMapLinkType)));
    if (mNodes != nullptr) {
        for (int32_t i = 0; i < kNodeCount; ++i) {
            mNodes[i].mNext = mFree;
            mFree = &mNodes[i];
        }
        mNodeCount = kNodeCount;
        ready = true;
        mNodesUsed = 0;
    }
    if (!ready && mNodes != nullptr) {
        gFree(reinterpret_cast<void*&>(mNodes));
        mNodeCount = 0;
        mNodesUsed = 0;
    }
}

CMapWho::~CMapWho() {
    if (mNodes != nullptr) {
        gFree(reinterpret_cast<void*&>(mNodes));
        mNodeCount = 0;
        mNodesUsed = 0;
    }
    Close();
}

void CMapWho::SetFinalCallback(void (*callback)()) { mFinalCallback = callback; }

int32_t CMapWho::Close() {
    if (mMap != nullptr) gFree(reinterpret_cast<void*&>(mMap));
    return 1;
}
