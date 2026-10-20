#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "FS_Online.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

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
