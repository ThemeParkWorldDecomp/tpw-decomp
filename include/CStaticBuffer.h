// PARTIAL ltms buffers. Mac slots: destructor 8, Close 12, AppendMB 16/20,
// AppendUnicode 24/28. Buffer implementations are outside u12.
#pragma once
#include "TbResource.h"
class CStaticBuffer {
public:
    virtual ~CStaticBuffer() = default;
    virtual void Close() = 0;
    virtual void AppendMB(const unsigned char*, uint32_t) = 0;
    virtual void AppendMB(uint16_t) = 0;
    virtual void AppendUnicode(const char16_t*, uint32_t) = 0;
    virtual void AppendUnicode(char16_t) = 0;
};
// The string table's output buffer (src/ltms/StaticUnicodeBuffer.cpp, unit w40; Mac size 0x14, vtable 0x10010f74 in the ltms library): a
// window of 16-bit characters [mCursor, mEnd) that text is written into, one character at a time through the virtual AppendUnicode.
// TbITable::FormatString builds one over the caller's array and closes it. The Mac object is vptr +0, cursor +4, end +8, converter +0xc,
// overflow +0x10; the host's members are native.
class CStaticUnicodeBuffer : public CStaticBuffer {
public:
    // 0x100009d8: Close, then the converter is let go if one is still held (Close has already dropped it), as the object ends.
    ~CStaticUnicodeBuffer() override;
    // 0x10000904: terminates the text and lets go of the converter. With a cursor: the terminator goes AT the cursor, or, when the
    // buffer is full (the cursor reached the end: that sets the overflow flag now) or has overflowed, on the last character (one
    // BEFORE the cursor: for a buffer of no room that is the character before the buffer, written all the same). The cursor is
    // cleared. A converter is cleared and Released (virtual slot 8).
    void Close() override;
    // 0x100004f0: appends the multibyte characters of `data` through the converter (nothing at all without one): `size` bytes' worth
    // (the loop ends when the cursor gets to data + size exactly, so a two-byte character that straddles the end runs on) or, for a
    // size of 0xffffffff, up to the terminating zero byte. Each character is the converter's table entry for TbIMBInfo::GetIndex's
    // index (0 when the index is not below the converter's count), appended through the virtual AppendUnicode(char16_t).
    void AppendMB(const unsigned char* data, uint32_t size) override;
    // 0x100006bc: one multibyte character; the two bytes of `value` are read high byte first (the Mac stores the halfword
    // big-endian), a lead byte taking the low one with it. Nothing without a converter.
    void AppendMB(uint16_t value) override;
    // 0x10000780: `count` characters of `text`, or up to the terminating zero for a count of 0xffffffff, stopping as soon as the
    // buffer has overflowed.
    void AppendUnicode(const char16_t* text, uint32_t count) override;
    // 0x10000884: one character; reaching the end of the buffer sets the overflow flag and the character is dropped.
    void AppendUnicode(char16_t value) override;
    char16_t* mCursor;            // Mac +4
    char16_t* mEnd;               // Mac +8
    TbIMBToUnicode* mConverter;   // Mac +12
    bool mOverflow;              // Mac +16
};
