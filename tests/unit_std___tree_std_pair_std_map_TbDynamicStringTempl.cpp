// Test library for the theme map's tree (unit v5126; libtpw_unit_std___tree_std_pair_std_map_TbDynamicStringTempl.dylib), main app:
//   src/FS_MasterSave_global_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp  find, clear, insert_one
//
// The callees outside the unit are fakes recording their arguments: TbStringBase<char>::operator<, destroy and insert_node_at.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"

using namespace calllog;

namespace {
uint64_t gNodeTable[16];   // the nodes insert_node_at may answer, by index
}

// ---- the fakes
template <>
bool TbStringBase<char>::operator<(const TbStringBase<char>& other) const {
    ev("lt", {lab(this), lab(&other)});
    return ret("lt") != 0;
}

void ParkThemeTree::destroy(node* subtree) { ev("destroy", {lab(this), lab(subtree)}); }

ParkThemeTree::node* ParkThemeTree::insert_node_at(node* parent, bool left, bool leftmost, const Pair& value) {
    ev("insert_node_at", {lab(this), lab(parent), left ? 1 : 0, leftmost ? 1 : 0, lab(&value)});
    int64_t k = ret("ins");
    return reinterpret_cast<node*>(static_cast<uintptr_t>(gNodeTable[k & 15]));
}

extern "C" {

uint64_t tpw_ptree_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"inserted", &gParkTreeInserted},
        {"notinserted", &gParkTreeNotInserted},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ptree_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ptree_table(uint32_t index, uint64_t address) { gNodeTable[index & 15] = address; }

int32_t tpw_ptree_layout(int64_t* out) {
    int32_t n = 0;
    alignas(ParkThemeTree::node) char buf[sizeof(ParkThemeTree::node)];
    ParkThemeTree::node* nd = reinterpret_cast<ParkThemeTree::node*>(buf);
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mRoot));
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree, mBegin));
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&nd->mRight) - buf);
    out[n++] = static_cast<int64_t>(reinterpret_cast<char*>(&nd->mValue) - buf);
    out[n++] = static_cast<int64_t>(offsetof(ParkThemeTree::InsertResult, mInserted));
    return n;
}

void tpw_ptree_find(uint64_t result, uint64_t tree, uint64_t key) {
    *reinterpret_cast<ParkThemeTree::Iterator*>(result) =
        reinterpret_cast<ParkThemeTree*>(tree)->find(*reinterpret_cast<const ParkThemeTree::Key*>(key));
}

void tpw_ptree_clear(uint64_t tree) { reinterpret_cast<ParkThemeTree*>(tree)->clear(); }

void tpw_ptree_insert(uint64_t result, uint64_t tree, uint64_t value) {
    *reinterpret_cast<ParkThemeTree::InsertResult*>(result) =
        reinterpret_cast<ParkThemeTree*>(tree)->insert_one(*reinterpret_cast<const ParkThemeTree::Pair*>(value));
}

}  // extern "C"
