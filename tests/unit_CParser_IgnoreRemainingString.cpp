// Test library for CParser's set helpers, the std::set<wchar_t> tree functions, the online set/map destructors and a few small functions
// (unit v51059; libtpw_unit_CParser_IgnoreRemainingString.dylib, tests/test_CParser_IgnoreRemainingString.py), main app:
//   src/COnlineParser_..._CParser_IgnoreRemainingString.cpp   all twenty functions and the globals they read.
//
// The callees outside the unit are recording fakes: WcharSetTree::insert_node_at, operator delete, the three online tree destroyers,
// Argument::Destruct and CPeep::FinalDestroy.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "online.h"
#include "email.h"
#include "ThingReceivers.h"
#include "CMsg.h"

using namespace calllog;

// ---- the fakes (the same event names and arguments as the module's PowerPC stubs; pointers as lab(p))
// insert_node_at: the parent as its value (0x10000 for the end node), flags as 0/1; answers a fixed fake node.
WcharSetTree::node* WcharSetTree::insert_node_at(node* parent, bool left, bool leftmost, const char16_t& value) {
    int64_t parentValue = parent == reinterpret_cast<node*>(&mRoot) ? 0x10000 : static_cast<int64_t>(parent->mValue);
    ev("insert_node_at", {lab(this), parentValue, left ? 1 : 0, leftmost ? 1 : 0, static_cast<int64_t>(value)});
    return reinterpret_cast<node*>(static_cast<uintptr_t>(0xABCDE0));
}

namespace cparser_callee {
void OperatorDelete(void* p) { ev("delete", {lab(p)}); }
}  // namespace cparser_callee

namespace {
// The node word is the low half of an arena address; the high half is the tree's.
void* WidenNode(void* tree, uint32_t node) {
    return reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(tree) & ~static_cast<uintptr_t>(0xFFFFFFFFu)) | node);
}
}  // namespace
namespace chatdata_callee {
void TreeWStringDestroy(void* tree, uint32_t node) { ev("TreeWStringDestroy", {lab(tree), lab(WidenNode(tree, node))}); }
void TreeBuddyDestroy(void* tree, uint32_t node) { ev("TreeBuddyDestroy", {lab(tree), lab(WidenNode(tree, node))}); }
void TreeNocaseDestroy(void* tree, uint32_t node) { ev("TreeNocaseDestroy", {lab(tree), lab(WidenNode(tree, node))}); }
}  // namespace chatdata_callee

Argument* Argument::Destruct(Argument* self, int16_t deleteFlag) {
    ev("Argument::Destruct", {lab(self), deleteFlag});
    return self;
}

void CPeep::FinalDestroy() { ev("CPeep::FinalDestroy", {lab(this)}); }

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_v51059_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"inserted", &gWcharSetInserted},
        {"notinserted", &gWcharSetNotInserted},
        {"countinit", &gWcharSetCountInit},
        {"listinit", &gTokenListInit},
        {"ipbase_vt", &gIPBaseVtableBlock[8]},
        {"ipsys_vt", &gIPSystemVtableBlock[8]},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51059_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
int32_t tpw_v51059_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CParser, mString));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mSeparators));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mWhitespace));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mCursor));
    out[n++] = static_cast<int64_t>(offsetof(CParser, mPos));
    out[n++] = static_cast<int64_t>(sizeof(WcharSetTree));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree, mBegin));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mLeft));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mRight));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mParent));
    out[n++] = static_cast<int64_t>(offsetof(WcharSetTree::node, mValue));
    out[n++] = static_cast<int64_t>(offsetof(v51059::WcharInsertResult, mInserted));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenList, mPrev));
    out[n++] = static_cast<int64_t>(offsetof(CParserTokenList, mNext));
    out[n++] = static_cast<int64_t>(sizeof(Argument));
    return n;
}
#pragma clang diagnostic pop

// ---- the functions, one wrapper each with C types only
uint32_t tpw_v51059_ignore(uint64_t obj) { return reinterpret_cast<CParser*>(obj)->IgnoreRemainingString() ? 1 : 0; }
void tpw_v51059_addws(uint64_t obj, uint32_t c) { reinterpret_cast<CParser*>(obj)->AddWhitespace(static_cast<char16_t>(c)); }
void tpw_v51059_addsep(uint64_t obj, uint32_t c) { reinterpret_cast<CParser*>(obj)->AddSeparator(static_cast<char16_t>(c)); }
void tpw_v51059_setupsep(uint64_t obj) { reinterpret_cast<CParser*>(obj)->SetupSeparatorSet(); }
uint64_t tpw_v51059_find(uint64_t tree, uint64_t key) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        cparser_callee::TreeFind(reinterpret_cast<void*>(tree), reinterpret_cast<const char16_t*>(key))));
}
void tpw_v51059_destroy(uint64_t tree, uint64_t root) {
    cparser_callee::TreeDestroy(reinterpret_cast<void*>(tree), reinterpret_cast<void*>(root));
}
void tpw_v51059_dealloc(uint64_t alloc, uint64_t node, uint32_t count) {
    v51059::NodeDeallocate(reinterpret_cast<void*>(alloc), reinterpret_cast<WcharSetTree::node*>(node), count);
}
void tpw_v51059_adestroy(uint64_t alloc, uint64_t p) {
    v51059::WcharAllocatorDestroy(reinterpret_cast<void*>(alloc), reinterpret_cast<char16_t*>(p));
}
void tpw_v51059_insert_one(uint64_t result, uint64_t tree, uint64_t key) {
    v51059::TreeInsertOne(reinterpret_cast<v51059::WcharInsertResult*>(result), reinterpret_cast<WcharSetTree*>(tree),
                          reinterpret_cast<const char16_t*>(key));
}
void tpw_v51059_tree_ctor(uint64_t tree, uint64_t less, uint64_t alloc) {
    parser_callee::TreeCtor(reinterpret_cast<void*>(tree), reinterpret_cast<const void*>(less), reinterpret_cast<const void*>(alloc));
}
uint64_t tpw_v51059_wset_dtor(uint64_t tree, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        v51059::WcharSetDtor(reinterpret_cast<WcharSetTree*>(tree), static_cast<int16_t>(flag))));
}
void tpw_v51059_final_destroy(uint64_t obj) { reinterpret_cast<COnlinePerson*>(obj)->FinalDestroy(); }
void tpw_v51059_list_ctor(uint64_t list) { online_parser_callee::ListCtor(reinterpret_cast<void*>(list)); }
void tpw_v51059_recv(uint64_t obj, uint64_t msg) {
    reinterpret_cast<COnlinePerson*>(obj)->ReceiveMessage(*reinterpret_cast<const CMsg*>(msg));
}
void tpw_v51059_idle(uint64_t obj) { reinterpret_cast<COnlinePerson*>(obj)->ProcessIdle(); }
void tpw_v51059_destroy_arg(uint64_t first, uint64_t last) {
    std_destroy::DestroyRangeClear(reinterpret_cast<Argument*>(first), reinterpret_cast<Argument*>(last));
}
void tpw_v51059_wsset_dtor(uint64_t self, int32_t flag) {
    online_callee::WStringSetDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag));
}
void tpw_v51059_map_dtor(uint64_t self, int32_t flag) {
    online_callee::BuddyMapDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag));
}
void tpw_v51059_nocase_dtor(uint64_t self, int32_t flag) {
    online_callee::NocaseSetDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag));
}
uint64_t tpw_v51059_ip_dtor(uint64_t self, int32_t flag) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
        v51059::IPSystemDtor(reinterpret_cast<void*>(self), static_cast<int16_t>(flag))));
}

}  // extern "C"
