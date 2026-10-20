// Unit v5701: std::__tree<std::wstring, std::less<std::wstring>, std::allocator<std::wstring>> (a std::set<std::wstring>), erase_one.
// Same convention as SubscriptionTree: the Mac object is count +0, root +4 (the end node's left word), the less functor +8; the host
// keeps native pointers. A node is node_base (left, right, parent) followed by the string at +0xc on the Mac.
#pragma once
#include <cstdint>

#include "MslSubscriptionTree.h"

struct MslWideString;   // std::basic_string<wchar_t> (opaque here)

// Unit v5993: insert_node_at 0x10227c78 and destroy 0x10223204 (src/std___tree_std_basic_string_std_less_std_basic_s_3.cpp).
namespace IME { struct MslWideStringRep; struct MslWideString; }
// A node of the set<wstring> tree: node_base, then the string (one pointer to its representation) at 0x0c; the Mac node is 0x10 bytes.
struct WStringTreeNode : MslRedBlackTree::node_base {
    IME::MslWideStringRep* mString;   // 0x0c
};
namespace wstring_tree_callee {
// 0x10223670 std::allocator<node>::deallocate(node*, unsigned long) (this = tree + 4)
void NodeDeallocate(void* allocator, WStringTreeNode* node, unsigned long count);
}

struct WStringTree {
    using node_base = MslRedBlackTree::node_base;

    uint32_t mCount;      // 0x00
    node_base* mRoot;     // 0x04 on the Mac: the end node's left word
    uint8_t mLess[8];     // 0x08 on the Mac: the empty std::less functor (its address is passed to operator())

    node_base* end() { return reinterpret_cast<node_base*>(&mRoot); }
    // 0x10228104 (stand-in): unlinks and frees the node at `position`.
    void erase(node_base* position);
    // 0x1021e664: erases the element equal to `key`; answers whether one was erased.
    bool erase_one(const MslWideString& key);
    // 0x10227c78: links a new node (a copy of `key`) under `parent` (left or right), counts it, balances, sets begin when `leftmost`.
    // The Mac begin word is at +0x08 (the mLess bytes here: the host reads and writes it with memcpy). A count of 0xffffffff throws.
    WStringTreeNode* insert_node_at(node_base* parent, bool left, bool leftmost, const IME::MslWideString* key);
    // 0x10223204: frees the node and everything below it.
    void destroy(node_base* node);
};

// 0x1021eb7c: std::less<std::wstring>::operator()(a, b) const
bool WideStringLess(void* functor, const MslWideString* a, const MslWideString* b);

// Unit v51044 (src/std_less_std_basic_string_operator.cpp)
namespace wstring_less_callee {
bool Lt(const MslWideString* a, const MslWideString* b);   // 0x10225e8c std::operator<(wstring, wstring); the test library fakes it
}
namespace wstring_alloc {
// 0x102238d4 std::allocator<std::wstring>::destroy(string*): the string is one word, a pointer to its rep (refs at 0x08, data at 0x0c on the Mac).
void Destroy(void* allocator, void* string);
}

// Unit v51058: the string accessors std::operator<(wstring, wstring) uses (the test library fakes them).
namespace wstring_less_callee {
uint32_t Size(const MslWideString* s);               // 0x1022605c size() const
const char16_t* Begin(const MslWideString* s);       // 0x10225ff0 begin() const
}
