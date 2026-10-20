// Test library for Email::SendToOutbox and Email::BuildBody (unit v5162; libtpw_unit_Email_SendToOutbox.dylib, tests/test_Email_SendToOutbox.py), main app:
//   src/Email_Send_Email_SendToOutbox.cpp
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: the string and list callees, Outbox::WriteMailData and
// UTimer::GetAbsolute. A string field is named in the log by its Mac offset: 0..0x4b in the Email, 0x1000 + offset in the Outbox.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "calllog.h"
#include "email.h"
#include "platform/arena.h"
#include "platform/samsutils.h"

using namespace calllog;

namespace {

Email* gEmail = nullptr;
Outbox* gOutbox = nullptr;
const void* gLastCtorStr = nullptr;
std::string gLastCtorText;
uint64_t gTime = 0;

// The log's name of a string or list field: the Email's offset, or 0x1000 + the Mac offset of the Outbox's member.
int64_t FieldId(const void* p) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    const uint8_t* e = reinterpret_cast<const uint8_t*>(gEmail);
    if (gEmail != nullptr && b >= e && b < e + sizeof(Email)) return b - e;
    if (gOutbox != nullptr) {
        if (p == &gOutbox->mF04) return 0x1000 + 4;
        if (p == &gOutbox->mF08) return 0x1000 + 8;
        if (p == &gOutbox->mF0C) return 0x1000 + 0xc;
        if (p == &gOutbox->mF18) return 0x1000 + 0x18;
    }
    return -2;
}

}  // namespace

// ---- the fakes
namespace email_callee {
void StringAssign(void* str, const char* text, uint32_t length) {
    ev("StringAssign", {FieldId(str), static_cast<int64_t>(length)}, {text});
}
void StringDtor(void* str, int16_t flag) {
    ev("StringDtor", {static_cast<int64_t>(flag), str == gLastCtorStr ? 1 : 0}, {});
}
void ListErase(void* list, uint32_t first, void* last) {
    (void)first;
    (void)last;
    // The iterators are the list's first node and its sentinel on the Mac (the stub checks that); here the list is named.
    ev("ListErase", {FieldId(list), 1, 1}, {});
}
}  // namespace email_callee

namespace outbox_callee {
void StringCtor(void* str, const char* text) {
    gLastCtorStr = str;
    gLastCtorText = text;
    ev("StringCtor", {}, {text});
}
void ListInsert(void* list, void* where, const void* str) {
    (void)where;
    ev("ListInsert", {FieldId(list), 1, str == gLastCtorStr ? 1 : 0}, {gLastCtorText.c_str()});
}
}  // namespace outbox_callee

namespace sendbody_callee {
void StringAppend(void* str, const char* text, uint32_t length) {
    ev("StringAppend", {FieldId(str), static_cast<int64_t>(length)}, {text});
}
}  // namespace sendbody_callee

int32_t Outbox::WriteMailData() {
    ev("WriteMailData", {0}, {});
    return static_cast<int32_t>(ret("WriteMailData"));
}

namespace SamsUtilities {
uint64_t UTimer::GetAbsolute() {
    ev("GetAbsolute", {}, {});
    return gTime;
}
}  // namespace SamsUtilities

extern "C" {

uint64_t tpw_se_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_se_base() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaBase())); }

int32_t tpw_se_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Email));
    out[n++] = static_cast<int64_t>(offsetof(Email, mListNext));
    out[n++] = static_cast<int64_t>(offsetof(Email, mString38));
    out[n++] = static_cast<int64_t>(offsetof(Email, mAttachment));
    return n;
}

uint32_t tpw_se_send(uint64_t email, uint64_t outbox) {
    gEmail = reinterpret_cast<Email*>(static_cast<uintptr_t>(email));
    gOutbox = reinterpret_cast<Outbox*>(static_cast<uintptr_t>(outbox));
    return static_cast<uint32_t>(gEmail->SendToOutbox(gOutbox));
}

void tpw_se_build(uint64_t email, uint32_t timeHigh, uint32_t timeLow) {
    gEmail = reinterpret_cast<Email*>(static_cast<uintptr_t>(email));
    gOutbox = nullptr;
    gTime = (static_cast<uint64_t>(timeHigh) << 32) | timeLow;
    gEmail->BuildBody();
}

}  // extern "C"
