// Email::Email 0x10202244 and Email::Callback 0x10202a54 (unit v5912).
#include "email.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

// The data at TOC slot -0x3b00 (0x1024faeb): the default sender, and 0x13 bytes on the second text.
const char kEmailTexts[] = "nobody@nowhere.net\0";   // the second text (offset 0x13) is the empty string

uint32_t WordAt(uint32_t offset) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const void*>(tpw::gArenaBase + offset), 4);
    return value;
}

// A string word is the offset of the string object; the object holds the character pointer at +0xc.
const char* Chars(uint32_t stringWord) { return reinterpret_cast<const char*>(tpw::gArenaBase + WordAt(stringWord + 0xc)); }

// The cursor is the unaligned word at +0x3e.
uint32_t Cursor(const Email* self) {
    uint32_t value;
    std::memcpy(&value, reinterpret_cast<const uint8_t*>(self) + 0x3e, 4);
    return value;
}

void SetCursor(Email* self, uint32_t value) { std::memcpy(reinterpret_cast<uint8_t*>(self) + 0x3e, &value, 4); }

const void* FieldAt(uint32_t offset) { return reinterpret_cast<const void*>(tpw::gArenaBase + offset); }

}  // namespace

Email* Email::Construct(Email* self) {
    uint8_t startup[0x190];
    emailcb_callee::StringInit(&self->mString00);
    emailcb_callee::ListInit(&self->mListSize);
    emailcb_callee::StringInit(&self->mString10);
    emailcb_callee::StringInit(&self->mString14);
    const uint32_t zero = 0;
    std::memcpy(self->mF18, &zero, 4);
    emailcb_callee::WStringInit(&self->mWString20);
    emailcb_callee::WStringInit(&self->mWString24);
    const uint32_t nine = 9;
    std::memcpy(self->mF28, &nine, 4);
    self->mAttachment.mOffset = 0;
    self->mMail.mOffset = 0;
    emailcb_callee::StringInit(&self->mString38);
    self->mF3c = 0;
    self->mCallback = 0;
    emailcb_callee::WinsockStart(0x101, startup);
    email_callee::StringAssign(&self->mString00, kEmailTexts, static_cast<uint32_t>(std::strlen(kEmailTexts)));
    return self;
}

void Email::Callback(uint32_t message, int32_t code, uint8_t* data) {
    (void)data;
    uint32_t local = 0;   // r1+0x68
    emailcb_callee::StringInit(&local);
    const uint32_t sentinel = tpw::ArenaOffset(this) + 8;
    if (message != 0x78) {
        if (message == 0xFFFFFFFBu) {
            if (mCallback != 0) email_callee::PtrGlue(mCallback, 1, -1);
        } else if (message == 0xFFFFFFFCu) {
            if (mCallback != 0) email_callee::PtrGlue(mCallback, mF3c == 0 ? 1 : 0, -1);
        }
    } else {
        bool notify = false;
        int32_t status = 5;
        switch (code) {
            case 0x1c2:
            case 0x226:
                notify = true;
                status = 7;
                break;
            case 0x1a5:
                notify = true;
                status = 2;
                break;
            case 0xdd:
                break;
            case 0x228: {
                if (mF30 == 1) {
                    uint32_t node = Cursor(this);
                    SetCursor(this, WordAt(node + 4));
                    uint32_t local2 = 0;   // r1+0x64
                    emailcb_callee::StringSubCtor(&local2, FieldAt(node + 8), 0, 0xFFFFFFFFu);
                    if (Cursor(this) == sentinel) mF30 = mF30 + 1;
                    emailcb_callee::MailDoTo(mMail.get(), Chars(local2));
                    email_callee::StringDtor(&local2, -1);
                }
                notify = true;
                status = 6;
                break;
            }
            case 0xfa:
                switch (static_cast<int32_t>(mF30)) {
                    case 0:
                        mF30 = 1;
                        emailcb_callee::MailDoFrom(mMail.get(), Chars(mString00));
                        SetCursor(this, mListNext);
                        break;
                    case 1: {
                        uint32_t node = Cursor(this);
                        SetCursor(this, WordAt(node + 4));
                        emailcb_callee::StringSubAssign(&local, FieldAt(node + 8), 0, 0xFFFFFFFFu);
                        if (Cursor(this) == sentinel) mF30 = mF30 + 1;
                        emailcb_callee::MailDoTo(mMail.get(), Chars(local));
                        break;
                    }
                    case 2:
                        BuildBody();
                        mF30 = mF30 + 1;
                        emailcb_callee::MailDoData(mMail.get(), Chars(mString38));
                        break;
                    case 3:
                        if (mF3d != 0) {
                            mF3c = 1;
                            emailcb_callee::MailQuit(mMail.get());
                        } else {
                            email_callee::StringAssign(&mString00, kEmailTexts, static_cast<uint32_t>(std::strlen(kEmailTexts)));
                            email_callee::ListErase(&mListSize, mListNext, &mListPrev);
                            uint8_t* attachment = mAttachment.get();
                            if (attachment != nullptr) {
                                uint32_t* words = reinterpret_cast<uint32_t*>(attachment);   // +0 string, +4 string, +8 data
                                if (words[2] != 0) {
                                    outbox_callee::OperatorDeleteArray(Ptr32<uint8_t>{words[2]}.get());
                                    words[2] = 0;
                                }
                                email_callee::StringDtor(&words[1], -1);
                                email_callee::StringDtor(&words[0], -1);
                                email_callee::OperatorDelete(attachment);
                            }
                            mAttachment.mOffset = 0;
                            const char* second = kEmailTexts + 0x13;
                            email_callee::StringAssign(&mString38, second, static_cast<uint32_t>(std::strlen(second)));
                            int32_t result = mOutbox.get()->NextMail(this);
                            if ((result & 0xff) == 0) {
                                mF3c = 1;
                                emailcb_callee::MailQuit(mMail.get());
                            } else {
                                mF30 = 0;
                                emailcb_callee::MailDoReset(mMail.get());
                            }
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                notify = true;
                status = 5;
                break;
        }
        if (notify && mCallback != 0) email_callee::PtrGlue(mCallback, 1, status);
    }
    email_callee::StringDtor(&local, -1);
}
