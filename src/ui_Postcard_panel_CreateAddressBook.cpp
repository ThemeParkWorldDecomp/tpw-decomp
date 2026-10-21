// ui_Postcard.cpp's address book panel (main app, unit v5012): CreateAddressBook 0x101b07a4, RemoveAddressBookEntry 0x101b0420 and
// NewEntryInAddressBook 0x101b0168. Neighbour of src/ui_Postcard_panel.cpp (never edited here).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "WindowScripts.inc"
#include "email.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"

InterfaceWindow* gAddressBookPanel;        // 0x103956f8, original name unknown
char16_t gAddressEditText[0x40];           // 0x10394ba6, original name unknown: the edit box's text (capacity 0x3f)
char16_t gAddressRemoveName[0x40];         // 0x10394b24, original name unknown: the address the removal dialog names

// 0x102ac1e6: the panel's script, the image's 184 words (generated at configure time, unit v5L42).
int16_t gAddressBookDescription[] = {TPW_WINDOW_SCRIPT_102AC1E6};

// 0x1024e1e1: "%ls" and, 4 bytes on, "hilight" (the selection and cursor draw object's name).
static const char kAddressStrings[] = "%ls\0hilight";
// 0x1024e366: L"%s" (swprintf's format) and, 6 bytes on, the edit box's starting text.
static const char16_t kWideStrings[] = u"%s\0";

namespace {

// The Mac's wcschr on 16-bit characters.
char16_t* FindWide(char16_t* text, char16_t wanted) {
    for (;; ++text) {
        if (*text == wanted) return text;
        if (*text == 0) return nullptr;
    }
}

uintptr_t Address(const void* pointer) { return reinterpret_cast<uintptr_t>(pointer); }

}  // namespace

void CreateAddressBook() {
    if (gAddressBookPanel != nullptr) return;
    gAddressBookPanel = gPostcardPanel->Build(gAddressBookDescription, AddressBookMsg);
    InterfaceTitleTextObject* title = new InterfaceTitleTextObject(nullptr);
    gAddressBookPanel->GetWindow(0x27)->SetClientDrawObject(title);
    InterfaceFont* titleFont = GetFontTableEntry(5);
    gAddressBookPanel->GetWindow(0x27)->SetFont(titleFont);
    gAddressBookPanel->GetWindow(0x27)->SetFontJust(1, 1);
    const char16_t* titleText = UIMain_GetString(0xd4);
    gAddressBookPanel->GetWindow(0x27)->SetText(const_cast<char16_t*>(titleText));
    UI_TITLE_COLOUR(gAddressBookPanel->GetWindow(0x27));
    gAddressBookPanel->setZ(static_cast<int16_t>(gPostcardPanel->mZ + 10));
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAddressBookPanel->GetWindow(0x26));
    list->SetColumnFormatsAll(PostcardLCFormat, PostcardLCPostFormat);
    list->SetColumnType(0, kLCText);
    list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(kAddressStrings + 4)));
    FillAddressList(0);
    InterfaceWindow* edit = gAddressBookPanel->GetWindow(0x25);
    InterfaceCursorPosTextObjectSprite* cursor = new InterfaceCursorPosTextObjectSprite(nullptr);
    edit->SetClientDrawObject(cursor);
    edit->SetFont(GetFontTableEntry(7));
    edit->SetFontColour(0xff, 0xff, 0xff, 0xff);
    edit->SetFontJust(0, 0);
    edit->SetCallbackFunction(NameEditMsg);
    static_cast<InterfaceEditBox*>(edit)->SetStaticTextArea(gAddressEditText, 0x3f);
    static_cast<InterfaceEditBox*>(edit)->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(kAddressStrings + 4)));
    edit->SetText(const_cast<char16_t*>(kWideStrings + 3));
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x134));
}

void RemoveAddressBookEntry() {
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gAddressBookPanel->GetWindow(0x26));
    int32_t current = list->mCurrentEntry;
    if (current < 0) return;
    int32_t data = 0;
    list->GetData(current, &data);
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    AddressBook* book = &gUserInfo->mAddressBook;
    const char16_t* name = gAddressRemoveName;
    if (data >= 0) {
        AddressBookNode* node = *book->First();
        const char* text = node->mText;
        for (int32_t count = data; count != 0; --count) {
            node = *book->Next();
            text = node->mText;
        }
        swprintf(gAddressRemoveName, 0x40, kWideStrings, text);
    } else {
        if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem;
        const uint8_t* info = reinterpret_cast<const uint8_t*>(gOnlineSystem->GetFTPDownloadParkInfo());
        const char16_t* parkName;
        std::memcpy(&parkName, info + 0x9c, sizeof parkName);   // QUIRK: a pointer at +0x9c, as the Mac's word
        name = parkName;
    }
    char16_t message[0x80];
    TbUniStringVTag tag(3, name);
    static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(message, 0x7f, 0xd7, &tag);
    dlgboxparams params;
    params.mText = message;
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = data;
    UI_DialogBox(params, static_cast<dlgbutton>(2), nullptr, static_cast<dlgbutton>(1), DeleteEntryOK, static_cast<dlgbutton>(0),
                 nullptr, false);
}

void NewEntryInAddressBook() {
    char16_t* text = gAddressEditText;
    char16_t* at = FindWide(text, 0x40);
    bool valid = false;
    if (Address(at) > Address(text)) {
        char16_t* dot = FindWide(at, 0x2e);
        if (Address(dot) > Address(at + 1)) valid = true;
    }
    if (valid) {
        wchar_t wide[0x40];
        size_t length = 0;
        while (length < 0x3f && text[length] != 0) {
            wide[length] = static_cast<wchar_t>(text[length]);
            ++length;
        }
        wide[length] = 0;
        char narrow[0x40];
        std::snprintf(narrow, 0x40, kAddressStrings, wide);
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
        gUserInfo->mAddressBook.Add(std::string(narrow));
        FillAddressList(1);
        static_cast<InterfaceListControl2*>(gAddressBookPanel->GetWindow(0x26))->SetCurrent(-1);
        AddToAddressField(text);
    } else {
        char16_t message[0x81];
        TbUniStringVTag tag(3, text);
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(message, 0x7f, 0xd5, &tag);
        dlgboxparams params;
        params.mText = message;
        params.mRed = 0xff;
        params.mGreen = 0xff;
        params.mBlue = 0xff;
        params.mValue8 = 4;
        UI_DialogBox(params, static_cast<dlgbutton>(1), nullptr, static_cast<dlgbutton>(0), nullptr, static_cast<dlgbutton>(0),
                     nullptr, false);
    }
}
