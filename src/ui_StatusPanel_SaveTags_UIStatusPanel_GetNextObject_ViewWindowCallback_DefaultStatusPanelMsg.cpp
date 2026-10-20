// Unit v5910: 0x10193af4 UIStatusPanel::DisplayStatusText, 0x10193c78 DefaultStatusPanelMsg, 0x10193dcc UIStatusPanel::InitEditBox,
// 0x10193fac the edit-name panel's handler (TwoEditsMsg on the Mac, TwoEditsMsgStatus here: the park-name handler already has the plain name).
#include <cstdint>
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "Mouse.h"
#include "ThingReceivers.h"
#include "TPSettings.h"
#include "ui_ErrorDisplay.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "MainWindowCallback.h"
#include "analyser.h"

// 0x1024de12: an empty string (the word there starts 0x0000).
char16_t gStatusPanelEmptyText[1] = {0};

namespace {

UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// __ptr_glue call of the panel's second table (the pointer at +0x20), slot at byte offset `slot`.
void CallStatusSlot(void* panel, uint32_t slot) {
    Ptr32<uint32_t> entry;
    entry.mOffset = Fields(panel)->mVtable.get()[slot / 4];
    typedef void (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(panel);
}

}  // namespace

// 0x10193af4 (128 bytes)
void UIStatusPanel::DisplayStatusText() {
    UIStatusPanelFields* f = Fields(this);
    if (gStaffRoomPanelThing != nullptr) {
        InterfaceWindow* window = f->mF18.get();
        if (window == nullptr) return;
        if (f->mF04 == 0) window->Show(0);
        else window->Show(1);
        UIMain_SetObjectStateText(f->mF18.get(), f->mF04);
    }
}

// 0x10193c78 (276 bytes)
int32_t DefaultStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    void* panel = window->getExtraParam(6);
    if (message == 0x1000a) {
        if (a == 0x2800) static_cast<UIStatusPanel*>(panel)->GetNextObject();
        else if (a == 0x2600) static_cast<UIStatusPanel*>(panel)->GetPreviousObject();
    } else if (message == 0x14) {
        if (gStaffRoomPanelThing != nullptr) CallStatusSlot(panel, 0x44);
        CallStatusSlot(panel, 0x24);
        CallStatusSlot(panel, 0x28);
        CallStatusSlot(panel, 0x10);
        Fields(panel)->mWindow.mOffset = 0;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x10193dcc (124 bytes)
void UIStatusPanel::InitEditBox(int32_t id) {
    UIStatusPanelFields* f = Fields(this);
    InterfaceWindow* edit = f->mWindow.get()->GetWindow(id);
    Ptr32<InterfaceWindow> stored;
    stored.set(edit);
    f->mF1C = stored.mOffset;
    edit->SetCallbackFunction(StatusPanelTitleMsg);
    edit->setExtraParam(0, this);
    UIMain_SetTitleText(f->mWindow.get(), id, gStatusPanelEmptyText);
}

// 0x10193fac (548 bytes)
int32_t TwoEditsMsgStatus(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x802) {
        if (a == 0x3943c) {
            window->GetWindow(0x3943d)->CaptureKb(0);
        } else {
            UI_PostMessage(window, 0x100, -1, 0);
        }
    } else if (message < 0x802) {
        if (message == 0x100) {
            if (a == -1) {
                CObject* object = static_cast<CObject*>(window->getExtraParam(0));
                object->SetSignNameA(gEditSignBufA);
                object->SetSignNameB(gEditSignBufB);
                RegenerateRideSign(object, gEditSignBufA, gEditSignBufB);
                CallStatusSlot(window->getExtraParam(1), 0xc);
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
            if (a == -2) {
                UI_SendMessage(window, 4, 0, 0);
                return 0;
            }
        } else if (message < 0x100 && message == 0x14) {
            UIMain_EnableAllKeyTables();
            static_cast<UIStatusPanel*>(window->getExtraParam(1))->EditNamePanelGone();
            UIMain_DecideKbCapture();
        }
    } else {
        if (message == 0x804) {
            UI_SendMessage(window, 4, 0, 0);
            return 0;
        }
        if (message < 0x804) {
            if (a == 0x3943c) window->GetWindow(0x3943d)->CaptureKb(0);
            else window->GetWindow(0x3943c)->CaptureKb(0);
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
