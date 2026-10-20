// Test library for COnlineControl::ProcessQueue, AddBuddy and LoadBuddy (unit v5695; libtpw_unit_COnlineControl_ProcessQueue.dylib,
// tests/test_COnlineControl_ProcessQueue.py), main app:
//   src/COnlineControl_..._WantToShutDown_COnlineControl_ProcessQueue.cpp
//
// The callees outside the unit are fakes that record what the real ones read and answer from the test's queues; the globals of other
// files the unit reads are defined here (zero at start like the Mac's bss).
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "UI_pump.h"
#include "FS_UserInfo.h"
#include "TPGlobals.h"
#include "UIInterface.h"
#include "Ptr32.h"
#include "std_basic_istream.h"
#include "std_basic_ifstream.h"
#include "std_deque_errorthingy.h"

using namespace calllog;
using namespace ignore_callee;

// ---- globals of other files the unit reads or writes, defined here
COnlineControl* gOnlineControl;
CFSUserInfo* gUserInfo;
// TPGlobals has members with constructors from files this library does not compile; only its address is used, so raw zeroed storage
// carries the symbol.
alignas(16) char gTPGlobalsStorage[0x8000] __asm__("_gTPGlobals");
UIInterface gUIInterface;
char gIosFailureVtbl[16];
char gIosFailureDtorVec[16];
uint32_t gIfsWVtblB[4];

namespace {

uint8_t* gThis = nullptr;   // the COnlineControl of the call: events name addresses inside it by offset
alignas(16) uint8_t gNode[0x40];
alignas(16) uint8_t gIosBlock[0x60];
alignas(16) uint8_t gFacet[0x80];
alignas(16) char16_t gPathText[0x80];

int32_t Rel(const void* p) { return static_cast<int32_t>(reinterpret_cast<const uint8_t*>(p) - gThis); }

uint32_t W(const void* base, size_t offset) {
    uint32_t value;
    std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(value));
    return value;
}

void SetW(void* base, size_t offset, uint32_t value) { std::memcpy(static_cast<uint8_t*>(base) + offset, &value, sizeof(value)); }

int32_t Delta(const void* a, const void* b) { return static_cast<int32_t>(reinterpret_cast<uintptr_t>(a) - reinterpret_cast<uintptr_t>(b)); }

size_t WLen(const char16_t* text) {
    size_t n = 0;
    while (text[n] != 0) n++;
    return n;
}

// The string object is a pointer to a representation whose data pointer sits at +0x10 (Mac +0xc).
const char16_t* TextOf(const void* string) {
    const uint8_t* rep = *static_cast<const uint8_t* const*>(string);
    const char16_t* text;
    std::memcpy(&text, rep + 0x10, sizeof(text));
    return text;
}

void MakeRep(void* self, const char16_t* text) {
    const size_t n = WLen(text);
    char16_t* copy = static_cast<char16_t*>(tpw::ArenaAllocZeroed(2 * (n + 1)));
    std::memcpy(copy, text, 2 * n);
    uint8_t* rep = static_cast<uint8_t*>(tpw::ArenaAllocZeroed(0x20));
    std::memcpy(rep + 0x10, &copy, sizeof(copy));
    *static_cast<uint8_t**>(self) = rep;
}

// A signature of a narrow text: its length and a weighted sum (the Python side computes the same).
int64_t SigLen(const char* s) { return static_cast<int64_t>(std::strlen(s)); }
int64_t SigSum(const char* s) {
    uint64_t sum = 0;
    for (size_t i = 0; s[i] != 0; i++) sum += (i + 1) * static_cast<uint8_t>(s[i]);
    return static_cast<int64_t>(sum & 0x7fffffff);
}

int64_t SigSumW(const char16_t* s) {
    uint64_t sum = 0;
    for (size_t i = 0; s[i] != 0; i++) sum += (i + 1) * static_cast<uint16_t>(s[i]);
    return static_cast<int64_t>(sum & 0x7fffffff);
}

