// ui_2dmap.cpp: name proven by __sinit_ui_2dmap_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "analyser.h"
#include "CellBase.h"
#include "Debug.h"
#include "DrawRenderClasses.h"
#include "DrawState.h"
#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_LoadSave.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "Game.h"
#include "helpers/MslSupportHelpers.h"
#include "helpers/ui_2dmapHelpers.h"
#include "InterfaceWindow.h"
#include "Light.h"
#include "lobby.h"
#include "Main.h"
#include "MainBalance.h"
#include "MapWho.h"
#include "MslWStringTree.h"
#include "online.h"
#include "platform/d3d_types.h"
#include "ppc_math.h"
#include "Ptr32.h"
#include "RideDB.h"
#include "std_basic_filebuf.h"
#include "std_basic_streambuf_wchar.h"
#include "std_cdeque.h"
#include "std_ctype_wchar.h"
#include "std_deque_errorthingy.h"
#include "tb_ezsprite.h"
#include "tb_mapzoom.h"
#include "tb_resource_display.h"
#include "tb_resource_texture.h"
#include "tb_starfield.h"
#include "tb_texturecache.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbRaConvert.h"
#include "TbTList.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPCoaster.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_ime.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "ui_tooltip.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

// Unit v5193: UpdateSatellite 0x1022b8d0 and UI2DMReturnScaleColour 0x1022bb44 (ui_worldLobbyController.cpp, ui_2dmap.cpp).

bfl::draw::CColour UI2DMReturnScaleColour(int32_t percent, const bfl::draw::CColour& a, const bfl::draw::CColour& b,
                                          const bfl::draw::CColour& c) {
    using bfl::draw::CColour;
    if (percent < 0) percent = 0;
    else if (percent > 100) percent = 100;
    const CColour* from;
    const CColour* to;
    int32_t steps;
    if (percent < 50) {
        from = &a;
        to = &b;
        steps = percent;
    } else {
        from = &b;
        to = &c;
        steps = percent - 50;
    }
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(steps) << 16);
    int32_t t = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * scaled) >> 32) >> 4;   // mulhw, srawi
    t += static_cast<int32_t>(static_cast<uint32_t>(t) >> 31);
    CColour result = *from;
    result.mRed = static_cast<uint8_t>(from->mRed + ((t * (static_cast<int32_t>(to->mRed) - from->mRed)) >> 16));
    result.mGreen = static_cast<uint8_t>(from->mGreen + ((t * (static_cast<int32_t>(to->mGreen) - from->mGreen)) >> 16));
    result.mBlue = static_cast<uint8_t>(from->mBlue + ((t * (static_cast<int32_t>(to->mBlue) - from->mBlue)) >> 16));
    result.mAlpha = 0xff;
    return result;
}

namespace {
template <class T>
inline T& MacAt(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}
}  // namespace

int16_t gMapPanelScript[] = {TPW_WINDOW_SCRIPT_102B2366};   // 0x102b2366: the image's words (generated, devtools/gen_game_tables.py)

// Unit v51058: twenty small functions of the main app: the world lobby controller's messages, the static initialisers of
// ui_worldLobbyController.cpp, ui_publishParkPanel.cpp, TPGlobals.cpp and OnlinePerson.cpp, the 2D map callback, two lobby screen
// callbacks that call a virtual slot, a few std::__cdeque / std::less helpers.

// 0x1022bf74 (the weak stand-in in the CUI2DMap file yields to this one)
int32_t UI_2DMap_CallbackFn(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    return gUI2DMap.MapCallback(window, message, a, b);
}

// 0x1022bff8: forwards the four arguments to the global map object's MapAreaCallback.
void UI_2DMap_MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    gUI2DMap.MapAreaCallback(window, message, a, b);
}

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

// CUI2DMap::MapCallback 0x1022c7e8 (2180 bytes, unit v5526), main app: the map panel's window message function.

namespace {

int32_t& W(CUI2DMap* map, size_t offset) { return *reinterpret_cast<int32_t*>(map->mBytes + offset); }
uint32_t& U(CUI2DMap* map, size_t offset) { return *reinterpret_cast<uint32_t*>(map->mBytes + offset); }
float& F(CUI2DMap* map, size_t offset) { return *reinterpret_cast<float*>(map->mBytes + offset); }

// The first jump table (0x102b28e4, keys 0x981 .. 0x98a of messages 0x10008 and 0x10009): the direction of the MoveCentre, -1 for none.
const int8_t kKeyTable[10] = {3, -1, -1, 0, -1, -1, -1, 1, -1, 2};
// The second table (0x102b28ac, keys 0x98c .. 0x999 of message 0x101): the handler, in the order of the listing, -1 for none.
const int8_t kMenuTable[14] = {5, 6, 3, 4, -1, -1, -1, -1, -1, -1, -1, 1, 0, 2};

}  // namespace

int32_t CUI2DMap::MapCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CUI2DMap* self = this;
    auto goDefault = [&]() { return window->CallDefaultWindowHandler(message, a, b); };
    auto move = [&](int32_t direction, float amount) { self->MoveCentre(static_cast<EUI2DMapMoveDirection>(direction), amount); };

    switch (message) {
    case 0x10001:
    case 0x10003:
        W(self, 0x10) = static_cast<int32_t>(static_cast<uint32_t>(a) >> 16);
        W(self, 0x14) = static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff);
        return goDefault();
    case 0x10008:
    case 0x10009: {
        uint32_t index = static_cast<uint32_t>(a) - 0x981u;
        if (index <= 9 && kKeyTable[index] >= 0) move(kKeyTable[index], 1.0f);
        return goDefault();
    }
    case 0x1000a:
        switch (a) {
        case 0x2800: move(3, 1.0f); break;
        case 0x2600: move(2, 1.0f); break;
        case 0x2500: move(0, 1.0f); break;
        case 0x2700: move(1, 1.0f); break;
        case 0x6d00: self->Zoom(static_cast<EUI2DMapZoom>(1), 0.5f); break;
        case 0x6b00: self->Zoom(static_cast<EUI2DMapZoom>(0), 0.5f); break;
        default: break;
        }
        return goDefault();
    case 0x1000b:
        if (a == 0x1b && ((static_cast<uint32_t>(b) >> 16) & 0xff) == 0) UI_SendMessage(window, 4, 0, 0);
        return 0;
    case 0x15:
        self->UpdateMapZoom();
        return goDefault();
    case 0x14:
        self->CloseSprites();
        self->HostArea()->FreeMemory();   // the Mac's area at 0x2c (ui_main.h)
        gei_Display.SetFogColor(U(self, 8));
        gei_MapWho->SetRenderOptions(W(self, 0xc));
        gGame.mF3C = 1;
        gGame.Unpause();
        UI_UnPauseTimerSystem();
        W(self, 4) = 0;
        UI_SendMessage(gGameWindow, 6, 1, 0);
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x130));
        UIMain_SetPanel(nullptr);
        UIMain_DecideKbCapture();
        MoveHelpText(-1, -1);
        return goDefault();
    case 0x1e: {
        // lbz: the byte as it is, whatever it holds
        if (*reinterpret_cast<const uint8_t*>(&gPlayerSettings.mCurrent.mRMBScrollOn) != 0) return goDefault();
        auto unit = [](int32_t v, float scale) {
            float f = static_cast<float>(v) / scale;
            float r = 0.0f;
            if (f >= 0.0f) {
                r = 1.0f;
                if (!(f > 1.0f)) r = f;
            }
            return r;
        };
        float fx = unit(W(self, 0x10), 2048.0f);
        float fy = unit(W(self, 0x14), 1536.0f);
        if (fx < 0.01f) {
            move(0, 250.0f * (0.01f - fx));
        } else if (fx > 0.99f) {
            move(1, 250.0f * (fx - 0.99f));
        }
        if (fy < 0.01f) {
            move(2, 250.0f * (0.01f - fy));
        } else if (fy > 0.99f) {
            move(3, 250.0f * (fy - 0.99f));
        }
        return goDefault();
    }
    case 0x100:
        switch (a) {
        case 0x986: self->Zoom(static_cast<EUI2DMapZoom>(0), 0.5f); break;
        case 0x985: self->Zoom(static_cast<EUI2DMapZoom>(1), 0.5f); break;
        case 0x987: U(self, 0x18) ^= 1; self->UpdateMap(); break;
        case 0x993: U(self, 0x18) ^= 2; self->UpdateMap(); break;
        case 0x994: U(self, 0x18) ^= 4; self->UpdateMap(); break;
        case 0x982: U(self, 0x18) ^= 8; self->UpdateMap(); break;
        case 0x989: U(self, 0x18) ^= 0x10; self->UpdateMap(); break;
        case 0x983: U(self, 0x18) ^= 0x20; self->UpdateMap(); break;
        case -1: {
            float x = F(self, 0x54);
            float r = 0.0f;
            if (!(x < 0.0f)) {   // bge: a NaN is "not less" and goes on
                float limit = static_cast<float>(W(self, 0x24) - 1);
                r = x;
                if (x > limit) r = limit;
            }
            F(self, 0x54) = r;
            float y = F(self, 0x58);
            r = 0.0f;
            if (!(y < 0.0f)) {
                float limit = static_cast<float>(W(self, 0x28) - 1);
                r = y;
                if (y > limit) r = limit;
            }
            F(self, 0x58) = r;
            int32_t ix = ppc_fctiwz(static_cast<double>(F(self, 0x54)));
            int32_t iy = ppc_fctiwz(static_cast<double>(F(self, 0x58)));
            float px = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(ix) * 10u));
            float py = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(iy) * 10u));
            SetCameraPOI(&px, &py, nullptr, nullptr, nullptr);
            // the panel window is a Ptr32 word (an arena offset) at 4
            UI_SendMessage(reinterpret_cast<Ptr32<InterfaceWindow>*>(self->mBytes + 4)->get(), 4, 0, 0);
            return 0;
        }
        default: break;
        }
        return goDefault();
    case 0x101: {
        uint32_t index = static_cast<uint32_t>(a) - 0x98cu;
        bool flag = (b == 1);
        if (index <= 13) {
            int32_t handler = kMenuTable[index];
            if (handler >= 0) {
                static const uint8_t kField[7] = {0x20, 0x20, 0x20, 0x1c, 0x1c, 0x1c, 0x1c};
                static const int32_t kValue[7] = {1, 2, 3, 1, 2, 3, 4};
                W(self, kField[handler]) = flag ? kValue[handler] : 0;
                self->UpdateMap();
            }
        }
        return goDefault();
    }
    default:
        return goDefault();
    }
}

