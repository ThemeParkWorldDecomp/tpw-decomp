// std::list<unsigned int, std::allocator<unsigned int>> of the Metrowerks library: insert 0x102092f8, merge 0x102095a0, remove 0x10208f44,
// splice 0x1020a9a0 (unit v5931). Iterators are node addresses (32 bit words in the arena); the sentinel is the list + 4.
#include <cstddef>
#include <cstdint>

#include "platform/arena.h"
#include "std_list_uint.h"
#include "ui_ime.h"

namespace std_list_uint {

namespace {

const char kLengthErrorType[] = "!std::exception!!std::logic_error!!std::length_error!!";   // 0x102500f9

// The words of a list are Ptr32 offsets into the arena (include/platform/arena.h); offset 0 is NULL.
Node* N(uint32_t a) { return reinterpret_cast<Node*>(tpw::ArenaBase() + a); }
uint32_t A(const void* p) { return p == nullptr ? 0 : tpw::ArenaOffset(p); }

// The std::length_error the library builds on the stack and throws (the fake of __throw returns, so the caller goes on).
void ThrowLengthError(const char* message) {
    IME::MslExceptionObject object;
    object.mVtbl = IME::gMslExcVtblB;
    object.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(message)) + 1));
    object.mRc.mCount = nullptr;
    if (object.mRc.mPtr != nullptr) {
        int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
        if (count != nullptr) *count = 1;
        object.mRc.mCount = count;
    }
    IME::MslStrcpy(object.mRc.mPtr, message);
    object.mVtbl = IME::gMslLengthErrorVtbl;
    IME::MslThrow(kLengthErrorType, &object, IME::gMslLengthErrorDtorTV);
}

}  // namespace

// 0x102092f8 (428): r3 = where the iterator is stored, r4 = the list, r5 = the position, r6 = the value.
void Insert(uint32_t* result, List* list, uint32_t pos, const uint32_t* value) {
    if (list->mSize > 0x7ffffffeu) ThrowLengthError("list::insert length error");
    Node* node = static_cast<Node*>(IME::MslNew(12));
    if (node != nullptr) node->mData = *value;
    N(N(pos)->mPrev)->mNext = A(node);
    node->mPrev = N(pos)->mPrev;
    N(pos)->mPrev = A(node);
    node->mNext = pos;
    list->mSize = list->mSize + 1;
    *result = A(node);
}

// 0x1020a9a0 (432)
void Splice(List* list, uint32_t pos, List* other) {
    if (other->mSize == 0) return;
    if (list->mSize > 0x7fffffffu - other->mSize) ThrowLengthError("list::splice length error");
    uint32_t sentinel = A(other) + 4;
    uint32_t first = other->mNext;
    uint32_t last = other->mPrev;
    other->mNext = sentinel;
    other->mPrev = sentinel;
    list->mSize = list->mSize + other->mSize;
    other->mSize = 0;
    N(N(pos)->mPrev)->mNext = first;
    N(first)->mPrev = N(pos)->mPrev;
    N(pos)->mPrev = last;
    N(last)->mNext = pos;
}

// 0x102095a0 (652)
void Merge(List* list, List* other) {
    if (list == other) return;
    if (list->mSize > 0x7fffffffu - other->mSize) ThrowLengthError("list::merge length error");
    uint32_t endA = A(list) + 4;
    uint32_t endB = A(other) + 4;
    uint32_t a = list->mNext;
    uint32_t b = other->mNext;
    while (a != endA && b != endB) {
        uint32_t value = N(a)->mData;
        if (N(b)->mData < value) {
            uint32_t count = 1;
            uint32_t p = N(b)->mNext;
            while (p != endB && N(p)->mData < value) {
                count++;
                p = N(p)->mNext;
            }
            uint32_t lastNode = N(p)->mPrev;
            N(N(b)->mPrev)->mNext = N(lastNode)->mNext;
            N(N(lastNode)->mNext)->mPrev = N(b)->mPrev;
            N(N(a)->mPrev)->mNext = b;
            N(b)->mPrev = N(a)->mPrev;
            N(a)->mPrev = lastNode;
            N(lastNode)->mNext = a;
            other->mSize = other->mSize - count;
            list->mSize = list->mSize + count;
            b = p;
        }
        a = N(a)->mNext;
    }
    if (b != endB) Splice(list, a, other);
}

// 0x10208f44 (336)
void Remove(List* list, const uint32_t* value) {
    uint32_t sentinel = A(list) + 4;
    uint32_t p = list->mNext;
    while (p != sentinel) {
        if (*value == N(p)->mData) {
            uint32_t q = N(p)->mNext;
            while (q != sentinel && *value == N(q)->mData) q = N(q)->mNext;
            if (p != q) {
                uint32_t before = N(q)->mPrev;
                N(N(p)->mPrev)->mNext = N(before)->mNext;
                N(N(before)->mNext)->mPrev = N(p)->mPrev;
                uint32_t d = p;
                while (d != q) {
                    uint32_t next = N(d)->mNext;
                    IME::MslDelete(N(d));
                    list->mSize = list->mSize - 1;
                    d = next;
                }
            }
            p = q;
            if (p == sentinel) return;
        }
        p = N(p)->mNext;
    }
}

}  // namespace std_list_uint