IgnoreIos* Ios() { return reinterpret_cast<IgnoreIos*>(gIosBlock); }

}  // namespace

// ---- the fakes (the same event name and arguments as the module's PowerPC stub)

// bthread_mutex (0x10200d8c, 0x10200de0)
void bthread_mutex::lock() { ev("lock", {Rel(this)}); }
void bthread_mutex::unlock() { ev("unlock", {Rel(this)}); }

// COnlineControl members outside the unit
COnlineControl::COnlineControl() { ev("COnlineControl", {newlab(this)}); }
void COnlineControl::ProcessVoteResponse(int32_t vote) { ev("ProcessVoteResponse", {lab(this), vote}); }

namespace {
void LogVector(const char* name, const COnlineControl* self, void* vector) {
    const uint32_t capacity = W(vector, 0);
    const uint32_t size = W(vector, 4);
    int64_t command = -1;
    if (size != 0) {
        const uint8_t* data = Ptr32<uint8_t>{W(vector, 8)}.get();
        command = static_cast<int32_t>(W(data, 8));
    }
    ev(name, {lab(self), static_cast<int32_t>(capacity), static_cast<int32_t>(size), command});
}
}  // namespace
void COnlineControl::ProcessResponse(void* vector) { LogVector("ProcessResponse", this, vector); }
void COnlineControl::ProcessInitResponse(void* vector) { LogVector("ProcessInitResponse", this, vector); }

uint32_t COnlineControl::GetDataForUser(const nocase_string& key) const {
    ev("GetDataForUser", {lab(this), lab(&key)});
    return static_cast<uint32_t>(ret("GetDataForUser"));
}

namespace errdeque_callee {
// The block table's slots: the word at +0xc of the cdeque is the table (a Ptr32), the slot index is the block number.
void* CdequeIndex(void* cdeque, uint32_t index) {
    ev("CdequeIndex", {index});
    return Ptr32<uint8_t>{W(cdeque, 0xc)}.get() + 4 * static_cast<size_t>(index);
}
}  // namespace errdeque_callee

namespace online_callee {
void* OperatorNew(uint32_t size) {
    ev("OperatorNew", {size});
    return tpw::ArenaAllocZeroed(size);
}
void* HandleToPointer(uint32_t handle) { return Ptr32<uint8_t>{handle}.get(); }

void ArgumentDequePopFront(void* deque) {
    ev("PopFront", {Rel(deque)});
    SetW(deque, 0x14, W(deque, 0x14) + 1);
    SetW(deque, 0x18, W(deque, 0x18) - 1);
}

void StringCtor(void* self) { ev("StringCtor", {newlab(self)}); }
void StringDtor(void* self, int16_t flag) { ev("StringDtor", {lab(self), flag}); }
void WStringCtorFrom(void* self, const char16_t* text) {
    ev("WStringCtorFrom", {newlab(self), static_cast<int64_t>(WLen(text)), SigSumW(text)});
    MakeRep(self, text);
}
void WStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("WStringCopy", {newlab(self), lab(other), pos, count});
    MakeRep(self, TextOf(other));
}
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {lab(self), flag}); }
void NocaseWStringCopy(void* self, const void* other, uint32_t pos, uint32_t count) {
    ev("NocaseWStringCopy", {newlab(self), lab(other), pos, count});
}
void NocaseWStringDtor(void* self, int16_t flag) { ev("NocaseWStringDtor", {lab(self), flag}); }
void* BuddyTreeFind(void* tree, const void* key) {
    ev("BuddyTreeFind", {Rel(tree), lab(key)});
    return ret("find") != 0 ? static_cast<void*>(gNode) : static_cast<void*>(static_cast<uint8_t*>(tree) + 4);
}
void BuddyTreeInsert(void* tree, const BuddyEntry* entry) {
    ev("BuddyTreeInsert", {Rel(tree), lab(entry), entry->mDirect, entry->mFlag});
}
void UIOnlineAddBuddy(void* ui, const char16_t* text, int32_t data) {
    ev("OnlineAddBuddy", {lab(ui), data, static_cast<int64_t>(WLen(text)), SigSumW(text)});
}
void UIOnlineUpdateContact(void* ui, const char16_t* text, int32_t data) {
    ev("OnlineUpdateContact", {lab(ui), data, static_cast<int64_t>(WLen(text)), SigSumW(text)});
}
}  // namespace online_callee

