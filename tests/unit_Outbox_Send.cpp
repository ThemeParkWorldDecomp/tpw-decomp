// Test library for Outbox::Send and Outbox::NextMail (unit v5218; libtpw_unit_Outbox_Send.dylib, tests/test_Outbox_Send.py), main app:
//   src/game/online/email.cpp   Outbox::Send, Outbox::NextMail
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues: the outbox's
// ReadMailData / DeleteMail / Save, Email::Send, the constructors, the login getters, operator new / delete, the string and list
// operations on the e-mail's word fields. A pointer into a known block is logged as block label * 0x1000 + offset (Where), a text as
// its length and a CRC-32. The globals of other files the unit reads (gOnlineSystem) are defined here.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "Ptr32.h"

using namespace calllog;

CFSOnlineSystem* gOnlineSystem;   // 0x10380ab0

namespace {

struct Block {
    uint64_t base;
    uint32_t size;
};
Block gBlocks[8];
void* gPrepEmail;
void* gPrepSys;
void* gPrepAtt;
const char16_t* gLoginAddress;
const char16_t* gLoginName;
const char16_t* gLoginPassword;

int64_t Where(const void* p) {
    uint64_t a = reinterpret_cast<uintptr_t>(p);
    if (a == 0) return 0;
    for (int i = 1; i < 8; i++)
        if (gBlocks[i].base != 0 && a >= gBlocks[i].base && a < gBlocks[i].base + gBlocks[i].size)
            return static_cast<int64_t>(i) * 0x1000 + static_cast<int64_t>(a - gBlocks[i].base);
    return -1;
}

int64_t Crc(const uint8_t* data, size_t n) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return static_cast<int64_t>(~crc);
}

int64_t CrcText(const char* text, size_t n) { return Crc(reinterpret_cast<const uint8_t*>(text), n); }

int64_t I(int64_t v) { return v; }

std::list<uint32_t>* ListOf(Outbox* ob, int which) {
    return which == 0x28 ? &ob->mF28 : which == 0x34 ? &ob->mF34 : &ob->mF40;
}

}  // namespace

// ---- the fakes
namespace email_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {I(size)});
    if (size == 0x4c) return gPrepEmail;
    if (size == 0x404) return gPrepSys;
    if (size == 0xc) return gPrepAtt;
    return tpw::ArenaAllocZeroed(size);
}
void OperatorDelete(void* p) { ev("delete", {Where(p)}); }
void AttachmentCtor(void* self, const char* path) {
    ev("AttachCtor", {Where(self), I(std::strlen(path)), CrcText(path, std::strlen(path))});
}
void StringAssign(void* str, const char* text, uint32_t length) { ev("StringAssign", {Where(str), I(length), CrcText(text, length)}); }
void StringDtor(void* str, int16_t flag) { ev("StringDtor", {Where(str), I(flag)}); }
void ListErase(void* list, uint32_t first, void* last) { ev("ListErase", {Where(list), I(first), Where(last)}); }
}  // namespace email_callee

namespace outbox_callee {
void EmailCtor(void* email) { ev("EmailCtor", {Where(email)}); }
void OnlineSystemCtor(void* system) { ev("SysCtor", {Where(system)}); }
const char16_t* LoginEmailAddress(CFSOnlineSystem* system) {
    ev("LoginEmailAddress", {Where(system)});
    return gLoginAddress;
}
const char16_t* LoginCustomerName(CFSOnlineSystem* system) {
    ev("LoginCustomerName", {Where(system)});
    return gLoginName;
}
const char16_t* LoginPassword(CFSOnlineSystem* system) {
    ev("LoginPassword", {Where(system)});
    return gLoginPassword;
}
int32_t LoginPlayerID(CFSOnlineSystem* system) {
    ev("LoginPlayerID", {Where(system)});
    return static_cast<int32_t>(ret("PlayerID"));
}
int32_t LoginCustomerID(CFSOnlineSystem* system) {
    ev("LoginCustomerID", {Where(system)});
    return static_cast<int32_t>(ret("CustomerID"));
}
void OperatorDeleteArray(void* p) { ev("deletea", {Where(p)}); }
void WStringAssign(void* str, const char16_t* text, uint32_t length) {
    ev("WStringAssign", {Where(str), I(length), Crc(reinterpret_cast<const uint8_t*>(text), size_t(length) * 2)});
}
void StringCtor(void* str, const char* text) {
    size_t n = std::strlen(text);
    ev("StringCtor", {I(n), CrcText(text, n)});
    char* copy = static_cast<char*>(tpw::ArenaAllocZeroed(static_cast<uint32_t>(n + 1)));
    std::memcpy(copy, text, n);
    *static_cast<uint32_t*>(str) = tpw::ArenaOffset(copy);
}
void ListInsert(void* list, void* where, const void* str) {
    uint32_t off = *static_cast<const uint32_t*>(str);
    const char* text = reinterpret_cast<const char*>(tpw::gArenaBase + off);
    ev("ListInsert", {Where(list), Where(where), I(std::strlen(text)), CrcText(text, std::strlen(text))});
}
}  // namespace outbox_callee

bool Outbox::ReadMailData(int32_t mailId) {
    ev("ReadMailData", {Where(this), I(mailId)});
    return (ret("ReadMailData") & 0xff) != 0;
}
void Outbox::DeleteMail(int32_t mailId) { ev("DeleteMail", {Where(this), I(mailId)}); }
void Outbox::Save() { ev("Save", {Where(this)}); }

