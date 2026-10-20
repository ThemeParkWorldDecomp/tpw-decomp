// std::list<std::string> members of the Metrowerks library (main app): insert 0x101b25e0, sort 0x1020811c, erase 0x102084d8 and
// 0x10208838, merge 0x10209898. Mac offsets are in include/ui_ime.h; the pointers here are native.
#include <cstddef>
#include <cstdint>

#include "ui_ime.h"

namespace IME {
namespace {

const char kLogicLengthType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x1024e382 / 0x102500f9
const char kInsertMessage[] = "list::insert length error";                                   // 0x1024e242
const char kMergeMessage[] = "list::merge length error";                                     // 0x1024fed9

MslStrNode* End(MslStrList* list) { return reinterpret_cast<MslStrNode*>(&list->mHead); }

// The compiler's inlined string destructor: the count (0 means not shared yet) goes down; at 0 the data and the representation are
// deleted, else the node forgets the representation. The Mac's test of the string's address against 0 (node +8) is always true.
void ReleaseString(MslStrNode* node) {
    MslStrRep* rep = node->mStr.mRep;
    if (rep->mRefs == 0) {
        uint32_t v = rep->mRefs;
        if (v == 0) v = 1;
        rep->mRefs = v;
    }
    uint32_t count = rep->mRefs - 1;
    rep->mRefs = count;
    if (count != 0) rep = nullptr;
    node->mStr.mRep = rep;
    if (node->mStr.mRep != nullptr) {
        MslDelete(node->mStr.mRep->mData);
        MslDelete(node->mStr.mRep);
    }
}

}  // namespace

// 0x101b25e0: a copy of *str goes before pos. A list at 0x7fffffff nodes throws length_error (the fakes return, so the
// original's code goes on to insert).
MslStrNode* MslStrList_insert(MslStrList* self, MslStrNode* pos, const MslString* str) {
    if (self->mSize > 0x7ffffffeu) {
        MslExceptionObject obj;
        size_t len = MslStrlen(kInsertMessage);
        char* text = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        MslRefCountedCharsCtor(&obj.mRc, text);
        MslStrcpy(obj.mRc.mPtr, kInsertMessage);
        obj.mVtbl = gMslLengthErrorVtbl;
        MslThrow(kLogicLengthType, &obj, gMslLengthErrorDtorTV);
    }
    MslStrNode* node = static_cast<MslStrNode*>(MslNew(sizeof(uint32_t) * 3));   // 12 bytes on the Mac
    MslString_construct(&node->mStr, str, 0, 0xffffffffu);
    pos->mPrev->mNext = node;
    node->mPrev = pos->mPrev;
    pos->mPrev = node;
    node->mNext = pos;
    self->mSize = self->mSize + 1;
    return node;
}

// 0x1020811c: merge sort. Two nodes are swapped by exchanging their links; more are cut in half, both halves sorted, merged.
void MslStrList_sort(MslStrList* self) {
    int32_t size = static_cast<int32_t>(self->mSize);
    if (size == 2) {
        MslStrNode* first = self->mHead.mNext;
        MslStrNode* second = first->mNext;
        if (MslString_less(&second->mStr, &first->mStr)) {
            MslStrNode* t = first->mPrev;
            first->mPrev = first->mNext;
            first->mNext = t;
            t = second->mPrev;
            second->mPrev = second->mNext;
            second->mNext = t;
            t = self->mHead.mPrev;
            self->mHead.mPrev = self->mHead.mNext;
            self->mHead.mNext = t;
        }
    } else if (size > 2 || size < 0) {
        uint32_t half = self->mSize >> 1;
        MslStrNode* mid = self->mHead.mNext;
        for (uint32_t k = half; k != 0; --k) mid = mid->mNext;
        // the second half moves to a list of its own on the stack
        MslStrList other;
        other.mHead.mNext = End(&other);
        other.mHead.mPrev = End(&other);
        MslStrNode* last = self->mHead.mPrev;
        mid->mPrev->mNext = last->mNext;
        last->mNext->mPrev = mid->mPrev;
        reinterpret_cast<MslStrNode*>(other.mHead.mPrev)->mNext = mid;
        mid->mPrev = other.mHead.mPrev;
        other.mHead.mPrev = last;
        last->mNext = End(&other);
        other.mSize = self->mSize - half;
        self->mSize = half;
        MslStrList_sort(self);
        MslStrList_sort(&other);
        MslStrList_merge(self, &other);
        MslStrList_clear(&other);
    }
}

// 0x102084d8: removes [first, last), deleting each node and its string; answers last.
MslStrNode* MslStrList_erase(MslStrList* self, MslStrNode* first, MslStrNode* last) {
    if (first == last) return last;
    MslStrNode* lastPrev = last->mPrev;
    first->mPrev->mNext = lastPrev->mNext;
    lastPrev->mNext->mPrev = first->mPrev;
    MslStrNode* cur = first;
    while (cur != last) {
        ReleaseString(cur);
        MslStrNode* next = cur->mNext;
        MslDelete(cur);
        self->mSize = self->mSize - 1;
        cur = next;
    }
    return last;
}

// 0x10208838: removes one node, answers the next.
MslStrNode* MslStrList_erase1(MslStrList* self, MslStrNode* pos) {
    MslStrNode* next = pos->mNext;
    pos->mPrev->mNext = next;
    pos->mNext->mPrev = pos->mPrev;
    ReleaseString(pos);
    MslDelete(pos);
    self->mSize = self->mSize - 1;
    return next;
}

// 0x10209898: merges the sorted list other into the sorted list self, moving runs of nodes; the rest of other goes to the end.
void MslStrList_merge(MslStrList* self, MslStrList* other) {
    if (self == other) return;
    if (self->mSize > 0x7fffffffu - other->mSize) {
        MslExceptionObject obj;
        size_t len = MslStrlen(kMergeMessage);
        obj.mRc.mPtr = static_cast<char*>(MslNewArray(static_cast<uint32_t>(len + 1)));
        obj.mRc.mCount = nullptr;
        if (obj.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(MslNew(4));
            if (count != nullptr) *count = 1;
            obj.mRc.mCount = count;
        }
        MslStrcpy(obj.mRc.mPtr, kMergeMessage);
        obj.mVtbl = gMslLengthErrorVtbl;
        MslThrow(kLogicLengthType, &obj, gMslLengthErrorDtorTV);
    }
    MslStrNode* end1 = End(self);
    MslStrNode* end2 = End(other);
    MslStrNode* it1 = self->mHead.mNext;
    MslStrNode* it2 = other->mHead.mNext;
    while (it1 != end1 && it2 != end2) {
        if (MslString_less(&it2->mStr, &it1->mStr)) {
            uint32_t count = 1;
            MslStrNode* scan = it2->mNext;
            while (scan != end2 && MslString_less(&scan->mStr, &it1->mStr)) {
                ++count;
                scan = scan->mNext;
            }
            MslStrNode* lastRun = scan->mPrev;
            it2->mPrev->mNext = lastRun->mNext;
            lastRun->mNext->mPrev = it2->mPrev;
            it1->mPrev->mNext = it2;
            it2->mPrev = it1->mPrev;
            it1->mPrev = lastRun;
            lastRun->mNext = it1;
            other->mSize = other->mSize - count;
            self->mSize = self->mSize + count;
            it2 = scan;
        }
        it1 = it1->mNext;
    }
    if (it2 != end2) MslStrList_splice(self, it1, other);
}

}  // namespace IME
