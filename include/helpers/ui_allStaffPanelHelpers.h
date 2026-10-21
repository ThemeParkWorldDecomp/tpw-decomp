// ui_allStaffPanelHelpers.h: file-local helpers that src/game/ui/ui_allStaffPanel.cpp shares with AdvisorScoring.cpp
// and ui_allItemsPanel.cpp. Before the regroup, the functions using them shared a source file (2 files in all), which
// had one copy of each; that copy now lives here, static inline, so each file that includes this header keeps its own
// internal copy.
#pragma once

#include "MainWindowCallback.h"
#include "World.h"

// The mulhw by 0x51eb851f, srawi 5 and the sign add of a signed divide by 100 of (happiness << 10).
static inline int32_t HappinessShown(int32_t happiness) {
    int32_t scaled = static_cast<int32_t>(static_cast<uint32_t>(happiness) << 10);
    int32_t high = static_cast<int32_t>((static_cast<int64_t>(0x51eb851f) * scaled) >> 32);
    int32_t q = high >> 5;
    return q + static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
}

// One button of the panel: fills the list, sets the title (slot +0x2c of the window's vtable is SetText) and the happiness bar (+0x24, SetValue).
static inline void ShowStaffKind(int32_t type, int32_t stringId, int32_t model) {
    FillAllStaffList(static_cast<StaffType>(type), 1);
    InterfaceWindow* title = gStaffPanel->GetWindow(0x329)->GetWindow(0x32a);
    title->SetText(const_cast<char16_t*>(UIMain_GetString(stringId)));
    InterfaceWindow* bar = gStaffPanel->GetWindow(0x329)->GetWindow(0x32d);
    int32_t happiness = gGameWorld->GetParkAnalyser()->AverageHappinessOfStaffType(static_cast<CThing::ThingModel>(model));
    bar->SetValue(HappinessShown(happiness));
}

// The Mac's list data word (a pointer there) is an offset from the arena base here.
template <class T>
static inline T* DataPointer(int32_t data) {
    return data != 0 ? reinterpret_cast<T*>(tpw::gArenaBase + static_cast<uint32_t>(data)) : nullptr;
}
