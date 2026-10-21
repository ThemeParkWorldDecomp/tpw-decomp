// email.cpp: name proven by __sinit_email_cpp in the main application.

#include "email.h"
#include "FS_Online.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "Ptr32.h"
#include "std_basic_filebuf.h"
#include "std_list_uint.h"
#include "TbDynamicStringTemplate.h"
#include "TPMain.h"
#include "UI_pump.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iterator>
#include <list>
#include <new>
#include <string>

// email.cpp range of the main app (SimTheme Park 2.04b3): the player's outbox and address book constructors, and the bullfrog
// thread library's mutex (the inventory files bthread_mutex here; UI_pump.h declares the class). PARTIAL: the outbox's destructor and the
// two loaders are in src/email_load.cpp (unit w90); the rest of the file is untranslated.

// The Mac constructs the members with the MSL constructors, in this order (the offsets of the listing): the strings at 0x00,
// 0x04, 0x08, the list of strings at 0x0c, the string at 0x18, the three lists of numbers at 0x28, 0x34 and 0x40 and the string
// at 0x50; then the short at 0x54 is 25 and the words at 0x58 and 0x5c are 0. The words 0x1c..0x27 and 0x4c are not written.
Outbox::Outbox() : mF00(), mF04(), mF08(), mF0C(), mF18(), mF28(), mF34(), mF40(), mF50(), mF54(25), mF58(0), mEmail(nullptr) {}

// email.cpp range of the main app (SimTheme Park 2.04b3), unit w90: the outbox's destructor 0x10203ba8 (276 bytes), Outbox::Load
// 0x10203ce0 (1,696 bytes) and AddressBook::Load 0x10206b2c (1,236 bytes).
//
// The original reads its two files with the MSL stream classes (a std::basic_ifstream on a local std::string, its getline into a
// 128-byte buffer, the stream's state bits tested by hand). The host reads the file through the platform's file calls instead
// (NS_MacDoze::CreateFile resolves the game's ':' paths against the user and game folders, which a plain std::ifstream could
// not) and TextFile below follows the MSL stream's rules: the same state bits and the same getline.

namespace {

// The strings of the main app's data (TOC word 0x102641c0 plus 0x2e5, 0x2ed and 0x330).
const char kOutboxFolder[] = ":outbox";
const char kOutboxFile[] = ":outbox.dat";
const char kAddressBookFile[] = ":addrbook.dat";

// atoi of the Mac's C library: a number beyond the 32-bit limits gives the limit (the host's atoi would wrap, or be undefined).
int32_t Atoi(const char* text) {
    const long long value = std::strtoll(text, nullptr, 10);
    if (value > INT32_MAX) return INT32_MAX;
    if (value < INT32_MIN) return INT32_MIN;
    return static_cast<int32_t>(value);
}

// The part of an MSL std::ifstream the loaders use: open for reading, getline into 128 characters, close. A file is read whole
// when it opens (the platform has no stream), and the handle stays open until Close, as the Mac's file does.
class TextFile {
public:
    ~TextFile() { Close(); }

    // basic_filebuf::open(name, in). False when there is no such file.
    bool Open(const char* name) {
        NS_MacDoze::InternalHandle* const handle =
            NS_MacDoze::CreateFile(name, 0x80000000u, 1, nullptr, 3, 0, nullptr);
        if (handle == nullptr || handle == reinterpret_cast<NS_MacDoze::InternalHandle*>(static_cast<intptr_t>(-1))) return false;
        mHandle = handle;
        const uint32_t size = NS_MacDoze::GetFileSize(mHandle, nullptr);
        if (size != 0 && size != 0xffffffffu) {
            mBytes.resize(size);
            uint32_t done = 0;
            NS_MacDoze::ReadFile(mHandle, &mBytes[0], size, &done, nullptr);
            mBytes.resize(done);
        }
        return true;
    }

    // basic_filebuf::close.
    void Close() {
        if (mHandle != nullptr) NS_MacDoze::CloseHandle(mHandle);
        mHandle = nullptr;
    }

    // basic_istream::getline(line, 128, '\n') at 0x102079f8, then the callers' test that the stream is still usable (neither
    // the fail nor the bad bit). A stream that already failed or reached the end is not read (it gets the fail bit, `line` stays);
    // a line stops at the newline (taken, not stored), at the end of the file (the end bit; a line read before it counts) or after
    // 127 characters (the fail bit, the next character stays); a call that took no character at all fails.
    bool GetLine(char (&line)[128]) {
        if (mEof || mFail) {
            mFail = true;
            return false;
        }
        uint32_t taken = 0;
        uint32_t stored = 0;
        while (true) {
            if (mPos >= mBytes.size()) {
                mEof = true;
                break;
            }
            const char c = mBytes[mPos];
            if (c == '\n') {
                ++mPos;
                ++taken;
                break;
            }
            if (stored == 127) {
                mFail = true;
                break;
            }
            line[stored++] = c;
            ++mPos;
            ++taken;
        }
        if (taken == 0) mFail = true;
        line[stored] = '\0';
        return !mFail;
    }

private:
    NS_MacDoze::InternalHandle* mHandle = nullptr;
    std::string mBytes;
    size_t mPos = 0;
    bool mEof = false;
    bool mFail = false;
};

// One block of the outbox file: numbers, one per line, up to an empty line or the end of the stream.
void ReadNumbers(TextFile& file, std::list<uint32_t>& numbers) {
    char line[128];
    while (file.GetLine(line) && line[0] != '\0') numbers.push_back(static_cast<uint32_t>(Atoi(line)));
}

}  // namespace

