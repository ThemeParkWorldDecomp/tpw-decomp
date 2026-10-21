// Test library for the std tree constructors, insert_one, destroyers, deque tear_down and four UI forwarders (unit v51126;
// libtpw_unit_std___tree_std_pair_std_map_nocase_string_BuddyI.dylib, tests/test_std___tree_std_pair_std_map_nocase_string_BuddyI.py), main app:
//   src/game/MslSupport.cpp, src/game/ui/UIInterface.cpp, src/game/ui/ui_FeatureStatus.cpp   the 20 functions and the seven globals of the trees
//
// The callees outside the unit (operator delete, the string destructors, the string compares, insert_node_at, the vector tear_down, the
// deque make_iterator and destroy, the cdeque iterator operations, InitOnlineUI, CloseWindow) are fakes that record their arguments and
// answer from the test's queues.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "UIInterface.h"
#include "calllog.h"
#include "email.h"
#include "online.h"
#include "platform/arena.h"
#include "std_basic_filebuf.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

using namespace calllog;
using v51126::ArgVectorPtrCdeque;
using v51126::TreeHeader3;
using v51126::TreeHeader4;
using v51126::TreeInsertResult;

namespace {

uint32_t A(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }
// A Mac word is an arena offset.
void* PW(uint32_t w) { return w != 0 ? static_cast<void*>(tpw::gArenaBase + w) : nullptr; }

}  // namespace

// ---- the fakes
namespace v51126_callee {
bool NocaseLess(const void* a, const void* b) {
    ev("NocaseLess", {lab(a), lab(b)});
    return ret("NocaseLess") != 0;
}
bool WStringLess(const void* a, const void* b) {
    ev("WStringLess", {lab(a), lab(b)});
    return ret("WStringLess") != 0;
}
uint32_t NocaseInsertNodeAt(void* tree, uint32_t parent, bool left, bool leftmost, const void* key) {
    ev("NocaseInsertNodeAt", {lab(tree), lab(PW(parent)), left ? 1 : 0, leftmost ? 1 : 0, lab(key)});
    return static_cast<uint32_t>(ret("NocaseInsertNodeAt"));
}
uint32_t WStringInsertNodeAt(void* tree, uint32_t parent, bool left, bool leftmost, const void* key) {
    ev("WStringInsertNodeAt", {lab(tree), lab(PW(parent)), left ? 1 : 0, leftmost ? 1 : 0, lab(key)});
    return static_cast<uint32_t>(ret("WStringInsertNodeAt"));
}
void OperatorDelete(void* p) { ev("OperatorDelete", {lab(p)}); }
void WStringDtor(void* string, int16_t flag) { ev("WStringDtor", {lab(string), flag}); }
void StringDtor(void* string, int16_t flag) { ev("StringDtor", {lab(string), flag}); }
void ArgumentVectorTearDown(void* vector) { ev("ArgumentVectorTearDown", {lab(vector)}); }
void CdequeDecrement(MslArgumentCursor* c) {
    ev("CdequeDecrement", {lab(PW(c->mCdeque)), static_cast<int64_t>(c->mIndex), lab(PW(c->mSlot))});
    c->mIndex -= 1;
}
void CursorAdd(MslArgumentCursor* c, int32_t count) {
    ev("CursorAdd", {lab(PW(c->mCdeque)), static_cast<int64_t>(c->mIndex), lab(PW(c->mSlot)), count});
    c->mIndex += static_cast<uint32_t>(count);
    c->mSlot += 4u * static_cast<uint32_t>(count);
}
}  // namespace v51126_callee

namespace argdeque_new {
void MakeIterator(MslArgumentDequeIter* out, const MslArgumentDeque* self, uint32_t index) {
    ev("MakeIterator", {lab(out), lab(self), static_cast<int64_t>(index)});
    uint32_t* w = reinterpret_cast<uint32_t*>(out);
    w[0] = index * 3 + 1;
    w[1] = index ^ 0x55;
    w[2] = index + 7;
    w[3] = 0x1000 + index;
    w[4] = 0x2000 + index;
    w[5] = index;
}
}  // namespace argdeque_new

void ArgumentDequeDestroy(MslArgumentDeque* self, MslArgumentDequeIter first, MslArgumentDequeIter last) {
    const uint32_t* f = reinterpret_cast<const uint32_t*>(&first);
    const uint32_t* l = reinterpret_cast<const uint32_t*>(&last);
    ev("DestroyFirst", {lab(self), f[0], f[1], f[2], f[3], f[4], f[5]});
    ev("DestroyLast", {l[0], l[1], l[2], l[3], l[4], l[5]});
}

void InitOnlineUI() { ev("InitOnlineUI", {}); }

void UIStatusPanel::CloseWindow() { ev("CloseWindow", {lab(this)}); }

