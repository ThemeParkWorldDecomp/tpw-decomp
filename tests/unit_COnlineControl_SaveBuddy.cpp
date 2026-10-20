// Test library for COnlineControl::SaveBuddy and COnlineControl::ProcessVoteResponse (unit v5342; libtpw_unit_COnlineControl_SaveBuddy.dylib,
// tests/test_COnlineControl_SaveBuddy.py), main app:
//   src/COnlineControl_..._SaveBuddy.cpp   OnlineSaveBuddy (the answer of SaveBuddy), COnlineControl::SaveBuddy, ::ProcessVoteResponse
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues. The stack objects the
// original builds (stream, filebuf, strings) are told apart by remembered addresses: an event holds "is this the object made earlier".
// The globals of other files are defined here (gText and gTPGlobals through an asm label, so no constructor runs).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "Text.h"
#include "TbITable.h"
#include "platform/samsutils.h"

using namespace calllog;

// ---- globals of other files
uint32_t gOfsWVtbl[4];
char gIosFailureDtorVec[16];
char gIosFailureVtbl[16];
CFSUserInfo* gUserInfo;
alignas(16) char gTPGlobalsStorage[sizeof(TPGlobals)] __asm__("_gTPGlobals");
alignas(16) char gTextStorage[sizeof(CText)] __asm__("_gText");

namespace {

uint32_t gWorld[8];   // 0 ios block, 1 path data, 2 user info block returned by new, 3 new[] block
const void* sStream;
const void* sBuf;
const void* sName;
const void* sFull;
const void* sFirst;
const void* sSecond;
COnlineControl* sCur;

int64_t B(bool value) { return value ? 1 : 0; }
std::string W(const char16_t* text) {
    std::string out;
    if (text)
        for (; *text; ++text) out.push_back(static_cast<char>(*text));
    return out;
}
void* P32(uint32_t value) { return value ? reinterpret_cast<void*>(tpw::gArenaBase + value) : nullptr; }
void Reset() { sStream = sBuf = sName = sFull = sFirst = sSecond = nullptr; sCur = nullptr; }

struct FailureObject {
    const void* mVtable;
    char* mMessage;
};

}  // namespace

// ---- the fakes of SaveBuddy
namespace ignore_callee {
void OfstreamCtor(IgnoreStream* stream, int32_t flag) {
    ev("ofs_ctor", {flag});
    sStream = stream;
    stream->mIos = static_cast<IgnoreIos*>(P32(gWorld[0]));
}
void UserInfoCtor(void* self) { ev("user_ctor", {lab(self)}); }
void ReturnOnlinePath(IgnorePathString*, CFSUserInfo* info) { ev("online_path", {lab(info)}); }
void PathCopy(IgnorePathString* self, const IgnorePathString*) {
    ev("path_copy", {});
    sFull = self;
    self->mData = static_cast<const char16_t*>(P32(gWorld[1]));
}
void PathDtor(IgnorePathString* self, int16_t flag) { ev("path_dtor", {flag, B(self == sFull)}); }
void PathAppendString(IgnorePathString* self, const void* other) {
    ev("path_apps", {B(self == sFull), static_cast<int64_t>(static_cast<const char*>(other) - gTPGlobalsStorage)});}
void PathAppendText(IgnorePathString* self, const char16_t* text) { ev("path_appt", {B(self == sFull)}, {W(text).c_str()}); }
void CharStringAssign(void* self, const char* text, uint32_t count) { ev("str_assign", {B(self == sName), count}, {text}); }
void CharStringAppend(void* self, const char* text, uint32_t count) { ev("str_append", {B(self == sName), count}, {text}); }
void SprintfWide(char* buffer, const char* format, const char16_t* text) {
    std::string t = W(text);
    ev("sprintf", {}, {format, t.c_str()});
    std::strncpy(buffer, t.c_str(), 259);
    buffer[259] = 0;
}
uint32_t FilebufOpen(IgnoreFilebuf* filebuf, const char* path, int32_t mode) {
    ev("open", {mode}, {path});
    sBuf = filebuf;
    filebuf->mFile = static_cast<uint32_t>(ret("file"));
    return static_cast<uint32_t>(ret("open"));
}
uint32_t FilebufClose(IgnoreFilebuf* filebuf) {
    ev("close", {B(filebuf == sBuf)});
    return static_cast<uint32_t>(ret("close"));
}
char* NewArray(uint32_t size) {
    ev("new_array", {size});
    return static_cast<char*>(P32(gWorld[3]));
}
void RefPtrCtor(void* self, char* pointer) {
    ev("refptr", {B(pointer == P32(gWorld[3]))});
    *static_cast<char**>(self) = pointer;
}
void FilebufDtor(IgnoreFilebuf* filebuf, int16_t flag) { ev("buf_dtor", {B(filebuf == sBuf), flag}); }
void OstreamDtor(IgnoreStream* stream, int16_t flag) {
    ev("os_dtor", {B(stream == sStream), flag, B(stream->mSlot == gOfsWVtbl)});
}
void IosDtor(void* ios, int16_t flag) {
    IgnoreIos* block = static_cast<IgnoreIos*>(P32(gWorld[0]));
    int32_t offset = static_cast<int32_t>(reinterpret_cast<intptr_t>(ios) - reinterpret_cast<intptr_t>(block));
    ev("ios_dtor", {flag, B(block->mBaseOffset == offset), B(block->mVtable == reinterpret_cast<const uint8_t*>(gOfsWVtbl) + 0xc)});
}
void SwprintfWide(char16_t* buffer, uint32_t count, const char16_t* format, const char16_t* text) {
    ev("swprintf", {count}, {W(format).c_str(), W(text).c_str()});
    uint32_t n = 0;
    while (text[n] && n < 31) {
        buffer[n] = text[n];
        ++n;
    }
    buffer[n] = 0;
}
void OstreamWrite(IgnoreStream* stream, const char16_t* text) { ev("os_write", {B(stream == sStream)}, {W(text).c_str()}); }
void TreeIncrement(uint32_t* node) {
    ev("increment", {lab(P32(*node))});
    *node = *static_cast<uint32_t*>(P32(*node + 4));
}
}  // namespace ignore_callee

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("new", {size});
    return P32(gWorld[2]);
}
void StringCtor(void* self) {
    ev("str_ctor", {});
    sName = self;
}
void StringDtor(void* self, int16_t flag) { ev("str_dtor", {B(self == sName), flag}); }
void WStringCtorFrom(void* self, const char16_t* text) {
    ev("wstr_from", {static_cast<int64_t>(reinterpret_cast<const char*>(text) - reinterpret_cast<const char*>(sCur))});
    sFirst = self;
    *static_cast<const char16_t**>(self) = text;
}
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("wstr_copy", {B(other == sFirst), pos, count});
    sSecond = self;
    *static_cast<const char16_t**>(self) = *static_cast<const char16_t* const*>(other);
}
void WStringDtor(void* self, int16_t flag) { ev("wstr_dtor", {self == sSecond ? 2 : (self == sFirst ? 1 : 0), flag}); }
}  // namespace online_callee

