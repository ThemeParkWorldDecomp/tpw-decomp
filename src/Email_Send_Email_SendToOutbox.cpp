// Email::SendToOutbox (0x10203010) and Email::BuildBody (0x102031a8), unit v5162.
// The Email is a block of 32-bit words at the Mac's offsets (see email.h): a string is a word (arena offset) of an MSL string
// representation whose word at +0xc is the arena offset of the characters; the address list is the MSL list whose sentinel is at +8.
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "email.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

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
char* Chars(uint32_t stringWord) { return reinterpret_cast<char*>(tpw::gArenaBase + Word(stringWord + 0xc)); }

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
    const char* text = Chars(mString00);
    email_callee::StringAssign(&outbox->mF04, text, static_cast<uint32_t>(std::strlen(text)));
    text = Chars(mString10);
    email_callee::StringAssign(&outbox->mF08, text, static_cast<uint32_t>(std::strlen(text)));
    const uint32_t sentinel = tpw::ArenaOffset(this) + 8;
    for (uint32_t node = mListNext; node != sentinel; node = Word(node + 4)) {
        uint32_t local[3] = {0, 0, 0};   // the temporary std::string
        outbox_callee::StringCtor(local, Chars(Word(node + 8)));
        outbox_callee::ListInsert(&outbox->mF0C, &outbox->mF0C, local);   // before the sentinel
        email_callee::StringDtor(local, -1);
    }
    text = Chars(mString14);
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

    CutLine(Chars(mString00));
    std::sprintf(line, kFromFormat, Chars(mString00));
    Append(&mString38, line);

    Append(&mString38, kToText);
    const uint32_t sentinel = tpw::ArenaOffset(this) + 8;
    uint32_t node = mListNext;
    while (node != sentinel) {
        CutLine(Chars(Word(node + 8)));
        std::sprintf(line, kStringFormat, Chars(Word(node + 8)));
        Append(&mString38, line);
        node = Word(node + 4);
        if (node != sentinel) Append(&mString38, kCommaText);
    }
    Append(&mString38, kNewline);

    CutLine(Chars(mString10));
    std::sprintf(line, kSubjectFormat, Chars(mString10));
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
        HexName(line, Chars(mWString20));
        Append(&mString38, line);
    }
    if (Word(mWString24) != 0) {
        Append(&mString38, kPasswordText);
        HexName(line, Chars(mWString24));
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
        std::sprintf(line, kAttachTypeFormat, Chars(Word(attachment + 4)), Chars(Word(attachment)));
        Append(&mString38, line);
        Append(&mString38, kBase64Text);
        std::sprintf(line, kLocationFormat, Chars(Word(attachment)));
        Append(&mString38, line);
        std::sprintf(line, kDispositionFormat, Chars(Word(attachment)));
        Append(&mString38, line);
        std::sprintf(line, kAttachIdFormat, Chars(Word(attachment)));
        Append(&mString38, line);
        Append(&mString38, reinterpret_cast<const char*>(tpw::gArenaBase + Word(attachment + 8)));
    }
    std::sprintf(line, kEndFormat, boundary);
    Append(&mString38, line);
}