namespace ignore_callee {
void IfstreamCtor(IgnoreStream* stream, int32_t flag) {
    ev("IfstreamCtor", {newlab(stream), flag});
    std::memset(gIosBlock, 0, sizeof(gIosBlock));
    stream->mIos = Ios();
    Ios()->mStreamBuf = static_cast<uint32_t>(ret("sb0"));
    Ios()->mExceptions = static_cast<uint32_t>(ret("exc"));
    Ios()->mState = static_cast<uint32_t>(ret("state0"));
}
void UserInfoCtor(void* self) { ev("UserInfoCtor", {newlab(self)}); }
void ReturnOnlinePath(IgnorePathString* result, CFSUserInfo* info) {
    ev("ReturnOnlinePath", {newlab(result), lab(info)});
    result->mLength = static_cast<uint32_t>(WLen(gPathText));
    result->mData = gPathText;
}
void PathCopy(IgnorePathString* self, const IgnorePathString* other) {
    ev("PathCopy", {newlab(self), lab(other)});
    self->mLength = other->mLength;
    self->mData = other->mData;
}
void PathDtor(IgnorePathString* self, int16_t flag) { ev("PathDtor", {lab(self), flag}); }
void PathAppendString(IgnorePathString* self, const void* other) {
    ev("PathAppendString", {lab(self), Delta(other, &gTPGlobals)});
}
void PathAppendText(IgnorePathString* self, const char16_t* text) {
    ev("PathAppendText", {lab(self), static_cast<int64_t>(WLen(text)), SigSumW(text)});
}
void CharStringAssign(void* self, const char* text, uint32_t count) { ev("CharStringAssign", {lab(self), count, SigLen(text), SigSum(text)}); }
void CharStringAppend(void* self, const char* text, uint32_t count) { ev("CharStringAppend", {lab(self), count, SigLen(text), SigSum(text)}); }
void SprintfWide(char* buffer, const char* format, const char16_t* text) {
    ev("SprintfWide", {SigLen(format), SigSum(format), static_cast<int64_t>(WLen(text)), SigSumW(text)});
    size_t n = 0;
    for (; text[n] != 0; n++) buffer[n] = static_cast<char>(text[n]);
    buffer[n] = 0;
}
uint32_t FilebufOpen(IgnoreFilebuf*, const char* path, int32_t mode) {
    ev("FilebufOpen", {mode, SigLen(path), SigSum(path)});
    return static_cast<uint32_t>(ret("FilebufOpen"));
}
uint32_t FilebufClose(IgnoreFilebuf*) {
    ev("FilebufClose", {0});
    return static_cast<uint32_t>(ret("FilebufClose"));
}
char* NewArray(uint32_t size) {
    ev("NewArray", {size});
    return static_cast<char*>(tpw::ArenaAllocZeroed(size));
}
void RefPtrCtor(void* self, char* pointer) {
    ev("RefPtrCtor", {0});
    *static_cast<char**>(self) = pointer;
}
char* StrCpy(char* to, const char* from) {
    ev("StrCpy", {SigLen(from), SigSum(from)});
    std::strcpy(to, from);
    return to;
}
void FilebufDtor(IgnoreFilebuf*, int16_t flag) { ev("FilebufDtor", {flag}); }
void IstreamDtor(IgnoreStream* stream, int16_t flag) {
    ev("IstreamDtor", {lab(stream), flag, Delta(stream->mSlot, gIfsWVtblB), Delta(stream->mIos->mVtable, gIfsWVtblB)});
}
void IosDtor(void* ios, int16_t flag) {
    const bool consistent = reinterpret_cast<uint8_t*>(Ios()) + Ios()->mBaseOffset == static_cast<uint8_t*>(ios);
    ev("IosDtor", {flag, consistent ? 1 : 0});
}
void IosGetloc(IgnoreLocale* out, IgnoreIos* ios) { ev("IosGetloc", {newlab(out), lab(ios)}); }
void* UseFacetCtype(IgnoreLocale* locale) {
    ev("UseFacet", {lab(locale)});
    return gFacet;
}
char16_t CtypeWiden(void* facet, char c) {
    ev("CtypeWiden", {lab(facet), c});
    return static_cast<char16_t>(ret("widen"));
}
void LocaleDtor(IgnoreLocale* locale, int16_t flag) { ev("LocaleDtor", {lab(locale), flag}); }
IgnoreStream* IstreamGetline(IgnoreStream* stream, char16_t* buffer, int32_t count, char16_t delimiter) {
    ev("Getline", {lab(stream), count, delimiter});
    static const char16_t* const kTexts[] = {u"alice", u"Bob", u"", u"abcdefghijklmnopqrstuvwxyz01234", u"Carol D"};
    const int64_t k = ret("getline");
    if (k > 0) {
        const char16_t* text = kTexts[k - 1];
        std::memcpy(buffer, text, 2 * (WLen(text) + 1));
    } else {
        Ios()->mStreamBuf = 0;
    }
    return stream;
}
}  // namespace ignore_callee