// CUI2DMap::MapAreaCallback 0x1022d0ac (2652 bytes), main app: the message function of the map area window.
//   0x10001 and 0x10003 (the mouse moved): the mouse position is stored (x in the high halfword of `a`, y in the low one), the visible
//   sprite nearest to it (within 5 units on both axes) becomes the hovered one (map + 0x20060), the map is redrawn when the hovered
//   thing changes to or from a staff/mechanic kind, the grid cell under the mouse gives the hovered thing id (0x20064) and the
//   name of the hovered thing is set as the text of the child window 0x991 of the parent. Then the default handler answers.
//   0x1e: with the setting byte 0x38 set and the drag flag (0x20084) set the centre moves by the mouse offset from the screen middle.
//   0x10004 and 0x10005 with a == 1 clear and set the drag flag. 0x10006 with a == 0 (a click): the centre moves to the click, is
//   clamped, the camera goes there and the status panel of the hovered thing opens; the answer is 0 without the default handler.
// The Mac pointers kept in the map object (the window at 4, the hovered sprite at 0x20060, the sprite list at 0x20080) are 32-bit words
// that hold native (arena) addresses on the host. The sprite's thing and the thing array hold native pointers.

uint16_t gMapNoThingA;   // 0x102b290c (name invented): compared with the hovered thing id after a mouse move
uint16_t gMapNoThingB;   // 0x102b290e (name invented): compared with the hovered thing id on a click

namespace mapcb_callee {
// The virtual at slot 0x2c of the table at window + 0x118 (called through __ptr_glue): faked by the test library.
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, const char16_t* text) { window->SetText(const_cast<char16_t*>(text)); }
}  // namespace mapcb_callee

namespace {

const char kThingZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x10251a08
const char16_t kEmptyText[] = u"";                                                                        // 0x10251aec

template <class T> T& Field(CUI2DMap* map, size_t offset) { return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(map) + offset); }

// Host-only places (the Mac keeps 32-bit words at 4, 0x20060 and the flag at 0x20084, which a native pointer cannot share with the
// list pointer at 0x20080): the hovered sprite, the drag flag and the map panel window.
constexpr size_t kHoverOffset = 0x200a0;
constexpr size_t kDragOffset = 0x200a8;
constexpr size_t kPanelOffset = 0x200b0;
CUI2DMapSprite*& HoverOf(CUI2DMap* map) { return Field<CUI2DMapSprite*>(map, kHoverOffset); }
CUI2DMapSprite* SpriteOf(CUI2DMapSprite* p) { return p; }
CUI2DMapSprite* WordOf(CUI2DMapSprite* p) { return p; }

uint8_t KindOf(const void* thing) { return static_cast<const uint8_t*>(thing)[2]; }
// The kind tests of the listing (4, 5, 6, 8, 7 one after the other): a staff or mechanic.
bool IsStaffKind(uint8_t kind) { return kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7; }

// The thing under a sprite or, without one, the thing of the hovered id (index 0 must stay empty).
void* ThingOfId(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, kThingZeroMessage);
    return gThingArray[id].mThing;
}

}  // namespace

void CUI2DMap::MapAreaCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    MapAreaCallbackResult(window, message, a, b);
}

