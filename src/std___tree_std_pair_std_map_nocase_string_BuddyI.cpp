// Unit v51126: the tree constructors and insert_one of the nocase set, the wstring set and the buddy map, the allocator deallocates,
// the Argument destroyers, the nocase wide string destructor, the tear_down of the deque of Argument vectors, two make_iterator /
// getloc pieces, and four forwarders of the UI. The structures keep the Mac's 32-bit words (the arena is 32-bit addressable).
#include <cstdint>
#include <cstring>

#include "platform/arena.h"
#include "UIInterface.h"
#include "email.h"
#include "online.h"
#include "std_basic_filebuf.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

namespace v51126 {

uint32_t gNocaseTreeCount = 0;          // 0x102b1f9c
uint8_t gNocaseTreeInserted = 1;        // 0x102b1fa0
uint8_t gNocaseTreeNotInserted = 0;     // 0x102b1fa1
uint32_t gWStringTreeCount = 0;         // 0x102b1fa4
uint8_t gWStringTreeInserted = 1;       // 0x102b1fa8
uint8_t gWStringTreeNotInserted = 0;    // 0x102b1fa9
uint32_t gBuddyTreeCount = 0;           // 0x102b1fac

namespace {

// Mac words are arena offsets on the host.
void* P(uint32_t address) { return address != 0 ? static_cast<void*>(tpw::gArenaBase + address) : nullptr; }
uint32_t* W(uint32_t address) { return static_cast<uint32_t*>(P(address)); }
uint32_t A(const void* p) { return p != nullptr ? tpw::ArenaOffset(p) : 0; }

using LessFn = bool (*)(const void*, const void*);
using InsertAtFn = uint32_t (*)(void*, uint32_t, bool, bool, const void*);

// insert_one of both set trees: walk down comparing, then insert unless the last node not above the key equals it.
void InsertOne(TreeInsertResult* result, TreeHeader3* tree, const void* key, LessFn less, InsertAtFn insertAt, const uint8_t* inserted,
               const uint8_t* notInserted) {
    uint32_t parent = 0;
    uint32_t current = tree->mRoot;
    uint32_t where = A(&tree->mRoot);
    bool left = true;
    bool leftmost = true;
    while (current != 0) {
        where = current;
        if (less(key, P(current + 0xc))) {
            current = W(current)[0];
            left = true;
        } else {
            parent = current;
            current = W(current)[1];
            left = false;
            leftmost = false;
        }
    }
    if (parent == 0 || less(P(parent + 0xc), key)) {
        uint32_t node = insertAt(tree, where, left, leftmost, key);
        result->mNode = node;
        result->mInserted = *inserted;
    } else {
        result->mNode = parent;
        result->mInserted = *notInserted;
    }
}

}  // namespace

void NocaseTreeCtor(TreeHeader3* self, const uint8_t* compare) {
    self->mCount = gNocaseTreeCount;
    self->mRoot = 0;
    (void)*compare;   // the byte is stored at 0x08 and the word store below overwrites it
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void WStringTreeCtor(TreeHeader3* self, const uint8_t* compare) {
    self->mCount = gWStringTreeCount;
    self->mRoot = 0;
    (void)*compare;
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void BuddyTreeCtor(TreeHeader4* self, const uint8_t* compare) {
    self->mCount = gBuddyTreeCount;
    self->mRoot = 0;
    self->mByte8[0] = *compare;
    self->mBegin = 0;
    self->mBegin = A(&self->mRoot);
}

void NocaseTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key) {
    InsertOne(result, self, key, v51126_callee::NocaseLess, v51126_callee::NocaseInsertNodeAt, &gNocaseTreeInserted, &gNocaseTreeNotInserted);
}

void WStringTreeInsertOne(TreeInsertResult* result, TreeHeader3* self, const void* key) {
    InsertOne(result, self, key, v51126_callee::WStringLess, v51126_callee::WStringInsertNodeAt, &gWStringTreeInserted,
              &gWStringTreeNotInserted);
}

void BuddyNodeDeallocate(void* allocator, void* node, uint32_t count) {
    (void)allocator;
    (void)count;
    v51126_callee::OperatorDelete(node);
}

void WStringNodeDeallocate(void* allocator, void* node, uint32_t count) {
    (void)allocator;
    (void)count;
    v51126_callee::OperatorDelete(node);
}

void* NocaseWStringDestruct(uint32_t* self, int16_t deleteFlag) {
    if (self != nullptr) {
        uint32_t rep = *self;
        uint32_t* r = W(rep);
        if (r[2] == 0) r[2] = 1;
        uint32_t refs = r[2] - 1;
        r[2] = refs;
        if (refs != 0) rep = 0;
        *self = rep;
        uint32_t now = *self;
        if (now != 0) {
            v51126_callee::OperatorDelete(P(W(now)[3]));
            v51126_callee::OperatorDelete(P(*self));
        }
        if (deleteFlag > 0) v51126_callee::OperatorDelete(self);
    }
    return self;
}

void ArgumentDestroy(Argument* argument) {
    if (argument != nullptr) {
        v51126_callee::WStringDtor(&argument->mWString, -1);   // 0x101468f8
        v51126_callee::StringDtor(&argument->mString, -1);     // 0x1000221c
    }
}

void ArgumentDestroyRange2(Argument* first, Argument* last) {
    for (; first < last; first++) ArgumentDestroy(first);
}

void ArgumentVectorDestroy(void* vector) {
    if (vector != nullptr) v51126_callee::ArgumentVectorTearDown(vector);
}

void ArgumentVectorDestroyRange(void* first, void* last) {
    for (uint8_t* p = static_cast<uint8_t*>(first); p < static_cast<uint8_t*>(last); p += 0xc) ArgumentVectorDestroy(p);
}

void* ArgVectorPtrCdequeDestruct(ArgVectorPtrCdeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mWordC != 0) v51126_callee::OperatorDelete(P(self->mWordC));
        if (deleteFlag > 0) v51126_callee::OperatorDelete(self);
    }
    return self;
}

