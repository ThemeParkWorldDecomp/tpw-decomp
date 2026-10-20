// Bullfrog strings: how the dynamic and static strings size their buffers (bullfrog
// shared: TbDynamicStringTemplate<char> AllocBuffer 0x1000e150 and ResizeTo 0x1000e200, the
// wchar_t instances 0x1000e8b4 and 0x1000e96c, TbStaticStringTemplate<char>::ResizeTo
// 0x10010558). Every constructor, assignment and append of TbString.cpp goes through
// these by the vtable.
#include "TbDynamicStringTemplate.h"

// A fresh buffer for `length` characters: the capacity is the next multiple of 8 above the
// length, plus the terminator. The old buffer is the caller's business.
template<class Char> void TbDynamicStringTemplate<Char>::AllocBuffer(uint32_t length) {
    mGrowBy = 8;
    mLength = static_cast<int32_t>(length);
    mCapacity = static_cast<int32_t>(static_cast<uint32_t>(mGrowBy) *
                                     (static_cast<uint32_t>(mLength) / static_cast<uint32_t>(mGrowBy) + 1));
    mData = new Char[static_cast<uint32_t>(mCapacity) + 1];
    mData[mLength] = 0;
}

// Within the capacity only the length moves. Beyond it a new buffer is allocated and the
// old text copied over (up to its terminator, so what lies between the old text's end and
// the new length is not initialised: the caller writes it).
template<class Char> void TbDynamicStringTemplate<Char>::ResizeTo(uint32_t length) {
    if (static_cast<uint32_t>(mCapacity) < length) {
        Char* old = mData;
        AllocBuffer(length);
        this->SBCstrcpy(mData, old);
        delete[] old;
    } else {
        mLength = static_cast<int32_t>(length);
        mData[mLength] = 0;
    }
}

// A static string cannot grow: a length that does not fit is cut to capacity - 1 and the
// truncation remembered.
template<class Char> void TbStaticStringTemplate<Char>::ResizeTo(uint32_t length) {
    if (length < static_cast<uint32_t>(mCapacity)) {
        mLength = static_cast<int32_t>(length);
        mTruncated = 0;
    } else {
        mLength = mCapacity - 1;
        mTruncated = 1;
    }
    mData[mLength] = 0;
}

// The static string's assignments and appends (0x10010658..0x10010a40): size first (which
// may truncate), then copy as much as the new length allows.
template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const TbStringBase<Char>& other) {
    ResizeTo(static_cast<uint32_t>(other.Length()));
    this->SBCstrncpy(mData, other.mData, static_cast<uint32_t>(mLength));
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char* text) {
    if (text == nullptr) {
        ResizeTo(0);
    } else {
        ResizeTo(this->SBCstrlen(text));
        this->SBCstrncpy(mData, text, static_cast<uint32_t>(mLength));
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator=(const Char& ch) {
    if (ch == 0) {
        ResizeTo(0);
    } else {
        ResizeTo(1);
        if (mTruncated == 0) mData[0] = ch;
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const TbStringBase<Char>& text) {
    const int32_t old = mLength;
    ResizeTo(static_cast<uint32_t>(mLength + text.Length()));
    this->SBCstrncat(mData, text.mData, static_cast<uint32_t>(mLength - old));
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char& ch) {
    if (ch != 0) {
        ResizeTo(static_cast<uint32_t>(mLength + 1));
        if (mTruncated == 0) mData[mLength - 1] = ch;
    }
    return *this;
}

template<class Char> TbStaticStringTemplate<Char>& TbStaticStringTemplate<Char>::operator+=(const Char* text) {
    if (text != nullptr) {
        const int32_t old = mLength;
        ResizeTo(static_cast<uint32_t>(mLength) + this->SBCstrlen(text));
        this->SBCstrncat(mData, text, static_cast<uint32_t>(mLength - old));
    }
    return *this;
}

template void TbDynamicStringTemplate<char>::AllocBuffer(uint32_t);
template void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t);
template void TbDynamicStringTemplate<char>::ResizeTo(uint32_t);
template void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t);
template void TbStaticStringTemplate<char>::ResizeTo(uint32_t);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const TbStringBase<char>&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char*);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator=(const char&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const TbStringBase<char>&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char&);
template TbStaticStringTemplate<char>& TbStaticStringTemplate<char>::operator+=(const char*);
