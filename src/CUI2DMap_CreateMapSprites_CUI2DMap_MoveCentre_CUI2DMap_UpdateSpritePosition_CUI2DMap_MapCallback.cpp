// CUI2DMap::MapCallback 0x1022c7e8 (2180 bytes, unit v5526), main app: the map panel's window message function.
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

#include "Game.h"
#include "Ptr32.h"
#include "InterfaceWindow.h"
#include "MapWho.h"
#include "TPCamera.h"
#include "TPSettings.h"
#include "UI_Base.h"
#include "tb_resource_display.h"
#include "ui_ErrorDisplay.h"
#include "ui_tooltip.h"

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