// The e-mail the outbox is writing is deleted (Email::~Email with the delete flag 1) and forgotten; the members' own
// destructors follow. (The Mac's tests against 0xffffffc0 and the like before the members' destructors are the Metrowerks
// sub-object NULL checks: nothing to write.)
Outbox::~Outbox() {
    if (mEmail != nullptr) {
        delete mEmail;
        mEmail = nullptr;
    }
}

// The lists are emptied first (mF28, mF34, mF40), then the folder name is built, then the file opened. The three blocks go to
// mF28, mF40 and mF34 in that order (the listing's inserts name +0x28, +0x40 and +0x34). The original's exception paths (the
// stream's "ios_base failure in clear" text, thrown only when an exception mask is set, which nothing sets) are not translated.
int32_t Outbox::Load(const char* path) {
    TextFile file;
    mF28.clear();
    mF34.clear();
    mF40.clear();
    mF00.assign(path);
    mF00.append(kOutboxFolder);
    std::string local(mF00);
    local.append(kOutboxFile);
    if (!file.Open(local.c_str())) return NS_MacDoze::CreateDirectory(mF00.c_str(), nullptr) != 0 ? 1 : 0;
    ReadNumbers(file, mF28);
    ReadNumbers(file, mF40);
    ReadNumbers(file, mF34);
    file.Close();
    return 1;
}

// Outbox::Save (0x1020461c) and Outbox::WriteMailData (0x10205154).
namespace {

const char kFailure[] = "ios_base failure in clear";   // 0x1024fde4
const char kNumberLine[] = "%d\n";                     // 0x1024fdfe
const char kNewline[] = "\n";                          // 0x1024fe02
const char kDatName[] = ":%08x.dat";                   // 0x1024fe04
const char kJpgName[] = ":%08x.jpg";                   // 0x1024fe0e
const char kNumber[] = "%d";                           // 0x1024fe18

// The stream's state gets the fail bit (and the bad bit when it has no buffer); the exception is raised when the state meets the mask.
void Fail(SaveStream& out) {
    out.ios->mState |= 4;
    if (out.ios->mRdbuf == 0) out.ios->mState |= 1;
    if ((out.ios->mState & out.ios->mMask) != 0) SaveStreamThrow(kFailure);
}

}  // namespace

void Outbox::Save() {
    char line[128];
    SaveStream out;
    SaveStreamCtor(&out);
    std::string name;
    name.assign(mF00);
    name.append(kOutboxFile, std::strlen(kOutboxFile));
    if (!SaveStreamOpen(&out, name.c_str())) Fail(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return;
    }
    for (uint32_t value : mF28) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    SaveStreamWrite(&out, kNewline);
    for (uint32_t value : mF40) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    SaveStreamWrite(&out, kNewline);
    for (uint32_t value : mF34) {
        std::sprintf(line, kNumberLine, static_cast<int>(value));
        SaveStreamWrite(&out, line);
    }
    if (!SaveStreamClose(&out)) Fail(out);
    SaveStreamDestroy(&out);
}

// Outbox::ReadMailData (unit v5148), main app: email.cpp.

namespace {

// The failbit (and badbit when there is no buffer) is set, and the stream throws when the state meets its exception mask.
void RaiseIfMasked(SaveStream& in) {
    SaveIos* ios = in.ios;
    ios->mState |= 4;
    if (ios->mRdbuf == 0) ios->mState |= 1;
    if ((ios->mState & ios->mMask) != 0) SaveStreamThrow(kFailure);
}

// widen('\n') then getline(buffer, 0x100, that character). The Mac's buffer is 132 bytes (a longer line overruns the frame).
void ReadLine(SaveStream& in, char* buffer) {
    char delim = outbox_callee::StreamWiden(&in);
    outbox_callee::StreamGetline(&in, buffer, 0x100, delim);
}

}  // namespace

