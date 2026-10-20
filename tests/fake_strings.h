// Functional bullfrog strings for the unit test libraries (tests/unit_<Name>.cpp): every
// TbDynamicStringTemplate and TbStringBase member the translations use, for char and
// char16_t, unrecorded (the PowerPC side's test_FSSystem.py string fakes do the same). Include
// it in exactly one file of a library.
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

#include "TbDynamicStringTemplate.h"

namespace {

template<class Char> std::size_t TextLength(const Char* text) {
    std::size_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}

template<class Char> void SetText(TbDynamicStringTemplate<Char>& s, const Char* text, std::size_t n) {
    Char* data = static_cast<Char*>(std::malloc((n + 1) * sizeof(Char)));
    if (n != 0) std::memcpy(data, text, n * sizeof(Char));
    data[n] = 0;
    s.mData = data;
    s.mLength = static_cast<int32_t>(n);
}

template<class Char> const TbStringBase<Char>& Full(const TbStringBase<Char>& s) {
    return s;
}

const char16_t kNoWideText[1] = {0};

}  // namespace

template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate()
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    const Char empty[1] = {0};
    SetText(*this, empty, 0);
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char* text)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, text, TextLength(text));
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbDynamicStringTemplate& other)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const TbStringBase<Char>& other)
    : mGrowBy(0) {
    mCapacity = 0;
    mData = nullptr;
    mLength = 0;
    SetText(*this, Full(other).mData, static_cast<std::size_t>(Full(other).mLength));
}
template<class Char> TbDynamicStringTemplate<Char>::~TbDynamicStringTemplate() {}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(
    const TbDynamicStringTemplate& other) {
    SetText(*this, other.mData, static_cast<std::size_t>(other.mLength));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    SetText(*this, Full(other).mData, static_cast<std::size_t>(Full(other).mLength));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator=(const Char* text) {
    SetText(*this, text, TextLength(text));
    return *this;
}
#ifdef TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW
// A library whose allocation observer sorts operator new calls into game objects by size defines
// TPW_FAKE_STRINGS_JOIN_WITHOUT_NEW before including this file: the join then allocates with malloc, as SetText does, and
// the observer never sees a string buffer
// (a std::basic_string temporary asks operator new for 32 bytes for a text of 23 to 30 characters, which is also the size of
// a host CSpriteFileIO).
template<class Char> void JoinText(TbDynamicStringTemplate<Char>& s, const Char* text, std::size_t n) {
    const std::size_t old = static_cast<std::size_t>(s.mLength);
    Char* data = static_cast<Char*>(std::malloc((old + n + 1) * sizeof(Char)));
    if (old != 0) std::memcpy(data, s.mData, old * sizeof(Char));
    if (n != 0) std::memcpy(data + old, text, n * sizeof(Char));
    data[old + n] = 0;
    s.mData = data;
    s.mLength = static_cast<int32_t>(old + n);
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    JoinText(*this, Full(text).mData, static_cast<std::size_t>(Full(text).mLength));
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char* text) {
    JoinText(*this, text, TextLength(text));
    return *this;
}
#else
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    std::basic_string<Char> joined(mData, static_cast<std::size_t>(mLength));
    joined.append(Full(text).mData, static_cast<std::size_t>(Full(text).mLength));
    SetText(*this, joined.c_str(), joined.size());
    return *this;
}
template<class Char> TbDynamicStringTemplate<Char>& TbDynamicStringTemplate<Char>::operator+=(const Char* text) {
    std::basic_string<Char> joined(mData, static_cast<std::size_t>(mLength));
    joined.append(text);
    SetText(*this, joined.c_str(), joined.size());
    return *this;
}
#endif
template<class Char> int32_t TbStringBase<Char>::Length() const { return Full(*this).mLength; }
// Right and Left: the whole text when it is not longer than `count`.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Right(uint32_t count) const {
    const TbStringBase<Char>& s = Full(*this);
    TbDynamicStringTemplate<Char> out;
    const uint32_t length = static_cast<uint32_t>(s.mLength);
    if (count >= length) SetText(out, s.mData, length);
    else SetText(out, s.mData + (length - count), count);
    return out;
}
template<class Char> TbStringBase<Char> TbStringBase<Char>::Left(uint32_t count) const {
    const TbStringBase<Char>& s = Full(*this);
    TbDynamicStringTemplate<Char> out;
    const uint32_t length = static_cast<uint32_t>(s.mLength);
    SetText(out, s.mData, count >= length ? length : count);
    return out;
}
// Mid: the characters from `start` (none past the end), at most `count` of them.
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start, uint32_t count) const {
    const TbStringBase<Char>& s = Full(*this);
    TbDynamicStringTemplate<Char> out;
    const uint32_t length = static_cast<uint32_t>(s.mLength);
    if (start >= length) return out;
    SetText(out, s.mData + start, count >= length - start ? length - start : count);
    return out;
}
template<class Char> TbStringBase<Char> TbStringBase<Char>::Mid(uint32_t start) const {
    const TbStringBase<Char>& s = Full(*this);
    TbDynamicStringTemplate<Char> out;
    const uint32_t length = static_cast<uint32_t>(s.mLength);
    if (start < length) SetText(out, s.mData + start, length - start);
    return out;
}
// operator==: the same characters (case matters).
template<class Char> bool TbStringBase<Char>::operator==(const TbStringBase& other) const {
    const TbStringBase<Char>& a = Full(*this);
    const TbStringBase<Char>& b = Full(other);
    return a.mLength == b.mLength &&
           std::memcmp(a.mData, b.mData, static_cast<std::size_t>(a.mLength) * sizeof(Char)) == 0;
}
#include "fake_string_slots.h"
template class TbStringBase<char>;
template class TbStringBase<char16_t>;
template class TbDynamicStringTemplate<char>;
template class TbDynamicStringTemplate<char16_t>;

