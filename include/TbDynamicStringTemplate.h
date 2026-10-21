// Bullfrog strings. Mac wchar_t is UTF-16, represented by char16_t here.
// TbStringBase is a borrowed view (16 bytes on Mac), with no virtual destructor.
// Mac slots: Format 8, ResizeTo c. Derived classes add the slots documented below.
#pragma once
#include <cstdint>
#include <cstdarg>

template<class Char> class TbDynamicStringTemplate;

template<class Char> class TbStringBase {
public:
    TbStringBase() : mLength(0), mData(nullptr) {}
    TbStringBase(Char* data, uint32_t length) : mLength(length), mData(data), mCapacity(0) {}
    virtual void Format(const Char*, ...) {} // w225: char 0x100137b0, wide 0x10013800
    virtual void ResizeTo(uint32_t) {} // w225: char 0x10011e20, wide 0x10012604
    int32_t Length() const;
    TbStringBase Right(uint32_t count) const;
    TbStringBase Left(uint32_t count) const;
    TbStringBase Mid(uint32_t start, uint32_t count) const;
    TbStringBase Mid(uint32_t start) const;
    bool operator==(const TbStringBase& other) const;
    bool operator==(const Char* text) const;
    bool operator!=(const Char* text) const;
    bool operator<(const TbStringBase& other) const;
    int32_t FindReverseOneOf(const Char* set, uint32_t start = 0x7fffffff) const;
    int32_t FindOneOf(const Char* set, uint32_t start = 0) const;
    int32_t FindReverse(const Char& ch, uint32_t start = 0x7fffffff) const;
    int32_t Find(const Char& ch, uint32_t start = 0) const;
    int32_t Find(const Char* text, uint32_t start = 0) const;
    Char* LockBuffer(uint32_t length); // Unit w222: wide copy 0x10011e58
    void ReleaseBuffer(uint32_t length = 0x7fffffff); // Unit w222: wide copy 0x10012e2c
    uint32_t SBCstrlen(const Char* text) const;
    Char* SBCstrcpy(Char* dst, const Char* src);
    Char* SBCstrncpy(Char* dst, const Char* src, uint32_t count);
    Char* SBCstrcat(Char* dst, const Char* src);
    Char* SBCstrncat(Char* dst, const Char* src, uint32_t count);
    // Unit w225 (src/bullfrog/FileSystem.cpp), char / wide addresses.
    bool operator==(const Char& ch) const; // 0x100116a4 / 0x10011ec8
    bool operator!=(const Char& ch) const; // 0x10011864 / 0x10012080
    bool operator!=(const TbStringBase& other) const; // 0x100119b0 / 0x100121c4
    bool operator<(const Char& ch) const; // 0x10011a48 / 0x1001225c
    bool operator<(const Char* text) const; // 0x10011aa4 / 0x100122a4
    bool operator>(const Char& ch) const; // 0x10011b70 / 0x10012370
    bool operator>(const Char* text) const; // 0x10011c00 / 0x100123e0
    bool operator>(const TbStringBase& other) const; // 0x10013508 / 0x10012c64
    int32_t FindReverse(const Char* text, uint32_t start) const; // 0x10011d04 / 0x100124e4
    int32_t SBCstrcmp(const Char* a, const Char* b) const; // 0x10010df8 / 0x100112a0
    int32_t SBCstrncmp(const Char* a, const Char* b, uint32_t count) const; // 0x10010e5c / 0x10011304
    const Char* SBCstrstr(const Char* text, const Char* needle) const; // 0x10010ec8 / 0x10011370
    int32_t SBCstrtoint(Char* text); // 0x10010ff8 / 0x100114a0
    int32_t SBCisdigit(Char ch); // 0x10011054 / 0x100114fc
    int32_t SBCvsprintf(Char* dst, const Char* format, va_list args); // 0x1001109c / 0x10011558
    int32_t SBCvsnprintf(Char* dst, uint32_t count, const Char* format, va_list args); // 0x10011108 / 0x100115c4
    int32_t mLength;        // 0x04
    Char* mData;            // 0x08
    int32_t mCapacity;      // 0x0c; unspecified in Mac views, zero on host
};

