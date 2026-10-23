// msl.cpp: the pieces of MSL (CodeWarrior's C library) the game depends on that the host C
// library does not give in the same form: the Mac's wchar_t is 16 bits, so its wide-string
// functions work on char16_t here, and code that indexes MSL's case tables directly gets
// the tables.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "platform/msl.h"
#include "log_file.h"   // the failure reports go to the log file too

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ::tpw::LogRawF("Assertion (%s) failed in \"%s\", line %d\n", condition, file, line);
    std::abort();
}

static uint32_t gRandState = 1;

int32_t msl_rand() {
    gRandState = gRandState * 0x41c64e6du + 0x3039u;
    return static_cast<int32_t>((gRandState >> 16) & 0x7fff);
}

void msl_srand(uint32_t seed) { gRandState = seed; }

// "C" locale: a to z map to A to Z (and back), every other byte to itself.
#define ROW(b) b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7, b + 8, b + 9, b + 10, b + 11, b + 12, b + 13, b + 14, b + 15
const unsigned char msl_upper_map[256] = {
    ROW(0x00), ROW(0x10), ROW(0x20), ROW(0x30), ROW(0x40), ROW(0x50),
    0x60, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    ROW(0x80), ROW(0x90), ROW(0xa0), ROW(0xb0), ROW(0xc0), ROW(0xd0), ROW(0xe0), ROW(0xf0)
};
const unsigned char msl_lower_map[256] = {
    ROW(0x00), ROW(0x10), ROW(0x20), ROW(0x30),
    0x40, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    ROW(0x60), ROW(0x70),
    ROW(0x80), ROW(0x90), ROW(0xa0), ROW(0xb0), ROW(0xc0), ROW(0xd0), ROW(0xe0), ROW(0xf0)
};
#undef ROW

uint32_t wcslen(const char16_t* text) {
    uint32_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    char16_t* out = dst;
    while ((*out++ = *src++) != 0) {}
    return dst;
}

// As strncpy: stops after `count` characters and pads a shorter source with NULs.
char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t count) {
    uint32_t i = 0;
    for (; i < count && src[i] != 0; ++i) dst[i] = src[i];
    for (; i < count; ++i) dst[i] = 0;
    return dst;
}

char16_t* wcscat(char16_t* dst, const char16_t* src) {
    wcscpy(dst + wcslen(dst), src);
    return dst;
}

// As strncat: appends at most `count` characters and always terminates.
char16_t* wcsncat(char16_t* dst, const char16_t* src, uint32_t count) {
    char16_t* out = dst + wcslen(dst);
    for (uint32_t i = 0; i < count && src[i] != 0; ++i) *out++ = src[i];
    *out = 0;
    return dst;
}

int32_t wcsncmp(const char16_t* a, const char16_t* b, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        if (a[i] == 0) break;
    }
    return 0;
}

const char16_t* wcsstr(const char16_t* text, const char16_t* needle) {
    const uint32_t length = wcslen(needle);
    if (length == 0) return text;
    for (; *text != 0; ++text)
        if (wcsncmp(text, needle, length) == 0) return text;
    return nullptr;
}

