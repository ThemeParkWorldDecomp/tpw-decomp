// Test library for Email::Email and Email::Callback (unit v5912; libtpw_unit_Email_Callback_2.dylib, tests/test_Email_Callback_2.py), main app:
//   src/game/online/EmailMessage.cpp
//
// The callees outside the unit are fakes that record the arguments the real ones read: the string, list and Mail functions, the
// callback glue, Email::BuildBody and Outbox::NextMail. The string and list constructors write zero words (the real ones write
// pointers the test does not model); the substring functions copy the source's string word.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace {

const uint8_t* gEmailBase = nullptr;

// The offset of a field inside the e-mail being built or called, else -1 (a local string).
int64_t OffOf(const void* p) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    if (gEmailBase != nullptr && b >= gEmailBase && b < gEmailBase + 0x4c) return static_cast<int64_t>(b - gEmailBase);
    return -1;
}

const void* AtOffset(uint32_t offset) { return reinterpret_cast<const void*>(tpw::gArenaBase + offset); }

}  // namespace

namespace emailcb_callee {
void StringInit(void* str) {
    ev("StringInit", {OffOf(str)});
    std::memset(str, 0, 4);
}
void WStringInit(void* str) {
    ev("WStringInit", {OffOf(str)});
    std::memset(str, 0, 4);
}
void ListInit(void* list) {
    ev("ListInit", {OffOf(list)});
    std::memset(list, 0, 12);
}
void StringSubCtor(void* dst, const void* src, uint32_t pos, uint32_t n) {
    ev("StringSubCtor", {static_cast<int64_t>(lab(dst)), static_cast<int64_t>(lab(src)), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    std::memcpy(dst, src, 4);
}
void StringSubAssign(void* dst, const void* src, uint32_t pos, uint32_t n) {
    ev("StringSubAssign", {static_cast<int64_t>(lab(dst)), static_cast<int64_t>(lab(src)), static_cast<int64_t>(pos), static_cast<int64_t>(n)});
    std::memcpy(dst, src, 4);
}
void WinsockStart(int32_t version, void*) { ev("WSAStartup", {static_cast<int64_t>(version)}); }
void MailDoTo(void* mail, const char* address) { ev("DoTo", {static_cast<int64_t>(lab(mail))}, {address}); }
void MailDoFrom(void* mail, const char* address) { ev("DoFrom", {static_cast<int64_t>(lab(mail))}, {address}); }
void MailDoData(void* mail, const char* text) { ev("DoData", {static_cast<int64_t>(lab(mail))}, {text}); }
void MailQuit(void* mail) { ev("Quit", {static_cast<int64_t>(lab(mail))}); }
void MailDoReset(void* mail) { ev("DoReset", {static_cast<int64_t>(lab(mail))}); }
}  // namespace emailcb_callee

namespace email_callee {
void OperatorDelete(void* p) { ev("OperatorDelete", {static_cast<int64_t>(lab(p))}); }
void PtrGlue(uint32_t tvector, int32_t a, int32_t b) {
    ev("PtrGlue", {static_cast<int64_t>(tvector), static_cast<int64_t>(a), static_cast<int64_t>(b)});
}
void StringAssign(void* str, const char* text, uint32_t length) {
    ev("StringAssign", {static_cast<int64_t>(lab(str)), static_cast<int64_t>(length)}, {text});
}
void StringDtor(void* str, int16_t flag) { ev("StringDtor", {static_cast<int64_t>(lab(str)), static_cast<int64_t>(flag)}); }
void ListErase(void* list, uint32_t first, void* last) {
    ev("ListErase", {static_cast<int64_t>(lab(list)), static_cast<int64_t>(lab(AtOffset(first))), static_cast<int64_t>(lab(last))});
}
}  // namespace email_callee

namespace outbox_callee {
void OperatorDeleteArray(void* p) { ev("OperatorDeleteArray", {static_cast<int64_t>(lab(p))}); }
}  // namespace outbox_callee

void Email::BuildBody() { ev("BuildBody", {static_cast<int64_t>(lab(this))}); }

int32_t Outbox::NextMail(Email* email) {
    ev("NextMail", {static_cast<int64_t>(lab(this)), static_cast<int64_t>(lab(email))});
    return static_cast<int32_t>(ret("NextMail"));
}

extern "C" {

// The arena offset of a host pointer: the word the Mac side keeps as an address.
uint32_t tpw_ecb_off(uint64_t p) { return tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(p))); }

uint64_t tpw_ecb_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_ecb_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Email, mCallback));
    out[n++] = static_cast<int64_t>(sizeof(Email));
    out[n++] = static_cast<int64_t>(offsetof(Email, mMail));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString38));
    out[n++] = static_cast<int64_t>(offsetof(Email, mOutbox));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListNext));
    return n;
}

void tpw_ecb_callback(uint64_t self, uint32_t message, int32_t code) {
    Email* email = reinterpret_cast<Email*>(static_cast<uintptr_t>(self));
    gEmailBase = reinterpret_cast<const uint8_t*>(email);
    email->Callback(message, code, nullptr);
}

uint64_t tpw_ecb_ctor(uint64_t self) {
    Email* email = reinterpret_cast<Email*>(static_cast<uintptr_t>(self));
    gEmailBase = reinterpret_cast<const uint8_t*>(email);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Email::Construct(email)));
}

}  // extern "C"
