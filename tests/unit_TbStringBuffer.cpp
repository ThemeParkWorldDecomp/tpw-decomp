// Test entry points for src/TbStringBuffer.cpp (library unit_TbStringBuffer). The string
// classes' other members are not under test here: they get the smallest definitions that
// let an object exist, so no other translation stands in for them.
#include <cstring>

#include "TbDynamicStringTemplate.h"

template<class C> TbDynamicStringTemplate<C>::TbDynamicStringTemplate() { mGrowBy = 0; }
template<class C> TbDynamicStringTemplate<C>::~TbDynamicStringTemplate() {}
template<class C> void TbDynamicStringTemplate<C>::Format(const C*, ...) {}
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbStringBase<C>&) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const TbDynamicStringTemplate&) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C*) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator=(const C&) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const TbStringBase<C>&) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C&) { return *this; }
template<class C> TbDynamicStringTemplate<C>& TbDynamicStringTemplate<C>::operator+=(const C*) { return *this; }
template<class C> TbStaticStringTemplate<C>::TbStaticStringTemplate(C* buffer, uint32_t capacity, int32_t) {
    mData = buffer;
    mCapacity = static_cast<int32_t>(capacity);
    mLength = 0;
    mTruncated = 0;
}
template<class C> TbStaticStringTemplate<C>::~TbStaticStringTemplate() {}
template<class C> void TbStaticStringTemplate<C>::Format(const C*, ...) {}
// ResizeTo copies the old text with the base class's own strcpy (tested in test_TbString).
template<class C> C* TbStringBase<C>::SBCstrcpy(C* dst, const C* src) {
    C* out = dst;
    while ((*out++ = *src++) != 0) {}
    return dst;
}
// The static string's operators use these as well (all four are tested in test_TbString).
template<class C> int32_t TbStringBase<C>::Length() const { return mLength; }
template<class C> uint32_t TbStringBase<C>::SBCstrlen(const C* text) const {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}
template<class C> C* TbStringBase<C>::SBCstrncpy(C* dst, const C* src, uint32_t count) {
    uint32_t i = 0;
    for (; i < count && src[i] != 0; ++i) dst[i] = src[i];
    for (; i < count; ++i) dst[i] = 0;
    return dst;
}
template<class C> C* TbStringBase<C>::SBCstrncat(C* dst, const C* src, uint32_t count) {
    C* out = dst;
    while (*out != 0) ++out;
    for (uint32_t i = 0; i < count && src[i] != 0; ++i) *out++ = src[i];
    *out = 0;
    return dst;
}
template class TbStringBase<char>;
template class TbStringBase<char16_t>;
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;
template class TbStaticStringTemplate<char>;

namespace {
// A dynamic string with the given fields over a buffer of capacity + 1 characters that
// starts with `text`; then `call` runs; then the fields and the buffer come back.
// out: [0] length, [1] capacity, [2] grow-by, [3] whether the buffer was replaced.
template<class C, class F> void Run(int32_t length, int32_t capacity, int32_t growBy, const uint16_t* text,
                                    uint32_t textCount, F call, int32_t* out, uint16_t* contents, uint32_t count) {
    TbDynamicStringTemplate<C> s;
    C* buffer = new C[static_cast<uint32_t>(capacity) + 1];
    for (uint32_t i = 0; i <= static_cast<uint32_t>(capacity); ++i) buffer[i] = static_cast<C>(0x5a);
    for (uint32_t i = 0; i < textCount && i <= static_cast<uint32_t>(capacity); ++i) buffer[i] = static_cast<C>(text[i]);
    s.mLength = length;
    s.mCapacity = capacity;
    s.mGrowBy = growBy;
    s.mData = buffer;
    call(s);
    out[0] = s.mLength;
    out[1] = s.mCapacity;
    out[2] = s.mGrowBy;
    out[3] = s.mData != buffer;
    for (uint32_t i = 0; i < count; ++i) contents[i] = static_cast<uint16_t>(s.mData[i]) & (sizeof(C) == 1 ? 0xff : 0xffff);
    if (s.mData != buffer) delete[] s.mData;   // AllocBuffer leaves the old buffer to its caller
    else delete[] buffer;
    s.mData = nullptr;
}
}  // namespace

extern "C" {

// op 0: AllocBuffer(length). op 1: ResizeTo(length). `wide` picks the 16-bit instance.
// For AllocBuffer the old buffer is freed here (the function only replaces the pointer).
void tpw_sb_dynamic(int32_t op, int32_t wide, int32_t length0, int32_t capacity0, int32_t growBy0,
                    const uint16_t* text, uint32_t textCount, uint32_t length, int32_t* out, uint16_t* contents,
                    uint32_t count) {
    if (wide) {
        char16_t* old = nullptr;
        Run<char16_t>(length0, capacity0, growBy0, text, textCount, [&](TbDynamicStringTemplate<char16_t>& s) {
            old = s.mData;
            if (op == 0) s.AllocBuffer(length);
            else s.ResizeTo(length);
            if (op != 0) old = nullptr;
        }, out, contents, count);
        delete[] old;
    } else {
        char* old = nullptr;
        Run<char>(length0, capacity0, growBy0, text, textCount, [&](TbDynamicStringTemplate<char>& s) {
            old = s.mData;
            if (op == 0) s.AllocBuffer(length);
            else s.ResizeTo(length);
            if (op != 0) old = nullptr;
        }, out, contents, count);
        delete[] old;
    }
}

// A static string over a caller buffer holding `initial`, then one assignment or append.
// op: 0 = string view, 1 = C string, 2 = character, 3 += string view, 4 += character,
// 5 += C string; `null` passes a NULL C string. out: length, capacity, truncated.
void tpw_sb_static_op(char* buffer, uint32_t capacity, int32_t op, const char* text, int32_t null, int32_t* out) {
    TbStaticStringTemplate<char> s(buffer, capacity, 0);
    s.mLength = static_cast<int32_t>(std::strlen(buffer));
    char copy[300];
    std::strcpy(copy, text);
    TbStringBase<char> view(copy, static_cast<uint32_t>(std::strlen(copy)));
    switch (op) {
    case 0: s = view; break;
    case 1: s = null ? static_cast<const char*>(nullptr) : text; break;
    case 2: s = text[0]; break;
    case 3: s += view; break;
    case 4: s += text[0]; break;
    default: s += null ? static_cast<const char*>(nullptr) : text; break;
    }
    out[0] = s.mLength;
    out[1] = s.mCapacity;
    out[2] = s.mTruncated;
}

// TbStaticStringTemplate<char>::ResizeTo over a caller buffer. out: length, capacity, truncated.
void tpw_sb_static(char* buffer, uint32_t capacity, int32_t length0, int32_t truncated0, uint32_t length, int32_t* out) {
    TbStaticStringTemplate<char> s(buffer, capacity, 0);
    s.mLength = length0;
    s.mTruncated = truncated0;
    s.ResizeTo(length);
    out[0] = s.mLength;
    out[1] = s.mCapacity;
    out[2] = s.mTruncated;
}

}  // extern "C"
