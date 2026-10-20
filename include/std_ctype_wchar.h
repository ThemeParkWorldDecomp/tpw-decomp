// std::ctype<wchar_t> (size unknown; only do_is is translated)
#pragma once
#include <cstdint>

// Unit v5735
struct StdCtypeWchar {
    uint32_t classify(char16_t c) const;                    // 0x1023b140 (glue import, not translated)
    bool do_is(uint32_t m, char16_t c) const;               // 0x102208d4 (76)
    // Unit v51105: the maps are gWUpperMap and gSwearLowerMap (256 halfwords).
    char16_t do_toupper(char16_t c) const;                  // 0x10229808 (36)
    char16_t do_tolower(char16_t c) const;                  // 0x102297b0 (36)
    char do_narrow(char16_t c, char dflt) const;            // 0x10229760 (28): below 0xff the signed byte, else dflt
};
// Unit v51105: ~ctype 0x10229860 (88 bytes), the Mac's hidden delete flag; answers this. Word 0 (the vtable) ends as 0x1029d2a0.
StdCtypeWchar* CtypeWcharDestruct(StdCtypeWchar* self, int16_t deleteFlag);