int32_t CUI2DMap::MapAreaCallbackResult(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    CUI2DMap* map = this;
    if (message == 0x10003 || message == 0x10001) {
        Field<int32_t>(map, 0x10) = static_cast<int32_t>(static_cast<uint32_t>(a) >> 16);
        Field<int32_t>(map, 0x14) = static_cast<int32_t>(static_cast<uint32_t>(a) & 0xffff);
        const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
        const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
        CUI2DMapSprite* const oldHover = HoverOf(map);
        const float mouseX = (static_cast<float>(Field<int32_t>(map, 0x10)) * screenW) / 2048.0f;
        const float mouseY = (static_cast<float>(0x600 - Field<int32_t>(map, 0x14)) * screenH) / 1536.0f;
        HoverOf(map) = nullptr;
        auto* list = Field<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>*>(map, 0x20080);
        auto* node = list->mHead;
        float best = 100000000.0f;
        while (node != nullptr) {
            CUI2DMapSprite* sprite = node->mData;
            node = node->mNext;
            if (sprite->mVisible == 0) continue;
            const float spriteX = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(sprite) + 0xc);
            const float spriteY = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(sprite) + 0x10);
            const float dx = std::fabs(mouseX - spriteX);
            const float dy = std::fabs(mouseY - spriteY);
            if (!(dx < 5.0f)) continue;
            if (!(dy < 5.0f)) continue;
            const float distance = static_cast<float>(std::sqrt(static_cast<double>(ppc_fmadds(dx, dx, dy * dy))));
            if (distance < best) {
                HoverOf(map) = sprite;
                best = distance;
            }
        }
        CUI2DMapSprite* const newHover = HoverOf(map);
        if (oldHover != newHover) {
            bool redraw = false;
            const void* oldThing = oldHover != nullptr ? oldHover->mThing : nullptr;
            if (oldThing != nullptr && IsStaffKind(KindOf(oldThing))) {
                redraw = true;
            } else {
                const void* newThing = newHover != nullptr ? newHover->mThing : nullptr;
                if (newThing != nullptr && IsStaffKind(KindOf(newThing))) redraw = true;
            }
            if (redraw) UpdateMap();
        }
        const float mapW = static_cast<float>(Field<int32_t>(map, 0x24));
        const float mapH = static_cast<float>(Field<int32_t>(map, 0x28));
        const float offsetX = ppc_fnmsubs(0.5f, screenW, mouseX);
        const float offsetY = ppc_fnmsubs(0.5f, screenH, mouseY);
        const float zoom = Field<float>(map, 0x5c);
        const float gridX = Field<float>(map, 0x54) + offsetX / (zoom * (480.0f / mapW));
        const float gridY = Field<float>(map, 0x58) + offsetY / (zoom * (425.0f / mapH));
        const int32_t cellX = ppc_fctiwz(static_cast<double>(gridX));
        const int32_t cellY = ppc_fctiwz(static_cast<double>(gridY));
        const uint8_t* cell = static_cast<const uint8_t*>(GetMapGridCell(cellX, cellY));
        if (cell == nullptr) {
            Field<uint16_t>(map, 0x20064) = 0;
        } else {
            Field<uint16_t>(map, 0x20064) = *reinterpret_cast<const uint16_t*>(cell + 4);
        }
        {
            TbDynamicStringTemplate<char16_t> text(kEmptyText);
            const CUI2DMapSprite* hovered = HoverOf(map);
            void* thing = hovered != nullptr ? hovered->mThing : nullptr;
            if (thing == nullptr) {
                const uint16_t id = Field<uint16_t>(map, 0x20064);
                if (id != gMapNoThingA) {
                    void* object = ThingOfId(id);
                    static_cast<CObject*>(object)->GetUIName(gUIMainConvertBuffer, 0x3fff);
                    text = static_cast<const char16_t*>(gUIMainConvertBuffer);
                }
            } else {
                const uint8_t kind = KindOf(thing);
                if (IsStaffKind(kind)) {
                    text = static_cast<const CStaff*>(thing)->GetName();
                } else if (kind == 1) {
                    auto* table = static_cast<TbITable*>(gText.mResourcePointers[6]);   // Mac 0x564
                    const int32_t value = *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(thing) + 0x1d8);
                    if (value == 0) {
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
                    } else {
                        TbIntegerVTag tag(0x18, value);
                        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
                    }
                    text = static_cast<const char16_t*>(gUIMainConvertBuffer);
                }
            }
            const char16_t* shown = text.mData;
            InterfaceWindow* parent = window->GetParent();
            InterfaceWindow* label = parent->GetWindow(0x991);
            mapcb_callee::WindowSetText(label, shown);
        }
    } else if (message == 0x1e) {
        if (reinterpret_cast<const uint8_t*>(&gPlayerSettings)[0x38] != 0 && Field<int32_t>(map, kDragOffset) != 0) {
            const float stepX = (5.0f * static_cast<float>(Field<int32_t>(map, 0x10) - 0x400)) / 2048.0f;
            MoveCentre(static_cast<EUI2DMapMoveDirection>(1), stepX);
            const float stepY = (5.0f * static_cast<float>(Field<int32_t>(map, 0x14) - 0x300)) / 1536.0f;
            MoveCentre(static_cast<EUI2DMapMoveDirection>(3), stepY);
        }
    } else if (message == 0x10006) {
        if (a == 0) {
            const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
            const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
            const float mouseX = (static_cast<float>(Field<int32_t>(map, 0x10)) * screenW) / 2048.0f;
            const float mouseY = (static_cast<float>(0x600 - Field<int32_t>(map, 0x14)) * screenH) / 1536.0f;
            const float mapW = static_cast<float>(Field<int32_t>(map, 0x24));
            const float mapH = static_cast<float>(Field<int32_t>(map, 0x28));
            const float offsetX = ppc_fnmsubs(0.5f, screenW, mouseX);
            const float offsetY = ppc_fnmsubs(0.5f, screenH, mouseY);
            Field<float>(map, 0x54) = Field<float>(map, 0x54) + offsetX / (Field<float>(map, 0x5c) * (480.0f / mapW));
            Field<float>(map, 0x58) = Field<float>(map, 0x58) + offsetY / (Field<float>(map, 0x5c) * (425.0f / mapH));
            void* target = nullptr;
            const CUI2DMapSprite* hoverSprite = HoverOf(map);
            if (hoverSprite != nullptr && hoverSprite->mThing != nullptr) {
                target = hoverSprite->mThing;
            } else if (Field<uint16_t>(map, 0x20064) != gMapNoThingB) {
                target = ThingOfId(Field<uint16_t>(map, 0x20064));
            }
            // Each coordinate: below 0.0 it becomes 0.0, above (size - 1) it becomes that (NaN passes).
            float cx = Field<float>(map, 0x54);
            if (cx < 0.0f) {
                cx = 0.0f;
            } else {
                const float limit = static_cast<float>(Field<int32_t>(map, 0x24) - 1);
                if (cx > limit) cx = limit;
            }
            Field<float>(map, 0x54) = cx;
            float cy = Field<float>(map, 0x58);
            if (cy < 0.0f) {
                cy = 0.0f;
            } else {
                const float limit = static_cast<float>(Field<int32_t>(map, 0x28) - 1);
                if (cy > limit) cy = limit;
            }
            Field<float>(map, 0x58) = cy;
            const int32_t ix = ppc_fctiwz(static_cast<double>(Field<float>(map, 0x54)));
            const int32_t iy = ppc_fctiwz(static_cast<double>(Field<float>(map, 0x58)));
            float poiX = static_cast<float>(ix * 10);
            float poiY = static_cast<float>(iy * 10);
            SetCameraPOI(&poiX, &poiY, nullptr, nullptr, nullptr);
            UI_SendMessage(Field<InterfaceWindow*>(map, kPanelOffset), 4, 0, 0);
            if (target != nullptr) UIMain_GotoStatusPanelForThing(target);
            return 0;
        }
    } else if (message == 0x10005) {
        if (a == 1) Field<int32_t>(map, kDragOffset) = 1;
    } else if (message == 0x10004) {
        if (a == 1) Field<int32_t>(map, kDragOffset) = 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// Unit v5959: the sprite list handling and the map picture of the 2D map (ui_2dmap.cpp, main app):
//   UnRegister 0x10230cd4, DeleteSpriteListEntries 0x10230bd4, UpdateAllSpritePositions 0x10230974, CloseSprites 0x102307ec,
//   Zoom 0x1022fa1c, LoadMap 0x1022dbdc, UpdateMap 0x1022db4c.

namespace {

typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> SpriteList;

template <class T>
T& Field_2(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

}  // namespace

// 0x102b2910: the path separator appended to the theme path (initial data, a colon).
char gUI2DMapSeparator = ':';

// 0x1022db4c (100 bytes)
void CUI2DMap::UpdateMap() {
    LoadMap(1);
    UpdateAllDrawRelated();
    gei_MapZoom.Update(0, 0, Field_2<int32_t>(this, 0x48) - 1, Field_2<int32_t>(this, 0x4c) - 1);
}
// 0x1022dbdc (460 bytes): loads <theme path>:<2dmap.tga> into a local area; with reload 0 first sets the map's own area (at 0x2c) up
// from the texture format and allocates it; the picture is converted into the map's area. The host's area is HostArea() (ui_main.h):
// it gets the same format, width and height; the Mac words at 0x44 and 0x50 (data, row bytes) are left, as nothing on the host reads them.
void CUI2DMap::LoadMap(int32_t reload) {
    TbDynamicStringTemplate<char> path(get_theme_path());
    path += gUI2DMapSeparator;
    path += *reinterpret_cast<const TbStringBase<char>*>(gUI2DMapTgaName);
    TbTGALoader loader;
    TbRenderArea area;
    loader.ReadHeader(path.mData, nullptr, &area);
    area.AllocMemory();
    loader.Load(area, path.mData, nullptr, nullptr);
    TbRenderArea* mapArea = HostArea();
    if (reload == 0) {
        Field_2<int32_t>(this, 0x48) = area.mWidth;
        Field_2<int32_t>(this, 0x4c) = area.mPictureHeight;
        // the texture format's pixel format (DDPIXELFORMAT at 0x30 of the settings), eight words
        const uint32_t* format = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(&gei_TextureSettings) + 0x30);
        uint32_t w[8];
        for (int32_t i = 0; i < 8; i++) w[i] = format[i];
        Field_2<uint32_t>(this, 0x2c) = w[4];
        Field_2<uint32_t>(this, 0x30) = w[5];
        Field_2<uint32_t>(this, 0x34) = w[6];
        Field_2<uint32_t>(this, 0x38) = w[7];
        Field_2<uint32_t>(this, 0x3c) = w[3];
        Field_2<uint32_t>(this, 0x40) = w[1] & 0x20;
        std::memcpy(mapArea->mF00, mBytes + 0x2c, sizeof(mapArea->mF00));   // the four masks
        mapArea->mDepth = Field_2<int32_t>(this, 0x3c);
        mapArea->mPalettised = Field_2<int32_t>(this, 0x40);
        mapArea->mWidth = area.mWidth;
        mapArea->mPictureHeight = area.mPictureHeight;
        mapArea->AllocMemory();
    }
    TbRAConv converter;
    converter.Move(*mapArea, area, nullptr);
    area.FreeMemory();
}

// Unit v51105: CUI2DMap::Register, GetMapGridCell, the destructor, CUI2DMapSprite::SetPosition, the empty grid cell constructor, and
// small pieces of the Metrowerks library (ctype<wchar_t>, the Argument deque, the nocase wide string).

// ---- CUI2DMap

// 0x102318f0: the sprite list's AddTail (translated elsewhere; the test library fakes it).
template <>
void TbTList<CUI2DMapSprite*, CUI2DMapSprite*>::AddTail(CUI2DMapSprite* value);

namespace ui2dmap_callee {
// The list's AddTail (0x102318f0); weak so the test library can fake it.
__attribute__((weak)) void ListAddTail(TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list, CUI2DMapSprite* sprite) {
    list->AddTail(sprite);
}
}  // namespace ui2dmap_callee

// 0x1022ddd0 (64 bytes)
void* CUI2DMap::GetMapGridCell(int32_t x, int32_t y) {
    if (x < 0 || x >= 0x80 || y < 0 || y >= 0x80) return nullptr;
    return reinterpret_cast<uint8_t*>(this) + 0x60 + ((y << 7) + x) * 8;
}

// CUI2DMap::UpdateSpritePosition, DrawCoasterTracks and StoreMapGridDetails (ui_2dmap.cpp), unit v5200. The map object has no members yet:
// its fields are read by Mac offset (see the class in include/ui_main.h).

// Globals nobody else defines (initialised data / BSS of the Mac).
uint16_t gUI2DMapGridThingListEnd = 0;           // 0x102b2912
uint32_t gUI2DMapColourTable[0x264 / 4] = {};    // 0x103970f0

namespace {

template <typename T>
T& At_2(void* base, int32_t offset) {
    return *reinterpret_cast<T*>(static_cast<char*>(base) + offset);
}

float FloatBits(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace

// 0x1022de40 (1104 bytes): fills the 128 x 128 grid of (kind, thing id) entries at +0x60 for the map's width x height cells: 9 hoarding, 10 path,
// 11 queue, 12 flagged (mask 0x40), and for ride / entrance / exit / track / pylon cells the kind of the first thing of the parent cell's list
// that is an object (3 for a database kind of 2, 1 or 8 for 0, 2 for 1, 4 to 7 by the object's flags for 3), with the thing's id at +4.
void CUI2DMap::StoreMapGridDetails() {
    void* self = this;
    char* world = reinterpret_cast<char*>(gGameWorld);
    char* cells = world + 0x2d8;
    for (int32_t y = 0; y < At_2<int32_t>(self, 0x28); y++) {
        for (int32_t x = 0; x < At_2<int32_t>(self, 0x24); x++) {
            char* slot = nullptr;
            if (x >= 0 && x < 0x80 && y >= 0 && y < 0x80) {
                slot = static_cast<char*>(self) + (x + y * 0x80) * 8 + 0x60;
            }
            At_2<int32_t>(slot, 0) = 0;
            At_2<uint16_t>(slot, 4) = 0;
            const uint16_t cellId = static_cast<uint16_t>(x + y * 0x80 + 1);
            CMapCell* cell = reinterpret_cast<CMapCell*>(cells + (static_cast<int32_t>(cellId) - 1) * 0x44);
            if (cell->IsHoarding() != 0) {
                At_2<int32_t>(slot, 0) = 9;
            } else if (cell->IsRide() != 0 || cell->IsEntrance() != 0 || cell->IsExit() != 0 || cell->IsTrack() != 0 ||
                       cell->IsCoasterPylon() != 0) {
                uint16_t parent;
                if (cell->IsTrack() != 0) {
                    CTrackCell* track = cell->GetTrackCellEquiv();
                    if (track->IsTrackX() != 0) {
                        const int32_t trackId = At_2<uint16_t>(track, 0x10);
                        track = reinterpret_cast<CTrackCell*>(reinterpret_cast<char*>(gGameWorld) + 0x2d8 + (trackId - 1) * 0x28 + 0x110000);
                    }
                    parent = At_2<uint16_t>(track, 0x10);
                } else {
                    parent = At_2<uint16_t>(cell, 0x10);
                }
                bool found = false;
                uint16_t thingId = At_2<uint16_t>(cells + (static_cast<int32_t>(parent) - 1) * 0x44, 0x24);
                while (thingId != gUI2DMapGridThingListEnd && !found) {
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                    if (static_cast<CThing*>(gThingArray[thingId].mThing)->mKind == 3) {
                        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                        CObject* object = static_cast<CObject*>(gThingArray[thingId].mThing);
                        const int32_t databaseKind = object->GetDBInfo()->mUIType;   // the Mac's +0x7a8, by name
                        if (databaseKind == 2) {
                            found = true;
                            At_2<int32_t>(slot, 0) = 3;
                            At_2<uint16_t>(slot, 4) = thingId;
                        } else if (databaseKind < 2) {
                            if (databaseKind == 0) {
                                if (cell->IsRide() == 0 && (cell->IsTrack() != 0 || cell->IsCoasterPylon() != 0)) {
                                    At_2<int32_t>(slot, 0) = 8;
                                } else {
                                    At_2<int32_t>(slot, 0) = 1;
                                }
                                found = true;
                                At_2<uint16_t>(slot, 4) = thingId;
                            } else if (databaseKind >= 0) {
                                found = true;
                                At_2<int32_t>(slot, 0) = 2;
                                At_2<uint16_t>(slot, 4) = thingId;
                            }
                        } else if (databaseKind < 4) {
                            const uint16_t flags = At_2<uint16_t>(object, 0x2e);
                            if ((flags & 1) != 0 || (flags & 0x40) != 0) {
                                At_2<int32_t>(slot, 0) = 5;
                            } else if ((flags & 0x10) != 0) {
                                At_2<int32_t>(slot, 0) = 6;
                            } else if ((flags & 2) != 0) {
                                At_2<int32_t>(slot, 0) = 7;
                            } else {
                                At_2<int32_t>(slot, 0) = 4;
                            }
                            found = true;
                            At_2<uint16_t>(slot, 4) = thingId;
                        }
                    }
                    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZeroMessage);
                    thingId = At_2<uint16_t>(gThingArray[thingId].mThing, 0xa);
                }
            } else if (cell->IsPath() != 0) {
                At_2<int32_t>(slot, 0) = 10;
            } else if (cell->IsQueue() != 0) {
                At_2<int32_t>(slot, 0) = 11;
            } else if (cell->IsFlag(0x40) != 0) {
                At_2<int32_t>(slot, 0) = 12;
            }
        }
    }
}

// CUI2DMap::UpdateAllDrawRelated (0x1022e2c4, 3688 bytes): draws the map grid (a filled cell and its borders per map cell), the coaster tracks,
// then the frames of the sprites that are showing.

namespace {

// 0x10251a08 (the other unit's copy is not visible from here).
const char kZeroMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

using SpriteList_2 = TbTList<CUI2DMapSprite*, CUI2DMapSprite*>;

// One 36 byte CColour as nine words.
struct Colour9 {
    uint32_t w[9];
};

const uint8_t* Entry(int32_t offset) { return reinterpret_cast<const uint8_t*>(gUI2DMapColours) + offset; }

void CopyEntry(Colour9& out, int32_t offset) { std::memcpy(&out, Entry(offset), sizeof(out)); }

uint32_t Word(const void* base, size_t offset) {
    uint32_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 4);
    return v;
}

int32_t SWord(const void* base, size_t offset) { return static_cast<int32_t>(Word(base, offset)); }

uint16_t Half(const void* base, size_t offset) {
    uint16_t v;
    std::memcpy(&v, static_cast<const uint8_t*>(base) + offset, 2);
    return v;
}

// The map cell (x, y): an eight byte entry at 0x60 of the map, NULL outside 128 x 128.
const uint8_t* Cell(const uint8_t* map, int32_t x, int32_t y) {
    if (x < 0 || x >= 0x80 || y < 0 || y >= 0x80) return nullptr;
    return map + 0x60 + (x + (y << 7)) * 8;
}

// The cell's thing, after the original's assert that row 0 is free.
CObject* ThingOf(const uint8_t* cell) {
    uint16_t id = Half(cell, 4);
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kZeroMessage);
    return static_cast<CObject*>(gThingArray[id].mThing);
}

Colour9 Scaled(int32_t index) {
    bfl::draw::CColour c = UI2DMReturnScaleColour(index, *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x1f8)),
                                                  *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x21c)),
                                                  *reinterpret_cast<const bfl::draw::CColour*>(Entry(0x240)));
    Colour9 out;
    std::memcpy(&out, &c, sizeof(out));
    return out;
}

