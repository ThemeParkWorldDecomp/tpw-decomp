// OnlineEditMsg 0x101e71a8 (1128 bytes) and OnlinePanelSubPanelMsg 0x101e70ec (120 bytes): the message functions of the online
// panel's edit box and of its sub panel.
#include <cstdint>
#include <cstddef>
#include <new>
#include <string>

#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "lobby.h"
#include "online.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "Mouse.h"

// The calls through __ptr_glue and the allocator, weak so the test library can fake them.
namespace online_edit_callee {
__attribute__((weak)) void* New(size_t size) { return ::operator new(size); }
// the virtual at slot 0x2c of the table at window+0x118
__attribute__((weak)) void WindowSetText(InterfaceWindow* window, char16_t* text) { window->SetText(text); }
// the virtual at slot 0x2c of the mouse mode's table
__attribute__((weak)) int32_t MouseModeOf(CMouseMode* mode) { return mode->GetMode(); }
}  // namespace online_edit_callee

namespace {

// gMouse is made on first use (the same code is inlined five times in the original).
void EnsureMouse() {
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(online_edit_callee::New(1));
        if (mouse != nullptr) {
            void* raw = online_edit_callee::New(8);
            CMMDefault* mode = nullptr;
            if (raw != nullptr) mode = new (raw) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
}

int32_t CurrentMouseMode() {
    if (gMouseMode == nullptr) return 0;
    return online_edit_callee::MouseModeOf(gMouseMode);
}

}  // namespace

// 0x101e71a8
int32_t OnlineEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x20) {
        UIMain_EnableAllKeyTables();
    } else if (message < 0x20) {
        if (message >= 0x1f) UIMain_DisableAllKeyTables();
    } else if (message == 0x1000b) {
        uint32_t modifiers = (static_cast<uint32_t>(b) >> 16) & 0xff;
        if (a == 0xd) {
            if (modifiers != 0) return window->CallDefaultWindowHandler(message, a, b);
            std::u16string text(gOnlineEditText);
            if (gOnlineControl == nullptr) {
                void* raw = online_edit_callee::New(0xe8c);
                COnlineControl* control = nullptr;
                if (raw != nullptr) control = new (raw) COnlineControl();
                gOnlineControl = control;
            }
            gOnlineControl->SendText(text);
            online_edit_callee::WindowSetText(window, gOnlineEmptyHelp);
            return 1;
        }
        if (a < 0xd && a == 9) {
            window->UnCaptureKb(0);
            gOnlinePanel->CaptureKb(0);
            return 1;
        }
        if (a == 0x73) {
            if (modifiers == 0 && gOnlineBlockToggle == 0) {
                uint8_t shown = gOnlinePanelShown;
                UI_SendMessage(gOnlinePanel, 6, shown == 0 ? 1 : 0, 0);
                UI_SendMessage(gOnlineWindow, 6, shown, 0);
                if (gOnlinePanelShown != 0) {
                    InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
                    w = w->GetWindow(0xc312b);
                    w = w->GetWindow(0xc312c);
                    w->CaptureKb(0);
                    EnsureMouse();
                    if (CurrentMouseMode() == 9) gOnlinePanel->setCursor(0x13);
                } else {
                    gOnlineWindow->CaptureKb(0);
                }
            }
            return window->CallDefaultWindowHandler(message, a, b);
        }
        if (static_cast<int16_t>(gKeyTableShortcuts->Search(static_cast<int16_t>(a), static_cast<int16_t>(modifiers))) == 0) {
            window->UnCaptureKb(0);
            gOnlinePanel->CaptureKb(0);
            EnsureMouse();
            bool handled;
            if (CurrentMouseMode() == 9) {
                void* raw = online_edit_callee::New(8);
                CMMOnline* online = nullptr;
                if (raw != nullptr) online = new (raw) CMMOnline();
                EnsureMouse();
                gMouse->AssignMouseMode(online);
                gOnlineWindow->setCursor(0);
                handled = true;
            } else {
                handled = false;
            }
            if (!handled) CreateMainMenu(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e70ec
int32_t OnlinePanelSubPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x402) ContactMenu(b);
    return window->CallDefaultWindowHandler(message, a, b);
}