void MslThrow(const char* type, MslThrowObject* object, const void* destructors) {
    ev("MslThrow", {SigLen(type), SigSum(type)});
    ev("MslThrowObject", {Delta(object->mVtbl, gIosFailureVtbl), SigLen(object->mMessage), SigSum(object->mMessage),
                          Delta(destructors, gIosFailureDtorVec)});
}

extern "C" {

// The host address of a global or block by the module's name for it.
uint64_t tpw_pq_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"gOnlineControl", &gOnlineControl}, {"gUserInfo", &gUserInfo}, {"gUIInterface", &gUIInterface}, {"gTPGlobals", gTPGlobalsStorage},
        {"node", gNode}, {"ios", gIosBlock}, {"facet", gFacet}, {"pathtext", gPathText},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pq_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The 32-bit word that stands for a host address inside Mac-layout memory (a Ptr32 offset), and the label of a pointer global.
uint32_t tpw_pq_word(uint64_t address) { return address == 0 ? 0 : tpw::ArenaOffset(reinterpret_cast<const void*>(static_cast<uintptr_t>(address))); }
int64_t tpw_pq_label_online() { return gOnlineControl != nullptr ? lab(gOnlineControl) : 0; }
int64_t tpw_pq_label_user() { return gUserInfo != nullptr ? lab(gUserInfo) : 0; }

int32_t tpw_pq_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mStreamBuf));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mState));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mExceptions));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreIos, mBaseOffset));
    out[n++] = static_cast<int64_t>(offsetof(IgnoreStream, mSlot));
    out[n++] = static_cast<int64_t>(offsetof(IgnorePathString, mLength));
    out[n++] = static_cast<int64_t>(offsetof(IgnorePathString, mData));
    out[n++] = static_cast<int64_t>(sizeof(BuddyEntry));
    return n;
}

uint32_t tpw_pq_ProcessQueue(uint64_t self) {
    gThis = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(self));
    reinterpret_cast<COnlineControl*>(gThis)->ProcessQueue();
    return 0;
}

uint32_t tpw_pq_AddBuddy(uint64_t self, uint64_t name, int32_t flags) {
    gThis = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(self));
    return reinterpret_cast<COnlineControl*>(gThis)->AddBuddy(*reinterpret_cast<std::u16string*>(static_cast<uintptr_t>(name)), flags) ? 1 : 0;
}

uint32_t tpw_pq_LoadBuddy(uint64_t self, uint64_t path) {
    gThis = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(self));
    return reinterpret_cast<COnlineControl*>(gThis)->LoadBuddy(reinterpret_cast<const char*>(static_cast<uintptr_t>(path))) ? 1 : 0;
}

}  // extern "C"