// The switch table at 0x102b2914 (types 0 to 12): the block each cell type runs; 0 is the default (nothing).
const int8_t kBlock[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

}  // namespace

void CUI2DMap::UpdateAllDrawRelated() {
    const uint8_t* map = reinterpret_cast<const uint8_t*>(this);
    bfl::draw::CDraw draw;
    draw.SetCurrent(HostArea());   // the Mac's area at 0x2c (ui_main.h)
    int32_t flags = 8;
    const int32_t perX = static_cast<int32_t>(ppc_fctiwz(static_cast<double>(480.0f / static_cast<float>(SWord(map, 0x24)))));
    const int32_t perY = static_cast<int32_t>(ppc_fctiwz(static_cast<double>(425.0f / static_cast<float>(SWord(map, 0x28)))));
    CUI2DMapDrawOps* ctx = UI2DMapDrawOps(draw.mRenderContext);

    int32_t y = 0x2a;
    for (int32_t row = 0; row < SWord(map, 0x28); row++) {
        int32_t x = 0xf;
        for (int32_t col = 0; col < SWord(map, 0x24); col++) {
            const uint8_t* cell = Cell(map, col, row);
            Colour9 fill;
            std::memset(&fill, 0, sizeof(fill));
            bool filled = false;
            const int32_t type = SWord(cell, 0);
            if (type == 1 || type == 2 || type == 3 || type == 8) {
                if (SWord(map, 0x20) == 2) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(thing->GetExcitementLevel(false) & 0xff);
                    filled = true;
                } else if (SWord(map, 0x20) == 3) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(thing->GetCustomerSatisfaction());
                    filled = true;
                }
            }
            if (!filled && static_cast<uint32_t>(type) <= 12) {
                const uint32_t mask = Word(map, 0x18);
                bool scaled = false;
                switch (kBlock[type]) {
                case 1: scaled = (mask & 1) != 0; break;
                case 2: scaled = (mask & 2) != 0; break;
                case 3: scaled = (mask & 4) != 0; break;
                case 4: scaled = (mask & 8) != 0; break;
                case 5: scaled = SWord(map, 0x1c) == 1; break;
                case 6: scaled = SWord(map, 0x1c) == 3; break;
                case 7: scaled = SWord(map, 0x1c) != 0; break;
                case 8: scaled = (mask & 0x10) != 0; break;
                case 9: CopyEntry(fill, 0x168); filled = true; break;
                case 10: CopyEntry(fill, 0x120); filled = true; break;
                case 11: CopyEntry(fill, 0x144); filled = true; break;
                case 12: CopyEntry(fill, 0x18c); filled = true; break;
                default: break;
                }
                if (scaled) {
                    CObject* thing = ThingOf(cell);
                    fill = Scaled(static_cast<int32_t>(ppc_fctiwz(static_cast<double>(thing->mF40))) & 0xff);
                    filled = true;
                }
            }
            if (filled) {
                int32_t rect[4] = {x, y, x + perX, y + perY};
                if (ctx != nullptr) ctx->Slot28(&draw.mViewPort, &draw.mClipRect, flags, rect, &fill);
            }

            Colour9 border;
            std::memset(&border, 0, sizeof(border));
            if (type >= 1 && type <= 7) {
                CObject* thing = ThingOf(cell);
                if (thing->IsClosed()) {
                    CopyEntry(border, 0x48);
                } else {
                    CopyEntry(border, 0x6c);
                }
            } else {
                CopyEntry(border, 0x6c);
            }
            if (type >= 1 && type <= 8) {
                const uint8_t* n = Cell(map, col, row - 1);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot10(&draw.mViewPort, &draw.mClipRect, flags, x, y, perX, &border);
                }
                n = Cell(map, col, row + 1);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot10(&draw.mViewPort, &draw.mClipRect, flags, x, y + (perX - 1), perX, &border);
                }
                n = Cell(map, col - 1, row);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot14(&draw.mViewPort, &draw.mClipRect, flags, x, y, perX, &border);
                }
                n = Cell(map, col + 1, row);
                if (n == nullptr || type != SWord(n, 0) || Half(cell, 4) != Half(n, 4)) {
                    if (ctx != nullptr) ctx->Slot14(&draw.mViewPort, &draw.mClipRect, flags, x + (perX - 1), y, perX, &border);
                }
            }
            x += perX;
        }
        y += perY;
    }

    if ((Word(map, 0x18) & 0x10) != 0) DrawCoasterTracks();

    flags = 0xc;
    SpriteList_2* list = nullptr;
    std::memcpy(&list, map + 0x20080, sizeof(list));
    SpriteList_2::CNode* node = list->mHead;
    while (node != nullptr) {
        CUI2DMapSprite* sprite = node->mData;
        node = node->mNext;
        sprite->UpdateVisibilityAndColour(SWord(map, 0x18), SWord(map, 0x1c), SWord(map, 0x20));
        if (sprite->mKind != 2) continue;
        const int32_t mode = sprite->mMode;
        const bool show = (mode == 1 && (Word(map, 0x18) & static_cast<uint32_t>(sprite->mMatch)) != 0) ||
                          (mode == 2 && SWord(map, 0x1c) == sprite->mMatch) || (mode == 3 && SWord(map, 0x20) == sprite->mMatch);
        if (!show) continue;
        const uint8_t* thing = static_cast<const uint8_t*>(sprite->mThing);
        const uint16_t first = Half(thing, 0x206);
        const uint16_t second = Half(thing, 0x208);
        const uint32_t a = static_cast<uint32_t>(first) - 1;
        const uint32_t b = static_cast<uint32_t>(second) - 1;
        if ((a & 0x7f) == 0) continue;
        if ((a >> 7) == 0) continue;
        if (!(static_cast<int32_t>(b & 0x7f) < 0x7f)) continue;
        if (!(static_cast<int32_t>(b >> 7) < 0x7f)) continue;
        int32_t rect[4];
        rect[0] = perX * static_cast<int32_t>(a & 0x7f) + 0xf;
        rect[1] = perY * static_cast<int32_t>(a >> 7) + 0x2a;
        rect[2] = perX * (static_cast<int32_t>(b & 0x7f) + 1) + 0xf;
        rect[3] = perY * (static_cast<int32_t>(b >> 7) + 1) + 0x2a;
        void* current = nullptr;
        std::memcpy(&current, map + 0x20060, sizeof(current));
        Colour9 frame;
        std::memset(&frame, 0, sizeof(frame));
        CopyEntry(frame, sprite == current ? 0x1d4 : 0x1b0);
        if ((flags & 4) != 0) {
            if (ctx != nullptr) ctx->Slot24(&draw.mViewPort, &draw.mClipRect, flags, rect, &frame);
        } else {
            if (ctx != nullptr) ctx->Slot28(&draw.mViewPort, &draw.mClipRect, flags, rect, &frame);
        }
    }
}

