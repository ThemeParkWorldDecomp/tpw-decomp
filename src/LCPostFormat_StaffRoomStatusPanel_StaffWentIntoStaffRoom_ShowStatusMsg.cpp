#include <cstdint>
#include "InterfaceWindow.h"
#include "Ptr32.h"
#include "ThingReceivers.h"
#include "analyser.h"
#include "ppc_math.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_iterators.h"

namespace {

// The host's virtual call on a Mac-layout panel: the table at +0x20 holds Ptr32 words, each the arena offset of a
// block whose first 8 bytes are a uint64_t (*)(void*).
void CallSlot(UIStatusPanelFields* f, uint32_t slot, void* self) {
    Ptr32<uint32_t> entry;
    entry.mOffset = f->mVtable.get()[slot / 4];
    typedef uint64_t (*Fn)(void*);
    (*reinterpret_cast<Fn*>(entry.get()))(self);
}

}  // namespace

// 0x101c2708: the show-status panel's message handler (the 0x100 command ids are 0xa089 to 0xa0a9).
int32_t ShowStatusMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ShowStatusPanel* panel = &gShowStatusPanel;
    UIStatusPanel* base = reinterpret_cast<UIStatusPanel*>(panel);
    UIStatusPanelFields* f = reinterpret_cast<UIStatusPanelFields*>(panel);
    int32_t* words = reinterpret_cast<int32_t*>(panel);
    if (message == 0x15) {
        if (window->setAnimTimer(0x80080, 4000) != 0) window->startAnimTimer(0x80080);
    } else if (message == 0x10) {
        if (a == 0x80080) panel->UpdateStatusWindow();
    } else if (message == 5) {
        panel->Remove();
        return 1;
    } else if (message == 0x14) {
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xd0));
    } else if (message == 0x1e) {
        UIMain_TickRidePreview();
    } else if (message == 0x100) {
        if (a == 0xa08c) {
            base->GetPreviousObject();
        } else if (a < 0xa08c) {
            if (a == 0xa089) {
                CreateAllItemsPanel(kUIObjSideshow, gStaffRoomPanelThing);
                return 0;
            }
            if (a < 0xa089) {
                if (a < 0 && a >= -2) {
                    panel->Remove();
                    return 0;
                }
            } else if (a >= 0xa08b) {
                base->EraseObject();
                return 0;
            } else {
                base->CloseObject(b == 1 ? 1 : 0);
                panel->UpdateStatusWindow();
            }
        } else {
            if (a == 0xa0a8) {
                base->Move();
                return 0;
            }
            if (a < 0xa0a8) {
                if (a < 0xa08e) base->GetNextObject();
            } else if (a < 0xa0aa) {
                UI_IterateThingList(kUIIterSideshows, _applytoall);
                CallSlot(f, 0x1c, panel);
                return 0;
            }
        }
    } else if (message == 0x800) {
        if (a == 0xa08f) {
            words[0x2c / 4] = b;
            CallSlot(f, 0x1c, panel);
        } else if (a < 0xa08f) {
            if (a >= 0xa08e) {
                words[0x24 / 4] = b;
                CallSlot(f, 0x1c, panel);
            }
        } else if (a < 0xa091) {
            words[0x28 / 4] = b;
            CallSlot(f, 0x1c, panel);
        }
    }
    return DefaultStatusPanelMsg(window, message, a, b);
}
