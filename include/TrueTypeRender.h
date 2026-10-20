// TrueTypeRender (main app, rsse.cpp range 0x100c9c90..0x100ca46c): the ride sign's text renderer. One object holds a
// LOGFONT, the font file's name and where the text goes; render_font_unicode draws a line of text through the Win32 GDI
// calls (NS_MacDoze) into a 2x supersampled DIB and box-filters it down into the caller's 8-bit mask.
//
// Layout (0x190 bytes on the Mac; no pointers, so the host size is the same).
#pragma once
#include <cstdint>
#include "platform/macdoze.h"

class Bitmap;
namespace bfl { namespace FileSystem { class CFile; } }

class TrueTypeRender {
public:
    // 0x100c9c90. Writes the LOGFONT (height, width, escapement, orientation 0, weight 400, flags, face "Arial Black"),
    // mF008 = 100 and mTextY = 0. It leaves mLeft, mRight, mF04C and mName alone.
    TrueTypeRender();
    // 0x100c9d50: the CodeWarrior deleting-destructor shell (`if (this && flag > 0) operator delete(this)`). The class owns
    // nothing, so the body is empty; the host's `delete` is the caller's.
    ~TrueTypeRender();
    // 0x100ca340. Reads mF04C (0x40 bytes), mName (0x104), mF008 and mTextY (swapped), the LOGFONT (0x3c bytes) and swaps
    // its first five words in place. Answers 1 whatever the reads returned.
    int32_t load_settings(bfl::FileSystem::CFile* file);
    // 0x100c9dc0. Draws `text` and answers 1, or 0 when an allocation or the DIB fails (see the .cpp for the leaks).
    int32_t render_font_unicode(NS_MacDoze::InternalHandle* dc, const char16_t* text, Bitmap& out);

    // The first inked row of the last rendered mask and the row after the last one (-1 and -1 when nothing was inked, 0
    // and 0 for empty text). A single inked row leaves mRight at -1 (see render_font_unicode).
    // (SignRenderer reads them as mLeft and mRight.)
    int32_t mLeft;                      // 0x00
    int32_t mRight;                     // 0x04
    int32_t mF008;                      // 0x08, constructor 100; loaded
    int32_t mTextY;                     // 0x0c, constructor 0; loaded; the y of TextOut
    NS_MacDoze::tagLOGFONTA mLogFont;   // 0x10, 0x3c bytes
    char mF04C[0x40];                   // 0x4c, loaded raw
    char mName[0x104];                  // 0x8c, loaded raw: the font file name
};
static_assert(sizeof(TrueTypeRender) == 0x190, "TrueTypeRender is 0x190 bytes");