// 0x1022f160 (832 bytes): draws every coaster track of the park on the map as short antialiased lines between 0.1 steps of the Bezier of each
// pair of controls, in the track's colour; the line routine depends on the render area's bytes per pixel ((depth + 7) >> 3).
void CUI2DMap::DrawCoasterTracks() {
    void* self = this;
    bfl::draw::CDraw draw;
    draw.SetCurrent(HostArea());   // the Mac's area at 0x2c (ui_main.h)
    const float mapW = static_cast<float>(At_2<int32_t>(self, 0x24));
    const float scale = 480.0f / (10.0f * mapW);   // f31
    const uint32_t tracks = static_cast<uint32_t>(ui_GetNumCoasterTracks());
    const char* table = reinterpret_cast<const char*>(gUI2DMapColourTable);
    for (uint32_t i = 0; i < tracks; i++) {
        void* firstControl;
        tag_sBezierControl* segment = GetNthCoasterTrackStart(i, &firstControl);
        const int32_t colourIndex = ppc_fctiwz(static_cast<double>(At_2<float>(firstControl, 0x40)));
        const bfl::draw::CColour colour = UI2DMReturnScaleColour(
            colourIndex & 0xff, *reinterpret_cast<const bfl::draw::CColour*>(table + 0x1f8),
            *reinterpret_cast<const bfl::draw::CColour*>(table + 0x21c), *reinterpret_cast<const bfl::draw::CColour*>(table + 0x240));
        bool first = true;
        s3DPoint previous;
        previous.x = 0.0f;
        previous.y = 0.0f;
        previous.z = 0.0f;
        if (segment != nullptr) {
            while (segment->mNext != nullptr) {
                for (float t = 0.0f; t <= 1.0f; t = t + 0.1f) {
                    s3DPoint point;
                    BezierPosition(segment, segment->mNext, t, &point);
                    if (first) {
                        first = false;
                    } else {
                        const float fromX = previous.x * scale;
                        const float fromZ = previous.z * scale;
                        TbPoint from;
                        from.x = ppc_fctiwz(static_cast<double>(fromX)) + 0xf;
                        from.y = ppc_fctiwz(static_cast<double>(fromZ)) + 0x2a;
                        const float toX = point.x * scale;
                        const float toZ = point.z * scale;
                        TbPoint to;
                        to.x = ppc_fctiwz(static_cast<double>(toX)) + 0xf;
                        to.y = ppc_fctiwz(static_cast<double>(toZ)) + 0x2a;
                        const TbRect& clip = draw.GetClipRect();
                        const TbRenderArea& area = *HostArea();
                        const uint32_t kind = static_cast<uint32_t>(At_2<int32_t>(self, 0x3c) + 7) >> 3;
                        if (kind == 3) {
                            tpw_antialias::Antialiased<3>(tpw_intensify::CTraits<3>(), area, clip, colour, from, to);
                        } else if (kind < 3) {
                            if (kind == 1) {
                                tpw_antialias::Antialiased<1>(tpw_intensify::CTraits<1>(), area, clip, colour, from, to);
                            } else if (kind != 0) {
                                tpw_antialias::Antialiased<2>(tpw_intensify::CTraits<2>(), area, clip, colour, from, to);
                            }
                        } else if (kind < 5) {
                            tpw_antialias::Antialiased<4>(tpw_intensify::CTraits<4>(), area, clip, colour, from, to);
                        }
                    }
                    previous = point;
                }
                segment = segment->mNext;
            }
        }
    }
}

// CUI2DMap::MoveCentre, InitSprites and UpdateMapZoom (unit v5083; ui_2dmap.cpp, main app).
// The map's fields are read by Mac offset from `this` (the class has no members yet), see include/ui_main.h.

namespace {

// The 32-bit unsigned conversion the original builds with the 0x43300000 magic and no sign flip.
float FromU32(uint32_t value) { return static_cast<float>(value); }

}  // namespace

typedef TbTList<CUI2DMapSprite*, CUI2DMapSprite*> CUI2DMapSpriteList;

void CUI2DMap::UpdateMapZoom() {
    const float mapW = static_cast<float>(Field_2<int32_t>(this, 0x24));
    const float mapH = static_cast<float>(Field_2<int32_t>(this, 0x28));
    const float screenW = FromU32(static_cast<uint32_t>(gScreenWidth));
    const float screenH = FromU32(static_cast<uint32_t>(gScreenHeight));
    const float zoom = Field_2<float>(this, 0x5c);
    const float viewW = screenW / zoom;
    const float viewH = screenH / zoom;
    const float centreX = ppc_fmadds(Field_2<float>(this, 0x54), 480.0f / mapW, 15.0f);
    const float centreY = ppc_fmadds(Field_2<float>(this, 0x58), 425.0f / mapH, 42.0f);
    const float halfW = 0.5f * viewW;
    const float halfH = 0.5f * viewH;
    const float width = screenW;
    const float height = screenH;

    float x0 = centreX - halfW;
    float y0 = centreY - halfH;
    float x1 = centreX + halfW;
    float y1 = centreY + halfH;
    if (x0 < 0.0f) x0 = 0.0f;
    if (y0 < 0.0f) y0 = 0.0f;
    float limitX = FromU32(static_cast<uint32_t>(Field_2<int32_t>(this, 0x48) - 1));
    if (x1 >= limitX) x1 = limitX;
    float limitY = FromU32(static_cast<uint32_t>(Field_2<int32_t>(this, 0x4c) - 1));
    if (y1 >= limitY) y1 = limitY;

    gei_MapZoom.Use(static_cast<float>(ppc_fctiwz(static_cast<double>(x0))), static_cast<float>(ppc_fctiwz(static_cast<double>(y0))),
                    static_cast<float>(ppc_fctiwz(static_cast<double>(x1))), static_cast<float>(ppc_fctiwz(static_cast<double>(y1))));

    const float zoom2 = Field_2<float>(this, 0x5c);
    float left = ppc_fnmsubs(zoom2, centreX - x0, 0.5f * width);
    float top = ppc_fnmsubs(zoom2, centreY - y0, 0.5f * height);
    float right = ppc_fmadds(zoom2, x1 - centreX, 0.5f * width);
    float bottom = ppc_fmadds(zoom2, y1 - centreY, 0.5f * height);
    left = (2.0f * left) / width - 1.0f;
    top = (2.0f * top) / height - 1.0f;
    right = (2.0f * right) / width - 1.0f;
    bottom = (2.0f * bottom) / height - 1.0f;

    auto clamp = [](float v) {
        if (v < -1.0f) return -1.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    };
    gei_MapZoom.Window(clamp(left), clamp(top), clamp(right), clamp(bottom));
    UpdateAllSpritePositions();
}
void CUI2DMap::MoveCentre(EUI2DMapMoveDirection direction, float amount) {
    float& x = Field_2<float>(this, 0x54);
    float& y = Field_2<float>(this, 0x58);
    switch (static_cast<int32_t>(direction)) {
    case 0: x = x - amount; break;
    case 1: x = x + amount; break;
    case 2: y = y + amount; break;
    case 3: y = y - amount; break;
    default: break;
    }
    // Each coordinate: below 0.0 it becomes 0.0, above (size - 1) it becomes that (NaN passes).
    float cx = x;
    if (cx < 0.0f) {
        cx = 0.0f;
    } else {
        float limit = static_cast<float>(Field_2<int32_t>(this, 0x24) - 1);
        if (cx > limit) cx = limit;
    }
    x = cx;
    float cy = y;
    if (cy < 0.0f) {
        cy = 0.0f;
    } else {
        float limit = static_cast<float>(Field_2<int32_t>(this, 0x28) - 1);
        if (cy > limit) cy = limit;
    }
    y = cy;
    UpdateMapZoom();
}