// ---- swprintf. A conversion is parsed as the C library does (flags, width, precision); the number conversions go to snprintf
// and the string conversions are padded here. Nothing is allocated: the text goes straight into the buffer, and only its length
// is counted past the end.
namespace {
struct WideSink {
    char16_t* buffer;
    uint32_t room;    // characters that fit before the terminator
    uint32_t total;   // characters the whole text has
    void put(char16_t ch) {
        if (total < room) buffer[total] = ch;
        ++total;
    }
};
}  // namespace

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    WideSink sink{buffer, size > 0 ? size - 1 : 0, 0};
    va_list args;
    va_start(args, format);
    for (const char16_t* p = format; *p != 0;) {
        if (*p != u'%') {
            sink.put(*p++);
            continue;
        }
        ++p;
        if (*p == u'%') {
            sink.put(u'%');
            ++p;
            continue;
        }
        char flags[8];
        uint32_t flagCount = 0;
        while ((*p == u'-' || *p == u'+' || *p == u' ' || *p == u'#' || *p == u'0') && flagCount < 7) flags[flagCount++] = static_cast<char>(*p++);
        flags[flagCount] = 0;
        bool left = std::strchr(flags, '-') != nullptr;
        int width = 0;
        if (*p == u'*') {
            width = va_arg(args, int);
            ++p;
            if (width < 0) {
                left = true;
                width = -width;
            }
        } else {
            while (*p >= u'0' && *p <= u'9') width = width * 10 + (*p++ - u'0');
        }
        int precision = -1;
        if (*p == u'.') {
            ++p;
            precision = 0;
            if (*p == u'*') {
                precision = va_arg(args, int);
                ++p;
            } else {
                while (*p >= u'0' && *p <= u'9') precision = precision * 10 + (*p++ - u'0');
            }
        }
        bool longModifier = false;
        while (*p == u'l' || *p == u'h' || *p == u'L') longModifier |= *p++ == u'l';
        const char16_t conversion = *p;
        if (conversion == 0) break;
        ++p;
        if (conversion == u's' || conversion == u'S' || conversion == u'c') {
            // %ls takes a wide string and a plain %s a narrow one, as MSL's wide formatter does (the narrow text is widened a byte at a
            // time, which is what its C locale does: the version text of UI_CreateVersionNumber is one). %S is kept as a wide string.
            // A NULL string prints as "(null)" like the C library.
            char16_t one[2] = {0, 0};
            const char16_t* text = nullptr;
            const char* narrow = nullptr;
            uint32_t length;
            if (conversion == u'c') {
                one[0] = static_cast<char16_t>(va_arg(args, int));
                text = one;
                length = 1;
            } else if (conversion == u's' && !longModifier) {
                narrow = va_arg(args, const char*);
                if (narrow == nullptr) narrow = "(null)";
                length = static_cast<uint32_t>(std::strlen(narrow));
            } else {
                text = va_arg(args, const char16_t*);
                if (text == nullptr) text = u"(null)";
                length = wcslen(text);
            }
            if (conversion != u'c' && precision >= 0 && static_cast<uint32_t>(precision) < length) length = static_cast<uint32_t>(precision);
            const uint32_t pad = static_cast<uint32_t>(width) > length ? static_cast<uint32_t>(width) - length : 0;
            if (!left) for (uint32_t i = 0; i < pad; ++i) sink.put(u' ');
            for (uint32_t i = 0; i < length; ++i) sink.put(narrow != nullptr ? static_cast<char16_t>(static_cast<unsigned char>(narrow[i])) : text[i]);
            if (left) for (uint32_t i = 0; i < pad; ++i) sink.put(u' ');
        } else {
            char spec[32];
            char number[64];
            int n = std::snprintf(spec, sizeof spec, "%%%s", flags);
            if (width > 0) n += std::snprintf(spec + n, sizeof spec - n, "%d", width);
            if (precision >= 0) n += std::snprintf(spec + n, sizeof spec - n, ".%d", precision);
            // %f %F %e %E %g %G take a double (a float argument is promoted to one, as the Mac's was) and go to snprintf in the C locale,
            // like MSL's; before they were read as %x of an int, which is the empty or wrong text the "%.2f %%" of the number text showed.
            const bool real = conversion == u'f' || conversion == u'F' || conversion == u'e' || conversion == u'E' || conversion == u'g' ||
                              conversion == u'G';
            const char letter = real ? static_cast<char>(conversion)
                                : (conversion == u'd' || conversion == u'i') ? 'd' : conversion == u'u' ? 'u' : conversion == u'X' ? 'X' : 'x';
            std::snprintf(spec + n, sizeof spec - n, "%c", letter);
            if (real) std::snprintf(number, sizeof number, spec, va_arg(args, double));
            else if (letter == 'd') std::snprintf(number, sizeof number, spec, va_arg(args, int));
            else std::snprintf(number, sizeof number, spec, va_arg(args, unsigned));
            for (const char* digit = number; *digit != 0; ++digit) sink.put(static_cast<char16_t>(*digit));
        }
    }
    va_end(args);
    if (size == 0) return -1;
    buffer[sink.total < sink.room ? sink.total : sink.room] = 0;
    return sink.total <= sink.room ? static_cast<int32_t>(sink.total) : -1;
}
