// The address book panel's message function and its list filler (ui_Postcard.cpp's panel, main app, unit v5055):
//   AddressBookMsg 0x101afb34, FillAddressList 0x101aff60
#include <cstdint>
#include <cstring>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "email.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

namespace {

// 0x1024e366: L"%s" (swprintf's format) and, 6 bytes on, the edit box's starting text.
const char16_t kWideStrings[] = u"%s\0";

constexpr int32_t kEditBox = 0x25;       // the address field of the panel
constexpr int32_t kAddressList = 0x26;   // the list of the panel
constexpr int32_t kListString = 0x121;   // the string of the last row ("other" entry of the online park)
constexpr int32_t kRemoveAddressNameSize = 0x40;

// The Mac's word at +0x9c of the FTP park info: a wide string pointer (4 bytes there, a native pointer here).
const char16_t* ParkText(CFSFTPParkInfo* info) {
    const char16_t* text;
    std::memcpy(&text, reinterpret_cast<const char*>(info) + 0x9c, sizeof(text));
    return text;
}

CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}

CFSOnlineSystem* OnlineSystem() {
    if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
    return gOnlineSystem;
}

// The text of the list row the control's data word names: a negative word is the online park's folder (its text as it is),
// any other the address at that place of the address book (copied to the removal dialog's buffer).
const char16_t* RowText(InterfaceListControl2* list, int32_t position) {
    int32_t index;
    list->GetData(position, &index);
    AddressBook* book = &UserInfo()->mAddressBook;
    if (index < 0) return ParkText(OnlineSystem()->GetFTPDownloadParkInfo());
    AddressBookNode** it = book->First();
    const char* text = (*it)->mText;
    while (index != 0) {
        it = book->Next();
        index--;
        text = (*it)->mText;
    }
    swprintf(gAddressRemoveName, kRemoveAddressNameSize, kWideStrings, text);
    return gAddressRemoveName;
}

}  // namespace

int32_t AddressBookMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == 0x24) {
            RemoveAddressBookEntry();
        } else if (a == -1) {
            RemoveAddressBook();
            return 0;
        }
    } else if (message < 0x100) {
        if (message == 0x14) {
            gAddressBookPanel = nullptr;
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x134));
        }
    } else if (message == 0x401) {
        InterfaceWindow* control = window->GetWindow(a);
        if (control != nullptr) {
            if (b == -1) {
                window->GetWindow(kEditBox)->SetText(const_cast<char16_t*>(kWideStrings + 3));
            } else {
                const char16_t* text = RowText(static_cast<InterfaceListControl2*>(control), b);
                window->GetWindow(kEditBox)->SetText(const_cast<char16_t*>(text));
            }
        }
    } else if (message == 0x400) {
        InterfaceWindow* control = window->GetWindow(a);
        if (control != nullptr && b != -1) {
            const char16_t* text = RowText(static_cast<InterfaceListControl2*>(control), b);
            AddToAddressField(const_cast<char16_t*>(text));
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

void FillAddressList(int32_t clear) {
    char16_t buffer[0x40];
    AddressBook* book = &UserInfo()->mAddressBook;
    AddressBookNode** it = book->First();
    int32_t row = 0;
    const char* text = (*it)->mText;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAddressBookPanel->GetWindow(kAddressList));
    if (clear != 0) list->RemoveAllItems();
    while (*text != 0) {
        swprintf(buffer, 0x40, kWideStrings, text);
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = buffer;
        list->AddItem(&item, row, -1, 0);
        row++;
        it = book->Next();
        text = (*it)->mText;
    }
    if (TheGameType().mType == kGameTypeOnline) {
        const char16_t* park = ParkText(OnlineSystem()->GetFTPDownloadParkInfo());
        if (park != nullptr && wcslen(park) != 0) {
            UI_LCAdd item;
            item.mRaw = 0;
            item.mText = UIMain_GetString(kListString);
            list->AddItem(&item, -1, -1, 0);
        }
    }
    list->UpdateDisplay();
}
