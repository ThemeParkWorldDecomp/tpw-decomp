// Bullfrog members outside u11. Shared functional fakes; the string unit records calls.
#pragma once
#include "TbDynamicStringTemplate.h"
#include "platform/msl.h"
#include <algorithm>
#include <cstring>
#include <string>

#ifndef TPW_STRING_EVENT
#define TPW_STRING_EVENT(kind, value, width) ((void)0)
#endif
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t length) {
    TPW_STRING_EVENT(1, length, sizeof(Char));
    mGrowBy = 8;
    mLength = length;
    mCapacity = (length / 8 + 1) * 8;
    mData = new Char[512];
    std::fill_n(mData, 512, Char(0x7e));
    mData[length] = 0;
}
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t length) {
    TPW_STRING_EVENT(2, length, sizeof(Char));
    // A fake with spare storage keeps aliases stable and exposes every requested resize.
    mLength = length;
    mData[length] = 0;
}
template<class Char> void TbStaticStringTemplate<Char>::Format(const Char*, ...) {}
template<class Char> void TbStaticStringTemplate<Char>::ResizeTo(uint32_t length) {
    TPW_STRING_EVENT(3, length, sizeof(Char));
    mTruncated = length >= static_cast<uint32_t>(mCapacity);
    mLength = mTruncated ? mCapacity - 1 : length;
    mData[mLength] = 0;
}
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const TbStringBase<Char>&) { return *this; }
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char*) { return *this; }
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char&) { return *this; }
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const TbStringBase<Char>&) { return *this; }
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char*) { return *this; }
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char&) { return *this; }

uint32_t wcslen(const char16_t* text) { return std::char_traits<char16_t>::length(text); }
char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    std::copy_n(src, wcslen(src) + 1, dst);
    return dst;
}
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count) {
    const uint32_t n = std::min(wcslen(src), count);
    std::copy_n(src, n, dst);
    std::fill(dst + n, dst + count, char16_t(0));
    return dst;
}
char16_t* wcscat(char16_t* dst, const char16_t* src) { wcscpy(dst + wcslen(dst), src); return dst; }
char16_t* wcsncat(char16_t* dst, const char16_t* src, uint32_t count) {
    const uint32_t n = std::min(wcslen(src), count);
    char16_t* end = dst + wcslen(dst);
    std::copy_n(src, n, end);
    end[n] = 0;
    return dst;
}
int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        if (a[i] == 0) break;
    }
    return 0;
}
const char16_t* wcsstr(const char16_t* text, const char16_t* needle) {
    const uint32_t n = wcslen(needle);
    do {
        if (wcsncmp(text, needle, n) == 0) return text;
    } while (*text++ != 0);
    return nullptr;
}
template void TbDynamicStringTemplate<char>::Format(const char*, ...);
template void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...);
template void TbDynamicStringTemplate<char>::ResizeTo(uint32_t);
template void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t);
template void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t);
template void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t);
template class TbStaticStringTemplate<char>;
