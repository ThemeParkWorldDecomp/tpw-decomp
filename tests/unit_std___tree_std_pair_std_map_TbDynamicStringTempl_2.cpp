// Test library for ParkThemeTree::destroy and insert_node_at (unit v5544; libtpw_unit_std___tree_std_pair_std_map_TbDynamicStringTempl_2.dylib), main app:
//   src/game/MslSupport.cpp
//
// The callees outside the unit are recording fakes (the same event names and arguments as the module's PowerPC stubs); the library globals
// the exception object points to are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_MasterSave.h"
#include "ui_ime.h"

using namespace calllog;

namespace IME {
char gMslExcVtblB[16];
char gMslExcVtblC[16];
char gMslLengthErrorVtbl[16];
char gMslLengthErrorDtorTV[8];
}  // namespace IME

namespace {

using Node = ParkThemeTree::node;

char* gNewNode = nullptr;   // the node the last operator new made

ptrdiff_t ValueOffset() {
    alignas(16) static char buf[256];
    Node* n = reinterpret_cast<Node*>(buf);
    return reinterpret_cast<char*>(&n->mValue) - buf;
}

ptrdiff_t ParentOffset() {
    alignas(16) static char buf[256];
    Node* n = reinterpret_cast<Node*>(buf);
    return reinterpret_cast<char*>(&n->mParent) - buf;
}

int64_t I(int64_t v) { return v; }

int64_t Hash(const char* text) {
    uint32_t h = 0;
    for (; *text != 0; ++text) h = h * 31u + static_cast<uint8_t>(*text);
    return h;
}

const void* NodeOf(const void* pair) { return static_cast<const char*>(pair) - ValueOffset(); }

}  // namespace

// The key's destructor (0x102382f0): the key is the first member of the node's pair.
template <>
TbDynamicStringTemplate<char>::~TbDynamicStringTemplate() {
    ev("KeyDtor", {I(lab(NodeOf(this)))});
}

// The destructor stores the vtable, so the virtuals it names must exist; nothing here calls them.
template <>
void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t) {}
template <>
void TbDynamicStringTemplate<char>::Format(const char*, ...) {}
template <>
void TbDynamicStringTemplate<char>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const TbDynamicStringTemplate<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char*) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const TbStringBase<char>&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char&) { return *this; }
template <>
TbDynamicStringTemplate<char>& TbDynamicStringTemplate<char>::operator+=(const char*) { return *this; }

namespace ParkThemeTreeCallee {
void PairDestroy(ParkThemeTree* tree, ParkThemeTree::Pair* value) { ev("PairDestroy", {I(lab(tree)), I(lab(NodeOf(value)))}); }
void NodeDeallocate(ParkThemeTree* tree, ParkThemeTree::node* node, unsigned long count) {
    ev("NodeDeallocate", {I(lab(tree)), I(lab(node)), I(count)});
}
void PairDtor(ParkThemeTree::Pair* pair, int16_t flag) { ev("PairDtor", {I(lab(NodeOf(pair))), I(flag)}); }
void PairCopy(ParkThemeTree::Pair* pair, const ParkThemeTree::Pair& value) {
    ev("PairCopy", {I(reinterpret_cast<char*>(pair) - gNewNode - ValueOffset()), I(lab(&value))});
}
void BalanceInsert(ParkThemeTree::node* node, ParkThemeTree::node* root) {
    ev("BalanceInsert", {I(reinterpret_cast<char*>(node) == gNewNode ? 1 : 0), I(lab(root))});
}
}  // namespace ParkThemeTreeCallee

namespace IME {
void* MslNew(uint32_t size) {
    ev("New", {I(size == sizeof(Node) ? 0x24 : size)});   // the Mac's node is 0x24 bytes
    char* p = static_cast<char*>(tpw::ArenaAllocZeroed(size < 256 ? 256 : size));
    *reinterpret_cast<uintptr_t*>(p + ParentOffset()) = static_cast<uintptr_t>(ret("new")) & 0xffffffffu;   // the block's old contents
    gNewNode = p;
    return p;
}
void* MslNewArray(uint32_t size) {
    ev("NewArray", {I(size)});
    return tpw::ArenaAllocZeroed(size);
}
void MslDelete(void* p) { ev("Delete", {I(lab(p))}); }
size_t MslStrlen(const char* s) {
    ev("Strlen", {Hash(s)});
    return std::strlen(s);
}
char* MslStrcpy(char* d, const char* s) {
    ev("Strcpy", {Hash(s)});
    return std::strcpy(d, s);
}
void MslRefCountedCharsCtor(MslRefCountedChars* self, char* p) {
    ev("RcCtor", {I(p != nullptr ? 1 : 0)});
    self->mPtr = p;
}
void MslThrow(const char* typeName, void* object, const void* dtorTV) {
    const MslExceptionObject* o = static_cast<const MslExceptionObject*>(object);
    // The globals live outside the arena (their addresses do not fit the 32-bit label table): labelled here, as the test does (900 ...).
    auto global = [](const void* p) -> int64_t {
        if (p == gMslExcVtblB) return 900;
        if (p == gMslExcVtblC) return 901;
        if (p == gMslLengthErrorVtbl) return 902;
        if (p == gMslLengthErrorDtorTV) return 903;
        return -1;
    };
    ev("Throw", {Hash(typeName), global(o->mVtbl), global(dtorTV), Hash(o->mRc.mPtr)});
}
}  // namespace IME

extern "C" {

uint64_t tpw_pt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"vtblB", IME::gMslExcVtblB},
        {"vtblC", IME::gMslExcVtblC},
        {"vtblLen", IME::gMslLengthErrorVtbl},
        {"dtorTV", IME::gMslLengthErrorDtorTV},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_pt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_pt_layout(int64_t* out) {
    int32_t n = 0;
    alignas(16) static char buf[256];
    Node* node = reinterpret_cast<Node*>(buf);
    out[n++] = reinterpret_cast<char*>(&node->mRight) - buf;
    out[n++] = reinterpret_cast<char*>(&node->mParent) - buf;
    out[n++] = reinterpret_cast<char*>(&node->mValue) - buf;
    ParkThemeTree* tree = reinterpret_cast<ParkThemeTree*>(buf);
    out[n++] = reinterpret_cast<char*>(&tree->mRoot) - buf;
    out[n++] = reinterpret_cast<char*>(&tree->mSlot8) - buf;
    out[n++] = reinterpret_cast<char*>(&tree->mBegin) - buf;
    return n;
}

void tpw_pt_destroy(uint64_t tree, uint64_t node) {
    reinterpret_cast<ParkThemeTree*>(tree)->destroy(reinterpret_cast<Node*>(node));
}

uint64_t tpw_pt_insert(uint64_t tree, uint64_t parent, uint32_t left, uint32_t leftmost, uint64_t value) {
    Node* made = reinterpret_cast<ParkThemeTree*>(tree)->insert_node_at(
        reinterpret_cast<Node*>(parent), left != 0, leftmost != 0, *reinterpret_cast<const ParkThemeTree::Pair*>(value));
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(made));
}

}  // extern "C"
