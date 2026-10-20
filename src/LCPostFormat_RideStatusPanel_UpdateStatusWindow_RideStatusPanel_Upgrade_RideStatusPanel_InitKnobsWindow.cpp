// Unit v51011: RideStatusPanel::InitWindow 0x101ba99c, UpdateWindow 0x101baaac, InitKnobsWindow 0x101bab6c, UpdateKnobsTitles 0x101baf28.
#include <cstdint>
#include "History.h"
#include "InterfaceWindow.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "RideDB.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ppc_math.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "InterfaceScrollBar.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "TPEngine.h"
#include "World.h"

// The Mac's string table at 0x1024e460: "hilight" at +0x10, "smallguage.wct" at +0x18 (the gradients' mesh).
const char gRideStatusMeshNames[0x28] = {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
                                         'h', 'i', 'l', 'i', 'g', 'h', 't', 0,   's', 'm', 'a', 'l', 'l', 'g', 'u', 'a',
                                         'g', 'e', '.', 'w', 'c', 't', 0,   0};

// 0x101ba99c: the object list (the two arrows go when there is less than a second object), the knobs, the window, the edit box.
void RideStatusPanel::InitWindow() {
    int32_t count = reinterpret_cast<UIStatusPanel*>(this)->FillObjectList(gRideStatusShowThing);
    if (count < 2) {
        mWindow->GetWindow(0x3e26)->Show(0);
        mWindow->GetWindow(0x3e27)->Show(0);
    }
    InitKnobsWindow();
    mWindow->Show(1);
    reinterpret_cast<UIStatusPanel*>(this)->InitEditBox(0x3e28);
}

// 0x101baaac: the name, the two ops-table calls (slots at +0x20 and +0x1c, the panel as argument), the knobs, the buttons.
void RideStatusPanel::UpdateWindow() {
    parkstatus_callee::WindowSetText(mNameWindow, UIMain_GetObjectName(reinterpret_cast<CObject*>(gStaffRoomPanelThing)));
    mOps->Slot6();
    mOps->Slot5();
    UpdateKnobsWindow();
    UpdateWhichButtons();
}

// 0x101bab6c: the knobs' gradients and texts. The stack CoolRect is built and destroyed whether or not there is a window.
void RideStatusPanel::InitKnobsWindow() {
    alignas(8) uint8_t rect[0x20];
    kidlist_callee::RectCtor(rect);
    if (mWindow != nullptr) {
        const char* mesh = gRideStatusMeshNames + 0x18;
        // the capacity bar (0x3e2d) and the speed bar (0x3e30): range 0 to 0x78, a gradient on the marker (0x3e2e) that starts at 0
        for (int32_t id : {0x3e2d, 0x3e30}) {
            InterfaceWindow* bar = mWindow->GetWindow(id);
            static_cast<InterfaceScrollBar*>(bar)->SetMinMax(0, 0x78);
            void* gradient = parkstatus_callee::OperatorNew(0x124);
            if (gradient != nullptr) parkstatus_callee::GradientCtor(gradient, mesh);
            bar->GetWindow(0x3e2e)->SetClientDrawObject(static_cast<InterfaceDrawObject*>(gradient));
            parkstatus_callee::WindowSetValue(bar->GetWindow(0x3e2e), 0);
        }
        // the duration bar
        static_cast<InterfaceScrollBar*>(mWindow->GetWindow(0x3e2f))->SetMinMax(0, 10);
        // the titles under the knobs
        for (int32_t id : {0x3e31, 0x3e33, 0x3e32}) {
            InterfaceWindow* title = mWindow->GetWindow(id);
            void* sprite = parkstatus_callee::OperatorNew(0x19c);
            if (sprite != nullptr) parkstatus_callee::SpriteCtor(sprite);
            title->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
            title->SetFontJust(1, 1);
            title->SetFont(GetFontTableEntry(6));
            title->SetFontColour(0, 0, 0, 0xff);
            title->SetClientDrawText(nullptr);
        }
    }
    kidlist_callee::RectDtor(rect);
}

// 0x101baf28: bit 1 the speed title (0x3e33), bit 2 the capacity title (0x3e31), bit 4 the duration title (0x3e32). Each formats a string
// of the text table (gText + 0x564) into the shared buffer with a one-number tag (the value without its sign, a flag for negative).
void RideStatusPanel::UpdateKnobsTitles(int32_t which) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[6]);
    char16_t* text = gUIMainConvertBuffer;
    auto format = [&](uint32_t string, uint32_t tagId, int32_t value) {
        int32_t negative = value < 0 ? 1 : 0;
        RideKnobTag tag(tagId, negative ? -value : value, negative);
        table->FormatString(text, 0x3fff, string, &tag);
    };
    if (which & 1) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e33);
        format(0x1aa, 5, mSpeed);
        window->SetClientDrawText(text);
    }
    if (which & 2) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e31);
        if (object->GetDBInfo()->mBalance.mValues[36].mInteger == 3) {
            format(0x1cf, 6, mCapacity);
        } else if (object->GetDBInfo()->mBalance.mValues[36].mInteger == 2) {
            format(0x1cd, 6, mCapacity);
        } else {
            format(0x1ab, 6, mCapacity);
        }
        window->SetClientDrawText(text);
    }
    if (which & 4) {
        InterfaceWindow* window = mWindow->GetWindow(0x3e32);
        int32_t unit = object->GetDBInfo()->mBalance.mValues[25].mInteger;
        if (unit == 2) {
            window->Show(1);
            format(0x1ae, 7, mDuration);
        } else if (unit < 2) {
            if (unit == 0) {
                window->Show(0);
            } else if (unit >= 0) {
                window->Show(1);
                format(mDuration == 1 ? 0x1ac : 0x1ad, 7, mDuration);
            }
        } else if (unit == 4) {
            window->Show(1);
            format(0x1b0, 7, mDuration);
        } else if (unit < 4) {
            window->Show(1);
            format(0x1af, 7, mDuration);
        }
        window->SetClientDrawText(text);
    }
}
