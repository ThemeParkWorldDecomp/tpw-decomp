// ui_onlineOutbox.cpp: name proven by __sinit_ui_onlineOutbox_cpp in the main application.

#include "Advisor.h"
#include "AdvisorScoring.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_MasterSave.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_OnlineLoginPanel.h"
#include "FS_UserInfo.h"
#include "Game.h"
#include "InterfaceFont.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "platform/upixels.h"
#include "Postcard.h"
#include "rsse.h"
#include "std_basic_ifstream.h"
#include "tb_resource_texture.h"
#include "TbDynamicStringTemplate.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbPixFmt.h"
#include "TbVTag.h"
#include "Text.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_mainmenu.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "ui_textmanager.h"
#include "WindowScripts.inc"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// ui_islandLobbyController.cpp of the main app (SimTheme Park 2.04b3), unit w32: the island lobby's controller and the golden
// key display, plus the mail icon (UpdateOutboxMailIcon, which the inventory files under ui_onlineOutbox.cpp). The controller is a
// small window tree built under the lobby's window; it shows the golden keys the player holds, the keys the current island
// costs, the island's name and two buttons. The island screen creates, shows and updates it (src/lobby_islands.cpp).

namespace {
constexpr int32_t kMailIcon = 0xbf432;   // the lobby window's mail icon

}  // namespace

// The lobby window's mail icon is shown while the player's outbox holds mail (a word of the outbox: the same test as the lobby's
// start). The user info is made on first use.
void UpdateOutboxMailIcon() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    if (gUserInfo->mOutbox.mF28.empty()) gLobby->mWindow->GetWindow(kMailIcon)->Show(0);
    else gLobby->mWindow->GetWindow(kMailIcon)->Show(1);
}

// The online outbox panel's helpers (the inventory files them under ui_onlineOutbox.cpp; unit v5017): the address list's scroll bar,
// the current e-mail's address text, the outbox list and the picture preview.
//
// The outbox cursor functions (Outbox::First and Next) are untranslated, and the Mac's outbox is read by its Mac offsets (the words are
// 32-bit pointers there): a word of the outbox or of an entry is read with Word() below, from the object's address plus the Mac offset.

InterfaceWindow* gOutboxWindow;      // 0x10395e3c
uint8_t* gOutboxCursor;              // 0x10395e38
uint32_t gOutboxPreview[9];          // 0x10395e10
const char16_t gEmailSeparators[3] = {0, 0x000a, 0};   // 0x1024eeca

namespace {

// The word of a Mac object at Mac offset `offset`. On the host a Mac word (a pointer or a number) is a slot of 8 bytes, so the slot
// sits at twice the Mac offset; the value is the slot's full width (pointers are 64 bits here).
uintptr_t Word(const void* object, uint32_t offset) {
    return *reinterpret_cast<const uintptr_t*>(reinterpret_cast<const uint8_t*>(object) + 2 * offset);
}
const void* Ptr(const void* object, uint32_t offset) { return reinterpret_cast<const void*>(Word(object, offset)); }

uint8_t* OutboxOf(CFSUserInfo* info) { return reinterpret_cast<uint8_t*>(&info->mOutbox); }

// The user info is made on first use (the same test as UpdateOutboxMailIcon).
CFSUserInfo* UserInfo() {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo;
    return gUserInfo;
}

}  // namespace