bool Outbox::ReadMailData(int32_t index) {
    using namespace outbox_callee;
    char buffer[0x100 + 4];
    SaveStream in;
    StreamCtor(&in);
    std::string name(mF00);
    std::sprintf(buffer, kDatName, static_cast<unsigned>(index));
    name.append(buffer, std::strlen(buffer));
    if (!StreamOpen(&in, name.c_str())) RaiseIfMasked(in);
    if (in.filebufOpen == 0) {
        StreamDestroy(&in);
        return false;
    }
    int32_t value;
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 0, &value, 4);   // +0x1c
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 4, &value, 4);   // +0x20
    ReadLine(in, buffer);
    value = std::atoi(buffer);
    std::memcpy(mF1C + 8, &value, 4);   // +0x24
    ReadLine(in, buffer);
    mF04.assign(buffer, std::strlen(buffer));
    ReadLine(in, buffer);
    mF08.assign(buffer, std::strlen(buffer));
    mF0C.clear();
    for (;;) {
        ReadLine(in, buffer);
        bool good = (in.ios->mState & 5) == 0 && in.ios->mRdbuf != 0;
        if (!good) break;
        mF0C.push_back(std::string(buffer));
    }
    if (!SaveStreamClose(&in)) RaiseIfMasked(in);
    mF18 = mF00;
    std::sprintf(buffer, kJpgName, static_cast<unsigned>(index));
    mF18.append(buffer, std::strlen(buffer));
    StreamDestroy(&in);
    return true;
}

// The host's real streams (weak: the test library defines recording fakes).
namespace outbox_callee {
__attribute__((weak)) void StreamCtor(SaveStream* s) {
    s->ios = new SaveIos();
    std::memset(s->ios, 0, sizeof(SaveIos));
    s->ios->mRdbuf = 1;
    s->filebufOpen = 0;
    s->impl = new std::ifstream();
}
__attribute__((weak)) bool StreamOpen(SaveStream* s, const char* name) {
    std::ifstream* f = static_cast<std::ifstream*>(s->impl);
    f->open(name, std::ios::in | std::ios::binary);
    s->filebufOpen = f->is_open() ? 1 : 0;
    return f->is_open();
}
__attribute__((weak)) char StreamWiden(SaveStream*) { return '\n'; }
__attribute__((weak)) void StreamGetline(SaveStream* s, char* buffer, int32_t size, char delim) {
    std::ifstream* f = static_cast<std::ifstream*>(s->impl);
    f->getline(buffer, size, delim);
    if (f->eof()) s->ios->mState |= 2;
    if (f->fail()) s->ios->mState |= 4;
}
__attribute__((weak)) void StreamDestroy(SaveStream* s) {
    delete static_cast<std::ifstream*>(s->impl);
    delete s->ios;
    s->impl = nullptr;
    s->ios = nullptr;
}
}  // namespace outbox_callee

int32_t Outbox::WriteMailData() {
    char line[128];
    SaveStream out;
    std::string datName;   // Mac stack +0x134
    std::string jpgName;   // Mac stack +0x130
    SaveStreamCtor(&out);
    // The next number is one more than the last of mF28 (0 when it is empty).
    int32_t number = mF28.empty() ? 0 : static_cast<int32_t>(mF28.back()) + 1;
    datName.assign(mF00);
    std::sprintf(line, kDatName, static_cast<unsigned>(number));
    datName.append(line, std::strlen(line));
    if (!SaveStreamOpen(&out, datName.c_str())) Fail(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return 0;
    }
    int32_t seconds = outboxwrite_callee::Time();
    const int32_t* tm = outboxwrite_callee::LocalTime(&seconds);
    std::memcpy(mF1C + 0, &tm[3], 4);                   // +0x1c  tm_mday
    int32_t month = tm[4] + 1;
    std::memcpy(mF1C + 4, &month, 4);                   // +0x20  tm_mon + 1
    int32_t year = tm[5] + 0x76c;
    std::memcpy(mF1C + 8, &year, 4);                    // +0x24  tm_year + 1900
    int32_t fields[3];
    std::memcpy(fields, mF1C, 12);
    for (int i = 0; i < 3; i++) {
        std::sprintf(line, kNumber, fields[i]);
        SaveStreamWrite(&out, line);
        SaveStreamWrite(&out, kNewline);
    }
    SaveStreamWrite(&out, mF04.c_str());
    SaveStreamWrite(&out, kNewline);
    SaveStreamWrite(&out, mF08.c_str());
    SaveStreamWrite(&out, kNewline);
    for (const std::string& text : mF0C) {
        SaveStreamWrite(&out, text.c_str());
        SaveStreamWrite(&out, kNewline);
    }
    if (!SaveStreamClose(&out)) Fail(out);
    jpgName.assign(mF00);
    std::sprintf(line, kJpgName, static_cast<unsigned>(number));
    jpgName.append(line, std::strlen(line));
    NS_MacDoze::MoveFile(mF18.c_str(), jpgName.c_str());
    mF18.assign(jpgName);
    mF28.push_back(static_cast<uint32_t>(number));
    mF34.push_back(static_cast<uint32_t>(number));
    mF34.sort();
    mF34.unique();
    Save();
    SaveStreamDestroy(&out);
    return 1;
}

