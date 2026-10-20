// Test library for the postcard panel's sending half (unit v5884; libtpw_unit_SendPostcard.dylib, tests/test_SendPostcard.py), main app:
//   src/ui_Postcard_panel_SendPostcard.cpp   IllCallYouBack, AddressesValid, AddToAddressField, SendPostcard and the globals they own
//
// The callees outside the unit are fakes that record what the real ones read (texts as FNV-1a hashes, pointers as labels) and answer
// from the test's queues; the C library's string functions are fakes with the C semantics. The globals of ui_Postcard_panel.cpp and the
// other files the unit reads are defined here, zero at start like the Mac's bss.
#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/macdoze.h"
#include "platform/msl.h"
#include "ui_Postcard.h"
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "CMessageCentre.h"
#include "CMsgEvent.h"
#include "email.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "InterfaceWindow.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
char gTempPath[0x200];
char16_t gPostcardAddress[0x60];
char16_t gPostcardText5da5[0x200];
Postcard* gPostcard;
InterfaceWindow* gPostcardPanel;
CFSOnlineSystem* gOnlineSystem;
CFSUserInfo* gUserInfo;
CMessageCentre* gMessageCentre;
OnlineConfig gOnlineConfig;
OnlineConfig::OnlineConfig() {}

namespace {

Email* gEmail = nullptr;
char16_t* gStrBuf = nullptr;        // +0 login e-mail, +0x100 name, +0x200 password, +0x300 the interface string 0xd3
InterfaceWindow* gWindow = nullptr;
std::map<const void*, std::string> gStrings;   // the temporaries of the strtok loop
char16_t* gWcsSave = nullptr;

int64_t HS(const char* s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint8_t>(s[i]);
        h *= 16777619u;
    }
    return h;
}
int64_t HS(const char* s) { return HS(s, std::strlen(s)); }
int64_t HW(const char16_t* s, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint16_t>(s[i]);
        h *= 16777619u;
    }
    return h;
}
int64_t HW(const char16_t* s) {
    size_t n = 0;
    while (s[n] != 0) ++n;
    return HW(s, n);
}
int64_t I(int64_t v) { return v; }
int64_t Off(const void* p) {
    return static_cast<int64_t>(reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(gEmail));
}

}  // namespace

// ---- the fakes
void AddUIError(int32_t error, TbITable* table) { ev("AddUIError", {I(error), I(lab(table))}); }

const char16_t* UIMain_GetString(int32_t id) {
    ev("UIMain_GetString", {I(id)});
    return gStrBuf + 0x180;
}

int32_t wcscmp(const char16_t* a, const char16_t* b) {
    ev("wcscmp", {HW(a), HW(b)});
    while (*a != 0 && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b ? 0 : (*a < *b ? -1 : 1);
}

char16_t* wcstok(char16_t* s, const char16_t* delimiters, char16_t**) {
    ev("wcstok", {I(s == nullptr), HW(delimiters)});
    if (s == nullptr) s = gWcsSave;
    if (s == nullptr) return nullptr;
    auto isDelimiter = [&](char16_t c) {
        for (const char16_t* d = delimiters; *d != 0; ++d)
            if (*d == c) return true;
        return false;
    };
    while (*s != 0 && isDelimiter(*s)) ++s;
    if (*s == 0) {
        gWcsSave = nullptr;
        return nullptr;
    }
    char16_t* start = s;
    while (*s != 0 && !isDelimiter(*s)) ++s;
    if (*s == 0) {
        gWcsSave = nullptr;
    } else {
        *s = 0;
        gWcsSave = s + 1;
    }
    return start;
}

char16_t* wcschr(const char16_t* text, char16_t c) {
    for (;; ++text) {
        if (*text == c) return const_cast<char16_t*>(text);
        if (*text == 0) return nullptr;
    }
}

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {I(lab(this)), I(id)});
    return gWindow;
}

namespace NS_MacDoze {
int32_t CopyFile(const char* from, const char* to, int32_t failIfExists) {
    ev("CopyFile", {HS(from), HS(to), I(failIfExists)});
    return 0;
}
}  // namespace NS_MacDoze

extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count) {
    (void)array;
    ev("ConstructArray", {I(ctor == kCSubscriptionHeaderCtorVector ? 1 : 0), I(dtor == kCSubscriptionHeaderDtorVector ? 2 : 0),
                          I(size), I(count)});
}

namespace sendpostcard_callee {
void* New(uint32_t size) {
    ev("New", {I(size)});
    return tpw::ArenaAllocZeroed(size + 0x1000);
}
int32_t Sprintf(char* dst, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    std::string out;
    for (const char* p = format; *p != 0;) {
        if (*p != '%') {
            out += *p++;
        } else if (p[1] == 's') {
            out += va_arg(ap, const char*);
            p += 2;
        } else if (p[1] == 'l' && p[2] == 's') {
            for (const char16_t* w = va_arg(ap, const char16_t*); *w != 0; ++w) out += static_cast<char>(*w);
            p += 3;
        } else if (std::strncmp(p, "%08d", 4) == 0) {
            char b[32];
            std::snprintf(b, sizeof b, "%08d", va_arg(ap, int));
            out += b;
            p += 4;
        } else {
            out += *p++;
        }
    }
    va_end(ap);
    std::memcpy(dst, out.c_str(), out.size() + 1);
    ev("Sprintf", {HS(format), HS(out.c_str())});
    return static_cast<int32_t>(out.size());
}
void PostcardSave(Postcard* card, char* path) { ev("PostcardSave", {I(lab(card)), HS(path)}); }
void EmailCtor(void* email) {
    gEmail = static_cast<Email*>(email);
    ev("EmailCtor", {I(newlab(email))});
}
void StringAssign(void* str, const char* text, uint32_t length) {
    ev("StringAssign", {Off(str), HS(text, length), I(length)});
}
void StringCtor(void* str, const char* text) {
    gStrings[str] = text;
    ev("StringCtor", {HS(text)});
}
void ListInsert(void* list, void* where, const void* str) {
    ev("ListInsert", {Off(list), Off(where), HS(gStrings[str].c_str())});
}
void StringDtor(void* str, int16_t flag) {
    ev("StringDtor", {HS(gStrings[str].c_str()), I(flag)});
    gStrings.erase(str);
}
void OnlineSystemCtor(void* system) { ev("OnlineSystemCtor", {I(newlab(system))}); }
void UserInfoCtor(void* info) { ev("UserInfoCtor", {I(newlab(info))}); }
void WindowSetText(InterfaceWindow* window, char16_t* text) { ev("WindowSetText", {I(lab(window)), HW(text)}); }
}  // namespace sendpostcard_callee

namespace outbox_callee {
const char16_t* LoginEmailAddress(CFSOnlineSystem* system) {
    ev("LoginEmail", {I(lab(system))});
    return gStrBuf;
}
const char16_t* LoginCustomerName(CFSOnlineSystem* system) {
    ev("LoginName", {I(lab(system))});
    return gStrBuf + 0x80;
}
const char16_t* LoginPassword(CFSOnlineSystem* system) {
    ev("LoginPassword", {I(lab(system))});
    return gStrBuf + 0x100;
}
int32_t LoginPlayerID(CFSOnlineSystem* system) {
    ev("LoginPlayerID", {I(lab(system))});
    return static_cast<int32_t>(ret("PlayerID"));
}
int32_t LoginCustomerID(CFSOnlineSystem* system) {
    ev("LoginCustomerID", {I(lab(system))});
    return static_cast<int32_t>(ret("CustomerID"));
}
void WStringAssign(void* str, const char16_t* text, uint32_t length) {
    ev("WStringAssign", {Off(str), HW(text, length), I(length)});
}
}  // namespace outbox_callee

