// ui_onlineOutbox.cpp (main app): the outbox window's sending status and the marks on the outbox mails (unit v5031).
#include <cstdint>

#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_ErrorDisplay.h"

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