// Outbox cursor and mark functions (First, Next, MarkToSend, MarkToDelete, Delete), std::operator== on strings, the std::basic_ifstream<char>
// constructor and Email::Attachment::~Attachment (unit v5950).
//
// The Mac's cursor (the word at +0x4c) is a node of the list at +0x28. The host list is a std::list, so the cursor is kept as the node's index
// in the four bytes of mF4C (index == size is the end node). The mark functions copy the cursor's id to the end of the list, sort it and
// remove the repeats, as the original's insert, sort and unique do.

extern char gIfstreamVtbl[];

namespace {

uint32_t Cursor(const Outbox* box) {
    uint32_t value;
    std::memcpy(&value, box->mF4C, 4);
    return value;
}

void SetCursor(Outbox* box, uint32_t value) { std::memcpy(box->mF4C, &value, 4); }

uint32_t IdAt(const std::list<uint32_t>& list, uint32_t index) {
    auto it = list.begin();
    std::advance(it, index);
    return *it;
}

// The word the Mac stores for a global's address: an arena offset, else the truncated address (test libraries define the globals outside).
uint32_t A(const void* p) {
    return tpw::InArena(p) ? tpw::ArenaOffset(p) : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p));
}

}  // namespace

// 0x10205718: the cursor to the first mail; ReadMailData of its id, 0 when there is none.
void Outbox::First() {
    SetCursor(this, 0);
    if (mF28.empty()) return;
    ReadMailData(static_cast<int32_t>(IdAt(mF28, Cursor(this))));
}

// 0x1020578c: the cursor to the next mail; at the end it goes back to the first and answers 0.
bool Outbox::Next() {
    if (mF28.empty()) return false;
    uint32_t next = Cursor(this) + 1;
    SetCursor(this, next);
    if (next == mF28.size()) {
        SetCursor(this, 0);
        return false;
    }
    return ReadMailData(static_cast<int32_t>(IdAt(mF28, next)));
}

// 0x10205828: the cursor's id into the list at +0x34, sorted, without repeats.
void Outbox::MarkToSend() {
    mF34.push_back(IdAt(mF28, Cursor(this)));
    mF34.sort();
    mF34.unique();
}

// 0x10205910: the same for the list at +0x40.
void Outbox::MarkToDelete() {
    mF40.push_back(IdAt(mF28, Cursor(this)));
    mF40.sort();
    mF40.unique();
}

// Outbox::Send 0x102059fc (1196 bytes) and Outbox::NextMail 0x10205ed0 (892 bytes), unit v5218.
// Both fill the e-mail the outbox is writing from the login (address, ids, name, password), the recipients (mF0C), the subject
// (mF08) and the attachment path (mF18). Send first makes or resets the e-mail and sends it; NextMail first drops the current mail
// from the three lists and fills the caller's e-mail.

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

// 0x10206278: deletes every mail marked in the list at +0x40 (its files, and its id from the lists at +0x34 and +0x28), empties that
// list, saves, and puts the cursor on the first mail as First does.
void Outbox::Delete() {
    for (std::list<uint32_t>::iterator it = mF40.begin(); it != mF40.end(); ++it) {
        uint32_t id = *it;
        DeleteMail(static_cast<int32_t>(id));
        mF34.remove(id);
        mF28.remove(id);
    }
    mF40.clear();
    Save();
    SetCursor(this, 0);
    if (mF28.empty()) return;
    ReadMailData(static_cast<int32_t>(IdAt(mF28, Cursor(this))));
}

// 0x1020702c
bool msl_char_string::equal(const MslCharString* a, const MslCharString* b) {
    return msl_char_string::compare(a, 0, a->mRep->len, b->mRep->data, b->mRep->len) == 0;
}

// 0x1020801c: the ios part is built only with the virtual-base flag (a halfword, tested after extsh); its vtable word takes the ios_base
// vtable and then the ostream one (the second store wins).
MacIfstream* BasicIfstreamCtor(MacIfstream* self, int16_t withVirtualBase) {
    const uint32_t iosBaseVt = A(gIosBaseVt);
    const uint32_t ostreamVt = A(gOstreamVt1);
    const uint32_t ifstreamVt = A(gIfstreamVtbl);
    if (withVirtualBase != 0) {
        self->mIos.set(&self->mIosPart);
        self->mIosPart.mVtbl = iosBaseVt;
        self->mIosPart.mVtbl = ostreamVt;
    }
    ifstream_char_callee::IstreamCtor(self, 0, A(&self->mFilebuf));
    self->mVtable = ifstreamVt;
    self->mIos->mVtbl = ifstreamVt + 0xc;
    self->mIos->mVbaseOffset = static_cast<int32_t>(reinterpret_cast<intptr_t>(&self->mIosPart) - reinterpret_cast<intptr_t>(self->mIos.get()));
    ifstream_char_callee::FilebufCtor(&self->mFilebuf, 0);
    return self;
}