// 0x101e378c: one list row for each mail of the outbox: the address text (into gUIMainConvertBuffer), and the three numbers of the current
// mail formatted by string 0x1c0 of the interface table (variable tags 0x10, 0x11, 0x12); the row's data word is its number | 0x1000000.
void FillOutboxList() {
    Outbox* outbox = &UserInfo()->mOutbox;
    uint8_t* cursor = reinterpret_cast<uint8_t*>(outbox) + 2 * 4;   // the Mac's outbox + 4
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
    outbox->First();
    uint32_t index = 0;
    do {
        const char* text = static_cast<const char*>(Ptr(Ptr(cursor, 4), 0xc));
        NS_MacDoze::MultiByteToWideChar(0, 0, text, -1, gUIMainConvertBuffer, 0x3fff);
        char16_t formatted[32];
        TbIntegerVTag first(0x10, static_cast<int32_t>(Word(cursor, 0x18)));
        TbIntegerVTag second(0x11, static_cast<int32_t>(Word(cursor, 0x1c)));
        TbIntegerVTag third(0x12, static_cast<int32_t>(Word(cursor, 0x20)));
        static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(formatted, 0x1f, 0x1c0, &first, &second, &third);
        UI_LCAdd item[4];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[2].mRaw = 0;
        item[3].mRaw = 0;
        item[0].mText = gUIMainConvertBuffer;
        item[1].mText = formatted;
        item[2].mInt = 1;
        item[3].mInt = 0;
        list->AddItem(item, static_cast<int32_t>(index | 0x1000000u), -1, 0);
        ++index;
    } while (outbox->Next());
    list->UpdateDisplay();
}

// 0x101e3960: the outbox goes to mail number `number` (the two top bits of the 6 bits at 0x03000000 are cleared, so the count
// is the low 24 bits and bits 26 up), the cursor is kept, and gPostcardText5da5 becomes the addresses of that mail, each followed by a newline.
void SetCurrentEmailInfo(int32_t number) {
    Outbox* outbox = &UserInfo()->mOutbox;
    gOutboxCursor = reinterpret_cast<uint8_t*>(outbox) + 2 * 4;   // the Mac's outbox + 4
    outbox->First();
    for (uint32_t count = static_cast<uint32_t>(number) & 0xfcffffffu; count != 0; --count) outbox->Next();
    uintptr_t node = Word(gOutboxCursor, 0x10);
    wcscpy(gPostcardText5da5, gEmailSeparators);
    while (node != reinterpret_cast<uintptr_t>(gOutboxCursor) + 2 * 0xc) {
        const char* text = static_cast<const char*>(Ptr(Ptr(reinterpret_cast<const void*>(node), 8), 0xc));
        NS_MacDoze::MultiByteToWideChar(0, 0, text, -1, gUIMainConvertBuffer, 0x3fff);
        wcscat(gPostcardText5da5, gUIMainConvertBuffer);
        wcscat(gPostcardText5da5, gEmailSeparators + 1);
        node = Word(reinterpret_cast<const void*>(node), 4);
    }
}

// 0x101e3aac: the address text is measured in a box as wide as the client rectangle and as high as it, in the interface's units (the
// real screen's pixels over 2048 and over 1536); when the text is higher than the client area the scroll bar is shown with the excess
// as its range, else it is hidden. A bar that does not move (SetValue(0) answers 0) leaves the window to take the text again.
void CalcEmailAddressScrollBar() {
    InterfaceWindow* window = gOutboxWindow->GetWindow(0xee2);
    InterfaceScrollBar* bar = static_cast<InterfaceScrollBar*>(window->GetWindow(0xee3));
    CoolRect client;
    window->GetClientRect(&client);
    const int32_t high = (client.mBottom - client.mTop) * static_cast<int32_t>(gUIRealScreenHeight);
    int32_t scaled = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * high) >> 32) >> 8;   // mulhw, srawi 8
    scaled = scaled + static_cast<int32_t>(static_cast<uint32_t>(scaled) >> 31);
    const int32_t wide = (client.mRight - client.mLeft) * static_cast<int32_t>(gUIRealScreenWidth);
    TbRect limit;
    limit.left = 0;
    limit.top = 0;
    limit.right = wide / 2048;
    limit.bottom = scaled;
    TbRect size;
    size.left = 0;
    size.top = 0;
    size.right = 0;
    size.bottom = 0;
    UI_MeasureText(gPostcardText5da5, limit, 6, 0, 0, 1, &size);
    const int32_t needed = size.bottom * 0x600 / static_cast<int32_t>(gUIRealScreenHeight);
    const int32_t shown = client.mBottom - client.mTop;
    if (needed > shown) {
        bar->Show(1);
        bar->SetMinMax(needed - shown, 0);
    } else {
        bar->Show(0);
    }
    if (static_cast<int16_t>(bar->SetValue(0)) == 0) window->SetText(gPostcardText5da5);
}