// 0x1022fa1c (128 bytes): the zoom float at 0x5c, 0 in (adds), 1 out (subtracts), any other kind leaves it; then kept in 1.0 .. 3.0
// (a NaN becomes 1.0).
void CUI2DMap::Zoom(EUI2DMapZoom zoom, float amount) {
    float& value = Field_2<float>(this, 0x5c);
    int32_t kind = static_cast<int32_t>(zoom);
    if (kind == 1) {
        value = value - amount;
    } else if (kind < 1 && kind >= 0) {
        value = value + amount;
    }
    float z = value;
    // fcmpo then bge / ble branch on "not less" / "not greater": a NaN passes both and stays
    float result;
    if (!(z < 1.0f)) {
        result = 3.0f;
        if (!(z > 3.0f)) result = z;
    } else {
        result = 1.0f;
    }
    value = result;
    UpdateMapZoom();
}

void CUI2DMap::InitSprites() {
    static const char kMessage[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
    int32_t count = 0;

    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gUI2DMapThingListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kMessage);
        const uint8_t* thing = static_cast<const uint8_t*>(gThingArray[cur].mThing);
        switch (thing[2]) {
        case 1: count += 2; break;
        case 4: count += 1; break;
        case 5: count += 1; break;
        case 6: count += 2; break;
        case 7: count += 2; break;
        default: break;
        }
        cur = gGameWorld->GetNextUsedThing(reinterpret_cast<const CThing*>(thing)).mIndex;
    }

    uint8_t* world = reinterpret_cast<uint8_t*>(gGameWorld);
    for (int32_t y = 0; y < Field_2<int32_t>(this, 0x28); y++) {
        for (int32_t x = 0; x < Field_2<int32_t>(this, 0x24); x++) {
            uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
            // index 0 reads before the cells (the halfword minus one is a widened int)
            if (Field_2<int32_t>(world, 0x2d8 + (static_cast<int32_t>(index) - 1) * 0x44 + 0x2c) != 0) count++;
        }
    }

    uint16_t id = Field_2<uint16_t>(world, 0x1da746);
    while (id != gUI2DMapObjectListEnd) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kMessage);
        uint8_t* thing = static_cast<uint8_t*>(gThingArray[id].mThing);
        if ((Field_2<uint16_t>(thing, 0x2e) & 0x10) != 0) count++;
        id = Field_2<uint16_t>(thing, 0xc);
    }

    gei_EZSprite.Init(count + 6, 0);
    gei_EZSprite.Enable();
    for (int32_t i = 0; i < 6; i++) {
        SSpriteEngineData* image = gei_EZSprite.LoadSprite(const_cast<char*>(gUI2DMapImages[i].mName));
        // the images are Ptr32 words at 0x20068 (see CreateMapSprites); the host keeps the native pointer too (HostImage)
        Field_2<uint32_t>(this, 0x20068 + i * 4) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(image));
        HostImage(i) = image;
    }
    // The original also stores the address of a data object (0x102b2998) over the new list's first word, the vtable of its
    // class; the host list keeps the vtable of TbTList.
    CUI2DMapSpriteList* list = new CUI2DMapSpriteList();
    Field_2<CUI2DMapSpriteList*>(this, 0x20080) = list;
    CreateMapSprites();
}

// CUI2DMap::CreateMapSprites 0x1022fd78 (2628 bytes, unit v5058): makes the map's sprites: for every used thing of a few kinds one or two
// sprites at the thing's map position, one sprite for every map cell with a nonzero word at +0x2c, and one for every ride of the object list
// with flag 0x10. The map's fields are read by Mac offset (see ui_main.h); the things by Mac offset too (they are loaded images).

namespace {

const char kAssertText[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";

uint8_t* Bytes(const void* p) { return const_cast<uint8_t*>(static_cast<const uint8_t*>(p)); }
uint8_t Byte(const void* p, size_t off) { return Bytes(p)[off]; }
uint16_t Half_2(const void* p, size_t off) { return *reinterpret_cast<const uint16_t*>(Bytes(p) + off); }
int32_t Word_2(const void* p, size_t off) { return *reinterpret_cast<const int32_t*>(Bytes(p) + off); }

// The image of the Mac word at off (0x20068 .. 0x2007c): the native pointer InitSprites keeps for it (CUI2DMap::HostImage).
void* Image(const CUI2DMap* map, size_t off) {
    return const_cast<CUI2DMap*>(map)->HostImage(static_cast<int32_t>((off - 0x20068) / 4));
}

int32_t Balance(size_t off) { return Word_2(&gMainBalance, off); }

// The two thing id loops read the table's thing with the assertion on row 0 first.
void* ThingOfId_2(uint16_t id) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kAssertText);
    return gThingArray[id].mThing;
}

}  // namespace

void CUI2DMap::CreateMapSprites() {
    CUI2DMap* map = this;

    // One sprite for a used thing: the position is fmadds(byte5, 1/256, byte4), fmadds(byte7, 1/256, byte6).
    auto place = [&](const void* thing, size_t imageOff, int32_t id, int32_t a, int32_t b, float scale) {
        CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, imageOff), static_cast<EUI2DMapSpriteID>(id), TUI2DMapIndividualShowStatus{a, b}, scale);
        sprite->mX = ppc_fmadds(static_cast<float>(Byte(thing, 5)), 0.00390625f, static_cast<float>(Byte(thing, 4)));
        sprite->mY = ppc_fmadds(static_cast<float>(Byte(thing, 7)), 0.00390625f, static_cast<float>(Byte(thing, 6)));
        sprite->mThing = const_cast<void*>(thing);
        UpdateSpritePosition(sprite);
    };

    uint16_t cur = gGameWorld->GetUsedThingHead().mIndex;
    while (cur != gThingIdNone) {
        const void* thing = ThingOfId_2(cur);
        switch (Byte(thing, 2)) {
        case 1:
            place(thing, 0x2006c, 1, 1, 0x20, 1.0f);
            place(thing, 0x20068, 0, 3, 1, 1.0f);
            break;
        case 4:
            place(thing, 0x20070, 2, 2, 2, 1.0f);
            break;
        case 5:
            place(thing, 0x20070, 2, 2, 1, 1.0f);
            break;
        case 6:
            place(thing, 0x20070, 2, 2, 4, 1.0f);
            place(thing, 0x20078, 4, 2, 4, 0.5f * static_cast<float>(Balance(0x5d0)));
            break;
        case 7:
            place(thing, 0x20070, 2, 2, 3, 1.0f);
            place(thing, 0x2007c, 5, 2, 3, 0.5f * static_cast<float>(Balance(0x618)));
            break;
        default:
            break;
        }
        cur = gGameWorld->GetNextUsedThing(static_cast<const CThing*>(thing)).mIndex;
    }

    const uint8_t* cells = Bytes(gGameWorld) + 0x2d8;
    for (int32_t y = 0; y < Word_2(map, 0x28); y++) {
        for (int32_t x = 0; x < Word_2(map, 0x24); x++) {
            uint16_t index = static_cast<uint16_t>(x + (y << 7) + 1);
            // the original's index minus one is a widened halfword, so index 0 reads before the cells
            int32_t type = Word_2(cells + (static_cast<int32_t>(index) - 1) * 0x44, 0x2c);
            if (type == 0) continue;
            float scale = 1.0f;
            if (type == 8) {
                scale = static_cast<float>(Balance(0x648));
            } else if (type == 7) {
                scale = static_cast<float>(Balance(0x600));
            }
            CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, 0x20074), static_cast<EUI2DMapSpriteID>(3), TUI2DMapIndividualShowStatus{2, 1}, 0.5f * scale);
            sprite->mX = 0.5f + static_cast<float>(x);
            sprite->mY = 0.5f + static_cast<float>(y);
            sprite->mThing = nullptr;
            UpdateSpritePosition(sprite);
        }
    }

    uint16_t id = Half_2(gGameWorld, 0x1da746);
    while (id != gObjectIdNone) {
        const void* thing = ThingOfId_2(id);
        if ((Half_2(thing, 0x2e) & 0x10) != 0) {
            CUI2DMapSprite* sprite = new CUI2DMapSprite(Image(map, 0x2007c), static_cast<EUI2DMapSpriteID>(5), TUI2DMapIndividualShowStatus{2, 3},
                                                        0.5f * static_cast<float>(Balance(0x630)));
            int32_t v = static_cast<int32_t>(static_cast<uint16_t>(Byte(thing, 4) + Byte(thing, 6) * 0x80 + 1)) - 1;
            sprite->mX = 0.5f + static_cast<float>(v & 0x7f);
            sprite->mY = 0.5f + static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(v) >> 7));
            sprite->mThing = nullptr;
            UpdateSpritePosition(sprite);
        }
        id = Half_2(thing, 0xc);
    }
}

// 0x102307ec (188 bytes)
void CUI2DMap::CloseSprites() {
    DeleteSpriteListEntries();
    SpriteList*& list = Field_2<SpriteList*>(this, 0x20080);
    if (list != nullptr) delete list;   // the virtual deleting destructor, flag 1
    list = nullptr;
    for (int32_t i = 0; i < 6; i++)
        gei_EZSprite.DeleteSprite(HostImage(i));   // the image of the word at 0x20068 + 4 * i, kept native on the host
    gei_EZSprite.Disable();
    gei_EZSprite.Close();
}

// 0x10230974 (96 bytes)
void CUI2DMap::UpdateAllSpritePositions() {
    SpriteList* list = Field_2<SpriteList*>(this, 0x20080);
    POSITION position = list->GetHeadPosition();
    while (position != nullptr) {
        CUI2DMapSprite* sprite = list->GetNext(position);
        UpdateSpritePosition(sprite);
    }
}

