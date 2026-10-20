// Bullfrog string functions, 0x1000e2c8..0x10013774. Substrings borrow their data.
#include "TbDynamicStringTemplate.h"
#include "platform/msl.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

template<class Char> uint32_t TbStringBase<Char>::SBCstrlen(const Char* text) const {
    if constexpr (std::is_same_v<Char, char>) return std::strlen(text);
    else return wcslen(text);
}
template<class Char> Char* TbStringBase<Char>::SBCstrcpy(Char* dst, const Char* src) {
    if constexpr (std::is_same_v<Char, char>) return std::strcpy(dst, src);
    else return wcscpy(dst, src);
}
template<class Char> Char* TbStringBase<Char>::SBCstrncpy(Char* dst, const Char* src, uint32_t count) {
    if constexpr (std::is_same_v<Char, char>) return std::strncpy(dst, src, count);
    else return wcsncpy(dst, src, count);
}
// w225: wide SBCstrcat 0x100113d4, 44 B.
template<class Char> Char* TbStringBase<Char>::SBCstrcat(Char* dst, const Char* src) {
    if constexpr (std::is_same_v<Char, char>) return std::strcat(dst, src);
    else return wcscat(dst, src);
}
template<class Char> Char* TbStringBase<Char>::SBCstrncat(Char* dst, const Char* src, uint32_t count) {
    if constexpr (std::is_same_v<Char, char>) return std::strncat(dst, src, count);
    else return wcsncat(dst, src, count);
}
template<class Char> int32_t TbStringBase<Char>::Length() const { return mLength; }
// w225: wide Mid 0x1001263c, 84 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start) const {
    start = std::min(start, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + start, static_cast<uint32_t>(mLength) - start);
}
// w225: wide Mid 0x100126c0, 124 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start, uint32_t count) const {
    start = std::min(start, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + start, std::min(count, static_cast<uint32_t>(mLength) - start));
}
// w225: wide Right 0x1001276c, 72 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Right(uint32_t count) const {
    count = std::min(count, static_cast<uint32_t>(mLength));
    return TbStringBase(mData + static_cast<uint32_t>(mLength) - count, count);
}
// w225: wide Left 0x100127e4, 60 B.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Left(uint32_t count) const {
    return TbStringBase(mData, std::min(count, static_cast<uint32_t>(mLength)));
}
// w225: wide text equality 0x10011f28, 156 B.
template<class Char> bool TbStringBase<Char>::operator==(const Char* text) const {
    if (text == nullptr) return mLength == 0;
    if (static_cast<uint32_t>(mLength) != SBCstrlen(text)) return false;
    TbStringBase other(const_cast<Char*>(text), mLength);
    return *this == other;
}
// w225: wide text inequality 0x100120ec, 168 B.
template<class Char> bool TbStringBase<Char>::operator!=(const Char* text) const { return !(*this == text); }
// w225: wide base equality 0x10011ff4, 76 B.
template<class Char> bool TbStringBase<Char>::operator==(const TbStringBase& other) const {
    if (mLength != other.mLength) return false;
    if constexpr (std::is_same_v<Char, char>) return std::strncmp(mData, other.mData, mLength) == 0;
    else return wcsncmp(mData, other.mData, mLength) == 0;
}
// w225: wide base comparison 0x10012d48, 164 B.
template<class Char> bool TbStringBase<Char>::operator<(const TbStringBase& other) const {
    const uint32_t n = std::min(static_cast<uint32_t>(mLength), static_cast<uint32_t>(other.mLength));
    int32_t result;
    if constexpr (std::is_same_v<Char, char>) result = std::strncmp(mData, other.mData, n);
    else result = wcsncmp(mData, other.mData, n);
    return result == 0 ? static_cast<uint32_t>(mLength) < static_cast<uint32_t>(other.mLength) : result < 0;
}
// w225: wide FindOneOf 0x10012938, 156 B.
template<class Char> int32_t TbStringBase<Char>::FindOneOf(const Char* set, uint32_t start) const {
    if (set == nullptr) return -1;
    for (uint32_t i = std::min(start, static_cast<uint32_t>(mLength)); i < static_cast<uint32_t>(mLength); ++i)
        for (const Char* p = set; *p != 0; ++p)
            if (mData[i] == *p) return i;
    return -1;
}
// w225: wide FindReverseOneOf 0x10012850, 168 B.
template<class Char> int32_t TbStringBase<Char>::FindReverseOneOf(const Char* set, uint32_t start) const {
    if (set == nullptr || mLength == 0) return -1;
    uint32_t i = std::min(start, static_cast<uint32_t>(mLength) - 1);
    do {
        for (const Char* p = set; *p != 0; ++p)
            if (mData[i] == *p) return i;
    } while (i-- != 0);
    return -1;
}
// w225: wide character Find 0x10012adc, 136 B.
template<class Char> int32_t TbStringBase<Char>::Find(const Char& ch, uint32_t start) const {
    if (ch == 0) return -1;
    for (uint32_t i = std::min(start, static_cast<uint32_t>(mLength)); i < static_cast<uint32_t>(mLength); ++i)
        if (mData[i] == ch) return i;
    return -1;
}
// w225: wide character FindReverse 0x10012a0c, 152 B.
template<class Char> int32_t TbStringBase<Char>::FindReverse(const Char& ch, uint32_t start) const {
    if (ch == 0 || mLength == 0) return -1;
    uint32_t i = std::min(start, static_cast<uint32_t>(mLength) - 1);
    do {
        if (mData[i] == ch) return i;
    } while (i-- != 0);
    return -1;
}
// w225: wide text Find 0x10012b98, 152 B.
template<class Char> int32_t TbStringBase<Char>::Find(const Char* text, uint32_t start) const {
    if (text == nullptr) return -1;
    start = std::min(start, static_cast<uint32_t>(mLength));
    const Char* result;
    if constexpr (std::is_same_v<Char, char>) result = std::strstr(mData + start, text);
    else result = wcsstr(mData + start, text);
    return result == nullptr ? -1 : static_cast<int32_t>(result - mData);
}
template<class Char> Char* TbStringBase<Char>::LockBuffer(uint32_t length) {
    ResizeTo(length);
    return mData;
}
template<class Char> void TbStringBase<Char>::ReleaseBuffer(uint32_t length) {
    if (length == 0x7fffffff) {
        ResizeTo(std::min(SBCstrlen(mData), static_cast<uint32_t>(mCapacity)));
    } else {
        ResizeTo(length);
        mData[length] = 0;
    }
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate() { AllocBuffer(0); }
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char* text) {
    AllocBuffer(text == nullptr ? 0 : this->SBCstrlen(text));
    if (text != nullptr) this->SBCstrcpy(mData, text);
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other)
    : TbDynamicStringTemplate(static_cast<const TbStringBase<Char>&>(other)) {}
