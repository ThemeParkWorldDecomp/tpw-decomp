// Email_Send.cpp (main app): the e-mail object's destructor, Reset, AttachData and Send (unit v5129).
// Email::~Email 0x1020248c, Email::Reset 0x10202588, Email::AttachData 0x10202950, Email::Send 0x10202ee4.
// The object keeps the Mac's layout as 32-bit words (see include/email.h); pointers are arena addresses.
#include "email.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

// The data at TOC slot -0x3b00 (0x1024faeb): the default sender "nobody@nowhere.net", and 0x13 bytes on, the text Reset puts in the
// string at +0x38 (an empty string).
const char kDefaultText[] = "nobody@nowhere.net\0";

}  // namespace

// The destructor's body; the Mac's destructor takes the delete flag in r4 (deleting form when it is > 0).
Email* Email::Destruct(Email* self, int16_t deleteFlag) {
    if (self != nullptr) {
        self->Reset();
        email_callee::WSACleanup();
        email_callee::StringDtor(&self->mString38, -1);
        email_callee::WStringDtor(&self->mWString24, -1);
        email_callee::WStringDtor(&self->mWString20, -1);
        email_callee::StringDtor(&self->mString14, -1);
        email_callee::StringDtor(&self->mString10, -1);
        email_callee::ListClear(&self->mListSize);
        email_callee::StringDtor(&self->mString00, -1);
        if (deleteFlag > 0) email_callee::OperatorDelete(self);
    }
    return self;
}

Email::~Email() { Destruct(this, 0); }

void Email::Reset() {
    const char* text = kDefaultText;
    email_callee::StringAssign(&mString00, text, static_cast<uint32_t>(std::strlen(text)));
    email_callee::ListErase(&mListSize, mListNext, &mListPrev);
    if (mAttachment.mOffset != 0) {
        email_callee::AttachmentDtor(mAttachment.get(), 1);
        mAttachment.mOffset = 0;
    }
    email_callee::StringAssign(&mString38, text + 0x13, static_cast<uint32_t>(std::strlen(text + 0x13)));
}

bool Email::AttachData(const char* path) {
    void* attachment = email_callee::OperatorNew(0xc);
    if (attachment != nullptr) email_callee::AttachmentCtor(attachment, path);
    mAttachment.set(static_cast<uint8_t*>(attachment));
    return mAttachment.mOffset == 0;
}

bool Email::Send(const char* server, int16_t port, char* extra, Outbox* outbox) {
    mF3d = 1;
    mOutbox.set(outbox);
    if (mOutbox.mOffset != 0) mF3d = 0;
    if (mListSize == 0) {
        if (mCallback != 0) email_callee::PtrGlue(mCallback, 1, 8);
        return true;
    }
    if (mMail.mOffset == 0) {
        void* mail = email_callee::OperatorNew(0x10);
        if (mail != nullptr) email_callee::MailCtor(mail);
        mMail.set(static_cast<uint8_t*>(mail));
        if (mMail.mOffset == 0) return true;
    }
    mF30 = 0;
    int32_t result = email_callee::MailConnect(mMail.get(), const_cast<char*>(server), static_cast<uint16_t>(port),
                                               &email_callee::MailCbfn, this, extra);
    return result == 0;
}
