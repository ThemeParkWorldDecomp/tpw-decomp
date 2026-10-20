// PARTIAL InterfaceFont. SetID's inline copy follows __sinit_Text_cpp.
// Has virtual functions (Mac vptr at +0x10); native C++ supplies its own layout.
#pragma once
#include <cstdint>
class InterfaceFont {
public:
    InterfaceFont();           // 0x10168ed8
    virtual ~InterfaceFont();  // 0x10168f28: frees the bitmap
    void SetID(int32_t id) { mID = id; } // 0x1015eecc
    int32_t GetHeight();       // 0x10168fc0
    void Release();            // 0x10168ff8: frees the bitmap
    // 0x1016906c (180 bytes, src/lobby_start_leaves.cpp): the built-in 8x8 font: a table of `a` entries of 8 bytes (new[]),
    // each with the byte `b`, the byte `e`, two bytes it leaves and a zero word, then mHeight = d (only when the table could
    // be made); mF02 = c in any case.
    void Gen8x8DebugFont(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e);
    // src/UI_drawtext.cpp (unit w35b). The glyph table is mBitmap: 8 bytes a glyph, the first a signed width.
    // The width of `text` from character `first` to `last` (both included, 16-bit characters): the spacing mF02 plus the
    // glyph's width for each. 0 for no text, no table, an empty text, a range outside the text or a range of one character
    // or fewer; a `last` before `first` means the end of the text. The loop reaches the terminator when `last` is the
    // length, and nothing checks a character against the table.
    int32_t GetWidth(const char16_t* text, int16_t first, int16_t last);   // 0x1016915c
    // The spacing plus the glyph's width, or 0 when there is no table or the character is not below mF00.
    int32_t GetCharWidth(char16_t character);   // 0x10169264
    int32_t LastCharIn(const char16_t* text, int16_t first, int16_t last);   // 0x101692d4: a stub that answers 0
    uint16_t mF00;             // 0x00  0 (the number of glyphs in the table, GetCharWidth's limit)
    uint16_t mF02, mF04;       // 0x02, 0x04  2 and 2 (mF02 is the spacing GetWidth adds to every glyph)
    int16_t mHeight;           // 0x06  not set by the constructor
    uint8_t* mBitmap;          // 0x08  an array made with new[]
    int32_t mID;               // 0x0c  not set by the constructor
};

// unit w37b: the old sprite-font selector (separate from CText's InterfaceFont wrappers).
namespace CFont {
enum FontId : int32_t { kFont0 = 0, kFont1 = 1 };
enum RenderType : int32_t { kOneColour = 0, kSprite = 1 };
int32_t Set(FontId font, RenderType render); // 0x1007da14
// The table's Mac stride is 0x18; Set reads only the font pointer at +0x0c.
struct FontEntry {
    uint32_t mF00[3];
    void* mFont;
    uint32_t mF10[2];
};
}
extern CFont::FontEntry* gCFontTable; // 0x102dc1a8, original name unknown
// Bullfrog text renderers and font selection are declared in LbDrawText.h (unit w224).
