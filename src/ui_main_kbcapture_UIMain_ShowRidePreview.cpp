// Unit v5013: the ride and sprite previews, MAKECOLUMNBUTTON and UIMain_ZoomToThing (main 0x10181400, 0x10181648, 0x101818e4, 0x10181b00).
#include <cstdint>

#include "Debug.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Text.h"
#include "TPCamera.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"

sRidePreviewInfo* gRidePreview = nullptr;   // 0x10380c44
CPeep* gSpritePreviewPeep = nullptr;        // 0x10380c3c
int32_t gSpritePreviewHandle = 0;           // 0x10380c40
uint16_t gZoomSkipThing = 0;                // 0x102a7866

// 0x1024d8b2: a six byte draw object name, and at +6 the assert text (0x1024d8b8)
static const char kStringPool[] = "!cbut\0Thing has been allocated in ThingArray index 0 - shouldn't happen\n";
static const char* const kColumnButtonDrawObject = kStringPool;
static const char* const kColumnButtonAssert = kStringPool + 6;

// 0x10181400
void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z) {
    float bottom = static_cast<float>(0x300 - static_cast<int32_t>(rect.mBottom)) / 768.0f;
    float top = static_cast<float>(0x300 - static_cast<int32_t>(rect.mTop)) / 768.0f;
    float right = static_cast<float>(static_cast<int32_t>(rect.mRight) - 0x400) * 0.0009765625f;
    float left = static_cast<float>(static_cast<int32_t>(rect.mLeft) - 0x400) * 0.0009765625f;
    float depth = CalcZTrans(z);
    if (gRidePreview != nullptr) DestroyRidePreview(gRidePreview);
    gRidePreview = CreateRidePreview(left, bottom, depth, right - left, top - bottom, id);
}

// 0x10181648
void UIMain_ShowSpritePreview(CoolRect& rect, CPeep* peep, int32_t z, float scale) {
    if (gSpritePreviewPeep != nullptr) UIMain_DestroySpritePreview();
    gSpritePreviewPeep = peep;
    int32_t script = *reinterpret_cast<int32_t*>(reinterpret_cast<char*>(peep) + 0xc);   // the peep's script handle
    if (script == 0) {
        gSpritePreviewPeep = nullptr;
        return;
    }
    float scaled = scale * 0.25f;
    int32_t offset;   // fctiwz
    if (scaled != scaled) offset = INT32_MIN;
    else if (scaled >= 2147483648.0f) offset = INT32_MAX;
    else if (scaled < -2147483648.0f) offset = INT32_MIN;
    else offset = static_cast<int32_t>(scaled);
    int32_t left = rect.mLeft, top = rect.mTop, right = rect.mRight, bottom = rect.mBottom;
    int32_t halfWidth = (right - left) >> 1;
    int32_t quarterHeight = (bottom - top) >> 2;
    int32_t centre = left + halfWidth;
    int32_t y = 0x300 - ((bottom - quarterHeight) - offset);
    float x = static_cast<float>(centre - 0x3f8) * 0.0009765625f;
    float yy = static_cast<float>(y) / 768.0f;
    float depth = CalcZTrans(z);
    gSpritePreviewHandle = ESP_Spawn2DCopyOfScriptAt(script, x, yy, depth, scale);
}

// 0x101818e4
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t font, int32_t red, int32_t green, int32_t blue,
                      int32_t helpText) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(kColumnButtonDrawObject)));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    InterfaceFont* chosen = nullptr;
    if (static_cast<uint32_t>(font) < 13) chosen = &gText.mFonts[gText.mResolutionGroup * 13 + font];
    window->SetFont(chosen);
    window->SetFontColour(red, green, blue, 0xff);
    UI_RGBA colour;
    colour.mR = static_cast<uint8_t>(red);
    colour.mG = static_cast<uint8_t>(green);
    colour.mB = static_cast<uint8_t>(blue);
    colour.mA = 0xff;
    InterfaceButton* button = static_cast<InterfaceButton*>(window);
    button->SetTextUpColour(colour);
    button->SetTextDownColour(colour);
    window->SetClientDrawText(text);
    window->mHelpText = helpText;
}

// 0x10181b00
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id) {
    uint16_t index = id.mIndex;
    if (index != gZoomSkipThing) {
        GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kColumnButtonAssert);
        char* thing = static_cast<char*>(gThingArray[index].mThing);
        if (thing != nullptr) {
            uint16_t v = static_cast<uint16_t>(static_cast<uint8_t>(thing[4]) + static_cast<uint8_t>(thing[6]) * 0x80 + 1);
            float x = static_cast<float>(static_cast<int32_t>(((v - 1) & 0x7f) * 10));
            float z = static_cast<float>(static_cast<int32_t>(((static_cast<uint32_t>(v) - 1) >> 7) * 10));
            SetCameraPOI(&x, &z, nullptr, nullptr, nullptr);
        }
    }
}
