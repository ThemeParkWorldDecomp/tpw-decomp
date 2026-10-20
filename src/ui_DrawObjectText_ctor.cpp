// InterfaceTextObjectSprite constructor, main 0x1018d11c (136 bytes); existing body separated for unit w43.
#include "ui_DrawObject.h"

InterfaceTextObjectSprite::InterfaceTextObjectSprite(void* data) : InterfaceDrawObjectSprite(data) {
    mJustify = 0;
    mVJustify = 1;
    mWrap = 0;
    mF0DF = 0;
    mOffsetX = 0;
    mOffsetY = 0;
    mFontId = -1;
    mColourMode = 0;
    TextColour()[0] = 0xff;
    TextColour()[1] = 0xff;
    TextColour()[2] = 0xff;
}
