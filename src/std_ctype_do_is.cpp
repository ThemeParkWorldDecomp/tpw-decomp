#include "std_ctype_wchar.h"

// 0x102208d4: true when any bit of the mask is set in classify(c)
bool StdCtypeWchar::do_is(uint32_t m, char16_t c) const {
    return (classify(c) & m) != 0;
}