namespace buddy_callee {
void Throw(const char* typeName, void* object, void* destructorVector) {
    FailureObject* failure = static_cast<FailureObject*>(object);
    ev("throw", {B(failure->mVtable == gIosFailureVtbl), B(destructorVector == gIosFailureDtorVec), B(failure->mMessage == P32(gWorld[3]))},
       {typeName, failure->mMessage});
}
}  // namespace buddy_callee

// ---- the fakes of ProcessVoteResponse
int32_t TbITable::FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const {
    ev("format", {lab(const_cast<TbITable*>(this)), static_cast<int64_t>(reinterpret_cast<char*>(out) - reinterpret_cast<char*>(sCur)), size, id});
    char16_t* p = out;
    *p++ = u'T';
    char digits[12];
    std::snprintf(digits, sizeof digits, "%u", id);
    for (const char* d = digits; *d; ++d) *p++ = static_cast<char16_t>(*d);
    *p = 0;
    return 0;
}

void COnlineControl::ReceiveText(std::u16string& text, int32_t colour) {
    const char16_t* data = *reinterpret_cast<const char16_t* const*>(&text);
    ev("receive", {lab(this), B(&text == sSecond), static_cast<int64_t>(reinterpret_cast<const char*>(data) - reinterpret_cast<const char*>(this)), colour},
       {W(data).c_str()});
}

void SamsUtilities::UDebug(const char* text, ...) { ev("udebug", {}, {text}); }

extern "C" {

uint64_t tpw_bud_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gUserInfo", &gUserInfo},
        {"gTPGlobals", gTPGlobalsStorage},
        {"gText", gTextStorage},
        {"gOfsWVtbl", gOfsWVtbl},
        {"gIosFailureDtorVec", gIosFailureDtorVec},
        {"gIosFailureVtbl", gIosFailureVtbl},
        {"world", gWorld},
        {"arena", reinterpret_cast<const void*>(tpw::ArenaBase())},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_bud_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_bud_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreIos, mBaseOffset));
    out[n++] = static_cast<int64_t>(offsetof(ignore_callee::IgnoreFilebuf, mFile));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&reinterpret_cast<CText*>(gTextStorage)->mResourcePointers[19]) - gTextStorage);
    out[n++] = static_cast<int64_t>(sizeof(void*));
    return n;
}

void tpw_bud_reset() { Reset(); }

uint32_t tpw_bud_save(uint64_t self, uint64_t path) {
    return OnlineSaveBuddy(reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self)), reinterpret_cast<const char*>(static_cast<uintptr_t>(path))) ? 1 : 0;
}

void tpw_bud_vote(uint64_t self, int32_t code) {
    sCur = reinterpret_cast<COnlineControl*>(static_cast<uintptr_t>(self));
    sCur->ProcessVoteResponse(code);
}

}  // extern "C"
