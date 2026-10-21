// Unit v5L31: a recording table for the status panels. The Mac keeps a vtable pointer at +0x20 of a panel; the host keeps the arena
// offset of a table made by UIStatusPanel_MakeTable (include/ui_main.h, src/game/ui/ui_StatusPanel.cpp). A test library includes this
// after ui_main.h and gives a hook: every slot of FakeStatusPanelTable<Hook>() calls Hook(panel, Mac slot offset, argument) and answers
// what the hook answers, so the library logs the slot reached as the Mac's __ptr_glue fake logs the entry's offset.
#pragma once

#include <cstdint>
#include <utility>

#include "ui_main.h"

using FakeStatusPanelHook = uint64_t (*)(void* panel, uint32_t offset, const void* arg);

template <FakeStatusPanelHook Hook>
struct FakeStatusPanelSlots {
    static constexpr int32_t kCount = 0x4c / 4;   // the two header words and the slots +0x08 to +0x48

    template <uint32_t Offset>
    static uint64_t Slot(void* panel, const void* arg) {
        return Hook(panel, Offset, arg);
    }
    template <std::size_t... I>
    static uint32_t* Make(std::index_sequence<I...>) {
        const UIStatusPanelSlot slots[kCount] = {(I < 2 ? nullptr : &Slot<static_cast<uint32_t>(4 * I)>)...};
        return UIStatusPanel_MakeTable(slots, kCount);
    }
};

// The table, made in the arena on first use and kept.
template <FakeStatusPanelHook Hook>
uint32_t* FakeStatusPanelTable() {
    static uint32_t* table = FakeStatusPanelSlots<Hook>::Make(std::make_index_sequence<FakeStatusPanelSlots<Hook>::kCount>());
    return table;
}
