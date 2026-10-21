// Test library for the e-mail object's destructor, Reset, AttachData and Send (unit v5129; libtpw_unit_Email_Send.dylib,
// tests/test_Email_Send.py), main app:
//   src/game/online/EmailMessage.cpp   Email::~Email (Destruct), Email::Reset, Email::AttachData, Email::Send
//
// The callees outside the unit (operator new and delete, the Mail and Attachment members, the string and list members, WSACleanup,
// the pointer glue) are fakes that record the arguments the real ones read and answer from the test's queues. No globals of other files.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

namespace {
uint64_t gNew[8];
int gNewCount = 0;
}  // namespace

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
namespace email_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    if (ret("new") == 0) return nullptr;
    void* p = tpw::ArenaAllocZeroed(size);
    if (gNewCount < 8) gNew[gNewCount++] = reinterpret_cast<uintptr_t>(p);
    return p;
}
void OperatorDelete(void* p) { ev("OperatorDelete", {lab(p)}); }
void MailCtor(void* mail) { ev("MailCtor", {newlab(mail)}); }
int32_t MailConnect(void* mail, char* server, uint16_t port, void (*callback)(int32_t, int32_t, uint8_t*, void*), void* context,
                    char* extra) {
    ev("MailConnect", {lab(mail), port, callback == &MailCbfn ? 0x10268718 : 0, lab(context)}, {server, extra});
    return static_cast<int32_t>(ret("MailConnect"));
}
void MailCbfn(int32_t, int32_t, uint8_t*, void*) {}
void AttachmentCtor(void* self, const char* path) { ev("AttachCtor", {newlab(self)}, {path}); }
void AttachmentDtor(void* self, int16_t flag) { ev("AttachDtor", {lab(self), flag}); }
void PtrGlue(uint32_t tvector, int32_t a, int32_t b) { ev("PtrGlue", {tvector, a, b}); }
void StringAssign(void* str, const char* text, uint32_t length) { ev("StringAssign", {lab(str), length}, {text}); }
void StringDtor(void* str, int16_t flag) { ev("StringDtor", {lab(str), flag}); }
void WStringDtor(void* str, int16_t flag) { ev("WStringDtor", {lab(str), flag}); }
void ListErase(void* list, uint32_t first, void* last) { ev("ListErase", {lab(list), first, lab(last)}); }
void ListClear(void* list) { ev("ListClear", {lab(list)}); }
void WSACleanup() { ev("WSACleanup", {}); }
}  // namespace email_callee

extern "C" {

uint64_t tpw_email_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The addresses operator new handed out in this case (0 past the count), and the reset before a case.
uint64_t tpw_email_new(int32_t i) { return i >= 0 && i < gNewCount ? gNew[i] : 0; }
void tpw_email_reset() { gNewCount = 0; }

// The base of the arena: a Ptr32 field holds the address minus this.
uint64_t tpw_email_base() { return static_cast<uint64_t>((uintptr_t)tpw::gArenaBase); }

int32_t tpw_email_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Email));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListSize));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString10));
    out[n++] = static_cast<int64_t>(offsetof(Email, mWString20));
    out[n++] = static_cast<int64_t>(offsetof(Email, mAttachment));
    out[n++] = static_cast<int64_t>(offsetof(Email, mMail));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString38));
    out[n++] = static_cast<int64_t>(offsetof(Email, mF3d));
    out[n++] = static_cast<int64_t>(offsetof(Email, mOutbox));
    out[n++] = static_cast<int64_t>(offsetof(Email, mCallback));
    return n;
}

uint64_t tpw_email_destruct(uint64_t self, int16_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Email::Destruct(reinterpret_cast<Email*>(static_cast<uintptr_t>(self)), flag)));
}
void tpw_email_reset_fn(uint64_t self) { reinterpret_cast<Email*>(static_cast<uintptr_t>(self))->Reset(); }
uint32_t tpw_email_attach(uint64_t self, uint64_t path) {
    return reinterpret_cast<Email*>(static_cast<uintptr_t>(self))->AttachData(reinterpret_cast<const char*>(static_cast<uintptr_t>(path)));
}
uint32_t tpw_email_send(uint64_t self, uint64_t server, int16_t port, uint64_t extra, uint64_t outbox) {
    return reinterpret_cast<Email*>(static_cast<uintptr_t>(self))
        ->Send(reinterpret_cast<const char*>(static_cast<uintptr_t>(server)), port,
               reinterpret_cast<char*>(static_cast<uintptr_t>(extra)), reinterpret_cast<Outbox*>(static_cast<uintptr_t>(outbox)));
}

}  // extern "C"
