// Bullfrog text drawing (Lbgfsave.cpp and the two LbDraw.cpp rectangle leaves), unit w224.
#include "LbDrawText.h"
#include "LbDraw.h"
#include "LbRect.h"
#include <type_traits>

void* gLbTextFont;
const TbTextRender* gLbTextRender;
int32_t gLbTextSelectSense;
uint32_t gLbCheapTextColour, gLbCheapTextIndex;
uint8_t gLbCheapTextGuard;
uint32_t gLbCheapUnicodeTextColour, gLbCheapUnicodeTextIndex;
uint8_t gLbCheapUnicodeTextGuard;
const TbTextRender _lbSpriteRender = {nullptr, nullptr, SpriteRender_OnMeasure, SpriteRender_OnDrawChar};
const TbTextRender _lbOneColourSpriteRender = {nullptr, nullptr, OneColourSpriteRender_OnMeasure, OneColourSpriteRender_OnDrawChar};

namespace {
bool IsSpace(uint8_t c) { return c == ' ' || (c >= 9 && c <= 13); }
template <typename T> uint32_t Character(T c) {
    if constexpr (std::is_same_v<T, char>) return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(c)));
    else return c;
}
// All layout arithmetic follows the Mac's wrapping 32-bit words.
TbSize Measure(uint32_t ch) {
    TbSize size = {0, 0};
    gLbTextRender->mOnMeasure(gLbTextFont, &size, ch);
    return size;
}
const TbSprite* Sprite(void* font, uint32_t ch) {
    return static_cast<const TbSprite*>(font) + static_cast<int32_t>(ch - 0x20);
}
const uint8_t kCheapGlyphs[0x2e4] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x20
    0x02, 0x02, 0x02, 0x00, 0x02, 0x00,  // 0x21 !
    0x05, 0x05, 0x00, 0x00, 0x00, 0x00,  // 0x22 "
    0x05, 0x07, 0x05, 0x07, 0x05, 0x00,  // 0x23 #
    0x06, 0x03, 0x02, 0x06, 0x03, 0x00,  // 0x24 $
    0x05, 0x04, 0x02, 0x01, 0x05, 0x00,  // 0x25 %
    0x02, 0x05, 0x02, 0x05, 0x06, 0x00,  // 0x26 &
    0x04, 0x02, 0x00, 0x00, 0x00, 0x00,  // 0x27
    0x02, 0x01, 0x01, 0x01, 0x02, 0x00,  // 0x28 (
    0x02, 0x04, 0x04, 0x04, 0x02, 0x00,  // 0x29 )
    0x00, 0x05, 0x02, 0x05, 0x00, 0x00,  // 0x2a *
    0x00, 0x02, 0x07, 0x02, 0x00, 0x00,  // 0x2b +
    0x00, 0x00, 0x00, 0x00, 0x02, 0x01,  // 0x2c ,
    0x00, 0x00, 0x07, 0x00, 0x00, 0x00,  // 0x2d -
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00,  // 0x2e .
    0x00, 0x00, 0x04, 0x02, 0x01, 0x00,  // 0x2f /
    0x07, 0x05, 0x05, 0x05, 0x07, 0x00,  // 0x30 0
    0x02, 0x03, 0x02, 0x02, 0x07, 0x00,  // 0x31 1
    0x03, 0x04, 0x02, 0x01, 0x07, 0x00,  // 0x32 2
    0x03, 0x04, 0x02, 0x04, 0x03, 0x00,  // 0x33 3
    0x01, 0x01, 0x05, 0x07, 0x04, 0x00,  // 0x34 4
    0x07, 0x01, 0x03, 0x04, 0x03, 0x00,  // 0x35 5
    0x01, 0x01, 0x07, 0x05, 0x07, 0x00,  // 0x36 6
    0x07, 0x04, 0x06, 0x04, 0x04, 0x00,  // 0x37 7
    0x07, 0x05, 0x02, 0x05, 0x07, 0x00,  // 0x38 8
    0x07, 0x05, 0x07, 0x04, 0x04, 0x00,  // 0x39 9
    0x00, 0x00, 0x02, 0x00, 0x02, 0x00,  // 0x3a :
    0x00, 0x00, 0x02, 0x00, 0x02, 0x01,  // 0x3b ;
    0x04, 0x02, 0x01, 0x02, 0x04, 0x00,  // 0x3c <
    0x00, 0x07, 0x00, 0x07, 0x00, 0x00,  // 0x3d =
    0x01, 0x02, 0x04, 0x02, 0x01, 0x00,  // 0x3e >
    0x02, 0x05, 0x04, 0x02, 0x00, 0x02,  // 0x3f ?
    0x00, 0x06, 0x05, 0x05, 0x01, 0x06,  // 0x40 @
    0x02, 0x05, 0x07, 0x05, 0x05, 0x00,  // 0x41 A
    0x03, 0x05, 0x03, 0x05, 0x03, 0x00,  // 0x42 B
    0x06, 0x01, 0x01, 0x01, 0x06, 0x00,  // 0x43 C
    0x03, 0x05, 0x05, 0x05, 0x03, 0x00,  // 0x44 D
    0x07, 0x01, 0x03, 0x01, 0x07, 0x00,  // 0x45 E
    0x07, 0x01, 0x03, 0x01, 0x01, 0x00,  // 0x46 F
    0x06, 0x01, 0x01, 0x05, 0x06, 0x00,  // 0x47 G
    0x05, 0x05, 0x07, 0x05, 0x05, 0x00,  // 0x48 H
    0x07, 0x02, 0x02, 0x02, 0x07, 0x00,  // 0x49 I
    0x04, 0x04, 0x04, 0x04, 0x03, 0x00,  // 0x4a J
    0x05, 0x05, 0x03, 0x05, 0x05, 0x00,  // 0x4b K
    0x01, 0x01, 0x01, 0x01, 0x07, 0x00,  // 0x4c L
    0x05, 0x07, 0x07, 0x05, 0x05, 0x00,  // 0x4d M
    0x05, 0x07, 0x07, 0x07, 0x05, 0x00,  // 0x4e N
    0x02, 0x05, 0x05, 0x05, 0x02, 0x00,  // 0x4f O
    0x03, 0x05, 0x03, 0x01, 0x01, 0x00,  // 0x50 P
    0x02, 0x05, 0x05, 0x07, 0x06, 0x00,  // 0x51 Q
    0x03, 0x05, 0x03, 0x05, 0x05, 0x00,  // 0x52 R
    0x06, 0x01, 0x02, 0x04, 0x03, 0x00,  // 0x53 S
    0x07, 0x02, 0x02, 0x02, 0x02, 0x00,  // 0x54 T
    0x05, 0x05, 0x05, 0x05, 0x07, 0x00,  // 0x55 U
    0x05, 0x05, 0x05, 0x05, 0x02, 0x00,  // 0x56 V
    0x05, 0x05, 0x07, 0x07, 0x05, 0x00,  // 0x57 W
    0x05, 0x05, 0x02, 0x05, 0x05, 0x00,  // 0x58 X
    0x05, 0x05, 0x02, 0x02, 0x02, 0x00,  // 0x59 Y
    0x07, 0x04, 0x02, 0x01, 0x07, 0x00,  // 0x5a Z
    0x03, 0x01, 0x01, 0x01, 0x03, 0x00,  // 0x5b [
    0x00, 0x00, 0x01, 0x02, 0x04, 0x00,  // 0x5c
    0x06, 0x04, 0x04, 0x04, 0x06, 0x00,  // 0x5d ]
    0x02, 0x05, 0x00, 0x00, 0x00, 0x00,  // 0x5e ^
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00,  // 0x5f _
    0x00, 0x00, 0x01, 0x02, 0x04, 0x00,  // 0x60 `
    0x03, 0x04, 0x06, 0x05, 0x06, 0x00,  // 0x61 a
    0x01, 0x03, 0x05, 0x05, 0x03, 0x00,  // 0x62 b
    0x00, 0x06, 0x01, 0x01, 0x06, 0x00,  // 0x63 c
    0x04, 0x06, 0x05, 0x05, 0x06, 0x00,  // 0x64 d
    0x00, 0x06, 0x07, 0x01, 0x06, 0x00,  // 0x65 e
    0x00, 0x02, 0x01, 0x03, 0x01, 0x01,  // 0x66 f
    0x00, 0x02, 0x05, 0x06, 0x04, 0x03,  // 0x67 g
    0x01, 0x01, 0x03, 0x05, 0x05, 0x00,  // 0x68 h
    0x02, 0x00, 0x03, 0x02, 0x07, 0x00,  // 0x69 i
    0x02, 0x00, 0x02, 0x02, 0x02, 0x01,  // 0x6a j
    0x01, 0x01, 0x05, 0x03, 0x05, 0x00,  // 0x6b k
    0x03, 0x02, 0x02, 0x02, 0x07, 0x00,  // 0x6c l
    0x00, 0x07, 0x07, 0x05, 0x05, 0x00,  // 0x6d m
    0x00, 0x03, 0x05, 0x05, 0x05, 0x00,  // 0x6e n
    0x00, 0x02, 0x05, 0x05, 0x02, 0x00,  // 0x6f o
    0x00, 0x03, 0x05, 0x03, 0x01, 0x01,  // 0x70 p
    0x00, 0x06, 0x05, 0x06, 0x04, 0x04,  // 0x71 q
    0x00, 0x05, 0x03, 0x01, 0x01, 0x00,  // 0x72 r
    0x00, 0x06, 0x03, 0x06, 0x03, 0x00,  // 0x73 s
    0x02, 0x07, 0x02, 0x02, 0x04, 0x00,  // 0x74 t
    0x00, 0x05, 0x05, 0x05, 0x07, 0x00,  // 0x75 u
    0x00, 0x05, 0x05, 0x05, 0x02, 0x00,  // 0x76 v
    0x00, 0x05, 0x05, 0x07, 0x07, 0x00,  // 0x77 w
    0x00, 0x05, 0x02, 0x02, 0x05, 0x00,  // 0x78 x
    0x00, 0x05, 0x05, 0x06, 0x04, 0x03,  // 0x79 y
    0x00, 0x07, 0x02, 0x01, 0x07, 0x00,  // 0x7a z
    0x06, 0x02, 0x01, 0x02, 0x06, 0x00,  // 0x7b {
    0x02, 0x02, 0x02, 0x02, 0x02, 0x00,  // 0x7c |
    0x03, 0x02, 0x04, 0x02, 0x03, 0x00,  // 0x7d }
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00,  // 0x7e ~
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07,  // 0x7f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x80
    0x05, 0x00, 0x05, 0x05, 0x07, 0x00,  // 0x81
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x82
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x83
    0x05, 0x00, 0x07, 0x07, 0x05, 0x00,  // 0x84
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x85
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x86
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x87
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x88
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x89
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x8a
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x8b
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x8c
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x8d
    0x05, 0x00, 0x07, 0x05, 0x07, 0x00,  // 0x8e
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x8f
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x90
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x91
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x92
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x93
    0x05, 0x02, 0x05, 0x07, 0x05, 0x00,  // 0x94
    0x05, 0x00, 0x07, 0x05, 0x07, 0x00,  // 0x95
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x96
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x97
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x98
    0x05, 0x00, 0x07, 0x05, 0x07, 0x00,  // 0x99
    0x05, 0x00, 0x05, 0x05, 0x07, 0x00,  // 0x9a
    0x00, 0x00,  // 0x2e2: the last two bytes, no full glyph
};
// Draw_CheapText mask table, bullfrog 0x10072d64, 16 BIG-ENDIAN words: byte j (0 = most significant) of a word
// covers pixel j (dst + j) of the 4-pixel cell, so mask 1 (0x000000ff) is pixel 3.
const uint32_t kCheapMasks[16] = {
    0x00000000, 0x000000ff, 0x0000ff00, 0x0000ffff, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x00ffffff,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};
} // namespace