// 0x101e3c94: the current mail's picture (a JPEG file named by the entry the cursor holds) is read into a bitmap and scaled into the
// client surface of the preview window (filter 2 for texture formats 2 and 4, 1 for 3 and 5, none for the others).
void UpdatePreview() {
    TbJPEGLoader loader;
    TbRenderArea area;
    InterfaceWindow* window = gOutboxWindow->GetWindow(0xee4);
    int32_t width;
    int32_t height;
    uint8_t* surface = window->GetClientObjectSurface(&width, &height);
    if (surface == nullptr) return;
    PostcardBitmap bitmap(gOutboxPreview[7], gOutboxPreview[8], 4);
    typedef decltype(SamsUtilities::kPixels32ARGB) Format;   // the format numbers 7 and 9 of the listing
    SamsUtilities::UPixels source(static_cast<Format>(7), gOutboxPreview[7], gOutboxPreview[8], nullptr);
    SamsUtilities::UPixels dest(static_cast<Format>(9), bitmap.mPixels, gOutboxPreview[7], gOutboxPreview[8], -1, nullptr);
    char* path = const_cast<char*>(static_cast<const char*>(Ptr(Ptr(gOutboxCursor, 0x14), 0xc)));
    read_JPEG_file(path, reinterpret_cast<uint8_t*>(source.GetData()));
    dest.Convert(source, false);
    switch (gei_TextureSettings.mFormat) {
        case 2:
        case 4:
            bitmap.resize_into_buffer(surface, width, height, 2);
            break;
        case 3:
        case 5:
            bitmap.resize_into_buffer(surface, width, height, 1);
            break;
        default:
            break;
    }
    window->FinishWithClientObjectSurface();
}

// Unit v5374: the outbox panel's column click (ui_onlineOutbox.cpp).

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

// ui_onlineOutbox.cpp (main app): the outbox window's sending status and the marks on the outbox mails (unit v5031).

InterfaceWindow* gOutboxSendingWindow;   // 0x10395e0c: the sending window, NULL while there is none
int32_t gMailStatusShownB;       // 0x10395e00
int32_t gMailStatusShownA;       // 0x10395e08
int32_t gMailStatusB;            // 0x10395dfc
int32_t gMailStatusA;            // 0x10395e04

// 0x101e4004: marks the mail `index` (the low 24 bits and the top 6, the two flag bits masked out) of the outbox: flag 0x1000000
// marks it to send (answer 1), else flag 0x2000000 marks it to delete (answer 0). A huge index walks the cursor that many times.
int32_t MarkOutboxEmail(int32_t flags) {
    int32_t count = flags & static_cast<int32_t>(0xFCFFFFFFu);
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
    Outbox* outbox = &gUserInfo->mOutbox;   // 0x64
    outbox->First();
    while (count != 0) {
        outbox->Next();
        count--;
    }
    outbox->UnmarkToSend();
    outbox->UnmarkToDelete();
    if ((flags & 0x1000000) != 0) {
        outbox->MarkToSend();
        return 1;
    }
    if ((flags & 0x2000000) != 0) outbox->MarkToDelete();
    return 0;
}

// Unit v51067: the small functions of the online UI between 0x101e0388 and 0x101ec444 (callbacks, remove / show / hide wrappers of the
// online panels, the contacts row functions, the string initialisers of three online UI files). Mac addresses in the comments.

// 0x101e4124, 20 bytes.
void OutboxCallback(EmailCompletionCode code, EmailErrorCode error) {
    gMailStatusA = code;
    gMailStatusB = error;
}

// 0x101e4184: when a window is set and the pair of numbers changed, the new pair is kept and the window hears message 4
// (both arms of the original's test post the same message).
void SendMailStatusDisplay() {
    InterfaceWindow* window = gOutboxSendingWindow;
    if (window == nullptr) return;
    int32_t a = gMailStatusA;
    if (gMailStatusShownA == a && gMailStatusShownB == gMailStatusB) return;
    gMailStatusShownA = gMailStatusA;
    gMailStatusShownB = gMailStatusB;
    if (a == 0) {
        UI_PostMessage(window, 4, 0, 0);
    } else {
        UI_PostMessage(window, 4, 0, 0);
    }
}