// 0x10230a0c (388 bytes): puts the sprite on the screen from its map place: the screen position in the sprite's +0x0c and +0x10, then
// SetPosition (x and y in -1..1 clip space, z 0.99 for the shown statuses 0 to 2 else 0.9999) and SetScale (half the zoom times the sprite's scale).
void CUI2DMap::UpdateSpritePosition(CUI2DMapSprite* sprite) {
    void* self = this;
    void* sp = sprite;
    const float mapW = static_cast<float>(At_2<int32_t>(self, 0x24));
    const float mapH = static_cast<float>(At_2<int32_t>(self, 0x28));
    // The screen size is converted as an unsigned word (no xoris on those two).
    const float screenW = static_cast<float>(static_cast<uint32_t>(gScreenWidth));
    const float screenH = static_cast<float>(static_cast<uint32_t>(gScreenHeight));
    const float perX = 480.0f / mapW;   // f7
    const float perY = 425.0f / mapH;   // f8
    const float half = 0.5f;

    float t = At_2<float>(sp, 4) - At_2<float>(self, 0x54);
    t = At_2<float>(self, 0x5c) * t;
    t = perX * t;
    At_2<float>(sp, 0x0c) = ppc_fmadds(half, screenW, t);

    t = At_2<float>(sp, 8) - At_2<float>(self, 0x58);
    t = At_2<float>(self, 0x5c) * t;
    t = perY * t;
    At_2<float>(sp, 0x10) = ppc_fmadds(half, screenH, t);

    const float two = 2.0f;
    float px = two * At_2<float>(sp, 0x0c);
    float py = two * At_2<float>(sp, 0x10);
    px = px / screenW;
    py = py / screenH;
    px = px - 1.0f;
    py = py - 1.0f;

    _D3DVECTOR position;
    position.x = px;
    position.y = py;
    const int32_t status = At_2<int32_t>(sp, 0x14);
    if (status < 3 && status >= 0) {
        position.z = FloatBits(0x3f7d70a4u);   // 0.99
    } else {
        position.z = FloatBits(0x3f7ff972u);   // 0.9999
    }
    sprite->SetPosition(position);

    float scale = half * At_2<float>(self, 0x5c);
    scale = scale * At_2<float>(sp, 0x24);
    sprite->SetScale(scale);
}

// 0x10230bd4 (100 bytes): deletes every sprite of the list (a NULL entry is only dropped).
void CUI2DMap::DeleteSpriteListEntries() {
    while (Field_2<SpriteList*>(this, 0x20080)->GetCount() != 0) {
        CUI2DMapSprite* sprite = Field_2<SpriteList*>(this, 0x20080)->RemoveHead();
        if (sprite != nullptr) delete sprite;
    }
}

// 0x10230c70 (44 bytes)
void CUI2DMap::Register(CUI2DMapSprite* sprite) {
    ui2dmap_callee::ListAddTail(At<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>*>(this, 0x20080), sprite);
}

// 0x10230cd4 (84 bytes): takes the sprite out of the list when it is there.
void CUI2DMap::UnRegister(CUI2DMapSprite* sprite) {
    SpriteList* list = Field_2<SpriteList*>(this, 0x20080);
    POSITION position = list->Find(sprite);
    if (position != nullptr) list->RemoveAt(position);
}

// Unit v51044: small functions of the online, lobby and 2D map code:
//   0x1021eb7c std::less<std::wstring>::operator()          0x1021ecb8 std::allocator<nocase_string>::destroy
//   0x1021e4dc std::basic_streambuf<wchar_t>::uflow         0x10222c78 std::allocator<pair<const nocase_string, BuddyInfo>>::destroy
//   0x102238d4 std::allocator<std::wstring>::destroy        0x10227840 std::pair<const nocase_string, BuddyInfo>::pair (copy)
//   0x1022b7e8 ShowWorldLobbySats  0x1022b85c HideWorldLobbySats  0x1022ba30 WorldLobbySetZoomButton
//   0x10230d64 CUI2DMapSprite::CUI2DMapSprite

// 0x10230d64
CUI2DMapSprite::CUI2DMapSprite(void* image, EUI2DMapSpriteID id, TUI2DMapIndividualShowStatus status, float scale) {
    const uint32_t vtable = 0x102b2958u;   // the Mac's vtable address (the host class has no vtable)
    std::memcpy(mF00, &vtable, 4);
    mSprite = gei_EZSprite.CreateSpriteInstance(image);
    mKind = id;
    mVisible = 1;
    mMode = status.mA;
    mMatch = status.mB;
    SetVisibility(0);
    std::memcpy(&mF20[4], &scale, 4);   // Mac 0x24
    uint32_t* details = static_cast<uint32_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
    details[4] |= 0x10000u;
    details[4] |= 0x20000u;
    TbTList<CUI2DMapSprite*, CUI2DMapSprite*>* list =
        *reinterpret_cast<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>**>(gUI2DMap.mBytes + 0x20080);
    list->AddTail(this);
}

// Unit v51047: std::basic_istream<char>::basic_istream 0x1020a4c0, CUIGrapher::Clear 0x101fd9d0 and its constructor 0x101fd6e8,
// CFSLoadSave::GetNextSaveFileDetails 0x101fcd08, CUI2DMapSprite::~CUI2DMapSprite 0x10230ea4, SetVisibility 0x10230f78 and SetScale
// 0x10231094, the program entry 0x102381a4 and InterfaceKnob::operator- 0x101d038c.

// The vtable block of CUI2DMapSprite (TOC slot -0x5e64, initial word 0x102b2950).
char gCUI2DMapSpriteVt[0x20];   // 0x102b2958

// ---- CUI2DMapSprite. The Mac's destructor takes a hidden delete flag; the host's does not (see CUI2DMapSpriteDtor).
CUI2DMapSprite::~CUI2DMapSprite() {
    *reinterpret_cast<uint32_t*>(this) = A(gCUI2DMapSpriteVt);
    auto* list = *reinterpret_cast<TbTList<CUI2DMapSprite*, CUI2DMapSprite*>**>(gUI2DMap.mBytes + 0x20080);
    POSITION position = list->Find(this);
    if (position != nullptr) list->RemoveAt(position);
    gei_EZSprite.DeleteSpriteInstance(mSprite);
}

// The Mac's destructor with its delete flag: NULL is left alone, a flag above 0 (signed halfword) frees the object. Answers this.
CUI2DMapSprite* CUI2DMapSpriteDtor(CUI2DMapSprite* self, int16_t flags) {
    if (self != nullptr) {
        self->~CUI2DMapSprite();
        if (flags > 0) UI2DMapDelete(self);
    }
    return self;
}

void CUI2DMapSprite::SetVisibility(int32_t visible) {
    if (mVisible == visible) return;
    mVisible = visible;
    uint8_t* details = static_cast<uint8_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
    uint32_t flags = *reinterpret_cast<uint32_t*>(details + 0x10);
    if (mVisible != 0)
        flags &= ~1u;
    else
        flags |= 1u;
    *reinterpret_cast<uint32_t*>(details + 0x10) = flags;
}

// 0x1022bf04 (76 bytes)
CUI2DMap* CUI2DMapDestruct(CUI2DMap* self, int16_t deleteFlag) {
    if (self != nullptr) {
        At<uint32_t>(self, 0) = ToWord(&gUI2DMapVtableSlot);
        if (deleteFlag > 0) UI2DMapDelete(self);
    }
    return self;
}

// 0x10231014 (64 bytes)
void CUI2DMapSprite::SetPosition(const _D3DVECTOR& position) {
    D3DVECTOR copy = position;
    gei_EZSprite.SetSpritePosition(mSprite, copy);
}

// 0x1022bed0 (4 bytes): blr
void TUI2DMapMapGridCellCtor(void* self) { (void)self; }

// The detail block is the sprite instance (SSpriteGameData): its sprite (Mac 0x28, a native pointer on the host) gives the image's
// width (Mac 0x0c), and the scale goes to Mac 0x2c (mF2C).
void CUI2DMapSprite::SetScale(float scale) {
    SSpriteGameData* details = static_cast<SSpriteGameData*>(gei_EZSprite.GetSpriteDetails(mSprite));
    float width = static_cast<float>(static_cast<double>(details->mSprite->mWidth));
    float scaled = scale * width;
    details->mF2C = scaled * 0.0625f;
}

namespace {
void SetFlag(CEZSprite& e, void* sprite, int32_t visible) {
    uint8_t* d = static_cast<uint8_t*>(e.GetSpriteDetails(sprite));
    uint32_t f = *reinterpret_cast<uint32_t*>(d + 0x10);
    f = visible == 0 ? (f | 1u) : (f & 0xfffffffeu);
    *reinterpret_cast<uint32_t*>(d + 0x10) = f;
}
}  // namespace

