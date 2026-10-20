// Unit v5991: the online contacts panel's receive-string / handler / buddy helpers (main app).
#include <cstdint>
#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "online.h"
#include "ui_Postcard.h"

int32_t gContactsMaxRows = 20;   // 0x102b0c54: rows kept in the list (initial word 0x14)

// Placeholders for the program (the host cannot call the Mac's virtuals through __ptr_glue); the test library defines the real ones.
namespace contacts_callee {
__attribute__((weak)) int32_t FontCharWidth(InterfaceFont*, char16_t) { return 0; }   // the font's virtual at +0x14 of the table at +0x10
__attribute__((weak)) void WindowSlot1c(InterfaceWindow*, int32_t) {}                  // the window's virtual at +0x1c of the table at +0x118
__attribute__((weak)) COnlineControl* NewOnlineControl() { return new COnlineControl(); }   // operator new(0xe8c) and the constructor
}  // namespace contacts_callee

static int32_t WLen(const char16_t* s) {
    int32_t n = 0;
    while (s[n] != 0) n++;
    return n;
}

// The index of the last character that still fits when the text is laid out from `start`; -1 when nothing starts there.
static int32_t BreakIndex(const char16_t* str, int32_t start, int32_t len, int32_t limit, InterfaceFont* font) {
    int32_t last = -1;
    if (start >= len) return -1;
    int32_t acc = static_cast<int16_t>(contacts_callee::FontCharWidth(font, str[start]));
    int32_t idx = start;
    while (acc < limit && idx < len) {
        last = idx;
        idx++;
        if (str[idx] != 0) acc += static_cast<int16_t>(contacts_callee::FontCharWidth(font, str[idx]));
    }
    return last;
}

// 0x101e7dd8
void OnlineReceiveString(char16_t* text, int32_t data) {
    if (gOnlinePanel == nullptr) return;
    InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc312b));
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t left, right;
    list->GetColumnPos(0, &left, &right);
    int32_t limit = (static_cast<int32_t>(gUIRealScreenWidth) * (static_cast<int32_t>(right) - static_cast<int32_t>(left))) / 2048;
    int32_t len = WLen(text);
    int32_t brk = BreakIndex(text, 0, len, limit, font);
    int32_t start = 0;
    while (brk != -1) {
        char16_t saved = text[brk + 1];
        text[brk + 1] = 0;
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = text + start;
        list->AddItem(&item, data, -1, 1);
        text[brk + 1] = saved;
        start = brk + 1;
        len = WLen(text);
        brk = BreakIndex(text, start, len, limit, font);
    }
    for (int32_t n = list->mItemCount; n >= gContactsMaxRows; n--) list->RemoveItem(0);
    list->SetCurrent(list->mItemCount - 1);
}

static COnlineControl* EnsureControl() {
    if (gOnlineControl == nullptr) gOnlineControl = contacts_callee::NewOnlineControl();
    return gOnlineControl;
}

// 0x101e7854
int32_t ContactListHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x101) {
        if (a == 0xc3126) {
            gOnlineContactsBusy = 0;
            FillContactList();
        } else {
            gOnlineContactsBusy = 1;
            FillBuddyList();
        }
    } else if (message == 0x100) {
        if (a == 0xc3129) {
            EnsureControl()->VoteForThisPark();
            InterfaceWindow* w = window->GetWindow(0xc3129);
            contacts_callee::WindowSlot1c(w, 0);
        } else if (a < 0xc3129) {
            if (a >= 0xc3128) {
                if (b == 1) EnsureControl()->EarmuffsOn();
                else EnsureControl()->EarmuffsOff();
            }
        } else if (a < 0xc312b) {
            UI_DoPostcard();
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e839c
void _OnlineAddBuddy(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = name;
        list->AddItem(&item, data, -1, 1);
    }
}

// 0x101e845c
void _OnlineRemoveBuddy(char16_t* name) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) list->RemoveItem(position);
    }
}

// 0x101e8524
void _OnlineUpdateBuddy(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 1) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) {
            list->SetData(position, data);
            list->UpdateDisplay();
        }
    }
}
