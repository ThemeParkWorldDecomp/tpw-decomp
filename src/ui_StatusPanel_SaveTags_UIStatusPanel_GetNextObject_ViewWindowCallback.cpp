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

namespace {
// The virtual call: table pointer at +0x20, each entry the arena offset of a block whose first bytes are the function.
void CallPanelSlot(void* panel, uint32_t slot) {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(panel);
    Ptr32<void (*)(void*)> entry;
    entry.mOffset = fields->mVtable.get()[slot / 4];
    (*entry.get())(panel);
}
}  // namespace

// 0x10193bac (140 bytes): message 0x10006 zooms to the thing and calls slot 0x34 of the panel in extra parameter 6.
int32_t ViewWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10006) {
        void* panel = window->getExtraParam(6);
        tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = gStaffRoomPanelThing->GetThingId();
        UIMain_ZoomToThing(id);
        CallPanelSlot(panel, 0x34);
        return 0;
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x10193e7c (240 bytes): 0x10001 white font, 0x10002 the title colour, 0x10006 with a == 0 slot 0x40 of the panel in
// extra parameter 0; then the default handler answers.
int32_t StatusPanelTitleMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10002) {
        UI_TITLE_COLOUR(window);
    } else if (message < 0x10002) {
        if (message > 0x10000) window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    } else if (message == 0x10006 && a == 0) {
        void* panel = window->getExtraParam(0);
        CallPanelSlot(panel, 0x40);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
