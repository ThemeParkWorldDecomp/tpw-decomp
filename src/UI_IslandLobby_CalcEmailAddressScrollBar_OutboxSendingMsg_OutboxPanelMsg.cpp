// ui_onlineOutbox.cpp (main app): the outbox panel's message function (OutboxPanelMsg 0x101e458c, 1336 bytes) and the function that
// empties the outbox and sends its mail (SendAndDeleteOutbox 0x101e4378, 488 bytes). Unit v5239.
#include <cstdint>

#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"

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
