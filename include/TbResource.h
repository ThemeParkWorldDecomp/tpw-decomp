// PARTIAL ltms resource interface. Release is the first virtual slot (+8 on Mac).
// TbResource and its font/converter subclasses are implemented by ltms.
#pragma once
#include <cstddef>
#include <cstdint>
#include <new>
#include "TbError.h"
class TbResource {
public:
    TbResource() : mReferences(1) {} // 0x10005aa8
    virtual int32_t Release();      // Mac +8
    virtual void AddRef();          // Mac +0xc
    virtual ~TbResource() = default; // Mac +0x10
    virtual int32_t ManagerRelease(); // Mac +0x14
    virtual void ManagerAddRef();   // Mac +0x18
    int32_t mReferences;            // Mac +4
};
// A font resource, as the game holds it. The 4-bit RLE font is the only kind (see
// TbIRLE4bitFont.h); its virtual slots follow TbResource's: 0x1c GetCharIndex, 0x20
// GetCharCount, 0x24 GetCharSize, 0x28 GetVerticalSpacing, 0x2c GetCaps, 0x30 GetType,
// 0x34 GetMBToUnicode. Mac vtable 0x100110f8.
class TbIMBToUnicode;
struct TbFontCharSize {
    uint32_t mWidth;
    uint32_t mHeight;
};
class TbIFont : public TbResource {
public:
    TbIFont() {}                 // 0x10002be4
    ~TbIFont() override;         // 0x10002cb8
    virtual uint32_t GetCharIndex(const unsigned char*& text) = 0;
    virtual uint16_t GetCharCount() = 0;
    virtual TbFontCharSize GetCharSize(uint16_t index) = 0;
    virtual uint8_t GetVerticalSpacing() = 0;
    virtual uint32_t GetCaps() = 0;
    virtual uint32_t GetType() = 0;
    virtual TbIMBToUnicode* GetMBToUnicode() = 0;
};

// Objects the resource factories build at the start of one array allocation, their data
// right behind them (the Mac frees them with the plain operator delete, which is the same
// allocator there). These make `new` and `delete` of such a class use the array forms, so
// every allocation and release pairs up on the host.
#define TB_RESOURCE_IN_ARRAY_BLOCK \
    static void* operator new(size_t size) { return ::operator new[](size); } \
    static void* operator new(size_t, void* where) noexcept { return where; } \
    static void operator delete(void* block) { ::operator delete[](block); } \
    static void operator delete(void*, void*) noexcept {}

// A multibyte character set: how lead bytes map to table indexes. Mac size 12, vtable
// 0x1001114c. Characters below mFirstLead are single bytes; from there on a second byte
// follows.
class TbIMBInfo : public TbResource {
public:
    TbIMBInfo(uint8_t firstLead = 0, uint16_t count = 0) : mFirstLead(firstLead), mCount(count) {}
    ~TbIMBInfo() override;                                  // 0x10002f3c
    // 0x1000063c: the table index of the character at `text`, which moves past it.
    uint32_t GetIndex(const unsigned char*& text) const;
    uint8_t mFirstLead;   // Mac +8
    uint16_t mCount;      // Mac +10
};

// Multibyte to Unicode: a table of UTF-16 values by character index. Mac size 16, vtable
// 0x100111c8; the table follows the object in its allocation.
class TbIMBToUnicode : public TbIMBInfo {
public:
    TB_RESOURCE_IN_ARRAY_BLOCK
    TbIMBToUnicode(uint8_t firstLead = 0, uint16_t count = 0, void* table = nullptr)   // 0x10003204
        : TbIMBInfo(firstLead, count), mTable(static_cast<uint16_t*>(table)) {}
    ~TbIMBToUnicode() override;                             // 0x100032a8
    // 0x10003338: at most `size` characters and the terminator.
    TbError ConvertString(char16_t* out, const unsigned char* source, uint16_t size);
    uint16_t* mTable;     // Mac +0xc, host-order words
};

// Unicode to multibyte: a tree of character ranges. Mac size 12, vtable 0x1001142c; the
// tree follows the object in its allocation, as host-order 16-bit words.
class TbIUnicodeToMB : public TbResource {
public:
    TB_RESOURCE_IN_ARRAY_BLOCK
    explicit TbIUnicodeToMB(void* data = nullptr) : mData(static_cast<uint16_t*>(data)) {}   // 0x100055fc
    ~TbIUnicodeToMB() override;                             // 0x10005674
    uint16_t GetMBValue(char16_t value) { return LocateValue(value); }   // 0x100056f8
    uint16_t LocateValue(char16_t value);                   // 0x10005750
    TbError ConvertString(unsigned char* out, const char16_t* source, uint16_t size);   // 0x100059ac
    uint16_t* mData; // Mac +8
};