// 0x10202894: frees the data block, destroys the two strings (the one at +4 first) and, for a positive flag, the object.
Email::Attachment* Email::Attachment::Destruct(Attachment* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mData.get() != nullptr) {
            outbox_callee::OperatorDeleteArray(self->mData.get());
            self->mData.mOffset = 0;
        }
        email_callee::StringDtor(&self->mString04, -1);
        email_callee::StringDtor(&self->mString00, -1);
        if (deleteFlag > 0) email_callee::OperatorDelete(self);
    }
    return self;
}

// Outbox::DeleteMail 0x1020638c: deletes "<mF00>:%08x.dat" and "<mF00>:%08x.jpg" for the mail id.
// The TOC slot -0x3b00 (0x1024faeb) + 0x319 and + 0x323 hold the two formats.
void Outbox::DeleteMail(int32_t mailId) {
    char buffer[0x80];
    std::string name;
    name.assign(mF00, 0, std::string::npos);
    std::sprintf(buffer, ":%08x.dat", static_cast<unsigned>(mailId));
    name.append(buffer, std::strlen(buffer));
    outboxdel_callee::Unlink(name.c_str());
    name.assign(mF00, 0, std::string::npos);
    std::sprintf(buffer, ":%08x.jpg", static_cast<unsigned>(mailId));
    name.append(buffer, std::strlen(buffer));
    outboxdel_callee::Unlink(name.c_str());
}

// The list's constructor, then the word at +0xc (the cursor) takes the word at +8 (the first node).
AddressBook::AddressBook() : mAddresses(), mCursor(mAddresses.begin()) {}

namespace {
// What the cursor functions answer a pointer to. The Mac answers the address of the node's string (node + 8), whose representation
// holds the characters' address at +0xc; at the end, the address of the empty string gEmailBlob (0x10396aa8). The callers read
// the text at once, before the next call.
AddressBookNode gAddressEntry;
AddressBookNode* gAddressEntryRef = &gAddressEntry;
}  // namespace

// 0x10206534: the cursor to the first node, then Next.
AddressBookNode** AddressBook::First() {
    mCursor = mAddresses.begin();
    return Next();
}

// 0x10206594: the cursor's address, the cursor moving on; at the end (the sentinel) the empty text, the cursor staying.
AddressBookNode** AddressBook::Next() {
    if (mCursor == mAddresses.end()) {
        gAddressEntry.mText = "";
        return &gAddressEntryRef;
    }
    gAddressEntry.mText = mCursor->c_str();
    ++mCursor;
    return &gAddressEntryRef;
}

// Inline in the original (the list's clear, an MSL instance at 0x1017fc04).
AddressBook::~AddressBook() {}

// AddressBook::Add (0x102065f8): inserts the address at the end of the list (insert before end(), the sentinel at +4), sorts the
// list, removes the repeats, then answers the position of the first entry equal to the address, or -1.
// The header declares Add as void (another unit's stand-in defines it so); the original answers the position in r3, which
// AddressBookAddIndex keeps and the test checks.
int32_t AddressBookAddIndex(AddressBook* self, const std::string& address) {
    self->mAddresses.push_back(address);
    self->mAddresses.sort();
    self->mAddresses.unique();
    int32_t index = 0;
    for (std::list<std::string>::iterator it = self->mAddresses.begin(); it != self->mAddresses.end(); ++it, ++index) {
        if (*it == address) return index;
    }
    return -1;
}

void AddressBook::Add(const std::string& address) {
    AddressBookAddIndex(this, address);
}

// AddressBook::Remove (0x10206728): erases the entry at the given position (a signed compare, so a negative position never matches).
// Answers 1 when an entry was erased, else 0.
int32_t AddressBook::Remove(int32_t position) {
    int32_t index = 0;
    for (std::list<std::string>::iterator it = mAddresses.begin(); it != mAddresses.end(); ++it) {
        int32_t current = index++;
        if (current == position) {
            mAddresses.erase(it);
            return 1;
        }
    }
    return 0;
}

// AddressBook::Save (0x102067cc, unit v5009): the address book written to "<path>:addrbook.dat".

namespace {
const char kClearFailure[] = "ios_base failure in clear";

// The stream's clear(): failbit set (and badbit when there is no buffer), raising when the state meets the exceptions mask.
void FailAndMaybeThrow(SaveStream& out) {
    out.ios->mState |= 4;
    if (out.ios->mRdbuf == 0) out.ios->mState |= 1;
    if ((out.ios->mState & out.ios->mMask) != 0) SaveStreamThrow(kClearFailure);
}
}  // namespace

