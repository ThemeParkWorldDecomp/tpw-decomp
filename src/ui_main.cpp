// ui_main.cpp (main app of SimTheme Park 2.04b3). PARTIAL: UIMain_ConvertMulti
// (0x10180334 and 0x101803cc, the same code for a const and a non-const argument) and UIMain_ConvertString (0x10181a7c) so far.
#include "ui_main.h"

#include "TbResource.h"
#include "Text.h"

char16_t gUIMainConvertBuffer[0x4000];   // 0x10380c48 (name invented; 0x8000 bytes: the next global is at 0x10388c48)

// A multibyte text as Unicode, in one shared buffer of 0x200 characters; NULL for NULL.
// The text is converted only when the language's Unicode-to-multibyte converter is
// loaded (resource 1), though it is the multibyte-to-Unicode one (resource 0) that does
// the work.
char16_t* UIMain_ConvertMulti(const unsigned char* text) {
    if (text == nullptr) return nullptr;
    if (gText.mResourcePointers[1] != nullptr) {
        static_cast<TbIMBToUnicode*>(gText.mResourcePointers[0])->ConvertString(gUIMainConvertBuffer, text, 0x200);
    }
    return gUIMainConvertBuffer;
}

char16_t* UIMain_ConvertMulti(unsigned char* text) { return UIMain_ConvertMulti(static_cast<const unsigned char*>(text)); }

uint8_t gUIMainMBBuffer[0x4000] = {};   // 0x10388c48 (name invented; the multibyte mirror image of the buffer above)

// 0x10181a7c, the one function of that name (it takes a char16_t*; the const overload in UI_drawtext.cpp forwards to it). The text in the
// language's code page, in gUIMainMBBuffer: converted only when the Unicode to multibyte converter (resource 1) is loaded, otherwise the
// buffer keeps whatever the last conversion left in it. Answers the buffer either way (a NULL text goes to the converter as it is).
uint8_t* UIMain_ConvertString(char16_t* text) {
    if (gText.mResourcePointers[1] != nullptr) {
        static_cast<TbIUnicodeToMB*>(gText.mResourcePointers[1])->ConvertString(gUIMainMBBuffer, text, 0x3fff);
    }
    return gUIMainMBBuffer;
}
