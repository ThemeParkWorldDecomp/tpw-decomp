// Test library for Outbox::ReadMailData (unit v5148; libtpw_unit_Outbox_ReadMailData.dylib, tests/test_Outbox_ReadMailData.py), main app:
//   src/email_load_Outbox_ReadMailData.cpp   Outbox::ReadMailData
//
// The stream functions (ctor, open, widen, getline, close, destroy) and the exception raiser are recording fakes; the lines they
// deliver come from a table the test fills, and the stream's state bits, mask and buffer come from the case's queues. The test
// drives one Outbox object, kept here.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <new>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"

using namespace calllog;

// Outbox::Outbox() is defined in a file this library does not compile.
Outbox::Outbox() {}

namespace {
SaveIos gIos;
std::vector<std::string> gLines;
alignas(Outbox) char gStorage[sizeof(Outbox)];
Outbox* gOutbox = nullptr;

std::string* StrField(int32_t off) {
    switch (off) {
        case 0x00: return &gOutbox->mF00;
        case 0x04: return &gOutbox->mF04;
        case 0x08: return &gOutbox->mF08;
        default: return &gOutbox->mF18;
    }
}
}  // namespace

// ---- the fakes
namespace outbox_callee {
void StreamCtor(SaveStream* s) {
    ev("ctor", {});
    std::memset(&gIos, 0, sizeof(gIos));
    s->ios = &gIos;
    s->impl = nullptr;
    s->filebufOpen = 0;
    gIos.mMask = static_cast<uint32_t>(ret("mask"));
    gIos.mRdbuf = static_cast<uint32_t>(ret("rdbuf"));
}
bool StreamOpen(SaveStream* s, const char* name) {
    ev("open", {8}, {name});
    s->filebufOpen = static_cast<uint32_t>(ret("openflag"));
    return ret("open") != 0;
}
char StreamWiden(SaveStream*) {
    char c = static_cast<char>(ret("widen"));
    ev("widen", {10});
    return c;
}
void StreamGetline(SaveStream* s, char* buffer, int32_t size, char delim) {
    ev("getline", {size, static_cast<int64_t>(delim)});
    uint64_t index = static_cast<uint64_t>(ret("line"));
    std::string text = gLines.empty() ? std::string() : gLines[index % gLines.size()];
    if (static_cast<int32_t>(text.size()) > size - 1) text.resize(size - 1);
    std::memcpy(buffer, text.c_str(), text.size() + 1);
    s->ios->mState |= static_cast<uint32_t>(ret("state"));
}
void StreamDestroy(SaveStream*) { ev("destroy", {}); }
}  // namespace outbox_callee

bool SaveStreamClose(SaveStream*) {
    ev("close", {});
    return ret("close") != 0;
}
void SaveStreamThrow(const char* what) { ev("throw", {}, {what}); }

extern "C" {

uint64_t tpw_ob_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ob_reset() {
    if (!gOutbox) gOutbox = new (gStorage) Outbox();
    gOutbox->mF00.clear();
    gOutbox->mF04.clear();
    gOutbox->mF08.clear();
    gOutbox->mF18.clear();
    gOutbox->mF0C.clear();
    std::memset(gOutbox->mF1C, 0, sizeof(gOutbox->mF1C));
    gLines.clear();
}
void tpw_ob_set_str(int32_t off, const char* text) { *StrField(off) = text; }
uint32_t tpw_ob_get_str(int32_t off, char* out, uint32_t size) {
    std::strncpy(out, StrField(off)->c_str(), size - 1);
    out[size - 1] = 0;
    return static_cast<uint32_t>(StrField(off)->size());
}
void tpw_ob_set_int(int32_t slot, int32_t v) { std::memcpy(gOutbox->mF1C + 4 * slot, &v, 4); }
int32_t tpw_ob_get_int(int32_t slot) {
    int32_t v;
    std::memcpy(&v, gOutbox->mF1C + 4 * slot, 4);
    return v;
}
void tpw_ob_list_add(const char* text) { gOutbox->mF0C.push_back(text); }
uint32_t tpw_ob_list_count() { return static_cast<uint32_t>(gOutbox->mF0C.size()); }
void tpw_ob_list_get(uint32_t i, char* out, uint32_t size) {
    auto it = gOutbox->mF0C.begin();
    std::advance(it, i);
    std::strncpy(out, it->c_str(), size - 1);
    out[size - 1] = 0;
}
void tpw_ob_line_add(const char* text) { gLines.push_back(text); }

uint32_t tpw_ob_read(int32_t index) { return static_cast<uint32_t>(gOutbox->ReadMailData(index)); }

}  // extern "C"