// 0x1001ba94 and 0x1001bb70, 32 B each.
void SpriteRender_OnMeasure(void* font, TbSize* size, uint32_t ch) {
    const TbSprite* sprite = Sprite(font, ch);
    size->width = sprite->mWidth;
    size->height = sprite->mHeight;
}
void OneColourSpriteRender_OnMeasure(void* font, TbSize* size, uint32_t ch) {
    const TbSprite* sprite = Sprite(font, ch);
    size->width = sprite->mWidth;
    size->height = sprite->mHeight;
}
// 0x1001baec, 72 B; 0x1001bbd4, 88 B.
uint32_t SpriteRender_OnDrawChar(void* font, int32_t x, int32_t y, uint32_t ch, TbColour colour) {
    const TbSprite* sprite = Sprite(font, ch);
    LbDraw_Sprite(x, y, sprite);
    return sprite->mWidth;
}
uint32_t OneColourSpriteRender_OnDrawChar(void* font, int32_t x, int32_t y, uint32_t ch, TbColour colour) {
    const TbSprite* sprite = Sprite(font, ch);
    LbDraw_OneColourSprite(x, y, sprite, colour);
    return sprite->mWidth;
}
// 0x1001bc74, 220 B.
TbResult LbDraw_SetTextFont(const TbTextRender* render, void* font) {
    if (gLbTextRender && gLbTextRender->mOnRelease) gLbTextRender->mOnRelease(gLbTextFont, render, font);
    gLbTextRender = render;
    if (render->mOnSelect) {
        TbResult result = render->mOnSelect(&gLbTextFont, font);
        if ((result.mCode >= 0) != (gLbTextSelectSense == 0)) {
            gLbTextRender = nullptr;
            return {-1};
        }
    } else gLbTextFont = font;
    return {0};
}
int32_t LbDraw_GetTextCharacterWidth(uint32_t ch) { return Measure(ch).width; }
int32_t LbDraw_GetTextCharacterHeight(uint32_t ch) { return Measure(ch).height; }

