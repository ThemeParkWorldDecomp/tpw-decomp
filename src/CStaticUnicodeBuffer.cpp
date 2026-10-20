// CStaticUnicodeBuffer (ltms library, 0x100004f0..0x100009d8; unit w40): the output buffer of the string table. TbITable::FormatString
// makes one over the caller's array (cursor at the start, end past the last character, the table's multibyte to Unicode converter,
// the overflow flag clear), the table's formatting bytecode appends to it through the virtual AppendMB and AppendUnicode, and Close
// terminates the text. The class is declared in include/CStaticBuffer.h.
#include "CStaticBuffer.h"

namespace {

// The converter's Unicode character for the multibyte character at `cursor`, which moves past it: the table entry for
// TbIMBInfo::GetIndex's index (taken as 16 bits), or 0 when the index is not below the converter's count.
char16_t ConvertOne(const TbIMBToUnicode* converter, const unsigned char*& cursor) {
    const uint32_t index = converter->GetIndex(cursor) & 0xffff;
    return index < converter->mCount ? converter->mTable[index] : 0;
}

}  // namespace

// 0x100009d8. The Mac's also resets the vtable to the class's own on entry and to the base class's on exit, and frees the object
// when its hidden flag is positive; the compiler does both here.
CStaticUnicodeBuffer::~CStaticUnicodeBuffer() {
    Close();
    if (mConverter != nullptr) mConverter->Release();
}

// 0x10000904. Close drops the converter itself, so the destructor never finds one.
void CStaticUnicodeBuffer::Close() {
    if (mCursor != nullptr) {
        if (!mOverflow && mCursor == mEnd) mOverflow = true;
        if (!mOverflow) {
            *mCursor = 0;
        } else {
            mCursor[-1] = 0;
        }
        mCursor = nullptr;
    }
    TbIMBToUnicode* converter = mConverter;
    if (converter != nullptr) {
        mConverter = nullptr;
        converter->Release();
    }
}

// 0x100004f0. A size of 0xffffffff means "up to the terminating zero". With any other the loop ends when the cursor reaches
// data + size EXACTLY: a two-byte character that straddles the end moves the cursor past it and the loop runs on (BUG kept).
void CStaticUnicodeBuffer::AppendMB(const unsigned char* data, uint32_t size) {
    if (mConverter == nullptr) return;
    const unsigned char* cursor = data;
    if (size == 0xffffffffu) {
        while (*cursor != 0) AppendUnicode(ConvertOne(mConverter, cursor));
    } else {
        const unsigned char* end = data + size;
        while (cursor != end) AppendUnicode(ConvertOne(mConverter, cursor));
    }
}

// 0x100006bc. The Mac keeps the halfword in memory big-endian, so GetIndex reads its high byte first.
void CStaticUnicodeBuffer::AppendMB(uint16_t value) {
    TbIMBToUnicode* converter = mConverter;
    if (converter == nullptr) return;
    const unsigned char bytes[2] = {static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value)};
    const unsigned char* cursor = bytes;
    AppendUnicode(ConvertOne(converter, cursor));
}

// 0x10000780. The character is tested against the end of the buffer before each is written, so the buffer can be filled exactly
// and the next one, not the last, raises the overflow flag; once it is raised the rest of the text is dropped.
void CStaticUnicodeBuffer::AppendUnicode(const char16_t* text, uint32_t count) {
    if (count == 0xffffffffu) {
        while (*text != 0) {
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *text++;
        }
    } else {
        while (count != 0) {
            --count;
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *text++;
        }
    }
}

// 0x10000884
void CStaticUnicodeBuffer::AppendUnicode(char16_t value) {
    if (!mOverflow && mCursor == mEnd) mOverflow = true;
    if (mOverflow) return;
    *mCursor++ = value;
}
