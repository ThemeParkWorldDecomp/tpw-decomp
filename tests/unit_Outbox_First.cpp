// Test library for the Outbox cursor and mark functions, std::operator== on strings, the std::basic_ifstream<char> constructor and
// Email::Attachment::~Attachment (unit v5950; libtpw_unit_Outbox_First.dylib, tests/test_Outbox_First.py), main app:
//   src/email_load_Outbox_Send_Outbox_WriteMailData_Outbox_First.cpp
//
// The callees outside the unit are fakes that record what the PowerPC stubs record: Outbox::ReadMailData, DeleteMail, Save, the string
// compare, the stream and filebuf constructors, the string destructor and the operator deletes. The globals of other files the unit
// stores the addresses of are defined here. Outbox's constructor and destructor are empty here (the test builds the lists itself).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "email.h"
#include "std_basic_filebuf.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
char gOstreamVt1[0x20];
char gIosBaseVt[0x20];
char gIfstreamVtbl[0x20];

namespace {
Outbox* gOb = nullptr;
char* gBase = nullptr;
const void* gA = nullptr;
const void* gS = nullptr;
void* gData = nullptr;
uint32_t gLastRead = 0;  // First and Delete answer void on the host; the answer is ReadMailData's, or 0 when no mail is read
}  // namespace

// ---- the fakes
Outbox::Outbox() {}
Outbox::~Outbox() {}

bool Outbox::ReadMailData(int32_t mailId) {
    ev("ReadMailData", {this == gOb ? 1 : 0, mailId});
    gLastRead = ret("ReadMailData") != 0;
    return gLastRead != 0;
}

void Outbox::DeleteMail(int32_t mailId) { ev("DeleteMail", {this == gOb ? 1 : 0, mailId}); }

void Outbox::Save() {
    ev("Save", {this == gOb ? 1 : 0, static_cast<int64_t>(mF28.size()), static_cast<int64_t>(mF34.size()),
                static_cast<int64_t>(mF40.size())});
}

namespace msl_char_string {
int32_t compare(const MslCharString* self, uint32_t pos, uint32_t n1, const char* s, uint32_t n2) {
    ev("compare", {static_cast<int64_t>(self->mRep->len), static_cast<int64_t>(pos), static_cast<int64_t>(n1),
                   static_cast<int64_t>(n2), static_cast<int64_t>(static_cast<uint8_t>(s[0])), self == gA ? 1 : 0, s == gS ? 1 : 0});
    return static_cast<int32_t>(ret("compare"));
}
}  // namespace msl_char_string

namespace ifstream_char_callee {
void IstreamCtor(MacIfstream* self, int32_t flag, uint32_t streambuf) {
    ev("IstreamCtor", {static_cast<int64_t>(reinterpret_cast<char*>(self) - gBase), static_cast<int64_t>(flag),
                       static_cast<int64_t>(static_cast<uint32_t>(streambuf - tpw::ArenaOffset(gBase)))});
    uint32_t off = static_cast<uint32_t>(ret("vb"));
    self->mIos.set(reinterpret_cast<MacOstreamIos*>(gBase + off));
}
void FilebufCtor(void* filebuf, uint32_t file) {
    ev("FilebufCtor", {static_cast<int64_t>(reinterpret_cast<char*>(filebuf) - gBase), static_cast<int64_t>(file)});
}
}  // namespace ifstream_char_callee

namespace email_callee {
void StringDtor(void* str, int16_t flag) {
    ev("StringDtor", {static_cast<int64_t>(reinterpret_cast<char*>(str) - gBase), static_cast<int64_t>(flag)});
}
void OperatorDelete(void* p) { ev("Delete", {p == gBase ? 1 : 0}); }
}  // namespace email_callee

namespace outbox_callee {
void OperatorDeleteArray(void* p) { ev("DeleteArray", {p == gData ? 1 : 0}); }
}  // namespace outbox_callee

extern "C" {

uint64_t tpw_ob_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gIfstreamVtbl", gIfstreamVtbl},
        {"gOstreamVt1", gOstreamVt1},
        {"gIosBaseVt", gIosBaseVt},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ob_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint64_t tpw_ob_arena() { return static_cast<uint64_t>((uintptr_t)tpw::gArenaBase); }

int32_t tpw_ob_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(Email::Attachment));
    out[n++] = static_cast<int64_t>(offsetof(MacIfstream, mIosPart));
    out[n++] = static_cast<int64_t>(sizeof(MacIfstream));
    return n;
}

// op: 0 First, 1 Next, 2 MarkToSend, 3 MarkToDelete, 4 Delete. data: three lists of 64 ids (mF28, mF34, mF40), counts: their sizes
// (in and out), cursor: the index in mF28 (in and out).
uint32_t tpw_ob_run(int32_t op, uint32_t* data, uint32_t* counts, uint32_t* cursor) {
    Outbox box;
    gOb = &box;
    box.mF28.assign(data, data + counts[0]);
    box.mF34.assign(data + 64, data + 64 + counts[1]);
    box.mF40.assign(data + 128, data + 128 + counts[2]);
    std::memcpy(box.mF4C, cursor, 4);
    uint32_t answer = 0;
    switch (op) {
        case 0: gLastRead = 0; box.First(); answer = gLastRead; break;
        case 1: answer = box.Next(); break;
        case 2: box.MarkToSend(); break;
        case 3: box.MarkToDelete(); break;
        default: gLastRead = 0; box.Delete(); answer = gLastRead; break;
    }
    std::list<uint32_t>* lists[3] = {&box.mF28, &box.mF34, &box.mF40};
    for (int k = 0; k < 3; k++) {
        counts[k] = static_cast<uint32_t>(lists[k]->size());
        uint32_t i = 0;
        for (uint32_t v : *lists[k]) data[64 * k + i++] = v;
    }
    std::memcpy(cursor, box.mF4C, 4);
    gOb = nullptr;
    return answer;
}

uint32_t tpw_ob_streq(uint32_t lenA, uint32_t lenB, uint64_t buffer) {
    MslCharRep ra{};
    ra.len = lenA;
    MslCharRep rb{};
    rb.len = lenB;
    rb.data = reinterpret_cast<char*>(buffer);
    MslCharString a{&ra};
    MslCharString b{&rb};
    gA = &a;
    gS = rb.data;
    return msl_char_string::equal(&a, &b) ? 1 : 0;
}

uint64_t tpw_ob_ifstream(uint64_t self, int32_t flag) {
    gBase = reinterpret_cast<char*>(self);
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(BasicIfstreamCtor(reinterpret_cast<MacIfstream*>(self), static_cast<int16_t>(flag))));
}

uint64_t tpw_ob_attach(uint64_t self, int32_t flag) {
    gBase = reinterpret_cast<char*>(self);
    Email::Attachment* a = reinterpret_cast<Email::Attachment*>(self);
    gData = a != nullptr ? a->mData.get() : nullptr;
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Email::Attachment::Destruct(a, static_cast<int16_t>(flag))));
}

}  // extern "C"
