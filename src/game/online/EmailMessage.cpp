// EmailMessage.cpp: guessed name (the binary has no __sinit for this file): the Email class (one message:
// AttachData, Send, Callback, Reset) with its socket send and base64 encoder. Email.cpp would differ from the
// proven email.cpp next to it only in case, which a case-insensitive file system cannot hold.

#include "email.h"
#include "FinanceGraphPanelMsg.h"
#include "FS_Online.h"
#include "LbDrawPrimitives.h"
#include "platform/arena.h"
#include "platform/samsutils.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Unit v51015: base64enc 0x10202114, Email::Attachment::Attachment 0x10202670, Connection::Connection 0x10201a84,
// Connection::SendCommandLine 0x10201b14, CUIGrapher::DrawAxes 0x101fda4c.

namespace {

// The alphabet at 0x102b16c4 (TOC slot -0x3afc).
const char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

}  // namespace

void base64enc(unsigned char* out, unsigned char* in, int count) {
    if (out == nullptr) return;
    if (in == nullptr) return;
    int column = 0;
    while (count > 0) {
        uint32_t word = static_cast<uint32_t>(*in++) << 8;
        if (count > 1) word |= *in++;
        word <<= 8;
        if (count > 2) word |= *in++;
        *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 18]);
        word &= 0x3ffff;
        *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 12]);
        word &= 0xfff;
        if (count > 1) {
            *out++ = static_cast<unsigned char>(kBase64Alphabet[word >> 6]);
            word &= 0x3f;
            if (count > 2)
                *out++ = static_cast<unsigned char>(kBase64Alphabet[word]);
            else
                *out++ = '=';
        } else {
            *out++ = '=';
            *out++ = '=';
        }
        column += 4;
        count -= 3;
        if (column == 0x4c) {
            *out++ = '\r';
            *out++ = '\n';
            column = 0;
        }
    }
    *out = 0;
}

// Email_Send.cpp (main app): the e-mail object's destructor, Reset, AttachData and Send (unit v5129).
// Email::~Email 0x1020248c, Email::Reset 0x10202588, Email::AttachData 0x10202950, Email::Send 0x10202ee4.
// The object keeps the Mac's layout as 32-bit words (see include/email.h); pointers are arena addresses.

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

// Email::Email 0x10202244 and Email::Callback 0x10202a54 (unit v5912).

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

// Email::SendToOutbox (0x10203010) and Email::BuildBody (0x102031a8), unit v5162.
// The Email is a block of 32-bit words at the Mac's offsets (see email.h): a string is a word (arena offset) of an MSL string
// representation whose word at +0xc is the arena offset of the characters; the address list is the MSL list whose sentinel is at +8.

namespace {

// The text at TOC slot -0x3b00 (0x1024faeb), from +0x13: the (empty) start of every message, and the formats BuildBody uses.
const char kHeaderText[] = "";
const char kBoundaryFormat[] = "--_BF_TPW_mpmbndry.%08x";
const char kFromFormat[] = "From: %s\r\n";
const char kToText[] = "To: ";
const char kStringFormat[] = "%s";
const char kCommaText[] = ", ";
const char kNewline[] = "\r\n";
const char kSubjectFormat[] = "Subject: %s\r\n";
const char kPlayerIdFormat[] = "X-BOSS-PlayerID: %d\r\n";
const char kCustomerIdFormat[] = "X-BOSS-CustomerID: %d\r\n";
const char kPlayerNameText[] = "X-BOSS-PlayerName:";
const char kHexFormat[] = " %02x";
const char kPasswordText[] = "X-BOSS-Password:";
const char kLangIdFormat[] = "X-BOSS-LangID: %d\r\n";
const char kMimeVersionText[] = "MIME-Version: 1.0\r\n";
const char kMultipartFormat[] = "Content-Type: multipart/related; boundary=\"%s\"; ";
const char kRelatedTypeText[] = "type=\"text/html\"; start=<__BF_HTML_MSG>\r\n\r\n";
const char kMimeNoticeText[] =
    "This message is in MIME format. Since your mail reader does not understand this format, some or all of this message may not be "
    "legible.\r\n";
const char kPartStartFormat[] = "\r\n--%s\r\n";
const char kHtmlTypeText[] = "Content-Type: text/html; charset=\"iso-8859-1\"\r\n";
const char kHtmlIdText[] = "Content-ID: <__BF_HTML_MSG>\r\n\r\n";
const char kAttachStartFormat[] = "\r\n\r\n--%s\r\n";
const char kAttachTypeFormat[] = "Content-Type: %s; name=\"%s\"\r\n";
const char kBase64Text[] = "Content-Transfer-Encoding: base64\r\n";
const char kLocationFormat[] = "Content-Location: %s\r\n";
const char kDispositionFormat[] = "Content-Disposition: attachment; filename=\"%s\"\r\n";
const char kAttachIdFormat[] = "Content-ID: <%s>\r\n\r\n";
const char kEndFormat[] = "\r\n\r\n--%s--\r\n";
// TOC slot -0x3b04 (0x102b1644): the HTML part.
const char kHtmlBody[] =
    "<html>\n<head>\n<title>Theme Park World (TM) Virtual Postcard</title>\n</head>\n<body>\n<img src=\"postcard.jpg\">\n<br></body>\n</html>";

uint32_t Word(uint32_t offset) { return *reinterpret_cast<const uint32_t*>(tpw::gArenaBase + offset); }

// The characters of an MSL string held as a word: the representation's word at +0xc.
char* Chars_2(uint32_t stringWord) { return reinterpret_cast<char*>(tpw::gArenaBase + Word(stringWord + 0xc)); }

// Cuts the text at the first CR or LF (the original blanks them one by one; the scan goes on to the end).
void CutLine(char* text) {
    for (; *text != '\0'; ++text)
        if (*text == '\r' || *text == '\n') *text = '\0';
}

void Append(void* str, const char* text) { sendbody_callee::StringAppend(str, text, static_cast<uint32_t>(std::strlen(text))); }

// The hex dump of a UTF-16 text (high bytes only, as the original reads it): " %02x" for each character, then a newline.
void HexName(char* buffer, const char* text) {
    for (;;) {
        if (text[0] != 0) break;
        if (text[1] == 0) break;
        std::sprintf(buffer, kHexFormat, static_cast<int>(text[1]));
        std::sprintf(buffer + 3, kHexFormat, static_cast<int>(text[0]));
        text += 2;
        buffer += 6;
    }
    std::sprintf(buffer, "%s", kNewline);
}

}  // namespace