template<class Char> class TbDynamicStringTemplate : public TbStringBase<Char> {
public:
    using TbStringBase<Char>::mLength;
    using TbStringBase<Char>::mData;
    using TbStringBase<Char>::mCapacity;
    TbDynamicStringTemplate();
    explicit TbDynamicStringTemplate(const Char* text);
    TbDynamicStringTemplate(const TbDynamicStringTemplate& other);
    TbDynamicStringTemplate(const TbStringBase<Char>& other);
    // Unit w225 (src/bullfrog/FileSystem.cpp).
    explicit TbDynamicStringTemplate(const Char& ch); // 0x10010414 / 0x1000f978
    void FormatV(const Char* format, va_list args); // 0x1000fabc / 0x1000f028
    void Format(const Char*, ...) override;                  // 0x1000e810 / 0x1000ef84, slot 8
    void ResizeTo(uint32_t length) override;                 // 0x0c, outside u11
    virtual void AllocBuffer(uint32_t length);               // 0x10, outside u11
    virtual ~TbDynamicStringTemplate();                     // 0x14
    virtual TbDynamicStringTemplate& operator=(const TbStringBase<Char>& other);
    virtual TbDynamicStringTemplate& operator=(const TbDynamicStringTemplate& other);
    virtual TbDynamicStringTemplate& operator=(const Char* text);
    virtual TbDynamicStringTemplate& operator=(const Char& ch);
    virtual TbDynamicStringTemplate& operator+=(const TbStringBase<Char>& text);
    virtual TbDynamicStringTemplate& operator+=(const Char& ch);
    virtual TbDynamicStringTemplate& operator+=(const Char* text);
    int32_t mGrowBy;        // 0x10
};

template<class Char> class TbStaticStringTemplate : public TbStringBase<Char> {
public:
    using TbStringBase<Char>::mLength;
    using TbStringBase<Char>::mData;
    using TbStringBase<Char>::mCapacity;
    TbStaticStringTemplate(Char* buffer, uint32_t capacity, int32_t clear);
    void Format(const Char*, ...) override;                  // w225: 0x10010b10, slot 8
    void ResizeTo(uint32_t length) override;                 // 0x0c, outside u11
    virtual ~TbStaticStringTemplate();                      // 0x10
    virtual TbStaticStringTemplate& operator=(const TbStringBase<Char>& other);
    virtual TbStaticStringTemplate& operator=(const Char* text);
    virtual TbStaticStringTemplate& operator=(const Char& ch);
    virtual TbStaticStringTemplate& operator+=(const TbStringBase<Char>& text);
    virtual TbStaticStringTemplate& operator+=(const Char& ch);
    virtual TbStaticStringTemplate& operator+=(const Char* text);
    int32_t mTruncated;     // 0x10
};

// Unit w217 (src/bullfrog/TbTimeStamp.cpp), instantiated for char and char16_t only.
class TbTimeStamp;
namespace NS_MacDoze { struct _SYSTEMTIME; }
// char 0x10027f88, 3,536 B; wchar_t 0x10027164, 3,540 B.
template<class Char> void ProcessFormatTag(Char** out, int32_t* room, const Char** format,
                                         NS_MacDoze::_SYSTEMTIME time);
// char 0x10028da8, 416 B; wchar_t 0x10028fc0, 360 B.
template<class Str, class Char> int32_t TemplatedStaticFormat(Str* text, const Char* format,
                                                           const TbTimeStamp* stamp);
// char 0x10026f7c, 364 B; wchar_t 0x10026dc0, 320 B.
template<class Str, class Char> int32_t TemplatedDynamicFormat(Str* text, const Char* format,
                                                            const TbTimeStamp* stamp);
