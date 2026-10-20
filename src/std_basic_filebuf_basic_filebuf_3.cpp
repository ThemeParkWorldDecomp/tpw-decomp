// Unit v5944: std::basic_filebuf<wchar_t>::basic_filebuf(FILE*) 0x10226ef4, std::basic_filebuf<wchar_t>::open 0x10221c30 and
// std::__tree<std::pair<const nocase_string, BuddyInfo>, ...>::insert_node_at 0x102273bc.
// The object words are the Mac's 32-bit fields; the arena is 32-bit addressable so pointers are stored as words.
#include <cstdint>

#include "std_basic_filebuf.h"
#include "std_basic_ifstream.h"

#include "ui_ime.h"
using IME::gMslExcVtblB;
using IME::gMslLengthErrorDtorTV;
using IME::gMslLengthErrorVtbl;

namespace {

// 0x10226ef4
inline uint32_t Word(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }
inline void* Ptr(uint32_t w) { return reinterpret_cast<void*>(static_cast<uintptr_t>(w)); }

// The mode strings in the order of the original's targets (TOC 0x10250e7f + 0x63c ...).
const char* const kModes[10] = {"w", "a", "r", "r+", "w+", "wb", "ab", "rb", "r+b", "w+b"};
// The jump table at 0x102b1fb4, indexed by (mode & ~2) - 8; -1 = the default (return NULL).
const int8_t kModeIndex[0x35] = {
    2, -1, -1, -1, 7, -1, -1, -1, 0, 1, -1, -1, 5, 6, -1, -1, 3, -1, -1, -1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, -1, -1, 5, -1, -1, -1, 4, -1, -1, -1, 9,
};

// The exception object the original builds on its stack: vtable, then a reference counted string (data, count).
struct LenExc {
    const void* vtbl;
    char* data;
    int32_t* refs;
};

}  // namespace

// 0x10226ef4 (272): basic_filebuf(FILE*)
BasicFilebufWchar* FilebufWcharConstruct(BasicFilebufWchar* self, uint32_t file) {
    self->mVtable = Word(gIfsWVtblC);
    self->mEback = 0;
    self->mGptr = 0;
    self->mEgptr = 0;
    for (int i = 0; i < 12; ++i) self->pad10[i] = 0;   // 0x10 .. 0x1b
    filebuf_char_callee::LocaleCtor(self->pad10 + 12);   // 0x1c
    self->mVtable = Word(gIfsWVtblD);
    self->mFile = file;
    for (int i = 0; i < 4; ++i) self->mState[i] = 0;
    self->mFlag4c = 0;
    self->mEback = Word(self->mBuf);
    self->mGptr = Word(self->mBuf);
    self->mEgptr = Word(self->mBuf);
    uint32_t loc[2];
    filebuf_wclose_callee::Getloc(loc, self);
    void* cvt = filebuf_wclose_callee::UseFacetCodecvtW(loc);   // the second argument is an uninitialised slot in the original
    self->mCodecvt = Word(cvt);
    filebuf_char_callee::LocaleDtor(loc, -1);
    self->mUnbuffered = static_cast<uint8_t>(filebuf_char_callee::CodecvtAlwaysNoconv(Ptr(self->mCodecvt)));
    return self;
}

// 0x10221c30 (396): open(name, mode); mode bit 2 = ate
BasicFilebufWchar* FilebufWcharOpen(BasicFilebufWchar* self, const char* name, uint32_t mode) {
    if (self->mFile != 0) return nullptr;
    uint32_t idx = (mode & ~2u) - 8;
    if (idx > 0x34) return nullptr;
    int k = kModeIndex[idx];
    if (k < 0) return nullptr;
    self->mFile = filebuf_char_callee::Fopen(name, kModes[k]);
    if (self->mFile == 0) return nullptr;
    if (mode & 2) {
        if (filebuf_char_callee::Fseek(self->mFile, 0, 2) != 0) {
            filebuf_char_callee::DeleteArray(Ptr(self->mEback));
            if (self->mFile != 0) {
                bool go = true;
                if (self->mFlag4c != 0) {
                    if (filebuf_wseek_callee::Unshift(self))
                        self->mFlag4c = 0;
                    else
                        go = false;
                }
                if (go) {
                    self->mEback = Word(self->mBuf);
                    self->mGptr = Word(self->mBuf);
                    self->mEgptr = Word(self->mBuf);
                    if (filebuf_char_callee::Fclose(self->mFile) == 0) self->mFile = 0;
                }
            }
            return nullptr;
        }
    }
    return self;
}

// 0x102273bc (536): insert_node_at(parent, left, setLeftmost, value)
BuddyTreeNode* BuddyTreeInsertNodeAt(BuddyTree* self, BuddyTreeNode* parent, bool left, bool setLeftmost, const void* pair) {
    if (self->mCount > 0xfffffffeu) {
        const char* msg = "tree::insert length error";
        LenExc exc;
        exc.vtbl = gMslExcVtblB;
        exc.data = static_cast<char*>(buddy_tree_callee::NewArray(buddy_tree_callee::Strlen(msg) + 1));
        exc.refs = nullptr;
        if (exc.data != nullptr) {
            int32_t* refs = static_cast<int32_t*>(buddy_tree_callee::New(4));
            if (refs != nullptr) *refs = 1;
            exc.refs = refs;
        }
        buddy_tree_callee::Strcpy(exc.data, msg);
        exc.vtbl = gMslLengthErrorVtbl;
        buddy_tree_callee::Throw("!std::exception!!std::logic_error!!std::length_error!!", &exc, gMslLengthErrorDtorTV);
        // the real __throw does not return; the fake does and the original falls through
    }
    BuddyTreeNode* node = static_cast<BuddyTreeNode*>(buddy_tree_callee::New(0x14));
    buddy_tree_callee::PairCtor(node->mPair, pair);
    node->mRight = nullptr;
    node->mLeft = nullptr;
    node->mParentColor = reinterpret_cast<uintptr_t>(parent) | (node->mParentColor & 1u);
    if (left)
        parent->mLeft = node;
    else
        parent->mRight = node;
    self->mCount = self->mCount + 1;
    buddy_tree_callee::BalanceInsert(node, self->mRoot);
    if (setLeftmost) self->mLeftmost = node;
    return node;
}
