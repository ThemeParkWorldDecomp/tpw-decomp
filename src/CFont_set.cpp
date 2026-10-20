// CFont::Set (main 0x1007da14, 144 bytes): selects an old sprite font, unit w37b.
#include "InterfaceFont.h"
#include "LbDrawText.h"

CFont::FontEntry* gCFontTable; // Mac 0x102dc1a8

int32_t CFont::Set(FontId font, RenderType render) {
    if (gCFontTable == nullptr || gCFontTable[font].mFont == nullptr) return 0;
    if (render == kSprite) {
        LbDraw_SetTextFont(&_lbSpriteRender, gCFontTable[font].mFont);
    } else if (render == kOneColour) {
        LbDraw_SetTextFont(&_lbOneColourSpriteRender, gCFontTable[font].mFont);
    }
    return 1;
}
