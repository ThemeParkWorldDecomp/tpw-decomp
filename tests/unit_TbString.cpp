#include "TbDynamicStringTemplate.h"
#include "TbTList.h"
#include <cstdlib>
#include <cstring>
#include <new>

namespace {
int32_t events[1024];
uint32_t eventCount;
bool active;
void event(int32_t kind, int32_t value, int32_t width) {
    if (!active) return;
    events[eventCount++] = kind;
    events[eventCount++] = value;
    events[eventCount++] = width;
}
}
#define TPW_STRING_EVENT(kind, value, width) event(kind, value, width)
#include "string_external_fakes.h"

// Only deallocations are recorded. Allocation sizes differ with host pointer widths.
void operator delete[](void* data) noexcept {
    event(4, data != nullptr, 0);
    std::free(data);
}
void operator delete(void* data) noexcept {
    event(5, data != nullptr, 0);
    std::free(data);
}

extern "C" {
void u11_begin() { eventCount = 0; active = true; }
uint32_t u11_events(int32_t* out) {
    active = false;
    std::memcpy(out, events, eventCount * sizeof(int32_t));
    return eventCount;
}

}

// Seed objects through named fields, never host offsets.
template<class Char> void runString(int32_t op, const Char* first, uint32_t firstLength,
                                  const Char* second, uint32_t secondLength,
                                  uint32_t n, uint32_t count, int32_t isNull, int32_t self,
                                  int32_t* fields, Char* bytes) {
    using String = TbDynamicStringTemplate<Char>;
    active = false;
    String* a = new String;
    delete[] a->mData;
    a->mData = new Char[op == 33 ? 513 : 512];
    if (op == 33) ++a->mData;
    std::fill_n(a->mData, 512, Char(0x7e));
    std::copy_n(first, firstLength + 1, a->mData);
    a->mLength = firstLength;
    a->mCapacity = 400;
    a->mGrowBy = 19;
    String* b = new String;
    delete[] b->mData;
    b->mData = new Char[512];
    std::fill_n(b->mData, 512, Char(0x7e));
    std::copy_n(second, secondLength + 1, b->mData);
    b->mLength = secondLength;
    b->mCapacity = 400;
    b->mGrowBy = 19;
    const Char* text = isNull ? nullptr : b->mData;
    String& other = self ? *a : *b;
    Char ch = second[0];
    if (op == 32) a->mCapacity = count % 32;
    const int32_t oldLength = a->mLength;
    const int32_t oldCapacity = a->mCapacity;
    const int32_t oldGrowBy = a->mGrowBy;
    u11_begin();
    int32_t result = 0;
    int32_t offset = 0;
    if (op <= 3) {
        active = false;
        delete[] a->mData;
        std::memset(static_cast<void*>(a), 0x55, sizeof(*a));
        active = true;
        if (op == 0) new (a) String;
        if (op == 1) new (a) String(text);
        if (op == 2) new (a) String(other);
        if (op == 3) new (a) String(static_cast<const TbStringBase<Char>&>(other));
    } else if (op == 4) {
        if (count == 1) delete a;
        else a->~String();
    } else if (op == 5) {
        result = &(a->operator=(static_cast<const TbStringBase<Char>&>(other))) == a;
    } else if (op == 6) {
        result = &(a->operator=(text)) == a;
    } else if (op == 7) {
        result = &(a->operator=(ch)) == a;
    } else if (op == 8) {
        result = &(a->operator+=(static_cast<const TbStringBase<Char>&>(other))) == a;
    } else if (op == 9) {
        result = &(a->operator+=(ch)) == a;
    } else if (op == 10) {
        result = &(a->operator+=(text)) == a;
    } else if (op == 11) {
        result = &(a->operator=(other)) == a;
    } else if (op == 12) {
        result = a->SBCstrlen(b->mData);
    } else if (op == 13) {
        result = a->SBCstrcpy(a->mData, b->mData) == a->mData;
    } else if (op == 14) {
        result = a->SBCstrncpy(a->mData, b->mData, n) == a->mData;
    } else if (op == 15) {
        result = a->SBCstrcat(a->mData, b->mData) == a->mData;
    } else if (op == 16) {
        result = a->SBCstrncat(a->mData, b->mData, n) == a->mData;
    } else if (op == 17) {
        result = a->Length();
    } else if (op >= 18 && op <= 21) {
        TbStringBase<Char> view;
        if (op == 18) view = a->Mid(n);
        if (op == 19) view = a->Mid(n, count);
        if (op == 20) view = a->Right(n);
        if (op == 21) view = a->Left(n);
        result = view.mLength;
        offset = view.mData - a->mData;
    } else if (op == 22) {
        result = a->operator==(text);
    } else if (op == 23) {
        result = a->operator==(other);
    } else if (op == 24) {
        result = a->operator!=(text);
    } else if (op == 25) {
        result = a->operator<(other);
    } else if (op == 26) {
        result = a->FindReverseOneOf(text, n);
    } else if (op == 27) {
        result = a->FindOneOf(text, n);
    } else if (op == 28) {
        result = a->FindReverse(ch, n);
    } else if (op == 29) {
        result = a->Find(ch, n);
    } else if (op == 30) {
        result = a->Find(text, n);
    } else if (op == 31) {
        if (self) {
            TbStringBase<Char> view(*a);
            result = view.LockBuffer(n) == a->mData;
        } else result = a->LockBuffer(n) == a->mData;
    } else if (op == 32) {
        if (self) {
            TbStringBase<Char> view(*a);
            view.ReleaseBuffer(n);
        } else a->ReleaseBuffer(n);
    } else if (op == 33) {
        if constexpr (std::is_same_v<Char, char>) {
        TbStaticStringTemplate<Char> s(a->mData, n, count);
        a->mLength = s.mLength;
        a->mCapacity = s.mCapacity;
        a->mGrowBy = s.mTruncated;
        }
    } else if (op == 34) {
        if constexpr (std::is_same_v<Char, char>) {
        auto* s = new TbStaticStringTemplate<Char>(a->mData, 400, 0);
        eventCount = 0;
        if (count == 1) delete s;
        else {
            s->~TbStaticStringTemplate();
            active = false;
            ::operator delete(s);
        }
        }
    }
    active = false;
    fields[0] = result;
    fields[1] = op == 4 ? oldLength : a->mLength;
    fields[2] = op == 4 ? oldCapacity : a->mCapacity;
    fields[3] = op == 4 ? oldGrowBy : a->mGrowBy;
    fields[4] = offset;
    fields[5] = b->mLength;
    fields[6] = b->mCapacity;
    fields[7] = b->mGrowBy;
    std::copy_n(b->mData, 512, bytes + 512);
    if (op != 4) std::copy_n(a->mData, 512, bytes);
    if (op == 33) --a->mData;
    if (op != 4) delete a;
    else if (count != 1) ::operator delete(a);
    delete b;
}

