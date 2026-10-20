// The all-staff panel's row maker AllStaffLCFormat (LCFormat 0x101991e8), its attach function AllStaffLCPostFormat (LCPostFormat
// 0x10199370) and the panel's button handler AllStaffPanelLCHandler 0x10198d54.
#include <cstdint>

#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_main.h"

int32_t gStaffListFormat;           // 0x103947e4
uint32_t gStaffListFont = 7;        // 0x102a9080

namespace {
// 0x1024df23 and 0x1024df33: the two gradient file names, 16 bytes apart.
const char kGradientNames[] = "ridestatbar.wct\0happygrad.wct";
}  // namespace

// 0x101991e8 (360 bytes): the row window of the list, 4 controls; the row height (kept in a global) is the font's height scaled by
// 0x600 / the real screen height, plus 6.
InterfaceWindow* AllStaffLCFormat() {
    if (gStaffListFormat == 0) {
        InterfaceFont* font = GetFontTableEntry(gStaffListFont);
        int32_t height = static_cast<int16_t>(font->GetHeight());
        gStaffListFormat = height * 0x600 / gUIRealScreenHeight + 6;
    }
    InterfaceWindow* window = new InterfaceWindow(nullptr, 0, 0, 0, 0, 0, static_cast<int16_t>(gStaffListFormat));
    window->CreateObject(1, 1, 999, 0, 0, 0, 0);
    window->CreateObject(1, 1, 999, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 4, 0, 0, 0, 0);
    window->CreateObject(9, 0x11, 5, 0, 0, 0, 0);
    return window;
}

// 0x10199370 (652 bytes): gives the row window and its four children their draw objects (two text sprites, a cash text, two gradients).
// The slot +0x24 of the window's vtable is SetValue; the CoolRect is made and destroyed without use.
void AllStaffLCPostFormat(InterfaceWindow* window) {
    CoolRect unused;
    window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    window->SetFont(GetFontTableEntry(gStaffListFont));
    window->SetClientDrawText(nullptr);

    InterfaceWindow* child = window->GetChild(0);
    child->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
    child->SetFont(GetFontTableEntry(gStaffListFont));
    child->SetClientDrawText(nullptr);

    child = child->GetNext();
    child->SetClientDrawObject(new InterfaceCashTextObject(nullptr));
    child->SetFontJust(2, 1);
    child->SetFont(GetFontTableEntry(gStaffListFont));
    child->SetValue(0);

    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    child->SetClientDrawObject(new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientNames)));
    child->SetValue(500);

    child = child->GetNext();
    UIMain_IndentClient(child, 0, 8);
    child->SetClientDrawObject(new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientNames + 16)));
}

namespace {
// The mulhw by 0x51eb851f, srawi 5 and the sign add of a signed divide by 100 of (happiness << 10).
int32_t HappinessShown(int32_t happiness) {
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(happiness) << 10);
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * scaled) >> 32);
    int32_t q = high >> 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

// One button of the panel: fills the list, sets the title (slot +0x2c of the window's vtable is SetText) and the happiness bar (+0x24, SetValue).
void ShowStaffKind(int32_t type, int32_t stringId, int32_t model) {
    FillAllStaffList(static_cast<StaffType>(type), 1);
    InterfaceWindow* title = gStaffPanel->GetWindow(0x329)->GetWindow(0x32a);
    title->SetText(const_cast<char16_t*>(UIMain_GetString(stringId)));
    InterfaceWindow* bar = gStaffPanel->GetWindow(0x329)->GetWindow(0x32d);
    int32_t happiness = gGameWorld->GetParkAnalyser()->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
    bar->SetValue(HappinessShown(happiness));
}
}  // namespace

// 0x10198d54 (1,104 bytes): a click (message 0x101, b == 1) on one of the five staff buttons 0x323 to 0x327; any other message falls to the default handler.
int32_t AllStaffPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101 && b == 1) {
        switch (a) {
            case 0x323: ShowStaffKind(0, 0x6b, 5); break;
            case 0x324: ShowStaffKind(4, 0x6f, 8); break;
            case 0x325: ShowStaffKind(2, 0x6e, 6); break;
            case 0x326: ShowStaffKind(1, 0x6c, 4); break;
            case 0x327: ShowStaffKind(3, 0x6d, 7); break;
            default: break;
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
