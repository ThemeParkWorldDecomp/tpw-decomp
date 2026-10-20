// The Metrowerks standard library's red-black tree, as the message centre's subscriptions use it (unit w107).
//
// MslRedBlackTree is std::__red_black_tree<__int128>: the node base and the three balancing functions every MSL set and map shares
// (the template argument is a placeholder, so there is one copy of the code for all trees). A node base is left +0, right +4 and the
// parent +8 with the colour in bit 0 (1 = red, 0 = black); the Mac's words are 32 bits, the host's are native.
//
// SubscriptionTree is std::__tree<CSubscription, std::less<CSubscription>, std::allocator<CSubscription>>, a std::set<CSubscription>.
// The Mac's tree object is 0xc bytes: +0 the node count, +4 the root, +8 the first node (begin). Its end() is the address of the root
// word, treated as a node whose left word is the root (so the root's parent is end(), and climbing out of the rightmost node arrives
// there). The host keeps the same two words (count and root) and DERIVES begin as the leftmost node: a native root pointer and begin
// would not fit in the 16 bytes a CSubscriptionHeader must keep (CMessageCentre is 29 of them, 0x1d0 bytes, which the older tests rely
// on), and begin is always the leftmost node, because insert, erase and clear all maintain it.
#pragma once
#include <cstdint>

struct MslRedBlackTree {
    struct node_base {
        node_base* mLeft;     // 0x00
        node_base* mRight;    // 0x04
        uintptr_t mParent;    // 0x08  parent pointer with the colour in bit 0 (1 = red)
    };

    // 0x10152db0: the in-order successor (the right subtree's leftmost node, else the first ancestor we are not the right child of).
    static void increment(node_base*& iterator);
    // 0x10153290: unlinks `node` and repairs the colours; `root` is the tree's root at the time of the call (the callers pass the
    // root word's value). The node is not freed.
    static void balance_erase(node_base* node, node_base* root);
    // 0x10153dec and 0x10153efc: the rotations; `root` follows the root when the rotated node was it.
    static void rotate_left(node_base* node, node_base*& root);
    static void rotate_right(node_base* node, node_base*& root);
    // 0x10153b24 (572): colours `node` red and repairs the colours up to `root` (the root ends black). Unit v5942.
    static void balance_insert(node_base* node, node_base* root);
};

// The set's element: one thing id (a uint16_t; the thing table's index).
struct CSubscription {
    uint16_t mThingId;   // 0x00
    void Load(void*& file);         // 0x10152bb4 (120): see LoadResult; the older test shims define this as void
    int32_t LoadResult(void*& file);   // 0x10152bb4 (120): reads 2 bytes and swaps them; 0, or -1 after a short read (unit v5942)
    void Save(void*& file) const;   // 0x10152b08 (124): see SaveResult (v5L15)
    int32_t SaveResult(void*& file) const;   // 0x10152b08: writes the id swapped; 0, or -1 after a short write (unit v51029)
};

struct SubscriptionInsertResult;   // Unit w123a: insert_one result, 0x101530e4

struct SubscriptionTree {
    // Unit w123a: interface only; insertion remains untranslated (0x101530e4).
    SubscriptionInsertResult insert_one(const CSubscription& key);
    using node_base = MslRedBlackTree::node_base;
    struct node : node_base {
        CSubscription mValue;   // 0x0c on the Mac
    };

    uint32_t mCount;     // 0x00 on the Mac: the number of nodes
    node_base* mRoot;    // 0x04 on the Mac (the host's pointers are 8 bytes, so this word sits at +8): the end node's left word

    // The end node is the root word itself (see the top of the file).
    node_base* end() { return reinterpret_cast<node_base*>(&mRoot); }
    // The first node: the leftmost one, end() when the tree is empty.
    node_base* begin();

    void Construct();                         // 0x1000ff04, std::__tree::__tree(const less&, const allocator&)
    void destroy(node* subtree);              // 0x1000fae0, frees the subtree below (and including) `subtree`, children first
    node_base* find(const CSubscription& key);   // 0x10152c58, the node holding `key` or end()
    // Unit w124: 0x101538d8, untranslated (a stand-in): links a new node holding `key` below `parent` and rebalances; answers the node.
    node_base* insert_node_at(node_base* parent, bool left, bool leftmost, const CSubscription& key);
    void clear();                             // 0x10152e64
    void erase(node_base* position);          // 0x10152f3c
};

// std::allocator<CSubscription>::destroy (0x1000feb0, a plain return) and std::allocator<tree node>::deallocate (0x1000fd84, which is
// operator delete of its pointer; the size argument is unused).
struct SubscriptionAllocator {
    static void destroy(CSubscription* value);                                 // 0x1000feb0
    static void deallocate(SubscriptionTree::node* node, unsigned long count);   // 0x1000fd84
};

// Unit w123a: native pointer and the independently returned inserted byte.
struct SubscriptionInsertResult {
    SubscriptionTree::node_base* mNode;
    uint8_t mInserted;
};
