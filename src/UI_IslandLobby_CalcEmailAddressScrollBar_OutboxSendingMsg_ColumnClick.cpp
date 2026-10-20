// Unit v5374: the outbox panel's column click (ui_onlineOutbox.cpp).
#include "InterfaceWindow.h"
#include "lobby.h"
#include "ui_controls.h"

// 0x101e3e98: a click on column 2 (send) or any other column above 1 (delete) toggles that flag (0x1000000 / 0x2000000) in the data
// word of the list's current entry, clears the other one, shows both as 0/1 in columns 2 and 3 and stores the word back.
// Columns 1 and below do nothing.
void ColumnClick(int32_t column) {
    if (column <= 1) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
    int32_t entry = list->mCurrentEntry;   // 0x14c
    int32_t data;
    UI_LCAdd item[4];
    list->GetData(entry, &data);
    if (column == 2) {
        if ((data & 0x1000000) != 0) {
            data &= ~0x1000000;
        } else {
            data |= 0x1000000;
            if ((data & 0x2000000) != 0) data &= ~0x2000000;
        }
    } else {
        if ((data & 0x2000000) != 0) {
            data &= ~0x2000000;
        } else {
            data |= 0x2000000;
            if ((data & 0x1000000) != 0) data &= ~0x1000000;
        }
    }
    item[0].mRaw = 0;
    item[1].mRaw = 0;
    item[2].mRaw = (data & 0x1000000) == 0x1000000;
    item[3].mRaw = (data & 0x2000000) == 0x2000000;
    list->ChangeItem(entry, item, 0xc, 1, -1);
    list->SetData(entry, data);
}
