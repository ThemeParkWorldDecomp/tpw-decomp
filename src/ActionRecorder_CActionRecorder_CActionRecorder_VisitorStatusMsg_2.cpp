// Unit v5929: the visitor status panel (ui_visitorStatus.cpp): Show, InitWindow, UpdateWindow, the window handler VisitorStatusMsg and the
// file's static initialiser. The panel has the UIStatusPanelFields layout (Mac offsets), read through that struct.
#include <cstdint>
#include <new>

#include "ActionRecorder.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "LbFile.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "Serialise.h"
#include "TPSound.h"
#include "TbITable.h"
#include "Text.h"
#include "bfl_FileSystem.h"
#include "platform/arena.h"
#include "ui_main.h"
#include "WindowScripts.inc"

CThing* gVisitorStatusThing = nullptr;               // 0x1037ec10
int16_t gVisitorStatusScript[] = {TPW_WINDOW_SCRIPT_102A4758};   // 0x102a4758: the image's words (generated, devtools/gen_game_tables.py)
int32_t gVisitorStatusWordEC3C = 0;                  // 0x1037ec3c
int32_t gVisitorStatusWordEC40 = 0;                  // 0x1037ec40
uint32_t* gVisitorStatusTable = nullptr;             // 0x102a4b5c

namespace {

UIStatusPanelFields* Fields(VisitorStatusPanel* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The virtual call through the table at +0x20 (each entry an arena offset of a block holding the function), as __ptr_glue does.
void CallSlot(VisitorStatusPanel* panel, uint32_t slot) {
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = Fields(panel)->mVtable.get()[slot / 4];
    (*entry.get())(panel);
}

}  // namespace

// 0x101433f8 (112 bytes)
void VisitorStatusPanel::Show(CThing* thing) {
    gVisitorStatusThing = thing;
    reinterpret_cast<UIStatusPanel*>(this)->OpenWindow(gVisitorStatusScript, VisitorStatusMsg, 0x3722, 0x3723, 0);
    SOUND_SendEvent(0, gSoundBanks[1], 0x81, 0, 0, 0);
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x12e));
}

// 0x10142f44 (264 bytes)
void VisitorStatusPanel::InitWindow() {
    UIStatusPanelFields* fields = Fields(this);
    reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gVisitorStatusThing);
    InterfaceWindow* window = fields->mWindow->GetWindow(0x3734);
    void* object = lender_callee::OperatorNew(0x19c);
    if (object != nullptr) lender_callee::SpriteCtor(object);
    window->SetClientDrawObject(static_cast<InterfaceDrawObject*>(object));
    window->SetFontJust(1, 1);
    window->SetFontWrap(1, 1);
    window->SetFont(GetFontTableEntry(6));
    window->SetFontColour(0, 0, 0, 0xff);
    window->SetClientDrawText(nullptr);
}

// 0x101430c0 (276 bytes)
void VisitorStatusPanel::UpdateWindow() {
    int32_t* words = reinterpret_cast<int32_t*>(this);
    if (gStaffRoomPanelThing == nullptr) {
        CallSlot(this, 0x34);
        return;
    }
    CallSlot(this, 0x20);
    CallSlot(this, 0x1c);
    // The thing is read again after the calls (the global may have changed); its word at the Mac's +0x1d8 is kept at the panel's +0x24.
    words[0x24 / 4] = *reinterpret_cast<const int32_t*>(reinterpret_cast<const uint8_t*>(gStaffRoomPanelThing) + 0x1d8);
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    if (words[0x24 / 4] == 0) {
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
    } else {
        ShowStatusTag tag(0x18, words[0x24 / 4]);
        table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
    }
    UIMain_SetTitleText(Fields(this)->mWindow.get(), 0x3736, gUIMainConvertBuffer);
}

// 0x1014320c (432 bytes)
int32_t VisitorStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    VisitorStatusPanel* panel = &gVisitorStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x12e));
    } else if (message == 0x1e) {
        UIMain_UpdateSpritePreview();
    } else if (message == 0x100) {
        if (a == 0x3721) {
            base->GetNextObject();
        } else if (a < 0x3721) {
            if (a == -2) {
                panel->Remove();
                return 0;
            }
            if (a >= 0x3720) base->GetPreviousObject();
        } else {
            if (a == 0x3738) {
                CreateAllKidsPanel(gStaffRoomPanelThing);
                return 0;
            }
            if (a == 0x3737) {
                panel->Locate();
                return 0;
            }
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}

// 0x101435c4 (100 bytes)
void __sinit_ui_visitorStatus_cpp() {
    gVisitorStatusWordEC40 = 0;
    gVisitorStatusWordEC3C = 0x10000;
    UIStatusPanel_Construct(reinterpret_cast<UIStatusPanel*>(&gVisitorStatusPanel));
    // The Mac stores the address of its table of virtual slots (0x102a4b5c); the host's is an arena block (src/ui_StatusPanel_tables.cpp).
    if (gVisitorStatusTable == nullptr) gVisitorStatusTable = VisitorStatusPanelTable();
    UIStatusPanelFields* fields = Fields(&gVisitorStatusPanel);
    fields->mVtable.set(gVisitorStatusTable);
    fields->mMode = -2;
}
