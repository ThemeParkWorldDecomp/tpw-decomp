// Weak definitions of per-unit callees that are calls through the cross-fragment glue into the Mac's C library (MSL in "c_c++ symbols
// shared") and NS_MacDoze. Unit v5L49: without them the launcher's generated stand-ins answered 0 (wcslen 0, nothing written).
// Each test library keeps its own recording fake, which wins over these; tests/test_glue_callees.py runs these against the Mac's code.
#include <cstdint>

#include "online.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"

namespace {

// MSL's wcstombs (c_c++ 0x100240f4) as __pformatter's %ls uses it: each wide character gives its low byte, up to `room` bytes; the zero
// that ends the text (a character whose low byte is 0) is stored and ends the copy. Answers the bytes before it.
uint32_t MslWcsToMbs(char* out, const char16_t* text, uint32_t room) {
    uint32_t count = 0;
    while (count < room) {
        const char byte = static_cast<char>(text[count] & 0xff);
        out[count] = byte;
        if (byte == 0) break;
        ++count;
    }
    return count;
}

}  // namespace

// wcslen (c_c++ 0x10033ff0), glue 0x102389c8: 16-bit units up to the zero.
__attribute__((weak)) uint32_t online_parser_callee::WcsLen(const char16_t* text) { return wcslen(text); }
__attribute__((weak)) uint32_t wostream_callee::Wcslen(const char16_t* text) { return wcslen(text); }

// NS_MacDoze::_wcsset (macdoze 0x1000392c), glue 0x102397f0: every character up to the zero becomes `c`.
__attribute__((weak)) void textobj_callee::WcsSet(char16_t* text, char16_t c) {
    for (char16_t* p = text; *p != 0; ++p) *p = c;
}

// sprintf (c_c++ 0x10027a78), glue 0x10238578, for the one format the callers give it, "%ls" (kFormatPath 0x1025135d and SaveBuddy's):
// __pformatter's %ls (0x10027058) converts the text with wcstombs into a 0x200-byte buffer of its own (NULL: the empty text) and copies
// that out, so `buffer` gets the low byte of each character up to the first whose low byte is 0. Other characters of the format are
// copied as they are and "%%" gives '%'. The Mac's buffer has no zero after 0x200 characters (strlen then reads on into its stack); this
// one stops there.
__attribute__((weak)) void ignore_callee::SprintfWide(char* buffer, const char* format, const char16_t* text) {
    static const char16_t kEmpty[1] = {0};
    char* out = buffer;
    for (const char* f = format; *f != 0; ++f) {
        if (f[0] == '%' && f[1] == '%') {
            *out++ = '%';
            ++f;
        } else if (f[0] == '%' && f[1] == 'l' && f[2] == 's') {
            char converted[0x201];
            const uint32_t length = MslWcsToMbs(converted, text != nullptr ? text : kEmpty, 0x200);
            for (uint32_t i = 0; i < length; ++i) *out++ = converted[i];
            f += 2;
        } else {
            *out++ = *f;
        }
    }
    *out = 0;
}
