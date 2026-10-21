// Unit v5L31: the status panels' tables of virtual slots, the ones their constructors store at +0x20 (the Mac's vtables 0x102a8644 and
// those of the nine panel classes). A Ptr32 cannot name the Mac's data, so each table is a block of the arena (UIStatusPanel_MakeTable),
// made on first use and kept. Slot by slot they are the Mac's: the class's own member where the Mac's table has one, the base's otherwise.
#include <cstdint>

#include "ui_main.h"

namespace {

template <class P, void (P::*F)()>
uint64_t Plain(void* panel, const void*) {
    (static_cast<P*>(panel)->*F)();
    return 0;
}
template <class P, void (P::*F)(CThing*)>
uint64_t WithThing(void* panel, const void* thing) {
    (static_cast<P*>(panel)->*F)(static_cast<CThing*>(const_cast<void*>(thing)));
    return 0;
}
template <class P, void (P::*F)(char16_t*)>
uint64_t WithName(void* panel, const void* name) {
    (static_cast<P*>(panel)->*F)(static_cast<char16_t*>(const_cast<void*>(name)));
    return 0;
}
template <class P, class R, R (P::*F)()>
uint64_t Answer(void* panel, const void*) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>((static_cast<P*>(panel)->*F)()));
}

constexpr int32_t kSlots = 0x48 / 4;   // the header words and the 16 slots of UIStatusPanel (+0x08 to +0x44)

struct Slots {
    UIStatusPanelSlot s[kSlots + 1] = {};   // one more for FeatureStatusPanel::RideIt (+0x48)
    UIStatusPanelSlot& operator[](uint32_t offset) { return s[offset / 4]; }
};

// The base's table (0x102a8644).
Slots BaseSlots() {
    Slots t;
    t[0x08] = Plain<UIStatusPanel, &UIStatusPanel::InitWindow>;
    t[0x0c] = Plain<UIStatusPanel, &UIStatusPanel::UpdateWindow>;
    t[0x10] = Plain<UIStatusPanel, &UIStatusPanel::DeInitWindow>;
    t[0x14] = Plain<UIStatusPanel, &UIStatusPanel::InitStatusWindow>;
    t[0x18] = Plain<UIStatusPanel, &UIStatusPanel::InitViewWindow>;
    t[0x1c] = Plain<UIStatusPanel, &UIStatusPanel::UpdateStatusWindow>;
    t[0x20] = Plain<UIStatusPanel, &UIStatusPanel::UpdateViewWindow>;
    t[0x24] = Plain<UIStatusPanel, &UIStatusPanel::DeInitStatusWindow>;
    t[0x28] = Plain<UIStatusPanel, &UIStatusPanel::DeInitViewWindow>;
    t[0x2c] = Plain<UIStatusPanel, &UIStatusPanel::DisplayStatusText>;
    t[0x30] = WithThing<UIStatusPanel, &UIStatusPanel::Show>;
    t[0x34] = Plain<UIStatusPanel, &UIStatusPanel::Remove>;
    t[0x38] = WithName<UIStatusPanel, &UIStatusPanel::ChangeName>;
    t[0x3c] = Answer<UIStatusPanel, char16_t*, &UIStatusPanel::GetName>;
    t[0x40] = Plain<UIStatusPanel, &UIStatusPanel::ShowEditNamePanel>;
    t[0x44] = Plain<UIStatusPanel, &UIStatusPanel::ApplyChanges>;
    return t;
}

// Every panel class has its own +0x08 to +0x28, Show (+0x30) and Remove (+0x34); +0x2c to +0x44 are the base's unless it overrides them.
template <class P>
Slots OwnSlots() {
    Slots t = BaseSlots();
    t[0x08] = Plain<P, &P::InitWindow>;
    t[0x0c] = Plain<P, &P::UpdateWindow>;
    t[0x10] = Plain<P, &P::DeInitWindow>;
    t[0x14] = Plain<P, &P::InitStatusWindow>;
    t[0x18] = Plain<P, &P::InitViewWindow>;
    t[0x1c] = Plain<P, &P::UpdateStatusWindow>;
    t[0x20] = Plain<P, &P::UpdateViewWindow>;
    t[0x24] = Plain<P, &P::DeInitStatusWindow>;
    t[0x28] = Plain<P, &P::DeInitViewWindow>;
    t[0x30] = WithThing<P, &P::Show>;
    t[0x34] = Plain<P, &P::Remove>;
    return t;
}

uint32_t* Make(const Slots& t) { return UIStatusPanel_MakeTable(t.s, kSlots + 1); }

}  // namespace

uint32_t* UIStatusPanelTable() {   // 0x102a8644 (name invented)
    static uint32_t* table = Make(BaseSlots());
    return table;
}

uint32_t* RideStatusPanelTable() {   // 0x102ad5fc
    static uint32_t* table = [] {
        Slots t = OwnSlots<RideStatusPanel>();
        t[0x44] = Plain<RideStatusPanel, &RideStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* ShopStatusPanelTable() {   // 0x102adbd8
    static uint32_t* table = [] {
        Slots t = OwnSlots<ShopStatusPanel>();
        t[0x44] = Plain<ShopStatusPanel, &ShopStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* ShowStatusPanelTable() {   // 0x102ae980
    static uint32_t* table = [] {
        Slots t = OwnSlots<ShowStatusPanel>();
        t[0x44] = Plain<ShowStatusPanel, &ShowStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* StaffStatusPanelTable() {   // 0x102af264
    static uint32_t* table = [] {
        Slots t = OwnSlots<StaffStatusPanel>();
        t[0x38] = WithName<StaffStatusPanel, &StaffStatusPanel::ChangeName>;
        t[0x3c] = Answer<StaffStatusPanel, const char16_t*, &StaffStatusPanel::GetName>;
        t[0x40] = Plain<StaffStatusPanel, &StaffStatusPanel::ShowEditNamePanel>;
        return Make(t);
    }();
    return table;
}

uint32_t* StaffRoomStatusPanelTable() {   // 0x102aed88
    static uint32_t* table = Make(OwnSlots<StaffRoomStatusPanel>());
    return table;
}

uint32_t* UpgradeStatusPanelTable() {   // 0x102af4dc
    static uint32_t* table = Make(OwnSlots<UpgradeStatusPanel>());
    return table;
}

uint32_t* FeatureStatusPanelTable() {   // 0x102a9ba0
    static uint32_t* table = [] {
        Slots t = OwnSlots<FeatureStatusPanel>();
        t[0x48] = Plain<FeatureStatusPanel, &FeatureStatusPanel::RideIt>;
        return Make(t);
    }();
    return table;
}

uint32_t* BogStatusPanelTable() {   // 0x102a9384
    static uint32_t* table = [] {
        Slots t = OwnSlots<BogStatusPanel>();
        t[0x44] = Plain<BogStatusPanel, &BogStatusPanel::ApplyChanges>;
        return Make(t);
    }();
    return table;
}

uint32_t* VisitorStatusPanelTable() {   // 0x102a4b5c
    static uint32_t* table = Make(OwnSlots<VisitorStatusPanel>());
    return table;
}
