#include <cstddef>
#include <cstdint>
#include <cstring>
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "Light.h"
#include "TPCoaster.h"
#include "TPMain.h"
#include "TbGraphicFile.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "ui_main.h"
#include "Main.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_texture.h"
#include "Advisor.h"
#include "AdvisorScoring.h"
#include "Game.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "tb_resource_display.h"
#include "tb_texturecache.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_tooltip.h"
#include "WindowScripts.inc"

namespace {
template <class T>
inline T& MacAt(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}
}  // namespace

int16_t gMapPanelScript[] = {TPW_WINDOW_SCRIPT_102B2366};   // 0x102b2366: the image's words (generated, devtools/gen_game_tables.py)

// 0x1022bf74 (stand-in): the map panel's message function, untranslated.
__attribute__((weak)) int32_t UI_2DMap_CallbackFn(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }

namespace mappanel_callee {
// operator new(0x19c), then the constructor 0x1018d11c with NULL.
__attribute__((weak)) InterfaceDrawObject* NewTextSprite() { return new InterfaceTextObjectSprite(nullptr); }
}  // namespace mappanel_callee

// 0x1022c080 (1848 bytes): opens the map panel (does nothing when the main menu is up or the panel is up already): pauses the game, saves the
// fog colour and the map render options into the map (+8, +0xc), builds the panel from its script and sets its windows up from the layer
// flags (+0x18) and the two filter words (+0x1c, +0x20), centres the map on the camera target (a tenth of it, in cells, kept inside the
// heightfield and the map's size), then stores the grid, the sprites and the map, starts the zoom on the map picture and tells the advisor.
void CUI2DMap::CreateMapPanel() {
    void* self = this;
    if (MainMenuUp() != 0 || MacAt<uint32_t>(self, 4) != 0) return;

    gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
    gGame.mF3C = 0;
    UI_PauseTimerSystem();
    ADV_Kill(1);
    MacAt<uint32_t>(self, 8) = gei_Display.mFogColor;
    gei_Display.SetFogColor(0);
    MacAt<int32_t>(self, 0xc) = gei_MapWho->GetRenderOptions();
    gei_MapWho->SetRenderOptions(0x2821);
    gei_TCache->Flush(0);
    UIMain_CheckPanel();
    UI_SendMessage(gGameWindow, 6, 0, 0);

    InterfaceWindow* panel = gUIRootWindow->Build(gMapPanelScript, UI_2DMap_CallbackFn);
    UIMain_SetPanel(panel);
    MacAt<uint32_t>(self, 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(panel));
    panel->CaptureKb(0);

    InterfaceWindow* title = panel->GetWindow(0x991);
    InterfaceDrawObject* sprite = mappanel_callee::NewTextSprite();
    title->SetClientDrawObject(sprite);
    title->SetFontJust(1, 1);
    title->SetFontWrap(1, 1);
    title->SetFont(GetFontTableEntry(4));
    title->SetFontColour(0xff, 0xff, 0, 0xff);
    title->SetClientDrawText(nullptr);

    entry_callee::WindowEnable(panel->GetWindow(0x990), 0);
    entry_callee::WindowEnable(panel->GetWindow(0x995), 0);
    const int16_t z = static_cast<int16_t>(panel->GetWindow(0x990)->mZ + 2);

    static const struct {
        int32_t id;
        uint32_t mask;
    } kLayerButtons[6] = {{0x982, 8}, {0x987, 1}, {0x989, 0x10}, {0x993, 2}, {0x994, 4}, {0x983, 0x20}};
    for (const auto& layer : kLayerButtons) {
        InterfaceWindow* button = panel->GetWindow(layer.id);
        button->setZ(z);
        if ((MacAt<uint32_t>(self, 0x18) & layer.mask) != 0) reinterpret_cast<InterfaceButton*>(button)->SetState(1, 1);
    }

    InterfaceCheckButtons* filterA = static_cast<InterfaceCheckButtons*>(panel->GetWindow(0x98b));
    const int32_t a = MacAt<int32_t>(self, 0x1c);
    if (a == 3) {
        filterA->SetCurrent(0x98c);
    } else if (a < 3) {
        if (a == 1) {
            filterA->SetCurrent(0x98e);
        } else if (a > 0) {
            filterA->SetCurrent(0x98f);
        }
    } else if (a < 5) {
        filterA->SetCurrent(0x98d);
    }

    InterfaceCheckButtons* filterB = static_cast<InterfaceCheckButtons*>(panel->GetWindow(0x996));
    const int32_t b = MacAt<int32_t>(self, 0x20);
    if (b == 2) {
        filterB->SetCurrent(0x997);
    } else if (b < 2) {
        if (b > 0) filterB->SetCurrent(0x998);
    } else if (b < 4) {
        filterB->SetCurrent(0x999);
    }

    reinterpret_cast<InterfaceButton*>(panel->GetWindow(0x98a))->SetAutoRepeat(0, 10);
    reinterpret_cast<InterfaceButton*>(panel->GetWindow(0x984))->SetAutoRepeat(0, 10);
    reinterpret_cast<InterfaceButton*>(panel->GetWindow(0x981))->SetAutoRepeat(0, 10);
    reinterpret_cast<InterfaceButton*>(panel->GetWindow(0x988))->SetAutoRepeat(0, 10);
    panel->GetWindow(0x992)->SetCallbackFunction(reinterpret_cast<InterfaceWindowCallback>(&UI_2DMap_MapAreaCallback));

    MacAt<uint32_t>(self, 0x20060) = 0;
    MacAt<uint16_t>(self, 0x20064) = 0;
    MacAt<uint32_t>(self, 0x20084) = 0;
    GetHFieldMaxVertexIndices(&MacAt<uint32_t>(self, 0x24), &MacAt<uint32_t>(self, 0x28));

    const float scale = 0.1f;
    const float fx = scale * gCameraTarget[0];
    const float fz = scale * gCameraTarget[2];
    int32_t cellX = ppc_fctiwz(fx);
    int32_t cellZ = ppc_fctiwz(fz);
    if (cellX < 0) cellX = 0;
    if (cellZ < 0) cellZ = 0;
    if (cellX >= gHeightfieldWidth) cellX = gHeightfieldWidth - 1;
    if (cellZ >= gHeightfieldHeight) cellZ = gHeightfieldHeight - 1;
    float centreX = static_cast<float>(static_cast<double>(cellX));
    float centreZ = static_cast<float>(static_cast<double>(cellZ));
    MacAt<float>(self, 0x54) = centreX;
    MacAt<float>(self, 0x58) = centreZ;

    float cx = MacAt<float>(self, 0x54);
    float resultX = 0.0f;
    if (!(cx < 0.0f)) {
        const float limit = static_cast<float>(static_cast<double>(MacAt<int32_t>(self, 0x24)));
        resultX = (cx <= limit) ? cx : limit;
    }
    MacAt<float>(self, 0x54) = resultX;

    float cz = MacAt<float>(self, 0x58);
    float resultZ = 0.0f;
    if (!(cz < 0.0f)) {
        const float limit = static_cast<float>(static_cast<double>(MacAt<int32_t>(self, 0x28)));
        resultZ = (cz <= limit) ? cz : limit;
    }
    MacAt<float>(self, 0x58) = resultZ;

    StoreMapGridDetails();
    InitSprites();
    LoadMap(0);
    // The Mac passes its area's data word (0x44) and row bytes (0x50); the host's pixels are in HostArea() (ui_main.h).
    gei_MapZoom.Init(HostArea()->mData, MacAt<int32_t>(self, 0x48), MacAt<int32_t>(self, 0x4c), HostArea()->mStride);
    gei_MapZoom.DisableFiltering();
    UpdateMap();
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x130));
    MoveHelpText(-1, 10);
}
