// PARTIAL InterfaceFontWrapper. Only the inline default constructor is translated.
// Weak placeholders of these virtual classes must go when their owners are translated
// to avoid emitting their vtables twice.
#pragma once
#include "InterfaceFont.h"
class TbIFont;
class InterfaceFontWrapper : public InterfaceFont {
public:
    InterfaceFontWrapper() : InterfaceFontWrapper(nullptr) {} // 0x1015ef00
    explicit InterfaceFontWrapper(TbIFont* font); // 0x10168ae0
    ~InterfaceFontWrapper() override;            // 0x10168b68
    // Unit v51029. The width of the characters first..last-1 of `text` (stops at a NUL): the sum of the virtual
    // character width (vtable slot +0x14 of the vptr at +0x10), as a short. 0x10168c7c.
    int32_t GetWidth(const char16_t* text, int16_t first, int16_t last);
    // 0x10168d38: the index of the last character that fits in `last` pixels (index - 1 after the loop).
    int32_t LastCharIn(const char16_t* text, int16_t first, int16_t last);
    TbIFont* mFont;                              // 0x14, original size 0x18
    // Unit v51039: 0x10168e04 (152, stand-in): 0 without a font; else the font's width for the character: the Unicode to multibyte
    // value (gText's resource 1), its index (the font's virtual at +0x1c) and its size (virtual +0x24), the width cut to 16 bits, signed.
    int32_t GetCharWidth(char16_t character);
    // Unit v51118: 0x10168c04 (64): 0 without a font, else the font's virtual at +0x28 (the vertical spacing, a byte).
    int32_t GetHeight();
};
#ifndef TPW_V51039_STRONG
__attribute__((weak)) inline int32_t InterfaceFontWrapper::GetCharWidth(char16_t) { return 0; }
#endif
struct TbFontCharSize;
namespace fontwrapper_callee {
uint16_t GetMBValue(void* converter, char16_t character);   // 0x1023b9c8 TbIUnicodeToMB::GetMBValue
uint32_t GetCharIndex(TbIFont* font, const unsigned char*& text);   // the virtual at vtable +0x1c (through __ptr_glue)
uint32_t GetCharWidthOf(TbIFont* font, uint16_t index);   // the virtual at vtable +0x24: the size's width (its first word)
uint8_t GetVerticalSpacing(TbIFont* font);   // v51118: the virtual at vtable +0x28 (weak default in the src file, a fake in the test)
}