// 0x10231134
void CUI2DMapSprite::UpdateVisibilityAndColour(int32_t mask, int32_t second, int32_t third) {
    const uint8_t* table = reinterpret_cast<const uint8_t*>(gUI2DMapColours);
    if ((mMode == 1 && (static_cast<uint32_t>(mask) & static_cast<uint32_t>(mMatch)) != 0) ||
        (mMode == 2 && second == mMatch) || (mMode == 3 && third == mMatch)) {
        if (mVisible != 1) {
            mVisible = 1;
            SetFlag(gei_EZSprite, mSprite, mVisible);
        }
        uint8_t col[0x24];
        std::memcpy(col, table + 0x24, 0x24);
        switch (mKind) {
        case 0: {
            uint8_t* t = static_cast<uint8_t*>(mThing);
            if (t && t[2] == 1) {
                float v = *reinterpret_cast<float*>(t + 0x19c);
                int32_t idx = ppc_fctiwz(v);
                bfl::draw::CColour r = UI2DMReturnScaleColour(
                    static_cast<int32_t>(static_cast<uint32_t>(idx) & 0xff),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x264),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x288),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x2ac));
                std::memcpy(col, &r, 0x24);
            }
            break;
        }
        case 1: std::memcpy(col, table + 0x2d0, 0x24); break;
        case 3: std::memcpy(col, table + 0x2f4, 0x24); break;
        case 4: {
            uint8_t* t = static_cast<uint8_t*>(mThing);
            if (t && t[2] == 6) {
                float v = *reinterpret_cast<float*>(t + 0x1f4);
                int32_t idx = ppc_fctiwz(v);
                bfl::draw::CColour r = UI2DMReturnScaleColour(
                    static_cast<int32_t>(static_cast<uint32_t>(idx) & 0xff),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x264),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x288),
                    *reinterpret_cast<const bfl::draw::CColour*>(table + 0x2ac));
                std::memcpy(col, &r, 0x24);
            }
            break;
        }
        case 5: std::memcpy(col, table + 0x318, 0x24); break;
        default: break;
        }
        uint32_t v = (static_cast<uint32_t>(col[1]) << 16) | (static_cast<uint32_t>(col[2]) << 24) |
                     (static_cast<uint32_t>(col[0]) << 8) | col[3];
        uint8_t* d = static_cast<uint8_t*>(gei_EZSprite.GetSpriteDetails(mSprite));
        *reinterpret_cast<uint32_t*>(d + 0x24) = v;
    } else if (mVisible != 0) {
        mVisible = 0;
        SetFlag(gei_EZSprite, mSprite, mVisible);
    }
}

// Host only: UI2DMapDrawOps (include/ui_main.h), the four slots CUI2DMap::UpdateAllDrawRelated calls on CDraw's render
// context. The Mac calls them through the context's vtable (0x100739a4, 0x10073958, 0x1007390c, 0x100738c0 for 8, 16, 24
// and 32 bits); the host's gDrawContextN have no vtable, so each answers an adapter onto its CRenderClassN.

namespace {

template <typename RenderClass>
struct RenderClassOps : CUI2DMapDrawOps {
    RenderClass mClass;

    static TbRenderArea* Area(const void* viewPort) { return static_cast<TbRenderArea*>(const_cast<void*>(viewPort)); }
    static const TbRect& Rect(const void* rect) { return *static_cast<const TbRect*>(rect); }
    static const bfl::draw::CColour& Colour(const void* colour) { return *static_cast<const bfl::draw::CColour*>(colour); }

    void Slot10(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) override {
        mClass.HorizontalLine(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), x, y, length, Colour(colour));
    }
    void Slot14(const void* viewPort, const void* clip, int32_t flags, int32_t x, int32_t y, int32_t length, const void* colour) override {
        mClass.VerticalLine(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), x, y, length, Colour(colour));
    }
    void Slot24(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) override {
        mClass.RectangleOutline(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), Rect(rect), Colour(colour));
    }
    void Slot28(const void* viewPort, const void* clip, int32_t flags, const int32_t* rect, const void* colour) override {
        mClass.RectangleFilled(Area(viewPort), Rect(clip), static_cast<uint32_t>(flags), Rect(rect), Colour(colour));
    }
};

RenderClassOps<bfl::draw::CRenderClass8> gOps8;
RenderClassOps<bfl::draw::CRenderClass16> gOps16;
RenderClassOps<bfl::draw::CRenderClass24> gOps24;
RenderClassOps<bfl::draw::CRenderClass32> gOps32;

}  // namespace

CUI2DMapDrawOps* UI2DMapDrawOps(void* context) {
    if (context == &bfl::draw::gDrawContext8) return &gOps8;
    if (context == &bfl::draw::gDrawContext16) return &gOps16;
    if (context == &bfl::draw::gDrawContext24) return &gOps24;
    if (context == &bfl::draw::gDrawContext32) return &gOps32;
    return static_cast<CUI2DMapDrawOps*>(context);
}

// Unit v5194: __sinit_ui_2dmap_cpp 0x102319d4 (3776 bytes), the static initialiser of ui_2dmap.cpp: the map object gUI2DMap, a table of 23
// colour entries and seven global strings, each registered for destruction at exit.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
extern "C" void __construct_array(void* array, void* constructor, int32_t flags, uint32_t size, uint32_t count);   // runtime glue 0x10238758

alignas(16) CUI2DMapColourEntry gUI2DMapColours[23];       // 0x103970f0
CUI2DMapGlobalNode gUI2DMapImageNodes[6];                  // 0x10397088
CUI2DMapGlobalNode gUI2DMapNameNode;                       // 0x103970e4
CUI2DMapGlobalNode gUI2DMapNode;                           // 0x103b74b4
alignas(16) uint8_t gUI2DMapTgaName[0x40];                 // 0x103970d0
alignas(16) uint8_t gUI2DMapNameStrings[6][0x40];          // 0x10397010
// The same six strings as InitSprites reads them (the name at +8 of a 0x14-byte entry is the string's text on the Mac): the host's
// strings are a different size, so InitSprites reads this table, which the initialiser fills with each string's text.
CUI2DMapImageEntry gUI2DMapImages[6];                      // 0x10397010
uint32_t gUI2DMapCounterA;                                 // 0x103b74c4
uint32_t gUI2DMapCounterB;                                 // 0x103b74c0
uint32_t gUI2DMapVtableSlot;                               // 0x102b296c
uint32_t gUI2DMapDtorVector[2];                            // 0x102689f8
uint32_t gUI2DMapCtorVector[2];                            // 0x10268a00

static_assert(sizeof(TbDynamicStringTemplate<char>) <= 0x40, "string storage");

namespace {

void StoreWord(uint8_t* base, uint32_t offset, uint32_t value) { std::memcpy(base + offset, &value, 4); }

// The original builds each entry in a stack temporary (colour bytes, four zero words, four words never written) and copies it.
const uint8_t kColours[23][4] = {
    {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}, {0x00, 0x00, 0x00, 0xff}, {0x00, 0xff, 0x00, 0xff},
    {0xff, 0xff, 0x00, 0xff}, {0x00, 0x4e, 0xfe, 0xff}, {0x03, 0xff, 0xfc, 0xff}, {0x0b, 0x42, 0x6e, 0xff},
    {0x1f, 0xa6, 0x1f, 0xff}, {0x25, 0xc6, 0x25, 0xff}, {0x00, 0x17, 0x00, 0xff}, {0x00, 0x17, 0x00, 0x80},
    {0x96, 0x19, 0x19, 0xc8}, {0xfa, 0x19, 0x19, 0xc8}, {0x00, 0x00, 0xf8, 0xff}, {0x00, 0xc8, 0xf8, 0xff},
    {0x00, 0xfc, 0x30, 0xff}, {0xf8, 0xa8, 0x20, 0xff}, {0x00, 0x84, 0xf8, 0xff}, {0x00, 0xfc, 0xf8, 0xff},
    {0x00, 0x00, 0x00, 0xff}, {0x00, 0x00, 0xff, 0xff}, {0xc8, 0xfa, 0xc8, 0xff},
};

const char* const kImageNames[6] = {
    ".:data:2dmap:vsprite.tga", ".:data:2dmap:rsprite.tga", ".:data:2dmap:ssprite.tga",
    ".:data:2dmap:lsprite.tga", ".:data:2dmap:esprite.tga", ".:data:2dmap:gsprite.tga",
};

}  // namespace

extern "C" void __sinit_ui_2dmap_cpp() {
    uint8_t* map = gUI2DMap.mBytes;
    gUI2DMapCounterA = 0;
    gUI2DMapCounterB = 0x10000;
    StoreWord(map, 0x00, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&gUI2DMapVtableSlot)));
    StoreWord(map, 0x10, 0);
    StoreWord(map, 0x14, 0);
    // The Mac constructs the map's render area at 0x2c; the host's lives at CUI2DMap::kHostAreaOffset (see ui_main.h).
    new (gUI2DMap.HostArea()) TbRenderArea();
    __construct_array(map + 0x60, gUI2DMapCtorVector, 0, 8, 0x4000);
    StoreWord(map, 0x04, 0);
    StoreWord(map, 0x18, 1);
    StoreWord(map, 0x1c, 1);
    StoreWord(map, 0x20, 1);
    StoreWord(map, 0x5c, 0x3f800000);   // 1.0f
    StoreWord(map, 0x20060, 0);
    __register_global_object(map, gUI2DMapDtorVector, &gUI2DMapNode);

    for (int i = 0; i < 23; i++) {
        std::memcpy(gUI2DMapColours[i].mColour, kColours[i], 4);
        std::memset(gUI2DMapColours[i].mZero, 0, sizeof(gUI2DMapColours[i].mZero));
        std::memset(gUI2DMapColours[i].mSpare, 0, sizeof(gUI2DMapColours[i].mSpare));
    }

    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    void* object = new (gUI2DMapTgaName) TbDynamicStringTemplate<char>("2dmap.tga");
    __register_global_object(object, destroy, &gUI2DMapNameNode);
    for (int i = 0; i < 6; i++) {
        object = new (gUI2DMapNameStrings[i]) TbDynamicStringTemplate<char>(kImageNames[i]);
        gUI2DMapImages[i].mName = static_cast<TbStringBase<char>*>(static_cast<TbDynamicStringTemplate<char>*>(object))->mData;
        __register_global_object(object, destroy, &gUI2DMapImageNodes[5 - i]);
    }
}
