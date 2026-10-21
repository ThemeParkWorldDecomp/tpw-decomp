// ui_Postcard.cpp: name proven by __sinit_ui_Postcard_cpp in the main application.

#include "Advisor.h"
#include "AdvisorQueue.h"
#include "analyser.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "HLSprite.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "intrface.h"
#include "keytable_actions.h"
#include "LoansPanelMsg.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ParkTime.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "Postcard.h"
#include "SamsUtilities_UVersion_Dtor_UVersion_2.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_parkStatus.h"
#include "ui_Postcard.h"
#include "UI_pump.h"
#include "UIInterface.h"
#include "WindowScripts.inc"
#include "World.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>

// The address book panel's message function and its list filler (ui_Postcard.cpp's panel, main app, unit v5055):
//   AddressBookMsg 0x101afb34, FillAddressList 0x101aff60

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

// Unit v51042: UVersion's destructor, the object-state text, the rank text object's Change, the lender list callbacks, the thought and
// name edit handlers (main app, the neighbours of ui_main_panel_UI_Congratulations.cpp).

// 0x101afea4 (132 bytes)
int32_t NameEditMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1000b && a == 0xd) NewEntryInAddressBook();
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

// ui_Postcard.cpp's address book panel (main app, unit v5012): CreateAddressBook 0x101b07a4, RemoveAddressBookEntry 0x101b0420 and
// NewEntryInAddressBook 0x101b0168. Neighbour of src/ui_Postcard_panel.cpp (never edited here).

InterfaceWindow* gAddressBookPanel;        // 0x103956f8, original name unknown
char16_t gAddressEditText[0x40];           // 0x10394ba6, original name unknown: the edit box's text (capacity 0x3f)
char16_t gAddressRemoveName[0x40];         // 0x10394b24, original name unknown: the address the removal dialog names

// 0x102ac1e6: the panel's script, the image's 184 words (generated at configure time, unit v5L42).
int16_t gAddressBookDescription[] = {TPW_WINDOW_SCRIPT_102AC1E6};

// 0x1024e1e1: "%ls" and, 4 bytes on, "hilight" (the selection and cursor draw object's name).
static const char kAddressStrings[] = "%ls\0hilight";
// 0x1024e366: L"%s" (swprintf's format) and, 6 bytes on, the edit box's starting text.
static const char16_t kWideStrings_2[] = u"%s\0";

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

// The runtime's operator new (0x102384b8, never NULL): a weak seam the test library replaces with a recording fake.
static_assert(sizeof(InterfaceTextObjectSprite) <= 0x400 && sizeof(InterfaceWindow) <= 0x400 && sizeof(CFSUserInfo) <= 0x400, "the allocation below is 0x400 at least");

namespace postcard_callee {
// The Mac allocates the sizes of its objects; the host's are larger (InterfaceTextObjectSprite 0x250 bytes for the Mac's 0x19c), so the block is
// at least 0x400 bytes.
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(std::max<size_t>(size, 0x400)); }
}  // namespace postcard_callee

// The Mac calls a virtual through the font entry's table here (__ptr_glue, r3 = the font); the class is not recovered, so the height
// getter stands in. The test library replaces this function with a recording fake.
__attribute__((weak)) int16_t PostcardFontVirtual(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }

// 0x101b0368: the "delete this address" dialog's OK button. The argument is the entry position, passed as a pointer.
int32_t DeleteEntryOK(void* position) {
    if (gUserInfo == nullptr) {
        void* memory = postcard_callee::OperatorNew(0xd4);
        CFSUserInfo* info = nullptr;
        if (memory != nullptr) info = new (memory) CFSUserInfo();
        gUserInfo = info;
    }
    gUserInfo->mAddressBook.Remove(static_cast<int32_t>(reinterpret_cast<intptr_t>(position)));
    FillAddressList(1);
    return 1;
}

