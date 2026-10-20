// ltms: the 4-bit run-length font (the only font format the game ships). Mac size 0x18,
// vtable 0x10011334; the font file follows the object in its allocation.
//
// File (little-endian; swapped in place by the constructor on the Mac): u32 "F4FB", a
// byte, the line height (byte 5), u16 character count, then one u32 per character: the
// offset from the start of the file of its record, a 0x18-byte header followed by the
// run-length data. The header's width is the u16 at 0x16.
#pragma once
#include "TbResource.h"

class TbIResourceIndex;

class TbIRLE4bitFont : public TbIFont {
public:
    TB_RESOURCE_IN_ARRAY_BLOCK
    TbIRLE4bitFont();                                              // 0x100040d8
    TbIRLE4bitFont(unsigned char* data, TbIResourceIndex* index);  // 0x100041e4
    ~TbIRLE4bitFont() override;                                    // 0x10004150
    uint32_t GetCharIndex(const unsigned char*& text) override;    // 0x1000442c
    uint16_t GetCharCount() override;                              // 0x100044b4
    TbFontCharSize GetCharSize(uint16_t index) override;           // 0x10004508
    uint8_t GetVerticalSpacing() override;                         // 0x10004590
    uint32_t GetCaps() override;                                   // 0x10004638
    uint32_t GetType() override;                                   // 0x100045ec
    TbIMBToUnicode* GetMBToUnicode() override;                     // 0x100048ec
    const unsigned char* GetCharData(uint16_t index) const;        // 0x10004684
    const unsigned char* GetCharHeader(uint16_t index) const;      // 0x100046f8

    unsigned char* mData;           // Mac +8: the file
    unsigned char* mHeader;         // Mac +0xc: the same address
    uint32_t* mOffsets;             // Mac +0x10: the file + 8
    TbIMBToUnicode* mMBToUnicode;   // Mac +0x14: the language's converter, held with a reference
};
