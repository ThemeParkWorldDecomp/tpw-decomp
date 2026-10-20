// Test library for OnlineChatData's constructor and destructor (unit v5292; libtpw_unit_OnlineChatData_OnlineChatData.dylib,
// tests/test_OnlineChatData_OnlineChatData.py), main app: src/..._SaveIgnore_OnlineChatData_OnlineChatData.cpp
//
// The callees (string, tree and list constructors and destructors, operator delete) are fakes that record the sub-object's offset from the
// object (gBase is set by the wrappers) and, for the tree destroys, the node word.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"

using namespace calllog;

namespace {
uintptr_t gBase = 0;
int64_t Off(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) - gBase); }
}  // namespace

namespace online_callee {
void WStringCtor(void* self) { ev("WStringCtor", {Off(self)}); }
void WStringDtor(void* self, int16_t flag) { ev("WStringDtor", {Off(self), flag}); }
void OperatorDelete(void* p) { ev("delete", {Off(p)}); }
}  // namespace online_callee

namespace chatdata_callee {
void TreeWStringCtor(void* tree) { ev("TreeWStringCtor", {Off(tree)}); }
void TreeBuddyCtor(void* tree) { ev("TreeBuddyCtor", {Off(tree)}); }
void TreeNocaseCtor(void* tree) { ev("TreeNocaseCtor", {Off(tree)}); }
void ListCtor(void* list) { ev("ListCtor", {Off(list)}); }
void ListClear(void* list) { ev("ListClear", {Off(list)}); }
void TreeWStringDestroy(void* tree, uint32_t node) { ev("TreeWStringDestroy", {Off(tree), node}); }
void TreeBuddyDestroy(void* tree, uint32_t node) { ev("TreeBuddyDestroy", {Off(tree), node}); }
void TreeNocaseDestroy(void* tree, uint32_t node) { ev("TreeNocaseDestroy", {Off(tree), node}); }
}  // namespace chatdata_callee

extern "C" {

uint64_t tpw_chat_addr(const char*) { return 0; }

uint64_t tpw_chat_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_chat_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(OnlineChatData, mTreeA));
    out[n++] = static_cast<int64_t>(offsetof(OnlineChatData, mTreeB));
    out[n++] = static_cast<int64_t>(offsetof(OnlineChatData, mTreeC));
    out[n++] = static_cast<int64_t>(offsetof(OnlineChatData, mFlag30));
    out[n++] = static_cast<int64_t>(offsetof(OnlineChatData, mList));
    return n;
}

uint32_t tpw_chat_ctor(uint64_t self) {
    gBase = static_cast<uintptr_t>(self);
    OnlineChatData* r = OnlineChatData::Construct(reinterpret_cast<OnlineChatData*>(self));
    return reinterpret_cast<uintptr_t>(r) == self;
}

uint32_t tpw_chat_dtor(uint64_t self, int32_t flag) {
    gBase = static_cast<uintptr_t>(self);
    OnlineChatData* r = OnlineChatData::Destruct(reinterpret_cast<OnlineChatData*>(self), static_cast<int16_t>(flag));
    return reinterpret_cast<uintptr_t>(r) == self;
}

}  // extern "C"