extern "C" void u11_string(int32_t wide, int32_t op, const void* first, uint32_t firstLength,
                const void* second, uint32_t secondLength, uint32_t n, uint32_t count,
                int32_t isNull, int32_t self, int32_t* fields, void* bytes) {
    if (wide) runString(op, static_cast<const char16_t*>(first), firstLength,
                        static_cast<const char16_t*>(second), secondLength, n, count,
                        isNull, self, fields, static_cast<char16_t*>(bytes));
    else runString(op, static_cast<const char*>(first), firstLength,
                   static_cast<const char*>(second), secondLength, n, count,
                   isNull, self, fields, static_cast<char*>(bytes));
}

extern "C" void u11_list(int32_t op, int32_t count, const char* text, const char* value,
                         int32_t* fields, char* contents) {
    using String = TbDynamicStringTemplate<char>;
    using List = TbTList<String, String&>;
    active = false;
    List list;
    String first(text);
    for (int32_t i = 0; i < count; ++i) list.AddTail(first);
    String second(value);
    u11_begin();
    String* removed = nullptr;
    List::CNode* node = nullptr;
    if (op == 0) {
        list.~List();
        eventCount = 0;
        new (&list) List;
    } else if (op == 1) {
        node = new List::CNode(second);
        // The host ABI constructs the by-value parameter in the caller. The original
        // entry receives that already-constructed parameter, so exclude its two events.
        active = false;
        std::memmove(events, events + 3, (eventCount - 6) * sizeof(int32_t));
        eventCount -= 6;
        active = true;
    } else if (op == 2) {
        list.AddTail(second);
    } else {
        // Return-value construction is elided on the host, as expected by the Mac ABI.
        removed = new String(list.RemoveHead());
    }
    active = false;
    fields[0] = list.mCount;
    fields[1] = list.mHead == nullptr;
    fields[2] = list.mTail == nullptr;
    fields[3] = list.mHead == nullptr || list.mHead->mPrev == nullptr;
    fields[4] = list.mTail == nullptr || list.mTail->mNext == nullptr;
    fields[5] = 0;
    fields[6] = 0;
    fields[7] = removed ? removed->mLength : 0;
    fields[8] = node ? node->mData.mLength : 0;
    fields[9] = node ? node->mNext == nullptr && node->mPrev == nullptr : 1;
    std::memset(contents, 0, 1024);
    if (removed) std::memcpy(contents, removed->mData, removed->mLength);
    if (node) std::memcpy(contents + 128, node->mData.mData, node->mData.mLength);
    for (auto* p = list.mHead; p != nullptr; p = p->mNext) {
        std::memcpy(contents + 256 + 64 * fields[5]++, p->mData.mData, p->mData.mLength);
        if (p->mNext && p->mNext->mPrev != p) fields[3] = 0;
    }
    for (auto* p = list.mTail; p != nullptr; p = p->mPrev) {
        std::memcpy(contents + 640 + 64 * fields[6]++, p->mData.mData, p->mData.mLength);
        if (p->mPrev && p->mPrev->mNext != p) fields[4] = 0;
    }
    delete removed;
    delete node;
}
