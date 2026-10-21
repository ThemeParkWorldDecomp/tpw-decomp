// Unit v51004: AllStaffPanelMsg 0x10198ac4 (596), UIStatusPanel::RequestMaintenance 0x101932a0 (88), FinancePanelPanelMsg 0x1019f434 (368).
#include <cstdint>
#include "AdvisorScoring.h"
#include "Debug.h"
#include "FinanceGraphPanelMsg.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Ptr32.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "platform/arena.h"
#include "ui_controls.h"
#include "ui_iterators.h"
#include "ui_main.h"

namespace {

// The list's data word: the staff member's address on the Mac, its arena offset here (FillAllStaffListCallback's PointerData; unit
// v5L31: the raw word read as an address sent StaffStatusPanel::Show a pointer to nothing).
CThing* DataThing(int32_t data) {
    return data != 0 ? reinterpret_cast<CThing*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr;
}

}  // namespace

// 0x10198ac4: the all-staff panel's messages. Bug kept: message 0x402 asks the thing id of the list's data pointer.
int32_t AllStaffPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x100:
        switch (a) {
        case -2: RemoveAllStaffPanel(); return 0;
        case 0x32e: CreateParkStatusPanel(); return 0;
        case 0x32f: CreateAllKidsPanel(nullptr); return 0;
        case 0x330: CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); return 0;
        default: break;
        }
        break;
    case 5:
        RemoveAllStaffPanel();
        return 1;
    case 0x10:
        if (static_cast<uint32_t>(a) - 0x80000u == 0x83u) UpdateListEntries();
        break;
    case 0x14:
        gStaffPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11d));
        break;
    case 0x15:
        UIMain_SetTitleText(window, 0x328, const_cast<char16_t*>(UIMain_GetString(100)));
        break;
    case 0x400: {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            gStaffStatusPanel.Show(DataThing(data));
        }
        return 0;
    }
    case 0x402: {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            CThing* thing = DataThing(data);
            tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id = thing->GetThingId();
            UIMain_ZoomToThing(id);
            RemoveAllStaffPanel();
            return 0;
        }
        break;
    }
    case 0x406:
        gStaffSort = b;
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// 0x101932a0: flags the panel's object for maintenance (on) or takes the flag off, then the panel's table slot at Mac +0x1c.
void UIStatusPanel::RequestMaintenance(bool on) {
    reinterpret_cast<CObject*>(gStaffRoomPanelThing)->RequestMaintenance(on ? 0 : 1);
    UIStatusPanel_CallSlot(this, 0x1c);
}

// 0x1019f434: the finance panel's messages.
int32_t FinancePanelPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    switch (message) {
    case 0x15:
        UIMain_SetTitleText(window, 0x12271, const_cast<char16_t*>(UIMain_GetString(0xa1)));
        break;
    case 5:
        RemoveFinancePanelPanel();
        return 1;
    case 0x14:
        gFinanceGraphPanel = nullptr;
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xc5));
        break;
    case 0x100:
        if (a == 0x1227f) { CreateLoansPanel(); return 0; }
        if (a == 0x12281) { CreateEntryPricePanel(); return 0; }
        if (a == 0x12280) { CreateStaffCostsPanel(); return 0; }
        if (a < 0 && a >= -2) { RemoveFinancePanelPanel(); return 0; }
        break;
    default:
        break;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}
