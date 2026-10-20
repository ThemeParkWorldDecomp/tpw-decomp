// Bullfrog string members, unit w225. Mac wchar_t is a 16-bit code unit.
#include "TbDynamicStringTemplate.h"
#include "platform/msl.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

namespace {
// Host support for the MSL imports that have no char16_t entry point.
int32_t wideCompare(const char16_t* a, const char16_t* b) {
    while (*a != 0 && *a == *b) { ++a; ++b; }
    return int32_t(*a) - int32_t(*b);
}
int32_t wideToInt(const char16_t* p) {
    while (*p == ' ' || (*p >= '\t' && *p <= '\r')) ++p;
    bool negative = *p == '-';
    if (*p == '+' || *p == '-') ++p;
    uint32_t value = 0;
    while (*p >= '0' && *p <= '9') value = value * 10 + (*p++ - '0');
    return static_cast<int32_t>(negative ? 0u - value : value);
}

// Parse directives here so 'long' remains 32 bits and wide pointers remain 16 bits.
// Like platform/msl.cpp's swprintf, wide %s widens narrow bytes and %ls reads UTF-16.
// The narrow entry has vsnprintf's would-have-written result; the wide entry returns -1 on truncation.
template<class Char> int32_t macVsnprintf(Char* dst, uint32_t size, const Char* format, va_list source) {
    va_list args;
    va_copy(args, source);
    uint32_t total = 0;
    auto put = [&](Char c) {
        if (size != 0 && total < size - 1) dst[total] = c;
        ++total;
    };
    for (const Char* p = format; *p != 0;) {
        if (*p != '%') { put(*p++); continue; }
        ++p;
        if (*p == '%') { put('%'); ++p; continue; }
        std::string flags;
        while (*p && std::strchr("-+ #0", int(*p))) flags += char(*p++);
        bool left = flags.find('-') != std::string::npos;
        int width = 0;
        if (*p == '*') {
            width = va_arg(args, int);
            ++p;
            if (width < 0) { left = true; flags += '-'; width = -width; }
        } else while (*p >= '0' && *p <= '9') width = width * 10 + (*p++ - '0');
        int precision = -1;
        if (*p == '.') {
            ++p;
            precision = 0;
            if (*p == '*') { precision = va_arg(args, int); ++p; }
            else while (*p >= '0' && *p <= '9') precision = precision * 10 + (*p++ - '0');
        }
        Char modifier = 0;
        if (*p == 'l' || *p == 'h' || *p == 'N' || *p == 'L' || *p == 'F') modifier = *p++;
        const char conversion = char(*p++);
        if (conversion == 's' || conversion == 'S' || conversion == 'c' || conversion == 'C') {
            const char* narrow = nullptr;
            const char16_t* wide = nullptr;
            uint32_t length;
            uint32_t character = 0;
            if (conversion == 'c' || conversion == 'C') {
                character = va_arg(args, int);
                length = 1;
            } else if (modifier == 'l' || (conversion == 'S' && modifier != 'h')) {
                wide = va_arg(args, const char16_t*);
                if (!wide) wide = u"(null)";
                length = wcslen(wide);
            } else {
                narrow = va_arg(args, const char*);
                if (!narrow) narrow = "(null)";
                length = std::strlen(narrow);
            }
            if (conversion == 's' || conversion == 'S') {
                if (precision >= 0) length = std::min(length, uint32_t(precision));
            }
            uint32_t padding = width > 0 && uint32_t(width) > length ? uint32_t(width) - length : 0;
            if (!left) for (uint32_t i = 0; i < padding; ++i) put(' ');
            for (uint32_t i = 0; i < length; ++i)
                put(Char(wide ? wide[i] : narrow ? uint8_t(narrow[i]) : character));
            if (left) for (uint32_t i = 0; i < padding; ++i) put(' ');
        } else {
            std::string spec = "%" + flags;
            if (width > 0) spec += std::to_string(width);
            if (precision >= 0) spec += "." + std::to_string(precision);
            spec += conversion;
            auto number = [&](auto value) {
                char local[256];
                const int n = std::snprintf(local, sizeof local, spec.c_str(), value);
                char* text = local;
                if (n >= int(sizeof local)) {
                    text = static_cast<char*>(std::malloc(size_t(n) + 1));
                    std::snprintf(text, size_t(n) + 1, spec.c_str(), value);
                }
                for (int i = 0; i < n; ++i) put(Char(uint8_t(text[i])));
                if (text != local) std::free(text);
            };
            if (std::strchr("eEfFgG", conversion)) {
                number(va_arg(args, double));
            } else if (conversion == 'd' || conversion == 'i') {
                int value = va_arg(args, int);
                if (modifier == 'h') value = int16_t(value);
                number(value);
            } else if (std::strchr("ouxX", conversion)) {
                unsigned value = va_arg(args, unsigned);
                if (modifier == 'h') value = uint16_t(value);
                number(value);
            } else if (conversion == 'p') {
                number(va_arg(args, void*));
            } else if (conversion == 'n') {
                if (modifier == 'h') *va_arg(args, int16_t*) = total;
                else *va_arg(args, int32_t*) = total;
            } else {
                put('%');
                put(Char(conversion));
            }
        }
    }
    va_end(args);
    if (size) dst[std::min(total, size - 1)] = 0;
    if constexpr (std::is_same_v<Char, char16_t>) {
        if (size == 0 || total >= size) return -1;
    }
    return static_cast<int32_t>(total);
}
} // namespace