// 0x101e4250: message 0x1e shows the status (answer 0); message 0x14 forgets the window, makes the user info when there is none
// and, when its outbox word at 0x8c is 0, shows the lobby window's child 0xbf432; then (and for every other message) the dialog
// box callback answers.
int32_t OutboxSendingMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        SendMailStatusDisplay();
        return 0;
    }
    if (message == 0x14) {
        gOutboxSendingWindow = nullptr;
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
        if (gUserInfo->mOutbox.mF28.size() == 0) {   // the Mac's word at 0x8c
            InterfaceWindow* child = gLobby->mWindow->GetWindow(0xbf432);
            child->Show(0);
        }
    }
    return DialogBoxCallback(window, message, a, b);
}

// ui_onlineOutbox.cpp (main app): the outbox panel's message function (OutboxPanelMsg 0x101e458c, 1336 bytes) and the function that
// empties the outbox and sends its mail (SendAndDeleteOutbox 0x101e4378, 488 bytes). Unit v5239.

extern OnlineConfig gOnlineConfig;   // 0x102b2b70 (include/TPMain.h)

// 0x10268500: the TVector of OutboxCallback (0x101e4124, untranslated). Only its address is used: the outbox's word at 0x58 holds it.
uint32_t gOutboxCallbackVector[2];

// 0x101e4378: makes the user info when there is none, gives the outbox the callback vector and deletes the outbox's mail. When `send`,
// sends the mail to the configured mail server (the EA FTP server on port 25 when none is set). A sent mail resets the four status
// words and shows the sending dialog (its window kept in gOutboxSendingWindow, handled by OutboxSendingMsg); a failure shows the
// lobby window's child 0xbf432 when the outbox's list at 0x8c is empty, else the error 0x39.
void SendAndDeleteOutbox(bool send) {
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
    Outbox* outbox = &gUserInfo->mOutbox;   // the Mac's user info + 0x64
    dlgboxparams params;
    params.mText = UIMain_GetString(0x13a);
    params.mRed = 0xff;
    params.mGreen = 0xff;
    params.mBlue = 0xff;
    params.mValue8 = 4;
    params.mValueC = 0;
    outbox->mF58 = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOutboxCallbackVector));
    outbox->Delete();
    if (!send) return;

    int32_t sent;
    const char* server = gOnlineConfig.mMailServer.c_str();
    if (server[0] == '\0')
        sent = outbox->Send("ftp.ea-europe.com", 0x19, nullptr);
    else
        sent = outbox->Send(server, gOnlineConfig.mMailPort, nullptr);

    if (static_cast<uint8_t>(sent) != 0) {
        gMailStatusShownA = -1;
        gMailStatusA = -1;
        gMailStatusShownB = -1;
        gMailStatusB = -1;
        gOutboxSendingWindow = UI_DialogBox(params, static_cast<dlgbutton>(0), nullptr, static_cast<dlgbutton>(0), nullptr,
                                            static_cast<dlgbutton>(0), nullptr, false);
        gOutboxSendingWindow->SetCallbackFunction(OutboxSendingMsg);
    } else {
        if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
        if (gUserInfo->mOutbox.mF28.size() == 0) {   // the Mac's word at 0x8c
            InterfaceWindow* child = gLobby->mWindow->GetWindow(0xbf432);
            child->Show(0);
        } else {
            AddUIError(0x39, nullptr);
        }
    }
}

namespace {

// The three loops of the original (messages 0xee5, 0xee6, 0xee7): every entry's data word loses its bits 0x03000000, gains `setBits`
// and is written back; the entry's columns 2 and 3 show the two bits.
void MarkAll(int32_t setBits) {
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
    int32_t count = list->mItemCount;
    for (int32_t i = 0; i < count; ++i) {
        int32_t flags;
        list->GetData(i, &flags);
        flags = (flags & static_cast<int32_t>(0xFCFFFFFF)) | setBits;
        UI_LCAdd item[4] = {};
        item[2].mInt = (flags & 0x01000000) == 0x01000000;
        item[3].mInt = (flags & 0x02000000) == 0x02000000;
        list->ChangeItem(i, item, 0xc, 0, -1);
        list->SetData(i, flags);
    }
    list->UpdateDisplay();
}

}  // namespace

