// PARTIAL ltms string table. Mac size 24, vtable 0x100113cc.
#pragma once
#include <cstdint>
#include "TbResource.h"
#include "TbVTag.h"

class TbIResourceIndex;

class TbITable : public TbResource {
public:
    TB_RESOURCE_IN_ARRAY_BLOCK
    TbITable() : mCount(0), mStrings(nullptr), mMBToUnicode(nullptr), mUnicodeToMB(nullptr) {}
    // 0x100049f0: `data` is the file after its header: `count` offsets, then the strings.
    TbITable(uint32_t count, void* data, TbIResourceIndex* index);
    int32_t Release() override;      // 0x10005370
    void AddRef() override;          // 0x10005324
    ~TbITable() override;            // 0x10004b98
    const unsigned char* GetConstString(uint32_t id) const;   // ltms 0x10005290
    // Formats string `id` (printf style, with the arguments) into `out`, at most `size`
    // characters (imported).
    int32_t FormatString(char16_t* out, uint32_t size, uint32_t id, ...) const;
    int32_t DoFormat(CStaticBuffer& out, uint32_t id, char* arguments) const;
    TbVTag* FindVariableTag(uint32_t id, char*& arguments) const;
    uint32_t mCount;                   // Mac +8
    // Mac +12: the offset table at the start of the file data, turned into pointers in
    // place. A pointer does not fit an offset's four bytes on a 64-bit host, so the table
    // built from a file keeps its pointers in an array of its own (mOwnedStrings).
    const unsigned char** mStrings;
    TbIMBToUnicode* mMBToUnicode;       // Mac +16
    TbIUnicodeToMB* mUnicodeToMB;       // Mac +20
    const unsigned char** mOwnedStrings = nullptr;   // host only: freed by the destructor
    // Unit v5032
    // 0x10004e30: the multibyte overload: formats string `id` into `out` (at most `size` bytes) through the
    // table's Unicode-to-multibyte converter (Mac +20). The arguments after `id` are the variable tags.
    int32_t FormatString(unsigned char* out, uint32_t size, uint32_t id, ...) const;
};

#include "CStaticBuffer.h"
// Unit v5032: the multibyte output buffer of TbITable::FormatString (Mac size 0x14, vptr +0, cursor +4, end +8,
// converter +0xc, overflow +0x10). Its methods live in other files (the destructor is the key function).
class CStaticMBBuffer : public CStaticBuffer {
public:
    ~CStaticMBBuffer() override;   // 0x10000b24: Close, then the converter is released if still held
    void Close() override;
    void AppendMB(const unsigned char*, uint32_t) override;
    void AppendMB(uint16_t) override;
    void AppendUnicode(const char16_t*, uint32_t) override;
    void AppendUnicode(char16_t) override;
    unsigned char* mCursor;       // Mac +4
    unsigned char* mEnd;          // Mac +8
    TbIUnicodeToMB* mConverter;   // Mac +12
    bool mOverflow;               // Mac +16
};