template<class Char> int32_t TbStringBase<Char>::SBCstrcmp(const Char* a, const Char* b) const {
    if constexpr (std::is_same_v<Char, char>) return std::strcmp(a, b);
    else return wideCompare(a, b);
}
template<class Char> int32_t TbStringBase<Char>::SBCstrncmp(const Char* a, const Char* b, uint32_t count) const {
    if constexpr (std::is_same_v<Char, char>) return std::strncmp(a, b, count);
    else return wcsncmp(a, b, count);
}
template<class Char> const Char* TbStringBase<Char>::SBCstrstr(const Char* text, const Char* needle) const {
    if constexpr (std::is_same_v<Char, char>) return std::strstr(text, needle);
    else return wcsstr(text, needle);
}
template<class Char> int32_t TbStringBase<Char>::SBCstrtoint(Char* text) {
    if constexpr (std::is_same_v<Char, char>) return std::atoi(text);
    else return wideToInt(text);
}
template<class Char> int32_t TbStringBase<Char>::SBCisdigit(Char ch) {
    const uint32_t c = std::is_same_v<Char, char> ? uint8_t(ch) : uint16_t(ch);
    return c >= '0' && c <= '9' ? 0x10 : 0;
}
template<class Char> int32_t TbStringBase<Char>::SBCvsprintf(Char* dst, const Char* fmt, va_list args) {
    return macVsnprintf(dst, 0x7fffffff, fmt, args);
}
template<class Char> int32_t TbStringBase<Char>::SBCvsnprintf(Char* dst, uint32_t count, const Char* fmt, va_list args) {
    return macVsnprintf(dst, count, fmt, args);
}
template<class Char> bool TbStringBase<Char>::operator==(const Char& ch) const {
    return uint32_t(mLength) < 2 && mData[0] == ch;
}
template<class Char> bool TbStringBase<Char>::operator!=(const Char& ch) const { return !(*this == ch); }
template<class Char> bool TbStringBase<Char>::operator!=(const TbStringBase& other) const {
    return mLength != other.mLength || SBCstrncmp(mData, other.mData, mLength) != 0;
}
template<class Char> bool TbStringBase<Char>::operator<(const Char& ch) const {
    if constexpr (std::is_same_v<Char, char>) return int8_t(mData[0]) < int8_t(ch);
    else return mData[0] < ch;
}
template<class Char> bool TbStringBase<Char>::operator>(const Char& ch) const {
    if constexpr (std::is_same_v<Char, char>)
        return uint32_t(mLength) <= 1 ? int8_t(mData[0]) > int8_t(ch) : int8_t(mData[0]) >= int8_t(ch);
    else return uint32_t(mLength) <= 1 ? mData[0] > ch : mData[0] >= ch;
}
template<class Char> bool TbStringBase<Char>::operator<(const Char* text) const {
    if (!text) return false;
    int32_t result = SBCstrncmp(mData, text, mLength);
    return result == 0 ? uint32_t(mLength) < SBCstrlen(text) : result < 0;
}
template<class Char> bool TbStringBase<Char>::operator>(const Char* text) const {
    if (!text) return true;
    int32_t result = SBCstrncmp(mData, text, mLength);
    return result == 0 ? uint32_t(mLength) > SBCstrlen(text) : result > 0;
}
template<class Char> bool TbStringBase<Char>::operator>(const TbStringBase& other) const {
    uint32_t n = std::min(uint32_t(mLength), uint32_t(other.mLength));
    int32_t result = SBCstrncmp(mData, other.mData, n);
    return result == 0 ? uint32_t(mLength) > uint32_t(other.mLength) : result > 0;
}
template<class Char> int32_t TbStringBase<Char>::FindReverse(const Char* text, uint32_t start) const {
    if (!text || mLength == 0) return -1;
    uint32_t n = SBCstrlen(text);
    if (start == 0xffffffff || start > uint32_t(mLength)) start = uint32_t(mLength) - n;
    if (n >= uint32_t(mLength) || start > uint32_t(mLength)) return -1;
    do {
        if (mData[start] == text[0] && SBCstrncmp(mData + start, text, n) == 0) return start;
    } while (start-- != 0);
    return -1;
}
template<class Char> TbDynamicStringTemplate<Char>::TbDynamicStringTemplate(const Char& ch) {
    AllocBuffer(1);
    mData[0] = ch;
}
template<class Char> void TbDynamicStringTemplate<Char>::Format(const Char* format, ...) {
    if (!format) { ResizeTo(0); return; }
    va_list args;
    va_start(args, format);
    FormatV(format, args);
    va_end(args);
}
template<class Char> void TbDynamicStringTemplate<Char>::FormatV(const Char* format, va_list args) {
    va_list scan;
    va_copy(scan, args);
    uint32_t total = 0;
    for (const Char* p = format; *p != 0; ++p) {
        if (*p != '%' || *++p == '%') { ++total; continue; }
        uint32_t width = 0;
        uint32_t precision = 0;
        while (*p != 0) {
            if (*p == '#') total += 2;
            else if (*p == '*') width = va_arg(scan, int);
            else if (*p != '-' && *p != '+' && *p != '0' && *p != ' ') break;
            ++p;
        }
        if (width == 0) {
            width = this->SBCstrtoint(const_cast<Char*>(p));
            while (*p && this->SBCisdigit(*p)) ++p;
        }
        if (*p == '.') {
            ++p;
            if (*p == '*') { precision = va_arg(scan, int); ++p; }
            else {
                precision = this->SBCstrtoint(const_cast<Char*>(p));
                while (*p && this->SBCisdigit(*p)) ++p;
            }
        }
        uint32_t flag = 0;
        if (*p == 'h') { flag = 0x10000; ++p; }
        else if (*p == 'l') { flag = 0x20000; ++p; }
        else if (*p == 'N' || *p == 'L' || *p == 'F') ++p;
        const int32_t ch = std::is_same_v<Char, char> ? int8_t(*p) : uint16_t(*p);
        const uint32_t key = ch | flag;
        uint32_t length = 0;
        if (key == ('c' | flag) || key == ('C' | flag)) {
            (void)va_arg(scan, int);
            length = 2;
        } else if (key == ('s' | flag) || key == ('S' | flag)) {
            const void* value = va_arg(scan, const void*);
            if (!value) length = 6;
            else {
                if (flag == 0x10000) length = std::strlen(static_cast<const char*>(value));
                else if (flag == 0x20000) length = wcslen(static_cast<const char16_t*>(value));
                else length = this->SBCstrlen(static_cast<const Char*>(value));
                // All six image words 0x1007234c..0x10072360 are 1.
                length = std::max(1u, length);
            }
        }
        if (length != 0) {
            length = std::max(length, width);
            if (precision != 0) length = std::min(length, precision);
        } else {
            switch (ch) {
            case 'G': case 'e': case 'f': case 'g':
                (void)va_arg(scan, double);
                length = std::max(0x80u, width + precision);
                break;
            case 'X': case 'd': case 'i': case 'o': case 'u': case 'x':
                (void)va_arg(scan, int);
                length = std::max(0x20u, width + precision);
                break;
            case 'p':
                (void)va_arg(scan, const void*);
                length = std::max(0x20u, width + precision);
                break;
            case 'n': (void)va_arg(scan, void*); break;
            }
        }
        total += length;
    }
    va_end(scan);
    Char* buffer = new Char[total + 1];
    int32_t length = this->SBCvsnprintf(buffer, total + 1, format, args);
    buffer[length] = 0;
    ResizeTo(length);
    this->SBCstrcpy(mData, buffer);
    delete[] buffer;
    mData[length] = 0;
    mLength = length;
}
template<class Char> void TbStaticStringTemplate<Char>::Format(const Char* format, ...) {
    if (!format) { ResizeTo(0); return; }
    va_list args;
    va_start(args, format);
    mLength = this->SBCvsnprintf(mData, mCapacity, format, args);
    va_end(args);
}

