// The fakes and entry points for unit w90 (src/email_load.cpp, src/TPEmbed_ime.cpp and the constructors of src/email.cpp;
// libtpw_unit_Email.dylib, used by test_Email.py).
//
// The loaders read their files through the platform's file calls (NS_MacDoze::CreateFile, GetFileSize, ReadFile, CloseHandle). Here
// the files are an in-memory table the test fills (tpw_email_file), and the calls record "open" (with the name) and "close", the
// events the PowerPC side records from its stand-ins for the MSL stream's open and close. CreateDirectory and Email::~Email are
// recording fakes as well. The outbox and the address book are real objects; the test sets and reads their members through
// the extern "C" functions below, which render a field as bytes in the shape the PowerPC side's reader produces:
//   strings: the bytes; lists of strings: each item followed by 0x01; lists of numbers: unsigned decimals joined by ','.
#include "calllog.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include "TPMain.h"
#include "email.h"
#include "platform/macdoze.h"
#include "ui_ime.h"

using calllog::ev;
using calllog::lab;

// ---- recording fakes of the callees

Email::~Email() { ev("~Email", {lab(this)}); }

// The only member the loaders read is the complaint address.
OnlineConfig::OnlineConfig() {}
OnlineConfig gOnlineConfig;

namespace {

// The files of the case (name -> bytes) and an open file's position.
std::map<std::string, std::string> gFiles;
struct OpenFile {
    std::string bytes;
};

}  // namespace

namespace NS_MacDoze {

int32_t CreateDirectory(const char* name, void* security) {
    ev("CreateDirectory", {lab(security)}, {name});
    return static_cast<int32_t>(calllog::ret("CreateDirectory"));
}

InternalHandle* CreateFile(const char* name, uint32_t, uint32_t, void*, uint32_t, uint32_t, InternalHandle*) {
    ev("open", {}, {name});
    const auto it = gFiles.find(name);
    if (it == gFiles.end()) return reinterpret_cast<InternalHandle*>(static_cast<intptr_t>(-1));
    auto* file = new OpenFile{it->second};
    return reinterpret_cast<InternalHandle*>(file);
}

uint32_t GetFileSize(InternalHandle* handle, uint32_t*) {
    return static_cast<uint32_t>(reinterpret_cast<OpenFile*>(handle)->bytes.size());
}

int32_t ReadFile(InternalHandle* handle, void* buffer, uint32_t count, uint32_t* done, void*) {
    auto* file = reinterpret_cast<OpenFile*>(handle);
    const uint32_t n = count < file->bytes.size() ? count : static_cast<uint32_t>(file->bytes.size());
    std::memcpy(buffer, file->bytes.data(), n);
    if (done != nullptr) *done = n;
    return n == count ? 1 : 0;
}

int32_t CloseHandle(InternalHandle* handle) {
    ev("close");
    delete reinterpret_cast<OpenFile*>(handle);
    return 1;
}

// bthread_mutex (src/email.cpp): not used by the test.
void EnterCriticalSection(void**) {}
void LeaveCriticalSection(void**) {}

}  // namespace NS_MacDoze

// ---- test entry points

namespace {

std::string join(const std::list<std::string>& items) {
    std::string out;
    for (const std::string& item : items) {
        out += item;
        out += '\x01';
    }
    return out;
}

std::list<std::string> split(const std::string& text) {
    std::list<std::string> out;
    std::string item;
    for (char c : text) {
        if (c == '\x01') {
            out.push_back(item);
            item.clear();
        } else {
            item += c;
        }
    }
    return out;
}

std::string join_numbers(const std::list<uint32_t>& items) {
    std::string out;
    bool first = true;
    for (uint32_t item : items) {
        if (!first) out += ',';
        out += std::to_string(item);
        first = false;
    }
    return out;
}

std::list<uint32_t> split_numbers(const std::string& text) {
    std::list<uint32_t> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find(',', at);
        if (end == std::string::npos) end = text.size();
        out.push_back(static_cast<uint32_t>(std::strtoul(text.substr(at, end - at).c_str(), nullptr, 10)));
        at = end + 1;
    }
    return out;
}

int32_t copy_out(const std::string& text, char* out, int32_t capacity) {
    const int32_t n = static_cast<int32_t>(text.size()) < capacity ? static_cast<int32_t>(text.size()) : capacity;
    std::memcpy(out, text.data(), static_cast<size_t>(n));
    return static_cast<int32_t>(text.size());
}

IME::CIME gIme;

}  // namespace

// IME::MslWideString (include/ui_ime.h): gIme's constructor and destructor (src/TPEmbed_ime.cpp, src/ui_ime_lifetime.cpp) reach the
// string library's pair, which launcher/pending_manual.cpp answers in the game; empty fakes here.
IME::MslWideString::MslWideString() {}
IME::MslWideString::~MslWideString() {}