// Weak stand-ins on the real std::ofstream; the unit's test library defines the recording fakes.
__attribute__((weak)) void SaveStreamCtor(SaveStream* s) {
    s->ios = new SaveIos();
    s->filebufOpen = 0;
    s->impl = new std::ofstream();
}
__attribute__((weak)) bool SaveStreamOpen(SaveStream* s, const char* name) {
    auto* f = static_cast<std::ofstream*>(s->impl);
    f->open(name, std::ios::out | std::ios::trunc);
    s->filebufOpen = f->is_open() ? 1 : 0;
    return f->is_open();
}
__attribute__((weak)) void SaveStreamWrite(SaveStream* s, const char* text) { *static_cast<std::ofstream*>(s->impl) << text; }
__attribute__((weak)) bool SaveStreamClose(SaveStream* s) {
    auto* f = static_cast<std::ofstream*>(s->impl);
    f->close();
    return !f->fail();
}
__attribute__((weak)) void SaveStreamDestroy(SaveStream* s) {
    delete static_cast<std::ofstream*>(s->impl);
    delete s->ios;
}
__attribute__((weak)) void SaveStreamThrow(const char* what) { throw std::ios_base::failure(what); }

int32_t AddressBook::Save(const char* path) {
    SaveStream out;
    SaveStreamCtor(&out);
    std::string name;
    name.assign(path, std::char_traits<char>::length(path));
    name.append(kAddressBookFile, sizeof(kAddressBookFile) - 1);
    if (!SaveStreamOpen(&out, name.c_str())) FailAndMaybeThrow(out);
    if (out.filebufOpen == 0) {
        SaveStreamDestroy(&out);
        return 0;
    }
    for (const std::string& address : mAddresses) {
        char line[128];
        std::sprintf(line, "%s\n", address.c_str());
        SaveStreamWrite(&out, line);
    }
    if (!SaveStreamClose(&out)) FailAndMaybeThrow(out);
    SaveStreamDestroy(&out);
    return 1;
}

// Every line that reads is an address (an empty line too, unlike the outbox file). The complaint address is added whether or
// not the file opened, then the list is sorted and its neighbouring repeats removed. (The original then walks the list to the
// complaint address with std::operator== and drops the answer: nothing to translate.)
int32_t AddressBook::Load(const char* path) {
    TextFile file;
    std::string local(path);
    local.append(kAddressBookFile);
    const bool opened = file.Open(local.c_str());
    if (opened) {
        char line[128];
        while (file.GetLine(line)) mAddresses.push_back(std::string(line));
        file.Close();
    }
    mAddresses.push_back(gOnlineConfig.mComplainEmail);
    mAddresses.sort();
    mAddresses.unique();
    return opened ? 1 : 0;
}

// email.cpp's static initialiser (0x1020afc0, 856 B): builds the language name strings, the app name and the "Language" key, and a
// small blob (a 3 byte buffer holding the default byte), registering each for destruction at exit.

extern "C" void __register_global_object(void* object, void* destructor, void* node);   // runtime glue 0x10239940
void UIMain_DestroyString(void* object);    // 0x103bcd38, the narrow string destructor glue
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue

// Weak: operator new (glue 0x102384b8); the test library replaces it with a recording fake. The blob is kept through a Ptr32, so
// its memory comes from the arena, as the fake's does (the host heap aborted the launcher: an address outside the arena).
__attribute__((weak)) void* EmailOperatorNew(std::size_t size) {
    return tpw::ArenaAllocZeroed(size != 0 ? size : 4);
}

namespace {
constexpr std::size_t kStringSize = sizeof(TbDynamicStringTemplate<char>);
}

alignas(16) uint8_t gEmailLanguageStrings[16 * kStringSize];
alignas(16) uint8_t gEmailAppNameString[kStringSize];
alignas(16) uint8_t gEmailLanguageKeyString[kStringSize];
alignas(16) uint8_t gEmailWideString[sizeof(TbDynamicStringTemplate<char16_t>)];
uint8_t gEmailNodes[19 * 12];
uint8_t gEmailBlobNode[12];
uint8_t gEmailBlobDtorVec[16];
uint8_t gEmailDefaultByte;
uint32_t gEmailBlob;

// 0x1020afc0
void __sinit_email_cpp() {
    static const char* const kLanguages[16] = {"English",  "French", "German",  "Spanish",
                                               "Swedish",  "Italian", "Dutch",  "Polish",
                                               "Danish",   "Japanese", "TraditionalChinese", "SimplifiedChinese",
                                               "Thai",     "Korean", "Hebrew",  "American"};
    void* destroy = reinterpret_cast<void*>(&UIMain_DestroyString);
    // The wide string at 0x10250130 (kFailureType + 0x98 in the data).
    auto* wide = new (gEmailWideString) TbDynamicStringTemplate<char16_t>(u"upload");
    __register_global_object(wide, reinterpret_cast<void*>(&UIMain_DestroyWString), gEmailNodes + 16 * 12);
    for (int i = 0; i < 16; i++) {
        auto* text = new (gEmailLanguageStrings + i * kStringSize) TbDynamicStringTemplate<char>(kLanguages[i]);
        __register_global_object(text, destroy, gEmailNodes + i * 12);
    }
    auto* app = new (gEmailAppNameString) TbDynamicStringTemplate<char>("Theme Park World");
    __register_global_object(app, destroy, gEmailNodes + 17 * 12);
    auto* key = new (gEmailLanguageKeyString) TbDynamicStringTemplate<char>("Language");
    __register_global_object(key, destroy, gEmailNodes + 18 * 12);

    gEmailBlob = 0;
    EmailBlob* blob = static_cast<EmailBlob*>(EmailOperatorNew(0x10));
    Ptr32<EmailBlob> link;
    link = blob;
    gEmailBlob = link.mOffset;
    if (blob != nullptr) {
        blob->mLength = 3;
        blob->mFlag = 1;
        Ptr32<char> data;
        data = static_cast<char*>(EmailOperatorNew(static_cast<std::size_t>(blob->mLength) + 1));
        blob->mDataOffset = data.mOffset;
    }
    Ptr32<EmailBlob> again;
    again.mOffset = gEmailBlob;
    reinterpret_cast<char*>(Ptr32<char>{again->mDataOffset}.get())[0] = static_cast<char>(gEmailDefaultByte);
    again->mF00 = 0;
    __register_global_object(&gEmailBlob, gEmailBlobDtorVec, gEmailBlobNode);
}