extern "C" {

uint64_t tpw_v51126_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"NocaseCount", &v51126::gNocaseTreeCount},       {"NocaseIns", &v51126::gNocaseTreeInserted},
        {"NocaseNot", &v51126::gNocaseTreeNotInserted},   {"WCount", &v51126::gWStringTreeCount},
        {"WIns", &v51126::gWStringTreeInserted},          {"WNot", &v51126::gWStringTreeNotInserted},
        {"BuddyCount", &v51126::gBuddyTreeCount},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v51126_arena() { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::gArenaBase)); }

uint64_t tpw_v51126_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_v51126_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(TreeHeader3));
    out[n++] = static_cast<int64_t>(sizeof(TreeHeader4));
    out[n++] = static_cast<int64_t>(offsetof(TreeHeader4, mBegin));
    out[n++] = static_cast<int64_t>(sizeof(TreeInsertResult));
    out[n++] = static_cast<int64_t>(sizeof(ArgVectorPtrCdeque));
    out[n++] = static_cast<int64_t>(sizeof(MslArgumentCursor));
    return n;
}

static void* P(uint64_t a) { return reinterpret_cast<void*>(static_cast<uintptr_t>(a)); }

void tpw_v51126_ctor(int32_t kind, uint64_t self, uint64_t compare) {
    const uint8_t* c = static_cast<const uint8_t*>(P(compare));
    if (kind == 0) v51126::NocaseTreeCtor(static_cast<TreeHeader3*>(P(self)), c);
    else if (kind == 1) v51126::WStringTreeCtor(static_cast<TreeHeader3*>(P(self)), c);
    else v51126::BuddyTreeCtor(static_cast<TreeHeader4*>(P(self)), c);
}

void tpw_v51126_insert_one(int32_t kind, uint64_t result, uint64_t tree, uint64_t key) {
    TreeInsertResult* r = static_cast<TreeInsertResult*>(P(result));
    TreeHeader3* t = static_cast<TreeHeader3*>(P(tree));
    if (kind == 0) v51126::NocaseTreeInsertOne(r, t, P(key));
    else v51126::WStringTreeInsertOne(r, t, P(key));
}

void tpw_v51126_dealloc(int32_t kind, uint64_t allocator, uint64_t node, uint32_t count) {
    if (kind == 0) v51126::BuddyNodeDeallocate(P(allocator), P(node), count);
    else v51126::WStringNodeDeallocate(P(allocator), P(node), count);
}

uint64_t tpw_v51126_nocase_dtor(uint64_t self, int32_t flag) {
    return A(v51126::NocaseWStringDestruct(static_cast<uint32_t*>(P(self)), static_cast<int16_t>(flag)));
}
void tpw_v51126_arg_destroy(uint64_t a) { v51126::ArgumentDestroy(static_cast<Argument*>(P(a))); }
void tpw_v51126_arg_range(uint64_t a, uint64_t b) { v51126::ArgumentDestroyRange2(static_cast<Argument*>(P(a)), static_cast<Argument*>(P(b))); }
void tpw_v51126_vec_destroy(uint64_t a) { v51126::ArgumentVectorDestroy(P(a)); }
void tpw_v51126_vec_range(uint64_t a, uint64_t b) { v51126::ArgumentVectorDestroyRange(P(a), P(b)); }
uint64_t tpw_v51126_cdeque_dtor(uint64_t self, int32_t flag) {
    return A(v51126::ArgVectorPtrCdequeDestruct(static_cast<ArgVectorPtrCdeque*>(P(self)), static_cast<int16_t>(flag)));
}
void tpw_v51126_tear_down(uint64_t self) { v51126::ArgumentDequeTearDown(static_cast<MslArgumentDeque*>(P(self))); }
void tpw_v51126_getloc(uint64_t out, uint64_t sb) { filebuf_wclose_callee::Getloc(P(out), P(sb)); }
void tpw_v51126_make_iterator(uint64_t out, uint64_t self, uint32_t index) {
    errdeque_callee::MakeIteratorMutable(static_cast<MslErrorDequeIterator*>(P(out)), static_cast<MslErrorDeque*>(P(self)), index);
}
void tpw_v51126_init_online(uint64_t self) { static_cast<UIInterface*>(P(self))->InitOnline(); }
void tpw_v51126_remove(uint64_t self) { static_cast<FeatureStatusPanel*>(P(self))->Remove(); }
void tpw_v51126_deinit_window(uint64_t self) { static_cast<FeatureStatusPanel*>(P(self))->DeInitWindow(); }
void tpw_v51126_deinit_status(uint64_t self) { static_cast<FeatureStatusPanel*>(P(self))->DeInitStatusWindow(); }

}  // extern "C"
