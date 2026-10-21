// Unit w224 (src/bullfrog/LbDrawText.cpp, src/bullfrog/LbRect.cpp), Bullfrog text drawing.
#pragma once
#include <cstdint>
#include "TbGraphicFile.h"
#include "TbColour.h"

// Mac 16 bytes. Member names are inferred from the four callback positions.
struct TbTextRender {
    TbResult (*mOnSelect)(void** fontSlot, void* font); // +0x00
    void (*mOnRelease)(void* oldFont, const TbTextRender* newRender, void* newFont); // +0x04
    void (*mOnMeasure)(void* font, TbSize* size, uint32_t ch); // +0x08
    uint32_t (*mOnDrawChar)(void* font, int32_t x, int32_t y, uint32_t ch, TbColour colour); // +0x0c
};
struct TbSprite {
    const uint8_t* mData; // Mac +0x00, never read by the text renderers
    uint16_t mWidth;      // Mac +0x04
    uint16_t mHeight;     // Mac +0x06 (host stride 16)
};
extern const TbTextRender _lbSpriteRender; // 0x10072a5c, Mac 16 B
extern const TbTextRender _lbOneColourSpriteRender; // 0x10072a6c, Mac 16 B
extern void* gLbTextFont; // 0x10074488, Mac 4 B, inferred name
extern const TbTextRender* gLbTextRender; // 0x1007448c, Mac 4 B, inferred name
extern int32_t gLbTextSelectSense; // 0x10072a7c, 4 B, inferred name
extern uint32_t gLbCheapTextColour; // 0x10074470, 4 B, inferred name
extern uint8_t gLbCheapTextGuard; // 0x10074474, 1 B, inferred name
extern uint32_t gLbCheapTextIndex; // 0x10074478, 4 B, inferred name
extern uint32_t gLbCheapUnicodeTextColour; // 0x1007447c, 4 B, inferred name
extern uint8_t gLbCheapUnicodeTextGuard; // 0x10074480, 1 B, inferred name
extern uint32_t gLbCheapUnicodeTextIndex; // 0x10074484, 4 B, inferred name

// Untranslated drawing-context calls. The text renderer ignores their answers.
void LbDraw_Sprite(int32_t x, int32_t y, const TbSprite* sprite); // 0x1001b630
void LbDraw_OneColourSprite(int32_t x, int32_t y, const TbSprite* sprite, TbColour colour); // 0x1001aa30
void SpriteRender_OnMeasure(void* font, TbSize* size, uint32_t ch); // 0x1001ba94, 32 B
uint32_t SpriteRender_OnDrawChar(void* font, int32_t x, int32_t y, uint32_t ch, TbColour colour); // 0x1001baec, 72 B
void OneColourSpriteRender_OnMeasure(void* font, TbSize* size, uint32_t ch); // 0x1001bb70, 32 B
uint32_t OneColourSpriteRender_OnDrawChar(void* font, int32_t x, int32_t y, uint32_t ch, TbColour colour); // 0x1001bbd4, 88 B
TbResult LbDraw_SetTextFont(const TbTextRender* render, void* font); // 0x1001bc74, 220 B
int32_t LbDraw_GetTextCharacterWidth(uint32_t ch); // 0x1001bd8c, 80 B
int32_t LbDraw_GetTextCharacterHeight(uint32_t ch); // 0x1001be10, 80 B
void LbDraw_Text(int32_t x, int32_t y, const char* text, TbColour colour); // 0x1001b6a8, 48 B
void LbDraw_UnicodeText(int32_t x, int32_t y, const char16_t* text, TbColour colour); // 0x1001b708, 48 B
void LbDraw_FormattedText(const TbRect* rect, const char* text, uint32_t flags, TbColour colour); // 0x1001b770, 48 B
void LbDraw_UnicodeFormattedText(const TbRect* rect, const char16_t* text, uint32_t flags, TbColour colour); // 0x1001b7e4, 48 B
uint32_t LbDraw_GetTextSize(const char* text, TbSize* size); // 0x1001b85c, 32 B
uint32_t LbDraw_GetUnicodeTextSize(const char16_t* text, TbSize* size); // 0x1001b8b0, 32 B
int32_t LbDraw_GetFormattedTextHeight(uint32_t width, const char* text); // 0x1001b90c, 32 B
int32_t LbDraw_GetUnicodeFormattedTextHeight(uint32_t width, const char16_t* text); // 0x1001b964, 32 B
void LbDraw_CheapText(int32_t x, int32_t y, const char* text, TbColour colour); // 0x1001b9c4, 48 B
void LbDraw_UnicodeCheapText(int32_t x, int32_t y, const char16_t* text, TbColour colour); // 0x1001ba28, 48 B

template <typename T> void Draw_CheapText(int32_t x, int32_t y, const T* text, TbColour colour); // char 0x1001c0ac, wide 0x1001be98
 template <typename T> int32_t Text_MeasureFormattedHeight(uint32_t width, const T* text); // char 0x1001c48c, wide 0x1001c2c0
 template <typename T> void Draw_FormattedText(const TbRect* rect, const T* text, uint32_t flags, TbColour colour); // char 0x1001cac4, wide 0x1001c660
 template <typename T> void Draw_Text(int32_t x, int32_t y, const T* text, TbColour colour); // char 0x1001d028, wide 0x1001cf2c
 template <typename T> void draw_text_chars(int32_t x, int32_t y, const T* text, uint32_t count, TbColour colour); // char 0x1001d124, wide 0x1001d1e4
 template <typename T> void draw_text_line(int32_t x, int32_t y, const T* text, uint32_t words, TbColour colour, uint32_t flags, int32_t newline, uint32_t width, uint32_t lineWidth); // char 0x1001d2a0, wide 0x1001d5f8
 template <typename T> uint32_t word_width(const T* text, uint32_t* len); // char 0x1001d94c, wide 0x1001da20
 template <typename T> uint32_t Text_Measure(const T* text, TbSize* size); // char 0x1001daf0, wide 0x1001dc9c
