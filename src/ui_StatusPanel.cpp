// ui_StatusPanel.cpp (main): InitTagPanel 0x10190350 (272 B), DeInitTagPanel 0x10190484 (76 B),
// CreateStaticTags 0x10191b28 (240 B), DestroyStaticTags 0x10191c40 (140 B), InstantRemoveAllTags 0x10192280 (112 B).
#include <new>
#include "ui_main.h"

InterfaceWindow* gTagPanel;        // 0x1038cd78
InterfaceWindow* gTagPanelSource;  // 0x1038cd80
sStaticTag gStaticTags[10];        // 0x1038cd84, Mac stride 0x208

void InitTagPanel() {
    gTagPanel = gUIRootWindow->Build(gTagPanelScript, TagDlgCallback);
    gTagPanel->unlink();
    gTagPanel->setExtraParam(0, reinterpret_cast<void*>(1));
    gTagPanel->Show(0);
    InterfaceWindow* textWindow = gTagPanel->GetWindow(0x10d32);
    if (textWindow != nullptr) {
        InterfaceTextObjectSprite* sprite = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
        textWindow->SetClientDrawObject(sprite);
        InterfaceFont* font = GetFontTableEntry(6);
        textWindow->SetFont(font);
        textWindow->SetFontWrap(1, 1);
        textWindow->SetFontJust(0, 0);
    }
}

void DeInitTagPanel() {
    if (gTagPanel != nullptr) UI_SendMessage(gTagPanel, 4, 0, 0);
    gTagPanelSource = nullptr;
}

void CreateStaticTags() {
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        tag.mActive = 0;
        InterfaceWindow* window = new (std::nothrow) InterfaceWindow(nullptr, 0, i + 900, 0, 0, 70, 65);
        if (window != nullptr) {
            window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("msgtag")));
            window->SetCallbackFunction(TagCallback);
            window->mHelpText = 0x1db;
        }
        tag.mWindow = window;
        if (window != nullptr) tag.mWindow->setExtraParam(2, tag.mPayload);
    }
}

void DestroyStaticTags() {
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        if (tag.mActive == 0 && tag.mWindow != nullptr) {
            delete tag.mWindow;
            tag.mWindow = nullptr;
        }
    }
}

void InstantRemoveAllTags() {
    for (int32_t i = 0; i < 10; ++i) {
        if (gStaticTags[i].mActive != 0) UI_SendMessage(gStaticTags[i].mWindow, 5, 0, 0);
    }
}
