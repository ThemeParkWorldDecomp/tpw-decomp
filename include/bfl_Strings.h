#pragma once
#include "TbError.h"
#include "TbDynamicStringTemplate.h"

// Unit w222 (src/bfl_Strings.cpp), Mac wchar_t is UTF-16.
namespace bfl::Strings {
TbError Convert(char16_t* dest, uint32_t size, const char* source); // 0x100662c8
TbError Convert(TbDynamicStringTemplate<char16_t>& dest, const char* source); // 0x10066360
TbError Convert(char* dest, uint32_t size, const char16_t* source); // 0x100664ac
TbError Convert(TbDynamicStringTemplate<char>& dest, const char16_t* source); // 0x1006654c
}