// 0x101b0620: LCFormat of the postcard file: a window 100 wide, as high as the font 7's height scaled to the screen, plus 6.
InterfaceWindow* PostcardLCFormat() {
    void* memory = postcard_callee::OperatorNew(0x138);
    if (memory != nullptr) {
        int32_t height = PostcardFontVirtual(GetFontTableEntry(7));
        int32_t row = height * 0x600 / gUIRealScreenHeight + 6;   // QUIRK: no check for a zero screen height
        new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(row));
    }
    return static_cast<InterfaceWindow*>(memory);
}

// 0x101b06d4: LCPostFormat of the postcard file.
void PostcardLCPostFormat(InterfaceWindow* window) {
    void* memory = postcard_callee::OperatorNew(0x19c);
    InterfaceTextObjectSprite* sprite = nullptr;
    if (memory != nullptr) sprite = new (memory) InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(7));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
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
        swprintf(gAddressRemoveName, 0x40, kWideStrings_2, text);
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
    edit->SetText(const_cast<char16_t*>(kWideStrings_2 + 3));
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x134));
}

// Unit v51122: the keyboard action wrappers (each calls one UIInterface member and answers 1), context_help, and small
// ui_mainpanel2 / panel remove functions.

// 0x101b0a84
void RemoveAddressBook() { UI_SendMessage(gAddressBookPanel, 4, 0, 0); }

// The postcard panel (main app, ui_Postcard.cpp, 0x101b0ae4..0x101b2418), the keyboard key and UIInterface forwarding call.

Postcard* gPostcard;
InterfaceWindow* gPostcardPanel;
int32_t gPostcardSent;
int32_t gPreviewDragging;
int32_t gPreviewLastX;
uint32_t gPreviewLastY;
int32_t gPreviewOffsetX;
int32_t gPreviewOffsetY;
uint8_t gAddressEdited;
uint8_t gMessageEdited;
uint8_t gTempPathFetched;
char gTempPath[0x200];
char16_t gPostcardMessage[0x100];
char16_t gPostcardAddress[0x60];
char16_t gPostcardText5da5[0x200];

namespace {
void FillPreview(uint8_t* surface, int32_t width, int32_t height) {
    const int32_t format = gei_TextureSettings.mFormat;
    if (format == 2 || format == 4)
        gPostcard->generate_preview_into_buffer(surface, width, height, 2);
    else if (format == 3 || format == 5)
        gPostcard->generate_preview_into_buffer(surface, width, height, 1);
}

void StyleEditBox(InterfaceEditBox* box, InterfaceCursorPosTextObjectSprite* sprite, int32_t wrap, int32_t stringId) {
    box->SetClientDrawObject(sprite);
    box->SetFont(GetFontTableEntry(0xb));
    box->SetFontColour(0xff, 0xff, 0xff, 0xff);
    box->SetFontWrap(wrap, 0);
    box->SetFontJust(0, 0);
    char cursorName[] = "hilight";
    box->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(cursorName));
    box->SetText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}
}

void RefreshPreviewWindow(InterfaceWindow* window) {
    TbRenderArea areaA;
    TbRenderArea areaB;
    TbRenderArea areaC;
    CoolRect rect;
    int32_t width;
    int32_t height;
    uint8_t* surface = window->GetClientObjectSurface(&width, &height);
    if (surface != nullptr && gPostcard != nullptr) {
        gPostcard->generate();
        UI_DrawPostcardText(gPostcardMessage, gPostcard->mCard.mPixels, gPostcard->mCard.mWidth,
                             gPostcard->mCard.mHeight, {0x142, 0x5e, 0x27c, 0xd7}, 0xff0000cc, 1);
        FillPreview(surface, width, height);
        window->FinishWithClientObjectSurface();
    }
}

// The postcard panel's sending half (ui_Postcard.cpp, main app, unit v5884): IllCallYouBack 0x101b0ca4 (the e-mail completion
// callback), AddressesValid 0x101b0d88, AddToAddressField 0x101b0e98 and SendPostcard 0x101b0f9c.

