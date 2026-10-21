// ui_RideStatusHelpers.h: file-local helpers that src/game/ui/ui_RideStatus.cpp shares with ui_shopStatus.cpp. Before
// the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "FillLenderList.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"

static inline UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The ride preview in the view window at +0x14 (the same body for the ride and the shop panel).
static inline void ShowPreview(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    if (fields->mF14.mOffset != 0 && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        alignas(16) uint8_t memory[sizeof(CoolRect) + 16];
        CoolRect* rect = reinterpret_cast<CoolRect*>(memory);
        kidlist_callee::RectCtor(rect);
        fields->mF14.get()->GetClientRect(rect);
        UIMain_ShowRidePreview(*rect, info->mRideTypeNumber, fields->mF14.get()->mZ + 10);
        kidlist_callee::RectDtor(rect);
    }
}

// The view window's text object (the same body for the ride and the shop panel): the window at +0x18.
static inline void InitView(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    InterfaceWindow* window = fields->mF18.get();
    if (window != nullptr) {
        window->setZ(static_cast<int16_t>(window->mZ + 0xb));
        fields->mF18.get()->Show(0);
        void* sprite = lender_callee::OperatorNew(0x19c);
        if (sprite != nullptr) lender_callee::SpriteCtor(sprite);
        fields->mF18.get()->SetClientDrawObject(static_cast<InterfaceDrawObject*>(sprite));
        fields->mF18.get()->SetFontJust(1, 1);
        InterfaceFont* font = GetFontTableEntry(8);
        fields->mF18.get()->SetFont(font);
        fields->mF18.get()->SetFontColour(0, 0, 0, 0xff);
        fields->mF18.get()->SetClientDrawText(nullptr);
    }
}
