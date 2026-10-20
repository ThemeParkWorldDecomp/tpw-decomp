// Metrowerks std::vector<const void*> insert / erase / resize and std::__locale_imp<__int128>::~__locale_imp (main app, unit v5916).
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "HLSprite.h"
#include "MapCell.h"
#include "TPEngine.h"
#include "std_basic_filebuf.h"
#include "ui_ime.h"

// 0x10073ed4 (736): insert n copies of *value before pos. The length error is built and "thrown" through the library's __throw; the
// fakes of the tests return from it, and the original goes on (kept).
void MslPtrVector::insert(const void** pos, uint32_t n, const void* const* value) {
    if (n == 0) return;
    if (n > 0x3fffffffu || mSize > 0x3fffffffu - n) {
        IME::MslExceptionObject exc;
        exc.mVtbl = IME::gMslExcVtblC;
        static const char kMessage[] = "vector::insert length error";   // 0x1023dc08
        exc.mVtbl = IME::gMslExcVtblB;
        exc.mRc.mPtr = static_cast<char*>(IME::MslNewArray(static_cast<uint32_t>(IME::MslStrlen(kMessage) + 1)));
        exc.mRc.mCount = nullptr;
        if (exc.mRc.mPtr != nullptr) {
            int32_t* count = static_cast<int32_t*>(IME::MslNew(4));
            if (count != nullptr) *count = 1;
            exc.mRc.mCount = count;
        }
        IME::MslStrcpy(exc.mRc.mPtr, kMessage);
        exc.mVtbl = IME::gMslLengthErrorVtbl;
        IME::MslThrow("!std::exception!!std::logic_error!!std::length_error!!", &exc, IME::gMslLengthErrorDtorTV);
    }
    const uint32_t newSize = mSize + n;
    if (newSize <= mCapacity) {
        const void** end = mData + mSize;
        if (end - pos != 0) {
            const void** src = end;
            const void** dst = end + n;
            while (src != pos) {
                --src;
                --dst;
                *dst = *src;
            }
        }
        for (uint32_t i = 0; i < n; i++) {
            *pos = *value;
            pos++;
        }
        mSize += n;
        return;
    }
    const void** old = mData;
    const uint32_t index = static_cast<uint32_t>(pos - old);
    uint32_t capacity = mCapacity;
    if (capacity == 0) capacity = 1;
    while (newSize > capacity) capacity <<= 1;
    mData = static_cast<const void**>(IME::MslNew(capacity * static_cast<uint32_t>(sizeof(void*))));
    if (index != 0) {
        const void** to = mData;
        for (const void** from = old; from != old + index; from++) {
            *to = *from;
            to++;
        }
    }
    for (uint32_t i = 0; i < n; i++) mData[index + i] = *value;
    const uint32_t oldSize = mSize;
    if (index < oldSize) {
        const void** to = mData + index + n;
        for (const void** from = old + index; from != old + oldSize; from++) {
            *to = *from;
            to++;
        }
    }
    if (old != nullptr) IME::MslDelete(const_cast<void**>(old));
    mSize = newSize;
    mCapacity = capacity;
}

// 0x10073e10 (116)
const void** MslPtrVector::erase(const void** first, const void** last) {
    if (first == last) return first;
    const void** end = mData + mSize;
    if (end - last != 0) {
        const void** to = first;
        for (const void** from = last; from != end; from++) {
            *to = *from;
            to++;
        }
    }
    mSize -= static_cast<uint32_t>(last - first);
    return first;
}

// 0x10073d58 (108)
void MslPtrVector::resize(uint32_t n, const void* value) {
    const uint32_t size = mSize;
    if (n > size) {
        insert(mData + size, n - size, &value);
    } else if (n < size) {
        erase(mData + n, mData + size);
    }
}

// 0x100737a0 (352): the facets lose a reference each (the last one deletes the facet through its virtual destructor), the
// character array loses one, then the arrays and, for a positive flag, the object itself are freed.
LocaleImp* LocaleImpDestroy(LocaleImp* self, int16_t deleteFlag) {
    if (self == nullptr) return self;
    const uint32_t count = self->mFacets.mSize;
    for (uint32_t i = 0; i < count; i++) {
        UseFacetObj* facet = static_cast<UseFacetObj*>(const_cast<void*>(self->mFacets.mData[i]));
        if (facet == nullptr) continue;
        facet->mRefs = facet->mRefs - 1;
        if (facet->mRefs == 0) use_facet_callee::FacetDelete(facet, 1);
    }
    LocaleCharArray* chars = self->mChars;
    if (chars->mRefs == 0) chars->mRefs = 1;
    chars->mRefs = chars->mRefs - 1;
    if (chars->mRefs != 0) chars = nullptr;
    self->mChars = chars;
    if (self->mChars != nullptr) {
        IME::MslDelete(self->mChars->mData);
        IME::MslDelete(self->mChars);
    }
    if (self->mFacets.mData != nullptr) IME::MslDelete(const_cast<void**>(self->mFacets.mData));
    if (deleteFlag > 0) IME::MslDelete(self);
    return self;
}