bool Email::Send(const char* server, int16_t port, char* extra, Outbox* outbox) {
    size_t ns = std::strlen(server);
    size_t ne = extra != nullptr ? std::strlen(extra) : 0;
    ev("EmailSend", {Where(this), I(port), I(extra != nullptr), Where(outbox), I(ns), CrcText(server, ns), I(ne),
                     CrcText(extra != nullptr ? extra : "", ne)});
    return ret("EmailSend") != 0;
}

extern "C" {

uint64_t tpw_ob_addr(const char* name) {
    if (std::strcmp(name, "gOnlineSystem") == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&gOnlineSystem));
    return 0;
}

uint64_t tpw_ob_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_ob_ptr32(uint64_t p) { return p == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(p))); }

uint64_t tpw_ob_unptr32(uint32_t off) { return off == 0 ? 0 : static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase + off)); }

int32_t tpw_ob_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(Email, mAttachment));
    out[n++] = static_cast<int64_t>(offsetof(Email, mCallback));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListPrev));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListNext));
    out[n++] = static_cast<int64_t>(offsetof(Email, mF18));
    out[n++] = static_cast<int64_t>(offsetof(Email, mF28));
    out[n++] = static_cast<int64_t>(sizeof(Email));
    return n;
}

// A new outbox: raw zeroed memory, the members built in place (the real constructor is not part of this library).
uint64_t tpw_ob_new_outbox() {
    void* raw = std::calloc(1, sizeof(Outbox));
    Outbox* ob = static_cast<Outbox*>(raw);
    new (&ob->mF00) std::string;
    new (&ob->mF04) std::string;
    new (&ob->mF08) std::string;
    new (&ob->mF0C) std::list<std::string>;
    new (&ob->mF18) std::string;
    new (&ob->mF28) std::list<uint32_t>;
    new (&ob->mF34) std::list<uint32_t>;
    new (&ob->mF40) std::list<uint32_t>;
    new (&ob->mF50) std::string;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ob));
}
uint32_t tpw_ob_outbox_size() { return static_cast<uint32_t>(sizeof(Outbox)); }

void tpw_ob_block(int32_t index, uint64_t base, uint32_t size) {
    gBlocks[index].base = base;
    gBlocks[index].size = size;
}
void tpw_ob_prep(uint64_t email, uint64_t system, uint64_t attachment) {
    gPrepEmail = reinterpret_cast<void*>(static_cast<uintptr_t>(email));
    gPrepSys = reinterpret_cast<void*>(static_cast<uintptr_t>(system));
    gPrepAtt = reinterpret_cast<void*>(static_cast<uintptr_t>(attachment));
}
void tpw_ob_login(uint64_t address, uint64_t name, uint64_t password) {
    gLoginAddress = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(address));
    gLoginName = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(name));
    gLoginPassword = reinterpret_cast<const char16_t*>(static_cast<uintptr_t>(password));
}

void tpw_ob_set_str(uint64_t ob, int32_t which, const char* text) {
    Outbox* o = reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob));
    (which == 0x08 ? o->mF08 : which == 0x18 ? o->mF18 : o->mF50) = text;
}
int32_t tpw_ob_get_str(uint64_t ob, int32_t which, char* out, int32_t max) {
    Outbox* o = reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob));
    const std::string& s = which == 0x08 ? o->mF08 : which == 0x18 ? o->mF18 : o->mF50;
    std::strncpy(out, s.c_str(), max - 1);
    out[max - 1] = 0;
    return static_cast<int32_t>(s.size());
}
void tpw_ob_rcpt_push(uint64_t ob, const char* text) { reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob))->mF0C.push_back(text); }
void tpw_ob_list_push(uint64_t ob, int32_t which, uint32_t value) {
    ListOf(reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob)), which)->push_back(value);
}
int32_t tpw_ob_list_read(uint64_t ob, int32_t which, uint32_t* out, int32_t max) {
    std::list<uint32_t>* l = ListOf(reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob)), which);
    int32_t n = 0;
    for (uint32_t v : *l)
        if (n < max) out[n++] = v;
    return static_cast<int32_t>(l->size());
}
void tpw_ob_set_n(uint64_t ob, int32_t which, uint64_t value) {
    Outbox* o = reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob));
    if (which == 0x54) o->mF54 = static_cast<uint16_t>(value);
    else if (which == 0x58) o->mF58 = static_cast<int32_t>(value);
    else o->mEmail = reinterpret_cast<Email*>(static_cast<uintptr_t>(value));
}
uint64_t tpw_ob_get_n(uint64_t ob, int32_t which) {
    Outbox* o = reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob));
    if (which == 0x54) return o->mF54;
    if (which == 0x58) return static_cast<uint32_t>(o->mF58);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(o->mEmail));
}

int32_t tpw_ob_Send(uint64_t ob, uint64_t server, int32_t port, uint64_t extra) {
    return reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob))
        ->Send(reinterpret_cast<const char*>(static_cast<uintptr_t>(server)), static_cast<int16_t>(port), reinterpret_cast<char*>(static_cast<uintptr_t>(extra)));
}
int32_t tpw_ob_NextMail(uint64_t ob, uint64_t email) {
    return reinterpret_cast<Outbox*>(static_cast<uintptr_t>(ob))->NextMail(reinterpret_cast<Email*>(static_cast<uintptr_t>(email)));
}

}  // extern "C"