void ArgumentDequeTearDown(MslArgumentDeque* self) {
    if (self->mCount == 0) return;
    MslArgumentDequeIter end;
    argdeque_new::MakeIterator(&end, self, self->mFirst + self->mSize);
    MslArgumentDequeIter begin;
    argdeque_new::MakeIterator(&begin, self, self->mFirst);
    MslArgumentDequeIter last = end;
    ArgumentDequeDestroy(self, begin, last);
    uint32_t count = self->mCount;
    uint32_t sum = self->mStart + count;
    MslArgumentCursor cursor;
    cursor.mCdeque = A(&self->mCapacity);
    cursor.mIndex = count;
    cursor.mSlot = sum < self->mCapacity ? self->mTable + sum * 4 : self->mTable + (sum - self->mCapacity) * 4;
    v51126_callee::CdequeDecrement(&cursor);
    uint32_t remaining = cursor.mIndex;
    uint32_t slot = self->mTable + self->mStart * 4;
    for (uint32_t i = 0; i != remaining;) {
        v51126_callee::OperatorDelete(P(*W(slot)));
        slot += 4;
        i++;
        if (slot == self->mTable + self->mCapacity * 4) slot = self->mTable;
    }
}

}  // namespace v51126

// 0x102229f8 / 0x10223670 are v51126::BuddyNodeDeallocate / WStringNodeDeallocate (distinct names, one body).

// std::basic_streambuf<wchar_t>::getloc(out) const: the two locale words are copied and the reference count is bumped.
namespace filebuf_wclose_callee {
void Getloc(void* out, void* streambuf) {
    uint32_t* o = static_cast<uint32_t*>(out);
    const uint8_t* sb = static_cast<const uint8_t*>(streambuf);
    uint32_t impl, refs;
    std::memcpy(&impl, sb + 0x1c, 4);
    std::memcpy(&refs, sb + 0x20, 4);
    o[0] = impl;
    o[1] = refs;
    uint32_t r = o[1];
    if (r != 0) {
        uint32_t* counter = reinterpret_cast<uint32_t*>(tpw::gArenaBase + r);
        *counter = *counter + 1;
    }
}
}  // namespace filebuf_wclose_callee

// std::deque<errorthingy>::make_iterator(ulong), the second instance.
namespace errdeque_callee {
void MakeIteratorMutable(MslErrorDequeIterator* out, MslErrorDeque* self, uint32_t index) {
    MslErrorDequeCursor cursor;
    auto slotAt = [](uint32_t slot) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + slot); };
    cursor.mCdeque = tpw::ArenaOffset(&self->mBlocks);
    cursor.mWord4 = 0;
    cursor.mSlot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    uint32_t block;
    uint32_t element;
    uint32_t blockSize = self->mBlockSize;
    if (index < blockSize) {
        block = self->mBlocks.mWord4 != 0 ? slotAt(cursor.mSlot) : 0;
        element = block + index * 8;
    } else {
        uint32_t quotient = blockSize != 0 ? index / blockSize : 0;
        v51126_callee::CursorAdd(reinterpret_cast<MslArgumentCursor*>(&cursor), static_cast<int32_t>(quotient));
        block = slotAt(cursor.mSlot);
        element = block + (index - quotient * blockSize) * 8;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = block + blockSize * 8;
    out->mIndex = index;
}
}  // namespace errdeque_callee

void UIInterface::InitOnline() { InitOnlineUI(); }

void FeatureStatusPanel::Remove() { reinterpret_cast<UIStatusPanel*>(this)->CloseWindow(); }

void FeatureStatusPanel::DeInitWindow() {}

void FeatureStatusPanel::DeInitStatusWindow() {}