extern "C" {

// The files table.
void tpw_email_files_clear() { gFiles.clear(); }
void tpw_email_file(const char* name, const char* data, int32_t size) { gFiles[name] = std::string(data, static_cast<size_t>(size)); }

// gOnlineConfig.mComplainEmail.
void tpw_email_complain(const char* data, int32_t size) { gOnlineConfig.mComplainEmail.assign(data, static_cast<size_t>(size)); }

void* tpw_email_new_outbox() { return new Outbox(); }
void* tpw_email_new_book() { return new AddressBook(); }
void* tpw_email_new_email() { return new Email(); }
void tpw_email_free(void* object, int32_t isBook) {
    calllog::forget(object);
    if (isBook != 0) delete static_cast<AddressBook*>(object);
    else delete static_cast<Outbox*>(object);
}

// Outbox fields: 0 mF00, 1 mF04, 2 mF08, 3 mF18, 4 mF50 (strings), 5 mF0C (list of strings), 6 mF28, 7 mF34, 8 mF40 (numbers),
// 9 mF54, 10 mF58 (numbers as decimal text), 11 mEmail (1 when set).
void tpw_email_outbox_set(void* object, int32_t field, const char* data, int32_t size) {
    auto* o = static_cast<Outbox*>(object);
    const std::string text(data, static_cast<size_t>(size));
    switch (field) {
        case 0: o->mF00 = text; break;
        case 1: o->mF04 = text; break;
        case 2: o->mF08 = text; break;
        case 3: o->mF18 = text; break;
        case 4: o->mF50 = text; break;
        case 5: o->mF0C = split(text); break;
        case 6: o->mF28 = split_numbers(text); break;
        case 7: o->mF34 = split_numbers(text); break;
        case 8: o->mF40 = split_numbers(text); break;
        case 9: o->mF54 = static_cast<uint16_t>(std::atoi(text.c_str())); break;
        case 10: o->mF58 = std::atoi(text.c_str()); break;
        default: break;
    }
}
void tpw_email_outbox_set_email(void* object, void* email) { static_cast<Outbox*>(object)->mEmail = static_cast<Email*>(email); }
int32_t tpw_email_outbox_get(void* object, int32_t field, char* out, int32_t capacity) {
    auto* o = static_cast<Outbox*>(object);
    std::string text;
    switch (field) {
        case 0: text = o->mF00; break;
        case 1: text = o->mF04; break;
        case 2: text = o->mF08; break;
        case 3: text = o->mF18; break;
        case 4: text = o->mF50; break;
        case 5: text = join(o->mF0C); break;
        case 6: text = join_numbers(o->mF28); break;
        case 7: text = join_numbers(o->mF34); break;
        case 8: text = join_numbers(o->mF40); break;
        case 9: text = std::to_string(o->mF54); break;
        case 10: text = std::to_string(o->mF58); break;
        case 11: text = o->mEmail != nullptr ? "1" : "0"; break;
        default: break;
    }
    return copy_out(text, out, capacity);
}
int32_t tpw_email_outbox_load(void* object, const char* path) { return static_cast<Outbox*>(object)->Load(path); }

// The destructor with the Mac's flag: positive deletes the object, else only the destructor runs (the block stays until
// tpw_email_release, since the Mac's caller owns it).
void tpw_email_outbox_destroy(void* object, int32_t flag) {
    auto* o = static_cast<Outbox*>(object);
    if (flag > 0) delete o;
    else o->~Outbox();
}
// Frees the block of an object whose destructor has run, without a delete event.
void tpw_email_release(void* object) {
    calllog::forget(object);
    ::operator delete(object);
}

// The address book: field 0 is its list.
void tpw_email_book_set(void* object, const char* data, int32_t size) {
    static_cast<AddressBook*>(object)->mAddresses = split(std::string(data, static_cast<size_t>(size)));
}
int32_t tpw_email_book_get(void* object, char* out, int32_t capacity) {
    return copy_out(join(static_cast<AddressBook*>(object)->mAddresses), out, capacity);
}
int32_t tpw_email_book_load(void* object, const char* path) { return static_cast<AddressBook*>(object)->Load(path); }

// The IME stubs: which 0 Activate (the four words of the result go to `words`), 1 UnActivate, 2 SetMaxCompositionLength(length).
void tpw_email_ime(int32_t which, int32_t length, int32_t* words) {
    switch (which) {
        case 0: {
            const IME::ActivationResult result = gIme.Activate(nullptr);
            for (int i = 0; i < 4; ++i) words[i] = result.mWords[i];
            break;
        }
        case 1: gIme.UnActivate(); break;
        default: gIme.SetMaxCompositionLength(length); break;
    }
}

}  // extern "C"
