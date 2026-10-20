// CStaticMBBuffer (ltms): the multibyte output buffer of TbITable::FormatString. The destructor is the key function (src/TbIMBToUnicode_TbIMBInfo_Dtor_TbIMBInfo.cpp, unit v51128), so the vtable lands there.
#include "TbITable.h"
#include "TbResource.h"

// 0x100000dc. A size of 0xffffffff means "up to the terminating zero". Stops quietly when the buffer is full (the flag is set when the
// cursor reaches the end, and is tested before every byte).
void CStaticMBBuffer::AppendMB(const unsigned char* data, uint32_t size) {
    if (!(mCursor < mEnd)) return;
    if (size == 0xffffffffu) {
        while (*data != 0) {
            if (!mOverflow && mCursor == mEnd) mOverflow = true;
            if (mOverflow) return;
            *mCursor++ = *data++;
        }
        return;
    }
    for (;;) {
        const bool done = (size == 0);
        size--;
        if (done) return;
        if (!mOverflow && mCursor == mEnd) mOverflow = true;
        if (mOverflow) return;
        *mCursor++ = *data++;
    }
}

// 0x100001e8. Low byte, then the high byte when it is not zero (the second byte is written without a room test: BUG kept).
void CStaticMBBuffer::AppendMB(uint16_t value) {
    if (!mOverflow && mCursor == mEnd) mOverflow = true;
    if (mOverflow) return;
    *mCursor++ = static_cast<unsigned char>(value);
    const unsigned char high = static_cast<unsigned char>(value >> 8);
    if (high == 0) return;
    *mCursor++ = high;
}

// 0x10000284. No converter: nothing. Each character goes through the virtual AppendMB(uint16_t) (vtable +0x14).
void CStaticMBBuffer::AppendUnicode(const char16_t* text, uint32_t size) {
    if (mConverter == nullptr) return;
    if (size == 0xffffffffu) {
        while (*text != 0) {
            const char16_t c = *text++;
            AppendMB(mConverter->GetMBValue(c));
        }
        return;
    }
    for (;;) {
        const bool done = (size == 0);
        size--;
        if (done) return;
        const char16_t c = *text++;
        AppendMB(mConverter->GetMBValue(c));
    }
}

// 0x10000394
void CStaticMBBuffer::AppendUnicode(char16_t c) {
    if (mConverter == nullptr) return;
    AppendMB(mConverter->GetMBValue(c));
}

// 0x10000420. Terminates the text (at the cursor, or at the last byte when the buffer is full), forgets the cursor, lets go of the converter.
void CStaticMBBuffer::Close() {
    unsigned char* cursor = mCursor;
    if (cursor != nullptr) {
        if (!mOverflow && cursor == mEnd) mOverflow = true;
        if (!mOverflow) {
            *mCursor = 0;
            mCursor = nullptr;
        } else {
            mCursor[-1] = 0;
            mCursor = nullptr;
        }
    }
    TbIUnicodeToMB* converter = mConverter;
    if (converter != nullptr) {
        mConverter = nullptr;
        converter->Release();
    }
}
