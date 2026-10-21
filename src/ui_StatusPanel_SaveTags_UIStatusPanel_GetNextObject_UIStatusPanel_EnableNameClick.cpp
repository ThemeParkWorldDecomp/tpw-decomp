// Unit v51114: the status panel's small members (EnableNameClick, EditNamePanelGone, GetPreviousObject, ObjectGone, CloseObject,
// ZoomTo, CloseWindow, GetName, the empty base versions and the constructor), the SamsUtilities byte swaps next to them and the
// rank text object's destructor.
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
#include "Ptr32.h"
#include "UI_Base.h"
#include "analyser.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include "ui_DrawObject.h"

namespace {

// The Mac layout of a status panel is read by offset (the class has no members).
uint8_t* Bytes(UIStatusPanel* panel) { return reinterpret_cast<uint8_t*>(panel); }

// A virtual call through the table at +0x20 on the Mac-layout object: the entry at `offset` of the table is the arena offset of a block
// whose first 8 bytes are the function (the host's way of a __ptr_glue call). r3 is the panel; r4 stays what it was (`thing` where the
// caller leaves it).
void CallSlot(UIStatusPanel* panel, uint32_t offset, CThing* thing = nullptr) {
    uint8_t* table = reinterpret_cast<Ptr32<uint8_t>*>(Bytes(panel) + 0x20)->get();
    uint32_t entry = *reinterpret_cast<uint32_t*>(table + offset);
    auto function = *reinterpret_cast<void (**)(UIStatusPanel*, CThing*)>(tpw::gArenaBase + entry);
    function(panel, thing);
}

// The iterator type of a panel mode, 0 for none (the compare tree of GetPreviousObject).
int32_t PreviousIteratorType(int32_t mode) {
    switch (mode) {
        case -2: return kUIIterVisitors;
        case -1: return kUIIterStaff;
        case 0: return kUIIterRides;
        case 1: return kUIIterShops;
        case 2: return kUIIterSideshows;
        case 3: return kUIIterPlainFeatures;
        case 0x3ea: return kUIIterBogs;
        case 0x3eb: return kUIIterStaffRooms;
        case 0x3ec: return kUIIterNonTrackUpgrades;
        default: return 0;
    }
}

}  // namespace

uint32_t* UIStatusPanelTable() {   // 0x102a8644 (name invented)
    static uint32_t* table = static_cast<uint32_t*>(tpw::ArenaAllocZeroed(32 * sizeof(uint32_t)));
    return table;
}

// 0x101929e8: the table, the thing cleared and the windows cleared.
void UIStatusPanel_Construct(UIStatusPanel* self) {
    uint8_t* bytes = Bytes(self);
    reinterpret_cast<Ptr32<uint32_t>*>(bytes + 0x20)->set(UIStatusPanelTable());
    gStaffRoomPanelThing = nullptr;
    *reinterpret_cast<uint32_t*>(bytes + 0xc) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x14) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x10) = 0;
    *reinterpret_cast<uint32_t*>(bytes + 0x18) = 0;
}

// 0x10194608: the panel's window (+0x1c) is enabled or disabled (its virtual at Mac vtable +0x1c; the argument is a halfword).
void UIStatusPanel::EnableNameClick(int32_t enable) {
    InterfaceWindow* window = reinterpret_cast<Ptr32<InterfaceWindow>*>(Bytes(this) + 0x1c)->get();
    window->Enable(static_cast<int16_t>(enable));
}

// 0x101945c4: forgets the edit-name window.
void UIStatusPanel::EditNamePanelGone() { *reinterpret_cast<uint32_t*>(Bytes(this) + 8) = 0; }

