// Seven small functions of ui_main.cpp (unit v5723): UIMain_ZoomToMapId 0x10181d34, UIMain_ZoomToPeep 0x10181c70,
// UIMain_GetShiftKeyState 0x10182088, UIMain_DestroySpritePreview 0x10181864, UIMain_UpdateSpritePreview 0x101817e4,
// UIMain_MidWindowParticles 0x10182404, UIMain_DestroyRidePreview 0x101815d4.
#include <cstdint>
#include <new>
#include "Debug.h"
#include "ThingReceivers.h"
#include "World.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "TPCamera.h"
#include "GameSys.h"
#include "Game.h"
#include "TbAudioSystem.h"
#include "UI_Base.h"
#include "InterfaceWindow.h"
#include "analyser.h"
#include "rsse.h"

// 0x10181d34: the map cell (index - 1) as column (low 7 bits) and row, ten units per cell.
void UIMain_ZoomToMapId(tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t>& id) {
    uint16_t v = id.mIndex;
    uint32_t m = static_cast<uint32_t>(v) - 1u;
    float x = static_cast<float>(static_cast<int32_t>((m & 0x7fu) * 10u));
    float y = static_cast<float>(static_cast<int32_t>((m >> 7) * 10u));
    SetCameraPOI(&x, &y, nullptr, nullptr, nullptr);
}

// 0x10181c70
void UIMain_ZoomToPeep(CPeep* peep) {
    if (peep == nullptr) return;
    float t = static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(gTickNow) - static_cast<uint32_t>(gLastTurnTime))) / 248.0f;
    float x, y, z;
    uint32_t stamp;
    peep->GetInterpolatedPeepPosition(t, 1, x, y, z, stamp);
    SetCameraPOI(&x, &z, nullptr, nullptr, nullptr);
}

// 0x10182088: modifier bits to the 2-bit fields: 0x200 -> 3, 0x1100 -> 0xc, 0x800 -> 0x30.
uint32_t UIMain_GetShiftKeyState() {
    uint32_t result = 0;
    if (SamsUtilities::gKeyboard == nullptr) SamsUtilities::gKeyboard = new SamsUtilities::UKeyboard;
    uint32_t bits = SamsUtilities::gKeyboard->GetModifiers();
    if (bits & 0x200) result |= 3;
    if (bits & 0x1100) result |= 0xc;
    if (bits & 0x800) result |= 0x30;
    return result;
}

// 0x10181864
void UIMain_DestroySpritePreview() {
    if (gSpritePreviewPeep != nullptr) {
        SCR_KillScript(gSpritePreviewHandle);
        gSpritePreviewPeep = nullptr;
    }
}

// 0x101817e4: the peep's word at +0xc is the source script.
void UIMain_UpdateSpritePreview() {
    if (gSpritePreviewPeep != nullptr) {
        int32_t source = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(gSpritePreviewPeep) + 0xc);
        if (source != 0) ESP_Update2DScript(gSpritePreviewHandle, source);
    }
}

// 0x10182404
void UIMain_MidWindowParticles(int32_t id, InterfaceWindow* window) {
    CoolRect rect;
    if (window->GetVisibility() != 0 && gGame.mF1C == 0) {
        window->GetPosition(&rect, 1);
        int32_t cx = rect.mLeft + ((rect.mRight - rect.mLeft) >> 1);
        int32_t cy = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
        int32_t px = static_cast<int32_t>(static_cast<uint32_t>(cx) * 100000u) / 2048;
        int32_t py = static_cast<int32_t>(static_cast<uint32_t>(cy) * 75000u) / 0x600;
        PAR_InitEmitter(id, px, 0, py);
    }
}

// 0x101815d4
void UIMain_DestroyRidePreview() {
    if (gRidePreview != nullptr) {
        DestroyRidePreview(gRidePreview);
        gRidePreview = nullptr;
    }
}