// 0x101e458c: message 0x15 tells the lobby window 6 (0, 0); 0x14 forgets the panel, frees the preview and tells it 6 (1, 0); 0x401
// selects the e-mail of the clicked row; 0x404 is a column click; 0x100 is a button: 0xee5 marks every mail to send, 0xee6 to delete,
// 0xee7 clears the marks, and -1 (send) counts the marked mails, checks the online system can send, sends and fetches the news (answer
// 0). Every other path ends in the dialog box callback.
int32_t OutboxPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == 0xee5) {
            MarkAll(0x01000000);
        } else if (a == 0xee6) {
            MarkAll(0x02000000);
        } else if (a == 0xee7) {
            MarkAll(0);
        } else if (a == -1) {
            int32_t marked = 0;
            if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
            int32_t count = list->mItemCount;
            for (int32_t i = 0; i < count; ++i) {
                int32_t data;
                list->GetData(i, &data);
                marked += MarkOutboxEmail(data);
            }
            bool ok;
            if (marked <= 0) {
                ok = true;
            } else {
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
                if (gOnlineSystem->CanWeSendEmail() != 0) {
                    ok = true;
                } else {
                    ok = false;
                    AddUIError(0x4e, nullptr);
                }
            }
            if (ok) {
                UI_SendMessage(window, 4, 0, 0);
                SendAndDeleteOutbox(marked != 0);
                if (gOnlineSystem == nullptr) gOnlineSystem = new CFSOnlineSystem();
                gOnlineSystem->LoggedInGetNews();
                return 0;
            }
        }
    } else if (message < 0x100) {
        if (message == 0x15) {
            UI_SendMessage(gLobby->mWindow, 6, 0, 0);
        } else if (message == 0x14) {
            gOutboxWindow = nullptr;
            reinterpret_cast<TbRenderArea*>(gOutboxPreview)->FreeMemory();
            UI_SendMessage(gLobby->mWindow, 6, 1, 0);
            UIMain_DecideKbCapture();
        }
    } else if (message == 0x404) {
        ColumnClick(b);
    } else if (message == 0x401) {
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(a));
        if (list != nullptr && b != -1) {
            int32_t data;
            list->GetData(b, &data);
            SetCurrentEmailInfo(data);
            CalcEmailAddressScrollBar();
            UpdatePreview();
        }
    }
    return DefaultDialogCallback(window, message, a, b);
}

// Unit v5981: InterfaceWindow::link (0x101ddf20), the island lobby controller's static initialiser (0x101e1e3c), the online login panel's
// message function (0x101e26fc), the outbox address panel's message function (0x101e4b00) and the JPEG loader (0x101e538c).

// 0x101e4b00: message 0x800 (scroll) moves the text up by b and has the window's virtual at vtable offset 0x2c (SetText) show the
// shared postcard text again; every message then goes to the default handler.
int32_t AdressesTextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x800) {
        window->SetTextOffset(0, 0 - b);
        window->SetText(gPostcardText5da5);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// The online outbox panel (ui_onlineOutbox.cpp, main app; unit v5096): CreateOnlineOutboxPanel 0x101e4f28 and its list's row functions,
// LCFormat 0x101e4bdc and LCPostFormat 0x101e4d20 (named OutboxLCFormat and OutboxLCPostFormat here: the same names exist at other addresses).

// 0x101e4bdc: a row window as high as the font (in the 1536 units, plus 6) and 100 wide, with three objects made in it.
// The font's height is a virtual call on the Mac (slot at 0xc of the vtable at 0x10), answered as a short.
InterfaceWindow* OutboxLCFormat() {
    InterfaceFont* font = GetFontTableEntry(7);
    int32_t height = static_cast<int16_t>(font->GetHeight());
    int32_t bottom = height * 0x600 / gUIRealScreenHeight + 6;
    InterfaceWindow* window = new InterfaceWindow(nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(bottom));
    if (window != nullptr) {
        window->CreateObject(1, 1, 1, 100, 0, 200, 50);
        window->CreateObject(6, 1, 2, 100, 0, 200, 50);
        window->CreateObject(6, 1, 2, 100, 0, 200, 50);
    }
    return window;
}

// 0x101e4d20: the row gets a text object, white, font 7; its first child a centred wrapped text object; the next two children
// the "i_boxtick" picture.
void OutboxLCPostFormat(InterfaceWindow* window) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
    window->SetFont(GetFontTableEntry(7));
    window->SetClientDrawText(nullptr);
    InterfaceWindow* child = window->GetChild(0);
    sprite = new InterfaceTextObjectSprite(nullptr);
    child->SetClientDrawObject(sprite);
    child->SetFontJust(1, 1);
    child->SetFontWrap(1, 1);
    child->SetFont(GetFontTableEntry(7));
    child->SetFontColour(0xff, 0xff, 0xff, 0xff);
    child->SetClientDrawText(nullptr);
    child = child->GetNext();
    child->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
    child = child->GetNext();
    child->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("i_boxtick")));
}