// 0x10194674 and its twins: nothing.
void UIStatusPanel::ApplyChanges() {}
void UIStatusPanel::ChangeName(char16_t*) {}
void UIStatusPanel::Remove() {}
void UIStatusPanel::Show(CThing*) {}
void UIStatusPanel::DeInitViewWindow() {}
void UIStatusPanel::DeInitStatusWindow() {}
void UIStatusPanel::UpdateViewWindow() {}
void UIStatusPanel::UpdateStatusWindow() {}
void UIStatusPanel::InitViewWindow() {}
void UIStatusPanel::InitStatusWindow() {}
void UIStatusPanel::DeInitWindow() {}
void UIStatusPanel::UpdateWindow() {}
void UIStatusPanel::InitWindow() {}

// 0x1019387c
char16_t* UIStatusPanel::GetName() { return UIMain_GetObjectName(reinterpret_cast<CObject*>(gStaffRoomPanelThing)); }

// 0x10193774: tells the panel's window (+0xc) message 4.
void UIStatusPanel::CloseWindow() {
    UI_SendMessage(reinterpret_cast<Ptr32<InterfaceWindow>*>(Bytes(this) + 0xc)->get(), 4, 0, 0);
}

// 0x10193330
void UIStatusPanel::ZoomTo() { UIMain_ZoomToThing(gStaffRoomPanelThing->GetThingId()); }

// 0x10193010: a flag closes the panel's object, none reopens it.
void UIStatusPanel::CloseObject(int32_t flag) {
    CObject* object = reinterpret_cast<CObject*>(gStaffRoomPanelThing);
    if (flag != 0) {
        object->CloseObject();
    } else {
        object->Reopen();
    }
}

// 0x10192f9c: the thing that goes is the panel's: slot 0x34 (r4 is still the thing).
void UIStatusPanel::ObjectGone(CThing* thing) {
    if (thing == gStaffRoomPanelThing) CallSlot(this, 0x34, thing);
}

// 0x10192de0: slot 0x44, then the previous thing of the mode's kind (wrapping); a different one becomes the panel's thing and slot 0xc runs.
void UIStatusPanel::GetPreviousObject() {
    CallSlot(this, 0x44);
    CThing* previous = nullptr;
    const int32_t type = PreviousIteratorType(*reinterpret_cast<int32_t*>(Bytes(this)));
    if (type != 0) previous = UI_WrapIterateThingPrevious(static_cast<UIIteratorTypes>(type), gStaffRoomPanelThing);
    if (previous != nullptr && previous != gStaffRoomPanelThing) {
        gStaffRoomPanelThing = previous;
        CallSlot(this, 0xc);
    }
}

namespace SamsUtilities {

TagId USwap32(TagId value) { return static_cast<TagId>(__builtin_bswap32(static_cast<uint32_t>(value))); }
TagButtonAction USwap32(TagButtonAction value) {
    return static_cast<TagButtonAction>(__builtin_bswap32(static_cast<uint32_t>(value)));
}
void USwapBlock16(char16_t* words, int32_t count) { _USwapBlock16(reinterpret_cast<uint16_t*>(words), count); }
// Weak: another unit (rsse_particles_init.cpp) defines the same instances at another address.
__attribute__((weak)) void USwapInplace32(int32_t& word) { word = static_cast<int32_t>(__builtin_bswap32(static_cast<uint32_t>(word))); }
__attribute__((weak)) void USwapInplace16(int16_t& half) { half = static_cast<int16_t>(__builtin_bswap16(static_cast<uint16_t>(half))); }

}  // namespace SamsUtilities

// 0x1018fa18: with the hidden delete flag; NULL does nothing. The sprite chain's destructors run in the order the Mac inlines them.
InterfaceRankTextObject* InterfaceRankTextObject_Dtor(InterfaceRankTextObject* self, int16_t deleteFlag) {
    if (self != nullptr) {
        rank_callee::FormatterDtor(&self->mFormatter, -1);   // (the Mac passes self + 0xe8, the formatter)
        rank_callee::SpriteListDtor(self, -1);
        rank_callee::EngineDataDtor(self, -1);
        rank_callee::DrawObjectDtor(self, 0);
        if (deleteFlag > 0) rank_callee::Delete(self);
    }
    return self;
}