#define INSTANTIATE(C) \
    template int32_t TbStringBase<C>::SBCstrcmp(const C*, const C*) const; \
    template int32_t TbStringBase<C>::SBCstrncmp(const C*, const C*, uint32_t) const; \
    template const C* TbStringBase<C>::SBCstrstr(const C*, const C*) const; \
    template int32_t TbStringBase<C>::SBCstrtoint(C*); \
    template int32_t TbStringBase<C>::SBCisdigit(C); \
    template int32_t TbStringBase<C>::SBCvsprintf(C*, const C*, va_list); \
    template int32_t TbStringBase<C>::SBCvsnprintf(C*, uint32_t, const C*, va_list); \
    template bool TbStringBase<C>::operator==(const C&) const; \
    template bool TbStringBase<C>::operator!=(const C&) const; \
    template bool TbStringBase<C>::operator!=(const TbStringBase&) const; \
    template bool TbStringBase<C>::operator<(const C&) const; \
    template bool TbStringBase<C>::operator<(const C*) const; \
    template bool TbStringBase<C>::operator>(const C&) const; \
    template bool TbStringBase<C>::operator>(const C*) const; \
    template bool TbStringBase<C>::operator>(const TbStringBase&) const; \
    template int32_t TbStringBase<C>::FindReverse(const C*, uint32_t) const; \
    template TbDynamicStringTemplate<C>::TbDynamicStringTemplate(const C&); \
    template void TbDynamicStringTemplate<C>::Format(const C*, ...); \
    template void TbDynamicStringTemplate<C>::FormatV(const C*, va_list);
INSTANTIATE(char)
INSTANTIATE(char16_t)
#undef INSTANTIATE

template void TbStaticStringTemplate<char>::Format(const char*, ...);
