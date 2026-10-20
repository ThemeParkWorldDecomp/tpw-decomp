// String helpers of the main app (0x10005354..0x1000585c; src/TPString.cpp). The
// original file is unknown (the inventory files the range under 3DFont.cpp); the name is
// invented.
#pragma once
#include <cstdint>

#include "TbDynamicStringTemplate.h"

bool TPStrGtr(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b);   // 0x10005354: a > b, ignoring case
bool TPStrEq(const TbDynamicStringTemplate<char>& a, const TbDynamicStringTemplate<char>& b);    // 0x100053f4: equal, ignoring case
bool TPUniStrEq(const TbDynamicStringTemplate<char16_t>& a, const TbDynamicStringTemplate<char16_t>& b);   // 0x1000548c
// 0x10005524: removes `extension` from the end of `name` when it ends with it (ignoring
// case); returns `name`.
TbDynamicStringTemplate<char>& RemoveExtension(const TbDynamicStringTemplate<char>& extension,
                                                TbDynamicStringTemplate<char>& name);
// 0x10005670: `out` = `in` converted to the system code page; the characters written
// (with the NUL), 0 when the conversion fails.
int32_t UniStringToString(TbDynamicStringTemplate<char>& out, const TbDynamicStringTemplate<char16_t>& in);
// 0x100057a0: the reverse conversion.
int32_t StringToUniString(TbDynamicStringTemplate<char16_t>& out, const TbDynamicStringTemplate<char>& in);