extern OnlineConfig gOnlineConfig;   // 0x102b2b70 (TPMain.h)

// New globals of this unit (no other file names them).
char gPostcardJpgPath[0x200];   // 0x10394c26, original name unknown: <temp path>postcard.jpg
int32_t gPostcardCounter;       // 0x10394b18, original name unknown: numbers the copies of the card sent
// The TVectors the Mac code passes by address (only their addresses matter here).
uint8_t kIllCallYouBackVector[8];             // 0x10267cf8: the e-mail callback
uint8_t kCSubscriptionHeaderCtorVector[8];    // 0x102657e8
uint8_t kCSubscriptionHeaderDtorVector[8];    // 0x10267288

// 0x101b0ca4: the error dialog for a failed send; nothing when the completion code is 0 (success).
void IllCallYouBack(EmailCompletionCode code, EmailErrorCode error) {
    if (code == 0) return;
    int32_t e = error;
    if (e == 2) {
        AddUIError(0x3a, nullptr);
    } else if (e < 2) {
        if (e == 0) {
            AddUIError(0x34, nullptr);
        } else if (e >= 0) {
            AddUIError(0x39, nullptr);
        }
    } else if (e < 5) {
        AddUIError(0x3b, nullptr);
    }
}

// 0x1024e366 + 8: the separators of the address list.
static const char16_t kAddressSeparators[] = u";, \n";

// 0x101b0d88: 1 when the address text is not empty and every address in it has an '@' followed, later than the next character, by
// a '.'.
int32_t AddressesValid() {
    char16_t buffer[0x20c];
    char16_t* state;
    wcscpy(buffer, gPostcardText5da5);
    if (buffer[0] == 0) return 0;
    char16_t* token = wcstok(buffer, kAddressSeparators, &state);
    while (token != nullptr) {
        char16_t* at = wcschr(token, u'@');
        bool good = false;
        if (reinterpret_cast<uintptr_t>(at) > reinterpret_cast<uintptr_t>(token)) {
            char16_t* dot = wcschr(at, u'.');
            good = reinterpret_cast<uintptr_t>(dot) > reinterpret_cast<uintptr_t>(at + 1);
        }
        if (!good) return 0;
        token = wcstok(nullptr, kAddressSeparators, &state);
    }
    return 1;
}

// 0x1024e366 + 0x12: swprintf's format of one address.
static const char16_t kAddressFormat[] = u";%ls";

// 0x101b0e98: appends the address to the To box's text (replacing the box's starting text) unless it is in there already, and
// shows the text in the box 0x5da5.
void AddToAddressField(char16_t* address) {
    char16_t* text = gPostcardText5da5;
    if (wcsstr(text, address) == nullptr) {
        char16_t buffer[0x40];
        swprintf(buffer, 0x40, kAddressFormat, address);
        if (wcscmp(text, UIMain_GetString(0xd3)) == 0) {
            wcscpy(text, buffer);
        } else {
            wcscat(text, buffer);
        }
        InterfaceWindow* box = gPostcardPanel->GetWindow(0x5da5);
        sendpostcard_callee::WindowSetText(box, text);
    }
}