// w225: wide base constructor 0x1000f7a8, 152 B.
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbStringBase<Char>& other) {
    AllocBuffer(other.mLength);
    this->SBCstrncpy(mData, other.mData, other.mLength);
    mData[other.mLength] = 0;
}
template<class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() { delete[] mData; }
// w225: wide base assignment 0x1000ead4, 156 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    if (static_cast<const TbStringBase<Char>*>(this) != &other) {
        if (static_cast<uint32_t>(mLength) < static_cast<uint32_t>(other.mLength)) ResizeTo(other.mLength);
        this->SBCstrncpy(mData, other.mData, other.mLength);
        ResizeTo(other.mLength);
    }
    return *this;
}
// w225: wide dynamic assignment 0x1000f614, 84 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbDynamicStringTemplate& other) {
    TbStringBase<Char> view(other);
    return operator=(view);
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char* text) {
    if (text == nullptr) {
        mLength = 0;
        mData[0] = 0;
    } else {
        ResizeTo(this->SBCstrlen(text));
        this->SBCstrcpy(mData, text);
    }
    return *this;
}
// w225: wide character assignment 0x1000ec88, 132 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char& ch) {
    if constexpr (std::is_same_v<Char, char16_t>) {
        // The wide listing branches before ResizeTo, then reloads the aliased unit.
        if (ch == 0) ResizeTo(0);
        else {
            ResizeTo(1);
            mData[0] = ch;
        }
    } else {
        ResizeTo(ch == 0 ? 0 : 1);
        if (ch != 0) mData[0] = ch;
    }
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    ResizeTo(static_cast<uint32_t>(mLength) + text.Length());
    this->SBCstrncat(mData, text.mData, text.Length());
    return *this;
}
// w225: wide character append 0x1000ee18, 120 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char& ch) {
    if (ch != 0) {
        ResizeTo(static_cast<uint32_t>(mLength) + 1);
        mData[mLength - 1] = ch;
    }
    return *this;
}
// w225: wide text append 0x1000eecc, 124 B.
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char* text) {
    if (text != nullptr) {
        ResizeTo(static_cast<uint32_t>(mLength) + this->SBCstrlen(text));
        this->SBCstrcat(mData, text);
    }
    return *this;
}
template<class Char> TbStaticStringTemplate<Char>::TbStaticStringTemplate(Char* buffer, uint32_t capacity, int32_t clear) {
    mCapacity = capacity;
    mData = buffer;
    ResizeTo(clear == 0 ? this->SBCstrlen(buffer) : 0);
}
template<class Char> TbStaticStringTemplate<Char>::~TbStaticStringTemplate() {}

template class TbStringBase<char>;
template class TbStringBase<char16_t>;
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;
template TbStaticStringTemplate<char>::TbStaticStringTemplate(char*, uint32_t, int32_t);
template TbStaticStringTemplate<char>::~TbStaticStringTemplate();
