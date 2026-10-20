// The destruction of the IME's candidate lists, main app, unit w92:
//   0x101f6bec IME::list_cand_data_t::~list_cand_data_t (100, ui_IME.cpp)
//   0x101f6ecc std::vector<std::basic_string<wchar_t>>::tear_down (124, ui_IME.cpp)
//   0x101f7d18 CUIIME::~CUIIME (116, ui_iterators.cpp)
// The Mac destructors take a deleting flag in r4 and guard against a null object (and, for a subobject, against the member
// offset making it null): on the host the destructor is the first part and `delete p` is the deleting call, as in src/UI_close.cpp.
#include "ui_ime.h"

#include <new>

// 0x101f6ecc. When the vector has a block: each of the `size` strings is destroyed in order (the string library's destructor, the
// flag -1 so that it does not free the element), then the block is freed. The capacity is not read; a vector with no block does nothing
// even when its size is not zero.
template <>
void MslVector<IME::MslWideString>::tear_down() {
    if (mData != nullptr) {
        for (IME::MslWideString* string = mData; string < mData + mSize; ++string) string->~MslWideString();
        ::operator delete(mData);
    }
}

// 0x101f6bec. Lets go of the strings (the first 0x10 bytes hold nothing that needs it).
IME::list_cand_data_t::~list_cand_data_t() {
    mStrings.tear_down();
}

// 0x101f7d18. The members go in reverse order of their offsets: the candidate list at +0x154, then the IME at +0x1c (the Mac calls
// both with the flag -1, so neither frees anything); the generated member destruction does exactly that.
CUIIME::~CUIIME() {}
