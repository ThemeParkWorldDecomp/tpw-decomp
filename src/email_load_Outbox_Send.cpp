// Outbox::Send 0x102059fc (1196 bytes) and Outbox::NextMail 0x10205ed0 (892 bytes), unit v5218.
// Both fill the e-mail the outbox is writing from the login (address, ids, name, password), the recipients (mF0C), the subject
// (mF08) and the attachment path (mF18). Send first makes or resets the e-mail and sends it; NextMail first drops the current mail
// from the three lists and fills the caller's e-mail.
#include <cstdint>
#include <cstring>
#include <list>
#include <string>

#include "email.h"
#include "Ptr32.h"

namespace {

// The data at TOC slot -0x3b00 (0x1024faeb): the default sender "nobody@nowhere.net" (19 bytes with its NUL), and 0x13 bytes on a
// second string.
const char kDefaultSender[] = "nobody@nowhere.net";
const char kSecondText[] = "";

// Email::Attachment as Send's reset reads it (0xc bytes on the Mac): two strings and an array pointer.
struct AttachmentWords {
    uint32_t mString00;
    uint32_t mString04;
    Ptr32<uint8_t> mData;   // 0x08  new[]'d array
};

uint32_t WideLength(const char16_t* text) {   // wcslen 0x102389c8
    uint32_t n = 0;
    while (text[n] != 0) n++;
    return n;
}

void MakeOnlineSystem() {   // the lazy "if (!gOnlineSystem) gOnlineSystem = new CFSOnlineSystem" the listing repeats
    if (gOnlineSystem == nullptr) {
        void* sys = email_callee::OperatorNew(0x404);
        if (sys != nullptr) outbox_callee::OnlineSystemCtor(sys);
        gOnlineSystem = static_cast<CFSOnlineSystem*>(sys);
    }
}

// The common tail: the login's words, the recipients, the subject and the attachment. Answers 0 when the attachment was not made.
int32_t FillEmail(Outbox* outbox, Email* email) {
    char address[140];
    MakeOnlineSystem();
    const char16_t* source = outbox_callee::LoginEmailAddress(gOnlineSystem);
    char* out = address;
    char c;
    do {
        c = static_cast<char>(*source++);
        *out++ = c;
    } while (c != 0);
    email_callee::StringAssign(&email->mString00, address, static_cast<uint32_t>(std::strlen(address)));
    MakeOnlineSystem();
    int32_t playerId = outbox_callee::LoginPlayerID(gOnlineSystem);
    std::memcpy(&email->mF18[0], &playerId, 4);
    MakeOnlineSystem();
    int32_t customerId = outbox_callee::LoginCustomerID(gOnlineSystem);
    std::memcpy(&email->mF18[4], &customerId, 4);
    MakeOnlineSystem();
    const char16_t* name = outbox_callee::LoginCustomerName(gOnlineSystem);
    outbox_callee::WStringAssign(&email->mWString20, name, WideLength(name));
    MakeOnlineSystem();
    const char16_t* password = outbox_callee::LoginPassword(gOnlineSystem);
    outbox_callee::WStringAssign(&email->mWString24, password, WideLength(password));
    int32_t nine = 9;
    std::memcpy(email->mF28, &nine, 4);
    for (const std::string& recipient : outbox->mF0C) {
        uint32_t temp = 0;
        outbox_callee::StringCtor(&temp, recipient.c_str());
        outbox_callee::ListInsert(&email->mListSize, &email->mListPrev, &temp);
        email_callee::StringDtor(&temp, -1);
    }
    const char* subject = outbox->mF08.c_str();
    email_callee::StringAssign(&email->mString10, subject, static_cast<uint32_t>(std::strlen(subject)));
    const char* path = outbox->mF18.c_str();
    void* attachment = email_callee::OperatorNew(0xc);
    if (attachment != nullptr) email_callee::AttachmentCtor(attachment, path);
    email->mAttachment = static_cast<uint8_t*>(attachment);
    return email->mAttachment ? 1 : 0;
}

}  // namespace

// Answers 1 when the e-mail went out (Email::Send answered 0), else 0. Needs a mail waiting in mF34.
int32_t Outbox::Send(const char* server, int16_t port, char* extra) {
    if (mF34.empty()) return 0;
    Email* email = mEmail;
    if (email == nullptr) {
        void* mem = email_callee::OperatorNew(0x4c);
        if (mem != nullptr) outbox_callee::EmailCtor(mem);
        mEmail = static_cast<Email*>(mem);
        if (mEmail == nullptr) return 0;
    } else {
        email_callee::StringAssign(&email->mString00, kDefaultSender, static_cast<uint32_t>(std::strlen(kDefaultSender)));
        email_callee::ListErase(&email->mListSize, email->mListNext, &email->mListPrev);
        uint8_t* attachment = email->mAttachment;
        if (attachment != nullptr) {
            AttachmentWords* words = reinterpret_cast<AttachmentWords*>(attachment);
            if (words->mData) {
                outbox_callee::OperatorDeleteArray(words->mData.get());
                words->mData = nullptr;
            }
            email_callee::StringDtor(&words->mString04, -1);
            email_callee::StringDtor(&words->mString00, -1);
            email_callee::OperatorDelete(attachment);
            email->mAttachment = nullptr;
        }
        email_callee::StringAssign(&email->mString38, kSecondText, static_cast<uint32_t>(std::strlen(kSecondText)));
    }
    mF50.assign(server, std::strlen(server));
    mF54 = static_cast<uint16_t>(port);
    mEmail->mCallback = static_cast<uint32_t>(mF58);
    if (!ReadMailData(static_cast<int32_t>(mF34.front()))) return 0;
    if (!FillEmail(this, mEmail)) return 0;
    return mEmail->Send(mF50.c_str(), static_cast<int16_t>(mF54), extra, this) ? 0 : 1;
}

// Drops the sent mail (the first of mF34) from the three lists, deletes its files and saves, then fills the caller's e-mail with the
// next mail. Answers 1 when the next mail was read and the attachment made. The original reads the first node of mF34 before any
// check: an empty mF34 reads the sentinel's neighbouring word; that is not translated (the host list has no such word).
int32_t Outbox::NextMail(Email* email) {
    uint32_t id = mF34.empty() ? 0 : mF34.front();
    mF40.remove(id);
    mF34.remove(id);
    mF28.remove(id);
    DeleteMail(static_cast<int32_t>(id));
    Save();
    if (mF34.empty()) return 0;
    if (!ReadMailData(static_cast<int32_t>(mF34.front()))) return 0;
    return FillEmail(this, email);
}