// email.cpp neighbour (main app): Argument's constructor and destructor (unit v5138).

// 0x102011a8: the char string is made empty, the wide string is a copy of the argument (whole), the command is cleared.
Argument* Argument::Construct(Argument* self, const void* text) {
    char* base = reinterpret_cast<char*>(self);
    online_callee::StringCtor(base);                                    // 0x1007363c
    online_callee::WStringCopy(base + 4, text, 0, 0xffffffffu);         // 0x101f7424
    self->mCommand = 0;
    return self;
}

// 0x1020127c: a NULL object is left alone; the wide string then the char string are destroyed, and the memory is freed when the
// hidden flag (a signed short) is above 0.
Argument* Argument::Destruct(Argument* self, int16_t deleteFlag) {
    if (self != nullptr) {
        char* base = reinterpret_cast<char*>(self);
        online_callee::WStringDtor(base + 4, -1);   // 0x101468f8
        online_callee::StringDtor(base, -1);        // 0x1000221c
        if (deleteFlag > 0) online_callee::OperatorDelete(self);   // 0x10238218
    }
    return self;
}

// bthread_mutex::~bthread_mutex (0x10200d04), unit v5120. Neighbour of src/email.cpp (lock, unlock).

namespace NS_MacDoze {
void DeleteCriticalSection(void** handle);   // 0x1023b338 (glue import)
}

// The Mac's destructor with its hidden delete flag (r4, a short): a NULL object does nothing; otherwise the critical section is
// deleted, and the object freed when the flag is above 0 (extsh, signed).
bthread_mutex* bthread_mutex::Destroy(bthread_mutex* self, int16_t deleteFlag) {
    if (self != nullptr) {
        NS_MacDoze::DeleteCriticalSection(&self->mHandle);
        if (deleteFlag > 0) online_callee::OperatorDelete(static_cast<void*>(self));   // operator delete, 0x10238218
    }
    return self;
}

// Unit v51057: small e-mail functions on Mac-layout objects (the address book cursor, the outbox's unmark calls, the MSL ios_base and
// list accessors, the destructors the e-mail classes share). Objects are byte pointers with words at the Mac's offsets.

extern uint32_t gEmailBlob;   // 0x10396aa8 (also declared in FS_Online.h)

uint32_t gStringListInitWord;   // 0x102b1794
uint32_t gUintListInitWord;     // 0x102b1790

namespace {

// Words are arena offsets (0: null); the globals the code stores or answers are truncated addresses (AG).
inline uint8_t* P(uint32_t address) { return address != 0 ? tpw::gArenaBase + address : nullptr; }
inline uint32_t A_2(const void* pointer) { return tpw::ArenaOffset(pointer); }
inline uint32_t AG(const void* pointer) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer)); }
inline uint32_t& W(const uint8_t* base, uint32_t offset) {
    return *reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(base) + offset);
}

// The list constructors: the first word is copied from a global, the sentinel (+4) points at itself (the two words the original
// loads from uninitialised stack slots are overwritten at once).
uint8_t* ListInit(uint8_t* self, uint32_t initWord) {
    W(self, 0) = initWord;
    uint32_t sentinel = A_2(self) + 4;
    W(self, 4) = sentinel;
    W(self, 8) = sentinel;
    return self;
}

}  // namespace

namespace email_callee {
// 0x102029f4: the mail connection's callback: (a, b, data, context) calls context->Callback(a, b, data).
void MailCbfn(int32_t a, int32_t b, uint8_t* data, void* context) {
    static_cast<Email*>(context)->Callback(static_cast<uint32_t>(a), b, data);
}
}  // namespace email_callee