// 0x1001daf0, 380 B; 0x1001dc9c, 376 B.
template <typename T> uint32_t Text_Measure(const T* text, TbSize* size) {
    TbSize local = {0, 0};
    if (!size) size = &local;
    size->width = 0;
    size->height = Measure(' ').height;
    uint32_t line = 0;
    for (; *text; ++text) {
        if (Character(*text) == '\n') {
            size->height = static_cast<uint32_t>(size->height) + Measure(' ').height;
            if (static_cast<uint32_t>(size->width) <= line) size->width = line;
            line = 0;
        } else line += Measure(Character(*text)).width;
    }
    if (static_cast<uint32_t>(size->width) <= line) size->width = line;
    return size->width;
}
// 0x1001d94c, 168 B; 0x1001da20, 164 B.
template <typename T> uint32_t word_width(const T* text, uint32_t* len) {
    uint32_t width = 0, count = 0;
    while (*text && !IsSpace(static_cast<uint8_t>(*text))) {
        width += Measure(Character(*text)).width;
        ++text;
        ++count;
    }
    if (len) *len = count;
    return width;
}
// 0x1001d124, 136 B; 0x1001d1e4, 132 B.
template <typename T> void draw_text_chars(int32_t x, int32_t y, const T* text, uint32_t count, TbColour colour) {
    while (count) {
        x = static_cast<uint32_t>(x) + gLbTextRender->mOnDrawChar(gLbTextFont, x, y, Character(*text), colour);
        --count;
        ++text;
    }
}
// 0x1001d028 and 0x1001cf2c, 204 B each.
template <typename T> void Draw_Text(int32_t x, int32_t y, const T* text, TbColour colour) {
    int32_t startX = x;
    while (*text) {
        if (Character(*text) == '\n') {
            y = static_cast<uint32_t>(y) + Measure(' ').height;
            x = startX;
            ++text;
        } else {
            uint32_t ch = static_cast<uint8_t>(*text++);
            x = static_cast<uint32_t>(x) + gLbTextRender->mOnDrawChar(gLbTextFont, x, y, ch, colour);
        }
    }
}
// 0x1001c48c, 408 B; 0x1001c2c0, 400 B.
template <typename T> int32_t Text_MeasureFormattedHeight(uint32_t width, const T* text) {
    uint32_t lines = 0, x = 0;
    while (*text) {
        uint32_t len;
        uint32_t w = word_width(text, &len);
        if (width < x + w) {
            if (x == 0) {
                while (*text) {
                    uint32_t cw = Measure(Character(*text)).width;
                    if (width < x + cw) break;
                    x += cw;
                    ++text;
                }
            }
            x = 0;
            ++lines;
        } else {
            text += len;
            x = w + static_cast<uint32_t>(Measure(' ').width) + x;
        }
        while (*text) {
            if (Character(*text) == '\n') { x = 0; ++lines; }
            else if (Character(*text) != ' ') break;
            ++text;
        }
    }
    return (lines + 1) * static_cast<uint32_t>(Measure(' ').height);
}
// 0x1001d2a0, 792 B; 0x1001d5f8, 788 B.
template <typename T> void draw_text_line(int32_t x, int32_t y, const T* text, uint32_t words, TbColour colour,
                                         uint32_t flags, int32_t newline, uint32_t width, uint32_t lineWidth) {
    uint32_t offset = 0, step = 0; // Mac leaves these unset when no horizontal flag is supplied.
    if (flags & 1) step = static_cast<uint32_t>(Measure(' ').width) << 8;
    else if (flags & 2) {
        offset = (width - (lineWidth - Measure(' ').width)) << 8;
        step = static_cast<uint32_t>(Measure(' ').width) << 8;
    } else if (flags & 4) {
        offset = ((width - (lineWidth - Measure(' ').width)) << 7) & 0xffffff00u;
        step = static_cast<uint32_t>(Measure(' ').width) << 8;
    } else if (flags & 8) {
        if (newline == 0 && words > 1) {
            uint32_t sw = Measure(' ').width;
            step = ((width - (lineWidth - words * sw)) << 16) / ((words - 1) << 8);
        } else step = static_cast<uint32_t>(Measure(' ').width) << 8;
    }
    if (words <= 1) {
        uint32_t len;
        word_width(text, &len);
        draw_text_chars(static_cast<uint32_t>(x) + (offset >> 8), y, text, len, colour);
    } else while (words) {
        uint32_t len;
        uint32_t w = word_width(text, &len);
        draw_text_chars(static_cast<uint32_t>(x) + (offset >> 8), y, text, len, colour);
        offset = (w << 8) + step + offset;
        text += len;
        while (*text && IsSpace(static_cast<uint8_t>(*text))) ++text;
        --words;
    }
}
// 0x1001cac4, 1060 B; 0x1001c660, 1056 B.
template <typename T> void Draw_FormattedText(const TbRect* rect, const T* text, uint32_t flags, TbColour colour) {
    int32_t width = LbRect_Width(rect);
    while (*text && IsSpace(static_cast<uint8_t>(*text))) ++text;
    const T* start = text;
    int passes = (flags & 0x10) ? 1 : 2;
    uint32_t y = 0; // Mac only initializes this in the one-pass case.
    uint32_t step = static_cast<uint32_t>(Measure(' ').height) << 8;
    while (--passes >= 0) {
        const T* lineStart = text;
        uint32_t lines = 0, x = 0, words = 0;
        while (*text) {
            uint32_t len;
            uint32_t w = word_width(text, &len);
            if (width < static_cast<int32_t>(x + w)) {
                if (x == 0) {
                    uint32_t count = 0;
                    words = 0;
                    while (*text) {
                        uint32_t cw = Measure(Character(*text)).width;
                        if (width < static_cast<int32_t>(x + cw)) break;
                        x += cw;
                        ++text;
                        ++count;
                    }
                    if (passes == 0) draw_text_chars(rect->left, static_cast<uint32_t>(rect->top) + (static_cast<int32_t>(y) >> 8), lineStart, count, colour);
                } else if (passes == 0) {
                    draw_text_line(rect->left, static_cast<uint32_t>(rect->top) + (static_cast<int32_t>(y) >> 8), lineStart, words, colour, flags, 0, width, x);
                }
                y += step;
                x = words = 0;
                ++lines;
            } else {
                text += len;
                uint32_t sw = Measure(' ').width;
                ++words;
                x = w + sw + x;
            }
            while (*text) {
                if (Character(*text) == '\n') {
                    if (words && passes == 0) draw_text_line(rect->left, static_cast<uint32_t>(rect->top) + (static_cast<int32_t>(y) >> 8), lineStart, words, colour, flags, 1, width, x);
                    y += step;
                    x = words = 0;
                    ++lines;
                } else if (!IsSpace(static_cast<uint8_t>(*text))) break;
                ++text;
            }
            if (x == 0) lineStart = text;
        }
        if (passes == 0) draw_text_line(rect->left, static_cast<uint32_t>(rect->top) + (static_cast<int32_t>(y) >> 8), lineStart, words, colour, flags, 0, width, x);
        if (passes == 1) {
            if (flags & 0x20) {
                uint32_t h = LbRect_Height(rect);
                y = (h - (lines + 1) * (static_cast<int32_t>(step) >> 8)) << 8;
            } else if (flags & 0x40) {
                uint32_t h = LbRect_Height(rect);
                y = static_cast<uint32_t>(static_cast<int32_t>(h - (lines + 1) * (static_cast<int32_t>(step) >> 8)) / 2) << 8;
            } else if (flags & 0x80) {
                if (static_cast<int32_t>(lines) <= 0) {
                    uint32_t h = LbRect_Height(rect);
                    y = static_cast<uint32_t>(static_cast<int32_t>(h - (static_cast<int32_t>(step) >> 8)) / 2) << 8;
                } else {
                    uint32_t sh = Measure(' ').height;
                    y = 0;
                    uint32_t h = LbRect_Height(rect);
                    step += ((h - (lines + 1) * sh) << 16) / (lines << 8);
                }
            }
            text = start;
        }
    }
}
// 0x1001c0ac and 0x1001be98, 476 B each.
template <typename T> void Draw_CheapText(int32_t x, int32_t y, const T* text, TbColour colour) {
    constexpr bool wide = !std::is_same_v<T, char>;
    auto& guard = wide ? gLbCheapUnicodeTextGuard : gLbCheapTextGuard;
    auto& index = wide ? gLbCheapUnicodeTextIndex : gLbCheapTextIndex;
    auto& cached = wide ? gLbCheapUnicodeTextColour : gLbCheapTextColour;
    if (!guard) { index = 0; guard = 1; }
    if (static_cast<int32_t>(static_cast<uint32_t>(y) + 6) > _lbCurrentClipRect.bottom || y < _lbCurrentClipRect.top) return;
    int32_t pitch = _lbpCurrentRA->mStride;
    uint8_t* dst = _lbCurrentOriginPtr + static_cast<int32_t>(static_cast<uint32_t>(x) + static_cast<uint32_t>(y) * pitch);
    uint32_t c = cached;
    if (index != colour.mIndex) {
        index = colour.mIndex;
        c = cached = colour.mIndex * 0x01010101u;
    }
    while (*text) {
        uint32_t ch = Character(*text++);
        if (_lbCurrentClipRect.left <= x) {
            if (_lbCurrentClipRect.right < static_cast<int32_t>(static_cast<uint32_t>(x) + 4)) return;
            int32_t glyph = static_cast<int32_t>((ch - 0x20) * 6);
            for (int r = 0; r < 6; ++r) {
                uint32_t mask = kCheapMasks[kCheapGlyphs[glyph + r]];
                for (int j = 0; j < 4; ++j) {
                    if ((mask >> (24 - 8 * j)) & 0xff) dst[r * pitch + j] = static_cast<uint8_t>(c >> (24 - 8 * j));
                }
            }
        }
        dst += 4;
        x = static_cast<uint32_t>(x) + 4;
    }
}

