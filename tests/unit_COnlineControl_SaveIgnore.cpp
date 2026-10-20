// Test library for COnlineControl::SaveIgnore and LoadIgnore (unit v5235; libtpw_unit_COnlineControl_SaveIgnore.dylib,
// tests/test_COnlineControl_SaveIgnore.py), main app:
//   src/COnlineControl_COnlineControl_COnlineControl_Locate_COnlineControl_WantToShutDown_COnlineControl_SaveIgnore.cpp
//
// Every callee outside the unit (the stream, filebuf, string and tree library, the user info, the chat) is a fake that records the
// arguments the real one reads and answers from the test's queues or memory. The globals of other files the unit reads are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "online.h"
#include "std_basic_ifstream.h"
#include "std_basic_istream.h"

using namespace calllog;
using namespace ignore_callee;

// ---- globals of other files the unit reads or writes, defined here
CFSUserInfo* gUserInfo;
// gTPGlobals: only its address matters here (the class has a constructor this library does not link), so raw storage defines the symbol.
alignas(16) char gTPGlobalsStorage[0x5000] __asm__("_gTPGlobals");
uint32_t gOfsWVtbl[4];
uint32_t gIfsWVtblB[4];
char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];

namespace {

struct Ctx {
    void* block;   // the stream's ios block
    void* self;    // the COnlineControl
} gCtx;
struct Lines {
    uint32_t n;
    uint32_t idx;
    char16_t text[8][32];
} gLines;
char16_t gPathBuf[64];
uint8_t gFacet[8];
const void* gSret;
const void* gPath;
const void* gStream;
const void* gNewArr;

int64_t Flag(bool b) { return b ? 1 : 0; }

std::vector<const void*> gHandles;

}  // namespace

// The host's stand-in for a 32-bit pointer word (the arena lies above 4 GB): index + 1 into a table, the same pointer the same handle.
uint32_t online_callee::PointerToHandle(const void* p) {
    if (p == nullptr) return 0;
    for (size_t i = 0; i < gHandles.size(); ++i)
        if (gHandles[i] == p) return static_cast<uint32_t>(i + 1);
    gHandles.push_back(p);
    return static_cast<uint32_t>(gHandles.size());
}
void* online_callee::HandleToPointer(uint32_t handle) {
    return handle == 0 || handle > gHandles.size() ? nullptr : const_cast<void*>(gHandles[handle - 1]);
}

