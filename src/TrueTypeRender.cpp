// TrueTypeRender (main app, rsse.cpp range): the constructor (0x100c9c90), the deleting-destructor shell (0x100c9d50),
// render_font_unicode (0x100c9dc0) and load_settings (0x100ca340).
//
// The NULL checks after new[] are the original's but dead in the game: the runtime's operator new[] throws.
#include "TrueTypeRender.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Bitmap.h"
#include "FS_System.h"
#include "platform/msl.h"
#include "platform/samsutils.h"

TrueTypeRender::TrueTypeRender() {
    mLogFont.lfHeight = 0;
    mLogFont.lfWidth = 0;
    mLogFont.lfEscapement = 0;
    mLogFont.lfOrientation = 0;
    mLogFont.lfWeight = 400;
    mLogFont.lfItalic = 0;
    mLogFont.lfUnderline = 0;
    mLogFont.lfStrikeOut = 0;
    mLogFont.lfCharSet = 1;
    mLogFont.lfOutPrecision = 4;
    mLogFont.lfClipPrecision = 1;
    mLogFont.lfQuality = 4;
    mLogFont.lfPitchAndFamily = 0;
    std::strcpy(mLogFont.lfFaceName, "Arial Black");   // the string at 0x1023ee9b
    mF008 = 100;
    mTextY = 0;
}

TrueTypeRender::~TrueTypeRender() {}

int32_t TrueTypeRender::load_settings(bfl::FileSystem::CFile* file) {
    TPFile_Read(mF04C, 0x40, 1, file);
    TPFile_Read(mName, 0x104, 1, file);
    TPFile_Read(&mF008, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&mF008), 1);
    TPFile_Read(&mTextY, 4, 1, file);
    SamsUtilities::_USwapBlock32(reinterpret_cast<uint32_t*>(&mTextY), 1);
    TPFile_Read(&mLogFont, 0x3c, 1, file);
    // Only the five int32 words are swapped (the inline byte swaps); the flag bytes and the face name stay as read.
    uint32_t* words = reinterpret_cast<uint32_t*>(&mLogFont);
    for (int32_t i = 0; i < 5; ++i) {
        const uint32_t v = words[i];
        words[i] = (v >> 24) | ((v >> 8) & 0xff00) | ((v << 8) & 0xff0000) | (v << 24);
    }
    return 1;
}

