// PARTIAL ltms class. The constructors, the destructor (0x10002400, the key function: the vtable is emitted by
// src/tb_fontrenderparams.cpp) and UseAlphaWriteMode are in that file (ltms 0x10002350, 0x10002400, 0x100024ac,
// 0x1000296c); the unit libraries that only fake the class define their own constructor and destructor.
#pragma once
#include <cstdint>
#include "TbColour.h"
#include "TbError.h"

class TbIFont;

// The render area the parameters hold (a Bullfrog library TbRenderArea, 0x28 bytes on the Mac): one host
// definition for the whole tree, in TbGraphicFile.h (unit w21's layout; w23 first declared it here as ten words).
// Only its default constructor is called from the font render parameters.
#include "TbGraphicFile.h"

class TbFontRenderParams {
public:
    TbFontRenderParams();                                      // 0x10002350
    // 0x100024ac: copies the fields one by one (the render area's words too) and then takes a reference on the font
    // (the font's virtual slot 0xc, AddRef) WITHOUT testing it for NULL.
    TbFontRenderParams(const TbFontRenderParams& other);
    virtual ~TbFontRenderParams();                             // 0x10002400: releases the font
    TbError UseAlphaWriteMode();                               // 0x1000296c
    // Unit w35a (src/ltms_fontrender.cpp). Each answers 0, or -1 when the font is missing or lacks the capability.
    TbError UseReColour(const TbColour& colour);               // 0x10002800: needs a font with caps bit 2
    TbError UsePreColoured();                                  // 0x10002888: needs a font with caps bit 1
    TbError SetFont(TbIFont* font);                            // 0x100029d0: always 0
    // GetFont takes a reference on the font it answers and nobody lets it go (CreateInfoList leaks one per call).
    // All of these are in src/ltms_fontrender.cpp (operator=, UseTexture, UseBlendedMode, GetStyle and GetType: unit w45a).
    TbFontRenderParams& operator=(const TbFontRenderParams& other);   // 0x100025e8: copies the words, takes a reference on the font
    // 0x10002718: a font with caps bit 2 (value 4): the render area and the point are copied in, the style becomes 2; else -1.
    TbError UseTexture(const TbRenderArea& area, const TbPoint& point);
    TbError UseBlendedMode();                                  // 0x100028f8: a font with caps bit 3 (value 8): the blend word becomes 1; else -1
    int32_t GetStyle() const;                                  // 0x10002ab8: the word at +0xc
    TbIFont* GetFont() const;                                  // 0x10002af4 (src/ltms_fontrender.cpp, unit w44)
    int32_t GetType() const;                                   // 0x10002b70: the font's type, 5 without a font
    // Opaque logical slots from +0x04..0x4f (original size 0x50). Some hold
    // pointers, so native slots must be wide enough for them. The slots by their Mac offsets:
    //   mState[0]  +0x04  the font (a TbIFont*, 0 for none)
    //   mState[1]  +0x08  not set by the default constructor
    //   mState[2]  +0x0c  3
    //   mState[3]  +0x10  1; UseAlphaWriteMode clears it
    //   mState[4..13]  +0x14..+0x3b  the render area (a TbRenderArea made by its constructor)
    //   mState[14] +0x3c, mState[15] +0x40  0
    //   mState[16]  +0x44..+0x48  five flag bytes (the first five bytes of the slot on the host), 0
    //   mState[18] +0x4c  0
    uintptr_t mState[19];
};