namespace v51057 {

// 0x10206594: the cursor (+0xc) is a node; at the sentinel (this + 4) the answer is the address of gEmailBlob (the TOC slot).
uint32_t AddressBookNext(uint8_t* self) {
    uint32_t current = W(self, 0xc);
    if (current != A_2(self) + 4) {
        W(self, 0xc) = W(P(current), 4);
        return current + 8;
    }
    return AG(&gEmailBlob);
}

// 0x10206534
uint32_t AddressBookFirst(uint8_t* self) {
    W(self, 0xc) = W(self, 8);
    return AddressBookNext(self);
}

// 0x102058b4
void OutboxUnmarkToSend(uint8_t* self) {
    uint32_t item = W(self, 0x4c);
    std_list_uint::Remove(reinterpret_cast<std_list_uint::List*>(self + 0x34), reinterpret_cast<const uint32_t*>(P(item + 8)));
}

// 0x102059a0
void OutboxUnmarkToDelete(uint8_t* self) {
    uint32_t item = W(self, 0x4c);
    std_list_uint::Remove(reinterpret_cast<std_list_uint::List*>(self + 0x40), reinterpret_cast<const uint32_t*>(P(item + 8)));
}

uint32_t IosExceptions(const uint8_t* ios) { return W(ios, 0x38); }

uint32_t* IosOrAssign(uint32_t* a, uint32_t b) {
    *a = *a | b;
    return a;
}

uint32_t IosState(uint8_t* ios) { return A_2(ios) + 0x34; }

uint32_t IosAnd(uint32_t a, uint32_t b) { return a & b; }

uint32_t IosRdstate(const uint8_t* ios) { return W(ios, 0x34); }

uint32_t BasicIosTie(const uint8_t* ios) { return W(ios, 0x3c); }

// cntlzw of (0 - state) >> 5: 1 only for a zero state.
uint32_t IosGood(const uint8_t* ios) { return W(ios, 0x34) == 0 ? 1u : 0u; }

// 0x10207f84: frees the object for a positive (signed 16 bit) flag. Answers this.
uint8_t* IstreamSentryDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr && flag > 0) MslOperatorDelete(self);
    return self;
}

// 0x102043a8: the stream's vtable word and the virtual ios part's back offset; a non-zero flag destroys the ios part.
uint8_t* IstreamDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        uint32_t ifstreamVt = AG(gIfstreamVtblC);
        W(self, 8) = ifstreamVt;
        uint8_t* ios = P(W(self, 0));
        W(ios, 0) = ifstreamVt + 0xc;
        ios = P(W(self, 0));
        W(ios, 0x44) = (A_2(self) + 0xc) - A_2(ios);
        if (flag != 0) {
            if (A_2(self) + 0xc != 0) {
                W(self, 0xc) = AG(gOstreamVt1);
                OstreamIosBaseDtor(reinterpret_cast<MacOstreamIos*>(self + 0xc), 0);
            }
        }
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x10208abc
uint8_t* StringListCtor(uint8_t* self) { return ListInit(self, gStringListInitWord); }

// 0x1020951c
uint8_t* UintListCtor(uint8_t* self) { return ListInit(self, gUintListInitWord); }

// 0x10203a4c
uint8_t* UintListDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        std_list_uint::Clear(reinterpret_cast<std_list_uint::List*>(self));
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x1020236c
uint8_t* StringListDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        email_callee::ListClear(self);
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

// 0x1020a0d4 / 0x1020a068: a string is a word that points at its rep: the size is the rep's word +0, the data pointer word +0xc.
uint32_t StringSize(const uint8_t* str) { return W(P(W(str, 0)), 0); }

uint32_t StringBegin(const uint8_t* str) { return W(P(W(str, 0)), 0xc); }

// 0x10209f04: a < b is compare(a, 0, a.size(), b.begin(), b.size()) < 0 (signed).
uint32_t StringLess(const uint8_t* a, const uint8_t* b) {
    uint32_t size = StringSize(b);
    uint32_t begin = StringBegin(b);
    uint32_t sizeA = StringSize(a);
    int32_t result = msl_char_string::compare(reinterpret_cast<const MslCharString*>(a), 0, sizeA, reinterpret_cast<const char*>(P(begin)), size);
    return result < 0 ? 1u : 0u;
}

// 0x10201fcc
uint8_t* CSockDtor(uint8_t* self, int16_t flag) {
    if (self != nullptr) {
        W(self, 0) = 0x102b1638;
        csock_callee::SockDtor(self, 0);
        if (flag > 0) MslOperatorDelete(self);
    }
    return self;
}

}  // namespace v51057

// EmailInfo::~EmailInfo 0x10203aec: the members go in reverse order (the string at +0x14, the list at +8, the strings at +4 and +0),
// then the object is deleted when the hidden flag is above 0. The list's address test in the listing (this + 8 is never 0) is
// always true.
EmailInfo* EmailInfo::Destruct(EmailInfo* self, int16_t deleteFlag) {
    if (self != nullptr) {
        email_callee::StringDtor(&self->mString14, -1);
        email_callee::ListClear(&self->mList08);
        email_callee::StringDtor(&self->mString04, -1);
        email_callee::StringDtor(&self->mString00, -1);
        if (deleteFlag > 0) email_callee::OperatorDelete(self);
    }
    return self;
}