// 0x101e4f28: see lobby.h.
int32_t CreateOnlineOutboxPanel() {
    int32_t result = 0;
    if (gUserInfo == nullptr) gUserInfo = new CFSUserInfo();
    if (gUserInfo->mOutbox.mF28.size() != 0) {   // the Mac's word at 0x8c
        gOutboxWindow = Dialog(gOutboxPanelData, OutboxPanelMsg, 1);
        InterfaceWindow* window = gOutboxWindow->GetWindow(0xee2);
        window->SetClientDrawObject(new InterfaceTextObjectSprite(nullptr));
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
        window->SetFontWrap(1, 0);
        window->SetFontJust(0, 0);
        window->SetCallbackFunction(AdressesTextPanelMsg);
        window = gOutboxWindow->GetWindow(0xee4);
        window->SetClientDrawObject(new InterfaceDrawObjectSprite(nullptr));
        TbRenderArea area(TbRenderArea::kCleared);   // the Mac's is not constructed; its address is all AllocMemory gets
        area.AllocMemory();
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gOutboxWindow->GetWindow(0xee8));
        list->SetColumnFormatsAll(OutboxLCFormat, OutboxLCPostFormat);
        list->SetColumnType(0, kLCText);
        list->SetColumnType(1, kLCText);
        list->SetColumnType(2, kLCNumber);
        list->SetColumnType(3, kLCNumber);
        list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
        MAKECOLUMNBUTTON(list->GetWindow(0x10), const_cast<char16_t*>(UIMain_GetString(0x136)), 8, 0xff, 0xef, 0, 0x187);
        MAKECOLUMNBUTTON(list->GetWindow(0x11), const_cast<char16_t*>(UIMain_GetString(0x137)), 8, 0xff, 0xef, 0, 0x188);
        MAKECOLUMNBUTTON(list->GetWindow(0x12), const_cast<char16_t*>(UIMain_GetString(0x138)), 8, 0xff, 0xef, 0, 0x189);
        MAKECOLUMNBUTTON(list->GetWindow(0x13), const_cast<char16_t*>(UIMain_GetString(0x139)), 8, 0xff, 0xef, 0, 0x18a);
        UIMain_SetTitleText(gOutboxWindow, 0xee9, const_cast<char16_t*>(UIMain_GetString(0x135)));
        FillOutboxList();
        gOutboxWindow->CaptureKb(0);
        result = 1;
        gLobbyScreens->mAdvisor->Add(AdvisorResponse{0x255}, 1);
    }
    return result;
}

// ui_onlineOutbox.cpp / ui_onlineParkFindPanel.cpp leftovers (unit v5579): the chat panel's postcard hand-over, the outbox
// file's static initialiser and the JPEG error handler.

extern "C" void __register_global_object(void* object, void* destructor, void* node);
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue (src/OnlineParkInfoPanelMsg.cpp)

alignas(16) uint8_t gOutboxStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395e40 (0x14 bytes on the Mac)
alignas(8) uint8_t gOutboxStringNode[24];       // 0x10395e54: the global object's registration node

