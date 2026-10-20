#include "InterfaceWindow.h"
#include "TbITable.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "World.h"
#include "platform/msl.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);   // 0x101a6234 (stand-in)

ParkDate gUIPostLoadDate;   // 0x1039641c

// 0x101efa54
void UIInterface::SetGameWindowCursor(int16_t cursor) {
    if (gGameWindow != nullptr) {
        UI_SetGameWindowCursor(cursor);
        return;
    }
    if (gOnlineWindow != nullptr) gOnlineWindow->setCursor(cursor);
    if (gOnlinePanel != nullptr) gOnlinePanel->setCursor(cursor);
}

// 0x101f0124
void UIInterface::PostLevelLoad() {
    ParkDate* d = &gUIPostLoadDate;
    const uint8_t* world = reinterpret_cast<const uint8_t*>(gGameWorld);
    UI_ClearSelected();
    if (*reinterpret_cast<const int32_t*>(world + 0x1da738) == 4) UI_BankruptAfterLoad();
    UIMain_EnableAllKeyTables();
    reinterpret_cast<const CParkTime*>(world + 0x2a0)
        ->GetFunnyTime(&d->mYear, &d->mMonth, &d->mDay, &d->mDayOfWeek, &d->mHour, &d->mMinute, &d->mSecond, nullptr);
    d->mYear = d->mYear - 0x76c;
    d->mDay = d->mDay - 1;
    d->mDay = d->mDay + 1;
    d->mMonth = d->mMonth - 1;
    ParkDate copy = *d;
    UI_MainPanelDateChange(copy.mDay, copy.mMonth + 1, copy.mYear);
}