bool Email::AttachData(const char* path) {
    ev("AttachData", {I(lab(this)), HS(path)});
    return ret("AttachData") != 0;
}
bool Email::Send(const char* server, int16_t port, char* extra, Outbox* outbox) {
    ev("EmailSend", {I(lab(this)), HS(server), I(port), I(lab(extra)), I(lab(outbox))});
    return ret("Send") != 0;
}
int32_t Email::SendToOutbox(Outbox* outbox) {
    ev("SendToOutbox", {I(lab(this)), I(outbox == &gUserInfo->mOutbox)});
    return 0;
}
int32_t CFSOnlineSystem::AreWeLoggedIn() {
    ev("AreWeLoggedIn", {I(lab(this))});
    return static_cast<int32_t>(ret("AreWeLoggedIn"));
}
int32_t CMessageCentre::PostMessage(const CMsg& message) {
    ev("PostMessage", {I(lab(this)), I(static_cast<const CMsgEvent&>(message).mEvent)});
    return 1;
}
CMsgEvent::CMsgEvent(EventType type) : mEvent(type) {
    mFlags = 0;
    ev("CMsgEvent", {I(type)});
}
CMsgEvent::~CMsgEvent() {}
int32_t CMsgEvent::GetType() const { return 0; }

extern "C" {

uint64_t tpw_sp_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gTempPath", gTempPath},
        {"gPostcardAddress", gPostcardAddress},
        {"gPostcardText5da5", gPostcardText5da5},
        {"gPostcardJpgPath", gPostcardJpgPath},
        {"gPostcardCounter", &gPostcardCounter},
        {"illvec", kIllCallYouBackVector},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_sp_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_sp_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Email));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString10));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString14));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListSize));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListPrev));
    out[n++] = static_cast<int64_t>(offsetof(Email, mF18));
    out[n++] = static_cast<int64_t>(offsetof(Email, mWString20));
    out[n++] = static_cast<int64_t>(offsetof(Email, mWString24));
    out[n++] = static_cast<int64_t>(offsetof(Email, mF28));
    out[n++] = static_cast<int64_t>(offsetof(Email, mCallback));
    out[n++] = static_cast<int64_t>(sizeof(gPostcardAddress));
    out[n++] = static_cast<int64_t>(sizeof(gPostcardText5da5));
    return n;
}

void tpw_sp_reset() {
    gEmail = nullptr;
    gStrings.clear();
    gWcsSave = nullptr;
}

void tpw_sp_set_ptrs(uint64_t card, uint64_t panel, uint64_t system, uint64_t info, uint64_t centre) {
    gPostcard = reinterpret_cast<Postcard*>(static_cast<uintptr_t>(card));
    gPostcardPanel = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(panel));
    gOnlineSystem = reinterpret_cast<CFSOnlineSystem*>(static_cast<uintptr_t>(system));
    gUserInfo = reinterpret_cast<CFSUserInfo*>(static_cast<uintptr_t>(info));
    gMessageCentre = reinterpret_cast<CMessageCentre*>(static_cast<uintptr_t>(centre));
}

void tpw_sp_set_extras(uint64_t strbuf, uint64_t window) {
    gStrBuf = reinterpret_cast<char16_t*>(static_cast<uintptr_t>(strbuf));
    gWindow = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(window));
}

void tpw_sp_set_config(const char* server, int32_t port) {
    gOnlineConfig.mMailServer = server;
    gOnlineConfig.mMailPort = static_cast<int16_t>(port);
}

// The labels of the three global pointers after the call (the host log's labels).
void tpw_sp_labs(int64_t* out) {
    out[0] = lab(gOnlineSystem);
    out[1] = lab(gUserInfo);
    out[2] = lab(gMessageCentre);
}

uint64_t tpw_sp_email() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gEmail)); }

void tpw_sp_illcall(int32_t code, int32_t error) {
    IllCallYouBack(static_cast<EmailCompletionCode>(code), static_cast<EmailErrorCode>(error));
}
uint32_t tpw_sp_valid() { return static_cast<uint32_t>(AddressesValid()); }
void tpw_sp_addto(uint64_t text) { AddToAddressField(reinterpret_cast<char16_t*>(static_cast<uintptr_t>(text))); }
void tpw_sp_send() { SendPostcard(); }

}  // extern "C"