// 0x101e55ec
namespace init_callee {
void LockChatPanelOff() {
    UI_SendMessage(gOnlinePanel, 6, 0, 0);
    UI_SendMessage(gOnlineWindow, 6, 1, 0);
    gOnlineBlockToggle = 1;
    gOnlineWindow->CaptureKb(0);
}
}  // namespace init_callee

// 0x101e5310: the error handler of the JPEG loader: prints through the manager's output_message (+8), then jumps out (jump buffer at +0x84).
void my_error_exit(jpeg_common_struct* cinfo) {
    uint8_t* err = cinfo->err.get();
    outbox_callee::ErrorOutput(*reinterpret_cast<uint32_t*>(err + 8), cinfo);
    outbox_callee::JpegLongJmp(err + 0x84, 1);
}

namespace outbox_callee {
// Stand-in for the import (setjmp): the launcher's build has no way to call it, so it answers "first return".
__attribute__((weak)) int32_t JpegSetJmp(void*) { return 0; }
}  // namespace outbox_callee

// 0x101e538c: decodes the JPEG file `path` into `pixels`, a row of output_width * output_components bytes per scanline. The Mac keeps
// the error manager (0x84 bytes), the jump buffer (0x11c) and the decompressor (0x1d0) together on the stack.
void read_JPEG_file(char* path, uint8_t* pixels) { read_JPEG_fileChecked(path, pixels); }
int32_t read_JPEG_fileChecked(char* path, uint8_t* pixels) {
    uint8_t* rows = pixels;
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        fprintf(reinterpret_cast<FILE*>(gIfsWFiles + 0xa8), "can't open %s\n", path);
        return 0;
    }
    struct {
        jpeg_error_mgr mgr;
        uint8_t jumpBuffer[0x11c];
        jpeg_decompress_struct cinfo;
    } frame;
    frame.cinfo.err = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(jpeg_std_error(&frame.mgr)));
    *reinterpret_cast<uint32_t*>(frame.mgr.mBytes) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_error_exit));
    if (outbox_callee::JpegSetJmp(frame.jumpBuffer) != 0) {
        jpeg_destroy_decompress(&frame.cinfo);
        fclose(file);
        return 0;
    }
    jpeg_CreateDecompress(&frame.cinfo, 0x3e, 0x1d0);
    jpeg_stdio_src(&frame.cinfo, file);
    jpeg_read_header(&frame.cinfo, 1);
    jpeg_start_decompress(&frame.cinfo);
    uint32_t components;
    std::memcpy(&components, reinterpret_cast<uint8_t*>(&frame.cinfo) + 0x7c, 4);   // output_components, inside f78
    uint32_t stride = frame.cinfo.output_width * components;
    while (frame.cinfo.output_scanline < frame.cinfo.output_height) {
        jpeg_read_scanlines(&frame.cinfo, &rows, 1);
        rows += stride;
    }
    jpeg_finish_decompress(&frame.cinfo);
    jpeg_destroy_decompress(&frame.cinfo);
    fclose(file);
    return 1;
}

// 0x101e551c
void __sinit_ui_onlineOutbox_cpp() {
    const char16_t* text = reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gEmailSeparators) + 6);
    TbDynamicStringTemplate<char16_t>* string = new (gOutboxStringStorage) TbDynamicStringTemplate<char16_t>(text);
    __register_global_object(string, reinterpret_cast<void*>(&UIMain_DestroyWString), gOutboxStringNode);
    TbSize size{0x280, 0xf0};
    TbPixFormat format(0xff0000, 0xff00, 0xff, 0xff000000u, 0x20, 0);
    new (gOutboxPreview) TbRenderArea(size, format, nullptr, 0);
}

// Window scripts that InterfaceWindow::Build reads and that no translated file defined (unit v5L42): the launcher had all-zero
// stand-ins for them, so opening the panel failed the lpNew assertion in ProcessLevel. A file of their own because each building
// function's unit test defines its array with its fakes; the words are the image's, generated at configure time
// (devtools/gen_game_tables.py, WindowScripts.inc).

int16_t gOutboxPanelData[] = {TPW_WINDOW_SCRIPT_102B05C4};         // 0x102b05c4, CreateOnlineOutboxPanel 0x101e4f28