// 0x101b0f9c: saves the card as <temp path>postcard.jpg, copies it to <temp path><counter>.jpg, builds the e-mail (subject, the
// addresses, the attachment) and either puts it in the outbox (not logged in) or sends it at once with the player's login.
void SendPostcard() {
    namespace cb = sendpostcard_callee;
    char fileName[0x13a];
    char addresses[0x201];
    char subject[0x61];
    char sender[0x80];

    cb::Sprintf(gPostcardJpgPath, "%s%s", gTempPath, "postcard.jpg");
    cb::PostcardSave(gPostcard, gPostcardJpgPath);
    int32_t count = gPostcardCounter;
    gPostcardCounter = count + 1;
    cb::Sprintf(fileName, "%s%08d.jpg", gTempPath, count);
    NS_MacDoze::CopyFile(gPostcardJpgPath, fileName, 0);

    Email* email = static_cast<Email*>(cb::New(0x4c));
    cb::EmailCtor(email);
    cb::Sprintf(addresses, "%ls", gPostcardText5da5);
    cb::Sprintf(subject, "%ls", gPostcardAddress);
    cb::StringAssign(&email->mString10, subject, static_cast<uint32_t>(std::strlen(subject)));
    char* token = std::strtok(addresses, ";, \n");
    while (token != nullptr) {
        uint32_t temporary[2];
        cb::StringCtor(temporary, token);
        cb::ListInsert(&email->mListSize, &email->mListPrev, temporary);
        cb::StringDtor(temporary, -1);
        token = std::strtok(nullptr, ";, \n");
    }
    cb::StringAssign(&email->mString14, fileName, static_cast<uint32_t>(std::strlen(fileName)));
    email->AttachData(fileName);
    email->mCallback = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(kIllCallYouBackVector));

    if (gOnlineSystem == nullptr) {
        void* mem = cb::New(0x404);
        cb::OnlineSystemCtor(mem);
        gOnlineSystem = static_cast<CFSOnlineSystem*>(mem);
    }
    if (gOnlineSystem->AreWeLoggedIn() == 0) {
        if (gUserInfo == nullptr) {
            void* mem = cb::New(0xd4);
            cb::UserInfoCtor(mem);
            gUserInfo = static_cast<CFSUserInfo*>(mem);
        }
        email->SendToOutbox(&gUserInfo->mOutbox);
        CMsgEvent event(static_cast<EventType>(8));
        if (gMessageCentre == nullptr) {
            void* mem = cb::New(0x1d0);
            __construct_array(mem, kCSubscriptionHeaderCtorVector, kCSubscriptionHeaderDtorVector, 0x10, 0x1d);
            gMessageCentre = static_cast<CMessageCentre*>(mem);
        }
        gMessageCentre->PostMessage(event);
        return;
    }

    if (gOnlineSystem == nullptr) {
        void* mem = cb::New(0x404);
        cb::OnlineSystemCtor(mem);
        gOnlineSystem = static_cast<CFSOnlineSystem*>(mem);
    }
    const char16_t* login = outbox_callee::LoginEmailAddress(gOnlineSystem);
    {
        char* out = sender;
        char c;
        do {
            c = static_cast<char>(*login++);
            *out++ = c;
        } while (c != 0);
    }
    cb::StringAssign(&email->mString00, sender, static_cast<uint32_t>(std::strlen(sender)));
    int32_t value = outbox_callee::LoginPlayerID(gOnlineSystem);
    std::memcpy(email->mF18, &value, 4);
    value = outbox_callee::LoginCustomerID(gOnlineSystem);
    std::memcpy(email->mF18 + 4, &value, 4);
    const char16_t* name = outbox_callee::LoginCustomerName(gOnlineSystem);
    outbox_callee::WStringAssign(&email->mWString20, name, wcslen(name));
    const char16_t* password = outbox_callee::LoginPassword(gOnlineSystem);
    outbox_callee::WStringAssign(&email->mWString24, password, wcslen(password));
    value = 9;
    std::memcpy(email->mF28, &value, 4);

    const char* server = gOnlineConfig.mMailServer.c_str();
    bool failed;
    if (*server == 0) {
        failed = email->Send("ftp.ea-europe.com", 0x19, nullptr, nullptr);
    } else {
        failed = email->Send(server, gOnlineConfig.mMailPort, nullptr, nullptr);
    }
    if (failed) AddUIError(0x39, nullptr);
}