// ---- the fakes: the same event names and arguments as the module's PowerPC stubs
void ignore_callee::OfstreamCtor(IgnoreStream* stream, int32_t flag) {
    ev("OfsCtor", {flag});
    stream->mIos = static_cast<IgnoreIos*>(gCtx.block);
    gStream = stream;
}
void ignore_callee::IfstreamCtor(IgnoreStream* stream, int32_t flag) {
    ev("IfsCtor", {flag});
    stream->mIos = static_cast<IgnoreIos*>(gCtx.block);
    gStream = stream;
}
void online_callee::StringCtor(void*) { ev("StringCtor", {}); }
void online_callee::StringDtor(void*, int16_t flag) { ev("StringDtor", {flag}); }
void* online_callee::OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    if (ret("new") != 0) return tpw::ArenaAllocZeroed(size);
    return nullptr;
}
void ignore_callee::UserInfoCtor(void*) { ev("UserInfoCtor", {}); }
void ignore_callee::ReturnOnlinePath(IgnorePathString* result, CFSUserInfo* info) {
    ev("ReturnOnlinePath", {Flag(info != nullptr)});
    gSret = result;
}
void ignore_callee::PathCopy(IgnorePathString* self, const IgnorePathString* other) {
    ev("PathCopy", {Flag(other == gSret)});
    gPath = self;
    self->mData = gPathBuf;
}
void ignore_callee::PathDtor(IgnorePathString* self, int16_t flag) {
    ev("PathDtor", {self == gSret ? 1 : (self == gPath ? 2 : 0), flag});
}
void ignore_callee::PathAppendString(IgnorePathString* self, const void* other) {
    ev("PathAppendString", {Flag(self == gPath), static_cast<int64_t>(static_cast<const uint8_t*>(other) - reinterpret_cast<const uint8_t*>(&gTPGlobals))});
}
void ignore_callee::PathAppendText(IgnorePathString*, const char16_t* text) { ev("PathAppendText", {}, {narrow(text).c_str()}); }
void ignore_callee::CharStringAssign(void*, const char* text, uint32_t count) { ev("CharStringAssign", {count}, {text}); }
void ignore_callee::CharStringAppend(void*, const char* text, uint32_t count) { ev("CharStringAppend", {count}, {text}); }
void ignore_callee::SprintfWide(char* buffer, const char* format, const char16_t* text) {
    ev("sprintf", {}, {format, narrow(text).c_str()});
    size_t i = 0;
    for (; text[i] != 0; ++i) buffer[i] = static_cast<char>(text[i]);
    buffer[i] = 0;
}
uint32_t ignore_callee::FilebufOpen(IgnoreFilebuf* filebuf, const char* path, int32_t mode) {
    ev("open", {mode}, {path});
    filebuf->mFile = static_cast<uint32_t>(ret("file"));
    return static_cast<uint32_t>(ret("open"));
}
uint32_t ignore_callee::FilebufClose(IgnoreFilebuf*) {
    ev("close", {});
    return static_cast<uint32_t>(ret("close"));
}
char* ignore_callee::NewArray(uint32_t size) {
    ev("NewArray", {size});
    gNewArr = new char[size];
    return static_cast<char*>(const_cast<void*>(gNewArr));
}
void ignore_callee::RefPtrCtor(void* self, char* pointer) {
    ev("RefPtr", {});
    *static_cast<char**>(self) = pointer;
}
char* ignore_callee::StrCpy(char* to, const char* from) {
    ev("strcpy", {}, {from});
    return std::strcpy(to, from);
}
void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    ev("throw", {}, {type});
    ev("throwobj", {static_cast<int64_t>(static_cast<const char*>(object->mVtbl) - gIosFailureVtbl),
                    static_cast<int64_t>(static_cast<const char*>(destructors) - gIosFailureDtorVec), Flag(object->mMessage == gNewArr)});
}
void ignore_callee::FilebufDtor(IgnoreFilebuf*, int16_t flag) { ev("FilebufDtor", {flag}); }
void ignore_callee::OstreamDtor(IgnoreStream* stream, int16_t flag) {
    ev("OstreamDtor", {reinterpret_cast<intptr_t>(stream->mSlot) - reinterpret_cast<intptr_t>(gOfsWVtbl),
                       reinterpret_cast<intptr_t>(stream->mIos->mVtable) - reinterpret_cast<intptr_t>(gOfsWVtbl) - 0xc, flag});
}
void ignore_callee::IstreamDtor(IgnoreStream* stream, int16_t flag) {
    ev("IstreamDtor", {reinterpret_cast<intptr_t>(stream->mSlot) - reinterpret_cast<intptr_t>(gIfsWVtblB),
                       reinterpret_cast<intptr_t>(stream->mIos->mVtable) - reinterpret_cast<intptr_t>(gIfsWVtblB) - 0xc, flag});
}
void ignore_callee::IosDtor(void* ios, int16_t flag) {
    IgnoreIos* block = static_cast<IgnoreIos*>(gCtx.block);
    ev("IosDtor", {Flag(block->mBaseOffset == static_cast<int32_t>(static_cast<uint8_t*>(ios) - reinterpret_cast<uint8_t*>(block))), flag});
}
void ignore_callee::SwprintfWide(char16_t* buffer, uint32_t count, const char16_t* format, const char16_t* text) {
    ev("swprintf", {count}, {narrow(format).c_str(), narrow(text).c_str()});
    uint32_t n = 0;
    for (const char16_t* f = format; *f != 0 && n + 1 < count;) {
        if (f[0] == u'%' && f[1] == u'l' && f[2] == u's') {
            for (const char16_t* t = text; *t != 0 && n + 1 < count; ++t) buffer[n++] = *t;
            f += 3;
        } else {
            buffer[n++] = *f++;
        }
    }
    buffer[n] = 0;
}
void ignore_callee::OstreamWrite(IgnoreStream*, const char16_t* text) { ev("OstreamWrite", {}, {narrow(text).c_str()}); }
void ignore_callee::TreeIncrement(uint32_t* node) {
    ev("TreeIncrement", {});
    uint32_t next;
    std::memcpy(&next, online_callee::HandleToPointer(*node), 4);
    *node = next;
}
void ignore_callee::IosGetloc(IgnoreLocale* out, IgnoreIos* ios) {
    ev("getloc", {Flag(ios == gCtx.block)});
    out->mWords[0] = 0x11;
    out->mWords[1] = 0x22;
}
void* ignore_callee::UseFacetCtype(IgnoreLocale* locale) {
    ev("UseFacet", {locale->mWords[0], locale->mWords[1]});
    return gFacet;
}
char16_t ignore_callee::CtypeWiden(void* facet, char c) {
    ev("widen", {Flag(facet == gFacet), c, 1});
    return static_cast<char16_t>(ret("widen"));
}
void ignore_callee::LocaleDtor(IgnoreLocale* locale, int16_t flag) { ev("LocaleDtor", {locale->mWords[0], locale->mWords[1], flag}); }
IgnoreStream* ignore_callee::IstreamGetline(IgnoreStream* stream, char16_t* buffer, int32_t count, char16_t delimiter) {
    ev("getline", {Flag(stream == gStream), count, delimiter});
    if (gLines.idx < gLines.n) {
        const char16_t* text = gLines.text[gLines.idx++];
        int32_t i = 0;
        for (; text[i] != 0 && i + 1 < count; ++i) buffer[i] = text[i];
        buffer[i] = 0;
    } else {
        stream->mIos->mState |= 4;
    }
    return stream;
}
void online_callee::WStringCtorFrom(void* self, const char16_t* text) {
    ev("WStringCtor", {}, {narrow(text).c_str()});
    size_t n = 0;
    while (text[n] != 0) ++n;
    char16_t* copy = new char16_t[n + 1];
    std::memcpy(copy, text, (n + 1) * 2);
    static_cast<IgnoreWString*>(self)->mText = copy;
}
void online_callee::WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    const IgnoreWString* from = static_cast<const IgnoreWString*>(other);
    ev("WStringCopy", {pos, static_cast<int32_t>(count)}, {narrow(from->mText).c_str()});
    static_cast<IgnoreWString*>(self)->mText = from->mText;
}
void online_callee::WStringDtor(void* self, int16_t flag) {
    ev("WStringDtor", {flag}, {narrow(static_cast<IgnoreWString*>(self)->mText).c_str()});
}
void COnlineControl::IgnoreOn(std::u16string& name) {
    const IgnoreWString* w = reinterpret_cast<const IgnoreWString*>(&name);
    ev("IgnoreOn", {Flag(this == gCtx.self)}, {narrow(w->mText).c_str()});
}
void ignore_callee::ChatIgnore(uint32_t chat, const IgnoreWString* name, COnlineControl* context) {
    ev("ChatIgnore", {chat, Flag(context == gCtx.self)}, {narrow(name->mText).c_str()});
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ign_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"userinfo", &gUserInfo}, {"tpg", gTPGlobalsStorage},   {"ofsvtbl", gOfsWVtbl},           {"ifsvtbl", gIfsWVtblB},
        {"ifv", gIosFailureVtbl}, {"dv", gIosFailureDtorVec}, {"ctx", &gCtx}, {"lines", &gLines}, {"pathbuf", gPathBuf},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block the module fills with Mac-offset words (32-bit addressable, like every Ptr32 the headers hold).
uint64_t tpw_ign_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

uint32_t tpw_ign_handle(uint64_t p) { return online_callee::PointerToHandle(reinterpret_cast<const void*>(p)); }

// The offsets and sizes the module relies on (HOST_LAYOUT).
int32_t tpw_ign_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mBaseOffset));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreFilebuf, mFile));
    out[n++] = static_cast<int64_t>(offsetof(IgnorePathString, mData));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreStream, mSlot));
    return n;
}

uint32_t tpw_ign_save(uint64_t self, uint64_t path) {
    return OnlineSaveIgnore(reinterpret_cast<COnlineControl*>(self), reinterpret_cast<const char*>(path)) ? 1 : 0;
}
uint32_t tpw_ign_load(uint64_t self, uint64_t path) {
    return reinterpret_cast<COnlineControl*>(self)->LoadIgnore(reinterpret_cast<const char*>(path)) ? 1 : 0;
}

}  // extern "C"