void LbDraw_Text(int32_t x, int32_t y, const char* s, TbColour c) { Draw_Text(x, y, s, c); }
void LbDraw_UnicodeText(int32_t x, int32_t y, const char16_t* s, TbColour c) { Draw_Text(x, y, s, c); }
void LbDraw_FormattedText(const TbRect* r, const char* s, uint32_t f, TbColour c) { Draw_FormattedText(r, s, f, c); }
void LbDraw_UnicodeFormattedText(const TbRect* r, const char16_t* s, uint32_t f, TbColour c) { Draw_FormattedText(r, s, f, c); }
uint32_t LbDraw_GetTextSize(const char* s, TbSize* z) { return Text_Measure(s, z); }
uint32_t LbDraw_GetUnicodeTextSize(const char16_t* s, TbSize* z) { return Text_Measure(s, z); }
int32_t LbDraw_GetFormattedTextHeight(uint32_t w, const char* s) { return Text_MeasureFormattedHeight(w, s); }
int32_t LbDraw_GetUnicodeFormattedTextHeight(uint32_t w, const char16_t* s) { return Text_MeasureFormattedHeight(w, s); }
void LbDraw_CheapText(int32_t x, int32_t y, const char* s, TbColour c) { Draw_CheapText(x, y, s, c); }
void LbDraw_UnicodeCheapText(int32_t x, int32_t y, const char16_t* s, TbColour c) { Draw_CheapText(x, y, s, c); }

#define INSTANTIATE(T) \
    template void Draw_CheapText<T>(int32_t, int32_t, const T*, TbColour); \
    template int32_t Text_MeasureFormattedHeight<T>(uint32_t, const T*); \
    template void Draw_FormattedText<T>(const TbRect*, const T*, uint32_t, TbColour); \
    template void Draw_Text<T>(int32_t, int32_t, const T*, TbColour); \
    template void draw_text_chars<T>(int32_t, int32_t, const T*, uint32_t, TbColour); \
    template void draw_text_line<T>(int32_t, int32_t, const T*, uint32_t, TbColour, uint32_t, int32_t, uint32_t, uint32_t); \
    template uint32_t word_width<T>(const T*, uint32_t*); \
    template uint32_t Text_Measure<T>(const T*, TbSize*);
INSTANTIATE(char)
INSTANTIATE(char16_t)
#undef INSTANTIATE

// LbDraw.cpp companions: 0x1002998c and 0x100299c8, 16 B each.
int32_t LbRect_Width(const TbRect* r) { return static_cast<uint32_t>(r->right) - r->left; }
int32_t LbRect_Height(const TbRect* r) { return static_cast<uint32_t>(r->bottom) - r->top; }