int32_t PostcardPreviewMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10004) {
        gPreviewDragging = 0;
        window->UnCaptureDevice();
    } else if (message == 0x10003 && gPreviewDragging != 0) {
        const int32_t dy = static_cast<int32_t>(static_cast<uint32_t>(gUIRealScreenWidth) *
                                               (static_cast<uint32_t>(gPreviewLastX) - static_cast<uint32_t>(a >> 16)));
        gPreviewOffsetY += dy / 2048;
        const int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(gUIRealScreenHeight) *
                                               (gPreviewLastY - (static_cast<uint32_t>(a) & 0xffff)));
        gPreviewOffsetX += dx / 1536;
        if (gPreviewOffsetY < 0) gPreviewOffsetY = 0;
        if (gPreviewOffsetX < 0) gPreviewOffsetX = 0;
        gPreviewLastX = a >> 16;
        gPreviewLastY = static_cast<uint32_t>(a) & 0xffff;
    } else if (message == 0x10005) {
        gPreviewDragging = 1;
        gPreviewLastY = static_cast<uint32_t>(b) & 0xffff;
        gPreviewLastX = b >> 16;
        window->CaptureDevice();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t PostcardPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        UIMain_DisableAllKeyTables();
    } else if (message == 5) {
        RemovePostcardPanel();
        return 1;
    } else if (message == 0x14) {
        gPostcardPanel = nullptr;
        delete gPostcard;
        gPostcard = nullptr;
        if (TheGameType().mType == kGameTypeOnline) ChatReinstateAfterPostcard();
        else UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x133));
        UIMain_DecideKbCapture();
        UIMain_EnableAllKeyTables();
    } else if (message == 0x801) {
        if (a == 0x5da3) gMessageEdited = 1;
        else if (a == 0x5da4) gAddressEdited = 1;
    } else if (message == 0x100) {
        if (a == 0x5da0) {
            CreateAddressBook();
        } else if (a == -1) {
            if (!gMessageEdited) static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gPostcardMessage, 0x100, 0xd9);
            if (!gAddressEdited) static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gPostcardAddress, 0x60, 0xd8);
            RefreshPreviewWindow(window->GetWindow(0x5d9f));
            if (!AddressesValid()) {
                dlgboxparams params;
                params.mText = UIMain_GetString(0xd6);
                params.mRed = 0xff;
                params.mGreen = 0xff;
                params.mBlue = 0xff;
                params.mValue8 = 4;
                UI_DialogBox(params, static_cast<dlgbutton>(1), nullptr, static_cast<dlgbutton>(0), nullptr,
                             static_cast<dlgbutton>(0), nullptr, false);
            } else {
                SendPostcard();
                RemovePostcardPanel();
            }
            gPostcardSent = 1;
            return 0;
        } else if (a == -2) {
            RemovePostcardPanel();
            return 0;
        } else if (a == 0x5da2) {
            RefreshPreviewWindow(window->GetWindow(0x5d9f));
        } else if (a == 0x5da1) {
            char16_t empty[] = u"";
            window->GetWindow(0x5da3)->SetText(empty);
            window->GetWindow(0x5da4)->SetText(empty);
            window->GetWindow(0x5da5)->SetText(empty);
        }
    }
    if (TheGameType().mType == kGameTypeOnline) return window->CallDefaultWindowHandler(message, a, b);
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

