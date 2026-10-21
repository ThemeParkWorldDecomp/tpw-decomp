// ui_showStatusHelpers.h: file-local helpers that src/game/ui/ui_showStatus.cpp shares with ui_upgradeStatus.cpp.
// Before the regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now
// lives here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "MainWindowCallback.h"
#include "ThingReceivers.h"

static inline UIStatusPanelFields* Fields(void* panel) { return reinterpret_cast<UIStatusPanelFields*>(panel); }

// The ride preview in the view window at +0x14 (the same body for the show and the upgrade panel).
static inline void UpdateRidePreview(void* panel) {
    UIStatusPanelFields* fields = Fields(panel);
    if (fields->mF14.get() != nullptr && gStaffRoomPanelThing != nullptr) {
        RideInfo* info = reinterpret_cast<CObject*>(gStaffRoomPanelThing)->GetDBInfo();
        CoolRect rect;
        fields->mF14.get()->GetClientRect(&rect);
        UIMain_ShowRidePreview(rect, info->mRideTypeNumber, fields->mF14.get()->mZ + 10);
    }
}
