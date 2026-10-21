// ui_progressBar.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "FS_Online.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "UI_pump.h"

// Unit v51111: small members of the interface controls and panels (main app).
//   InterfaceKnob::GetValue 0x101d0354, SetValue 0x101d02e8, ~InterfaceKnob 0x101d0194, InterfaceImage::GetValue 0x101d0054,
//   InterfaceLightPanel::~ 0x101d097c and GetValue 0x101d0b5c, UI_HelpTextEnabled 0x101cab08, InterfaceBar::GetValue 0x101c8968,
//   InterfaceTextBox::DrawFunction 0x101d80f8, InterfaceVaryBox::~ 0x101d8d74, SetInc 0x101d8ee8, SetValue 0x101d8f2c,
//   GetValue 0x101d8f84, operator- 0x101d8fc0, RemoveYearEndPanel 0x101c73e4, UpgradeStatusPanel::Remove 0x101c66e0,
//   UpdateWindow 0x101c6304, DeInitWindow 0x101c62c8, DeInitStatusWindow 0x101c6090, InterfaceWindow::CaptureKeys 0x101dce4c,
//   stopAnimTimer 0x101dd754, ProgBarMsg 0x101dff64.

// ---- the progress bar's message function: the destroy message (0x14) forgets the bar, then the default handler.
int32_t ProgBarMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x14) gUIProgressBar = nullptr;
    return window->CallDefaultWindowHandler(message, a, b);
}

// gUIProgressBar (0x10395c6c) is defined by src/ui_progress_cursor.cpp
int32_t gUIProgressBarText;        // 0x10395c68

// 0x101dffd8 (492 bytes): without a bar, builds the window from the script and gives it the gradient, a centred text sprite in the child
// 0x2241 (font 6, black), the text, depth 0x23; the bar is then disabled (virtual 0x1c) after its value is set to 0 (virtual 0x24).
// With a bar, only the text of the child changes (virtual 0x2c).
void CreateUIProgressBar(char16_t* text) {
    if (gUIProgressBar == nullptr) {
        gUIProgressBar = gUIRootWindow->Build(gProgBarScript, ProgBarMsg);
        InterfaceDrawEngineHGradient* gradient = new InterfaceDrawEngineHGradient(gProgBarMeshName);
        gUIProgressBar->SetClientDrawObject(gradient);
        gUIProgressBar->SetValue(0);
        InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
        gUIProgressBar->GetWindow(0x2241)->SetClientDrawObject(sprite);
        gUIProgressBar->GetWindow(0x2241)->SetFontJust(1, 1);
        gUIProgressBar->GetWindow(0x2241)->SetFontWrap(1, 1);
        InterfaceFont* font = GetFontTableEntry(6);
        gUIProgressBar->GetWindow(0x2241)->SetFont(font);
        gUIProgressBar->GetWindow(0x2241)->SetFontColour(0, 0, 0, 0xff);
        gUIProgressBar->GetWindow(0x2241)->SetClientDrawText(text);
        gUIProgressBar->setZ(0x23);
        gUIProgressBar->Enable(0);
    } else {
        gUIProgressBar->GetWindow(0x2241)->SetText(text);
    }
}

// 0x101e01f0 (120 bytes): shows the bar with interface string `text`; an existing bar is only changed when the id differs. Remembers the id.
void CreateUIProgressBar(int32_t text) {
    if (gUIProgressBar == nullptr) {
        CreateUIProgressBar(const_cast<char16_t*>(UIMain_GetString(text)));
    } else if (text != gUIProgressBarText) {
        CreateUIProgressBar(const_cast<char16_t*>(UIMain_GetString(text)));
    }
    gUIProgressBarText = text;
}

// Two small leaves of the UI (main app, SimTheme Park 2.04b3; unit w75; the inventory files both under ui_cityLobbyController.cpp):
//   0x101cc8ec CursorLibrary::RemoveAll (120)
//   0x101e0294 FuckOffUIProgressBar (64)

InterfaceWindow* gUIProgressBar;       // 0x10395c6c

// 0x101e0294. Tells the progress bar's window message 4 (close), when there is one. The global is left as it was.
void FuckOffUIProgressBar() {
    if (gUIProgressBar != nullptr) UI_SendMessage(gUIProgressBar, 4, 0, 0);
}

// 0x101e0300 (92 bytes): sets the bar's value (virtual 0x24) to percent * 1024 / 100 (the compiler's multiply-high form, rounding toward zero).
void UpdateUIProgressBar(int32_t percent) {
    InterfaceWindow* bar = gUIProgressBar;
    if (bar != nullptr) {
        int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(percent) << 10);
        int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51EB851F) * scaled) >> 32);
        high >>= 5;
        high += static_cast<int32_t>(static_cast<uint32_t>(high) >> 31);
        bar->SetValue(high);
    }
}