int32_t Email::SendToOutbox(Outbox* outbox) {
    if (outbox == nullptr) return 0;
    // The iterators have no host counterpart: the original erases from the list's first node to its sentinel (everything).
    email_callee::ListErase(&outbox->mF0C, 0, &outbox->mF0C);
    const char* text = Chars_2(mString00);
    email_callee::StringAssign(&outbox->mF04, text, static_cast<uint32_t>(std::strlen(text)));
    text = Chars_2(mString10);
    email_callee::StringAssign(&outbox->mF08, text, static_cast<uint32_t>(std::strlen(text)));
    const uint32_t sentinel = tpw::ArenaOffset(this) + 8;
    for (uint32_t node = mListNext; node != sentinel; node = Word(node + 4)) {
        uint32_t local[3] = {0, 0, 0};   // the temporary std::string
        outbox_callee::StringCtor(local, Chars_2(Word(node + 8)));
        outbox_callee::ListInsert(&outbox->mF0C, &outbox->mF0C, local);   // before the sentinel
        email_callee::StringDtor(local, -1);
    }
    text = Chars_2(mString14);
    email_callee::StringAssign(&outbox->mF18, text, static_cast<uint32_t>(std::strlen(text)));
    return outbox->WriteMailData();
}

void Email::BuildBody() {
    char boundary[68];
    char line[132];
    // Microseconds to milliseconds, the low word printed.
    const uint32_t stamp = static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
    std::sprintf(boundary, kBoundaryFormat, stamp);
    email_callee::StringAssign(&mString38, kHeaderText, static_cast<uint32_t>(std::strlen(kHeaderText)));

    CutLine(Chars_2(mString00));
    std::sprintf(line, kFromFormat, Chars_2(mString00));
    Append(&mString38, line);

    Append(&mString38, kToText);
    const uint32_t sentinel = tpw::ArenaOffset(this) + 8;
    uint32_t node = mListNext;
    while (node != sentinel) {
        CutLine(Chars_2(Word(node + 8)));
        std::sprintf(line, kStringFormat, Chars_2(Word(node + 8)));
        Append(&mString38, line);
        node = Word(node + 4);
        if (node != sentinel) Append(&mString38, kCommaText);
    }
    Append(&mString38, kNewline);

    CutLine(Chars_2(mString10));
    std::sprintf(line, kSubjectFormat, Chars_2(mString10));
    Append(&mString38, line);

    int32_t id;
    std::memcpy(&id, &mF18[0], 4);
    std::sprintf(line, kPlayerIdFormat, id);
    Append(&mString38, line);
    std::memcpy(&id, &mF18[4], 4);
    std::sprintf(line, kCustomerIdFormat, id);
    Append(&mString38, line);

    if (Word(mWString20) != 0) {
        Append(&mString38, kPlayerNameText);
        HexName(line, Chars_2(mWString20));
        Append(&mString38, line);
    }
    if (Word(mWString24) != 0) {
        Append(&mString38, kPasswordText);
        HexName(line, Chars_2(mWString24));
        Append(&mString38, line);
    }

    std::memcpy(&id, &mF28[0], 4);
    std::sprintf(line, kLangIdFormat, id);
    Append(&mString38, line);
    Append(&mString38, kMimeVersionText);
    std::sprintf(line, kMultipartFormat, boundary);
    Append(&mString38, line);
    std::sprintf(line, "%s", kRelatedTypeText);
    Append(&mString38, line);
    Append(&mString38, kMimeNoticeText);
    std::sprintf(line, kPartStartFormat, boundary);
    Append(&mString38, line);
    Append(&mString38, kHtmlTypeText);
    Append(&mString38, kHtmlIdText);
    Append(&mString38, kHtmlBody);

    if (mAttachment.mOffset != 0 && Word(mAttachment.mOffset + 8) != 0) {
        const uint32_t attachment = mAttachment.mOffset;
        std::sprintf(line, kAttachStartFormat, boundary);
        Append(&mString38, line);
        std::sprintf(line, kAttachTypeFormat, Chars_2(Word(attachment + 4)), Chars_2(Word(attachment)));
        Append(&mString38, line);
        Append(&mString38, kBase64Text);
        std::sprintf(line, kLocationFormat, Chars_2(Word(attachment)));
        Append(&mString38, line);
        std::sprintf(line, kDispositionFormat, Chars_2(Word(attachment)));
        Append(&mString38, line);
        std::sprintf(line, kAttachIdFormat, Chars_2(Word(attachment)));
        Append(&mString38, line);
        Append(&mString38, reinterpret_cast<const char*>(tpw::gArenaBase + Word(attachment + 8)));
    }
    std::sprintf(line, kEndFormat, boundary);
    Append(&mString38, line);
}