int32_t TrueTypeRender::render_font_unicode(NS_MacDoze::InternalHandle* dc, const char16_t* text, Bitmap& out) {
    const uint32_t length = wcslen(text);
    if (length == 0) {
        mLeft = 0;
        mRight = 0;
        // bytes per pixel * (width * height), as the signed loop bound the listing computes
        const int32_t total = static_cast<int32_t>(out.mBytesPerPixel * (out.mWidth * out.mHeight));
        for (int32_t i = 0; i < total; ++i) out.mData[i] = 0;
        return 1;
    }

    char* narrow = new char[length + 1];
    if (narrow == nullptr) return 0;
    // snprintf(narrow, length + 1, "%ls", text) with MSL's 16-bit wchar_t (the host's is wider, so widen first)
    wchar_t* wide = static_cast<wchar_t*>(std::malloc((length + 1) * sizeof(wchar_t)));
    for (uint32_t i = 0; i <= length; ++i) wide[i] = text[i];
    std::snprintf(narrow, length + 1, "%ls", wide);
    std::free(wide);

    // The bitmap header and a 256-entry grey palette: a DIB twice as wide and high as the mask (top-down, 8 bits).
    uint8_t* raw = new uint8_t[0x42c];
    if (raw == nullptr) return 0;   // narrow leaks
    NS_MacDoze::tagBITMAPINFO* info = reinterpret_cast<NS_MacDoze::tagBITMAPINFO*>(raw);
    info->bmiHeader.biSize = 0x28;
    info->bmiHeader.biWidth = static_cast<int32_t>(out.mWidth) * 2;
    info->bmiHeader.biHeight = static_cast<int32_t>(out.mHeight) * -2;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 8;
    info->bmiHeader.biCompression = 0;
    info->bmiHeader.biSizeImage = 0;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0x100;
    info->bmiHeader.biClrImportant = 0x100;
    info->bmiHeader.biSizeImage = 0x8000;
    NS_MacDoze::tagRGBQUAD* palette = reinterpret_cast<NS_MacDoze::tagRGBQUAD*>(raw + 0x28);
    for (int32_t i = 0; i < 0x100; ++i) {
        palette[i].rgbRed = static_cast<uint8_t>(i);
        palette[i].rgbGreen = static_cast<uint8_t>(i);
        palette[i].rgbBlue = static_cast<uint8_t>(i);   // rgbReserved is not written
    }
    void* bits = nullptr;
    NS_MacDoze::InternalHandle* dib = NS_MacDoze::CreateDIBSection(dc, info, 0, &bits, nullptr, 0);
    delete[] raw;
    if (dib == nullptr) return 0;   // narrow leaks

    NS_MacDoze::InternalHandle* memDC = NS_MacDoze::CreateCompatibleDC(dc);
    NS_MacDoze::InternalHandle* oldBitmap = NS_MacDoze::SelectObject(memDC, dib);
    NS_MacDoze::SetMapMode(memDC, 1);
    const int32_t savedWidth = mLogFont.lfWidth;
    NS_MacDoze::InternalHandle* font = NS_MacDoze::CreateFontIndirect(&mLogFont);
    NS_MacDoze::InternalHandle* oldFont = NS_MacDoze::SelectObject(memDC, font);
    NS_MacDoze::tagSIZE size;
    NS_MacDoze::GetTextExtentPoint(memDC, narrow, static_cast<int32_t>(length), &size);

    const int32_t limit = static_cast<int32_t>(out.mWidth) << 1;
    if (size.cx >= limit) {
        // The text is too wide: bisect lfWidth (lo..hi) until the extent fits, stopping with that font still selected.
        int32_t hi = mLogFont.lfWidth;
        if (hi == 0) hi = -mLogFont.lfHeight;
        int32_t lo = 1;
        int32_t mid = ((hi - 1) >> 1) + 1;
        bool done;
        do {
            mLogFont.lfWidth = mid;
            font = NS_MacDoze::CreateFontIndirect(&mLogFont);
            oldFont = NS_MacDoze::SelectObject(memDC, font);
            NS_MacDoze::GetTextExtentPoint(memDC, narrow, static_cast<int32_t>(length), &size);
            if (hi - lo < 3 && size.cx < limit) {
                done = true;
            } else {
                done = false;
                NS_MacDoze::SelectObject(memDC, oldFont);
                NS_MacDoze::DeleteObject(font);
                if (size.cx < limit) {
                    lo = mid;
                    mid = mid + ((hi - mid) >> 1);
                } else {
                    hi = mid;
                    mid = lo + ((mid - lo) >> 1);
                }
            }
        } while (!done);
    }
    mLogFont.lfWidth = savedWidth;
    NS_MacDoze::SetBkColor(memDC, 0);
    NS_MacDoze::SetTextColor(memDC, 0xffffff);
    NS_MacDoze::TextOut(memDC, 0x100 - size.cx / 2, mTextY, narrow, static_cast<int32_t>(length));
    delete[] narrow;
    NS_MacDoze::SelectObject(memDC, oldFont);
    NS_MacDoze::DeleteObject(font);

    // Box-filter the DIB (two rows of 2 * width bytes per mask row) down into the mask.
    out.realloc(out.mWidth, out.mHeight, 1);
    const uint8_t* top = static_cast<const uint8_t*>(bits);
    uint8_t* dst = out.mData;
    const int32_t width = static_cast<int32_t>(out.mWidth);
    for (int32_t y = 0; y < static_cast<int32_t>(out.mHeight); ++y) {
        const uint8_t* a = top;
        const uint8_t* b = top + width * 2;
        for (int32_t x = 0; x < static_cast<int32_t>(out.mWidth); ++x) {
            const int32_t sum = a[0] + a[1] + b[0] + b[1];
            *dst++ = static_cast<uint8_t>(sum >> 2);
            a += 2;
            b += 2;
        }
        top += static_cast<int32_t>(out.mWidth) * 4;
    }
    NS_MacDoze::SelectObject(memDC, oldBitmap);
    NS_MacDoze::DeleteObject(dib);
    NS_MacDoze::DeleteDC(memDC);

    // The first row with ink and the row after the second and later ones. The scan stops at the first non-zero byte, so
    // a row whose only ink is its last column does not count, and a single inked row leaves mRight at -1.
    mLeft = -1;
    mRight = -1;
    const uint8_t* row = out.mData;
    for (int32_t y = 0; y < static_cast<int32_t>(out.mHeight); ++y) {
        int32_t x = 0;
        uint8_t seen = 0;
        const uint8_t* p = row;
        while (x < static_cast<int32_t>(out.mWidth) && seen == 0) {
            seen |= *p++;
            ++x;
        }
        if (x < static_cast<int32_t>(out.mWidth)) {
            if (mLeft < 0) mLeft = y;
            else mRight = y + 1;
        }
        row += out.mWidth;
    }
    return 1;
}