void CreatePostcardPanel() {
    if (gPostcardPanel != nullptr) return;
    if (gOnlineSystem == nullptr) gOnlineSystem = new (std::nothrow) CFSOnlineSystem;
    if (!gOnlineSystem->CanWeSendEmail()) {
        AddUIError(0x4e, nullptr);
        return;
    }
    InterfaceWindow* parent;
    if (TheGameType().mType == kGameTypeOnline) parent = gOnlineWindow;
    else {
        UIMain_CheckPanel();
        parent = gGameWindow;
    }
    InterfaceWindow* panel = parent->Build(gPostcardPanelDescription, PostcardPanelMsg);
    if (panel != nullptr) {
        gPostcardPanel = panel;
        if (TheGameType().mType != kGameTypeOnline) UIMain_SetPanel(gPostcardPanel);
        UIMain_SetTitleText(gPostcardPanel, 0x5d9e, const_cast<char16_t*>(UIMain_GetString(0xd0)));
        auto* box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da5));
        if (box != nullptr) {
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            std::memset(gPostcardText5da5, 0, sizeof(gPostcardText5da5));
            box->SetStaticTextArea(gPostcardText5da5, 0x1ff);
            StyleEditBox(box, sprite, 1, 0xd3);
        }
        box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da4));
        if (box != nullptr) {
            std::memset(gPostcardAddress, 0, sizeof(gPostcardAddress));
            box->SetStaticTextArea(gPostcardAddress, 0x5f);
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            StyleEditBox(box, sprite, 0, 0xd1);
        }
        box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da3));
        if (box != nullptr) {
            std::memset(gPostcardMessage, 0, sizeof(gPostcardMessage));
            box->SetStaticTextArea(gPostcardMessage, 0xff);
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            StyleEditBox(box, sprite, 1, 0xd2);
        }
        InterfaceWindow* preview = gPostcardPanel->GetWindow(0x5d9f);
        if (preview != nullptr) {
            preview->SetCallbackFunction(PostcardPreviewMsg);
            preview->SetClientDrawObject(new (std::nothrow) InterfaceDrawObjectSprite(nullptr));
            TbRenderArea areaA;
            TbRenderArea areaB;
            CoolRect rect;
            int32_t width;
            int32_t height;
            uint8_t* surface = preview->GetClientObjectSurface(&width, &height);
            if (surface != nullptr && gPostcard != nullptr) {
                gPostcard->generate();
                FillPreview(surface, width, height);
                preview->FinishWithClientObjectSurface();
            }
        }
    }
    gMessageEdited = 0;
    gAddressEdited = 0;
    if (TheGameType().mType != kGameTypeOnline) UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x133));
}

void RemovePostcardPanel() { UI_SendMessage(gPostcardPanel, 4, 0, 0); }

void UI_ActuallyDoPostcard() {
    gPostcardSent = 0;
    if (TheGameType().mType != kGameTypeOnline) {
        gGame.mF3C = 1;
        gGame.Unpause();
    }
    delete gPostcard;
    gPostcard = new (std::nothrow) Postcard;
    gPostcard->load_sources();
    DDSURFACEDESC2 desc = gei_DDraw->LockBackbuffer();
    gPostcard->grab(reinterpret_cast<_DDSURFACEDESC2&>(desc));
    gei_DDraw->UnlockBackbuffer();
    gPostcard->generate();
    gUIRootWindow->Show(1);
    UI_ShowCursor();
    UIMain_GetFullyOutOfFirstPerson();
    if (TheGameType().mType == kGameTypeOnline) ChatGoFullScreen();
    else if (GameWindowToggle_Hidden()) ToggleGameWindow();
    if (!gTempPathFetched) {
        NS_MacDoze::GetTempPath(0x200, gTempPath);
        gTempPathFetched = 1;
    }
    CreatePostcardPanel();
    gPostcardWanted = 0;
}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

int32_t gPostcardDelay;
int32_t gPostcardWanted;
// ---- the postcard

bool UI_PostcardPending() {
    if (gPostcardDelay > 0) gPostcardDelay = gPostcardDelay - 1;
    bool pending = false;
    if (gPostcardDelay == 0 && gPostcardWanted != 0) pending = true;
    return pending;
}

void UI_DoPostcard() {
    UIMain_UISound(0x95);
    if (TheGameType().mType == kGameTypeOnline) ChatPrePostCard();
    else {
        gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
        gGame.mF3C = 0;
    }
    gUIRootWindow->Show(0);
    UI_HideCursor();
    BTN_HighLight_Kill();
    ADV_Kill(1);
    gPostcardDelay = 3;
    gPostcardWanted = 1;
}

// __sinit_ui_Postcard_cpp 0x101b30c8: the static initialiser of ui_Postcard.cpp. It constructs eighteen
// narrow strings and one wide string in bss and registers each for destruction at exit.

