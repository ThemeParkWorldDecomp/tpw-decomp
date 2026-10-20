// The postcard panel's sending half (ui_Postcard.cpp, main app, unit v5884): IllCallYouBack 0x101b0ca4 (the e-mail completion
// callback), AddressesValid 0x101b0d88, AddToAddressField 0x101b0e98 and SendPostcard 0x101b0f9c.
#include "ui_Postcard.h"
#include <cstring>
#include <new>
#include "Postcard.h"
#include "Game.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "HLSprite.h"
#include "Advisor.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"
#include "email.h"
#include "CMsgEvent.h"
#include "CMessageCentre.h"
#include "FS_UserInfo.h"
#include "platform/macdoze.h"
#include "platform/msl.h"

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