extern "C" void __register_global_object(void* object, void* destructor, void* node);
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// The bss block 0x10395700 to 0x10395960 of ui_Postcard.cpp:
//   0x000 wide string (0x14)   0x014 node   0x020 string   0x034 node   0x040 string   0x054 node
//   0x060 sixteen strings (0x14 each, the languages)   0x1a0 sixteen nodes (12 each, in reverse order)
alignas(16) unsigned char gPostcardBlob[0x260];

namespace {
const char* const kLanguages =
    "English\0French\0German\0Spanish\0Swedish\0Italian\0Dutch\0Polish\0Danish\0Japanese\0"
    "TraditionalChinese\0SimplifiedChinese\0Thai\0Korean\0Hebrew\0American";
}

void __sinit_ui_Postcard_cpp() {
    void* dtor = reinterpret_cast<void*>(&UIMain_DestroyString);
    const char* text = kLanguages;
    for (int i = 0; i < 16; i++) {
        unsigned char* obj = gPostcardBlob + 0x60 + 0x14 * i;
        new (obj) TbDynamicStringTemplate<char>(text);
        __register_global_object(obj, dtor, gPostcardBlob + 0x1a0 + 0xc * (15 - i));
        while (*text) text++;
        text++;
    }
    new (gPostcardBlob + 0x40) TbDynamicStringTemplate<char>("Theme Park World");
    __register_global_object(gPostcardBlob + 0x40, dtor, gPostcardBlob + 0x54);
    new (gPostcardBlob + 0x20) TbDynamicStringTemplate<char>("Language");
    __register_global_object(gPostcardBlob + 0x20, dtor, gPostcardBlob + 0x34);
    new (gPostcardBlob) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(gPostcardBlob, reinterpret_cast<void*>(&UIMain_DestroyWString), gPostcardBlob + 0x14);
}

// ui_Postcard.cpp's 238-word panel script (main 0x102ac00a). Window records followed by their child records.

int16_t gPostcardPanelDescription[238] = {
    0, 1, 1, 0, 23965, 0, 186, 30, 2018, 1007,
    17, -1, -1,
    18, -1, -1,
    1, 16896, -2194,
    0, 1, 1, 0, 23966, 0, 809, 66, 1281, 146,
    17, -1, -1,
    18, -1, -1,
    5,
    0, 1, 1, 0, 23967, 0, 336, 144, 1671, 675,
    17, -1, -1,
    18, -1, -1,
    3, 366, 168, 1646, 648,
    1, 15424, -25732,
    5,
    0, 2, 1, 0, 23968, 0, 1688, 794, 1842, 948,
    17, 324, 0,
    18, -1, -1,
    1, -31979, 2760,
    5,
    0, 2, 1, 0, 23969, 0, 1688, 475, 1842, 629,
    17, 323, 0,
    18, -1, -1,
    1, -19212, 23100,
    5,
    0, 2, 1, 0, 23970, 0, 1688, 311, 1842, 465,
    17, 322, 0,
    18, -1, -1,
    1, -24599, -24197,
    5,
    0, 5, 129, 0, 23971, 0, 341, 691, 970, 952,
    17, 319, 0,
    18, -1, -1,
    3, 364, 709, 951, 928,
    1, 15424, -25732,
    5,
    0, 5, 1, 0, 23972, 0, 981, 689, 1670, 781,
    17, 320, 0,
    18, -1, -1,
    3, 1004, 713, 1654, 757,
    1, 15424, -25732,
    5,
    0, 5, 129, 0, 23973, 0, 983, 788, 1672, 953,
    17, 321, 0,
    18, -1, -1,
    3, 1004, 806, 1654, 932,
    1, 15424, -25732,
    5,
    0, 2, 1, 0, -1, -1, 1878, 731, 1961, 815,
    17, 325, 0,
    18, -1, -1,
    1, -7029, 5287,
    5,
    0, 2, 1, 0, -2, -1, 1889, 820, 1972, 903,
    17, 2, 0,
    18, -1, -1,
    1, 16207, 5804,
    5,
    5,
    5,
};
