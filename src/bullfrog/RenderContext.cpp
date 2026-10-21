// RenderContext.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of RenderContext code, named after it.

#include "helpers/RenderContextHelpers.h"
#include "LbDraw.h"
#include "LbDrawText.h"
#include "LbPalette.h"
#include "LbSurface.h"
#include "RenderContext8.h"
#include "RenderContextDepthCircles.h"
#include "TpwPropFont.h"

// Units w234-w238: indexed and true-colour drawing primitives from TbSystemInfo.cpp and Draw.cpp.

namespace {
uint32_t ShiftRight(uint32_t value, int16_t shift) {
    const uint32_t amount = uint16_t(shift) & 63;
    return amount < 32 ? value >> amount : 0;
}

uint32_t ShiftLeft(uint32_t value, int16_t shift) {
    const uint32_t amount = uint16_t(shift) & 63;
    return amount < 32 ? value << amount : 0;
}

uint16_t Pack16(uint32_t colour) {
    const auto channel = [colour](const TbMaskInfo& caps) {
        return caps.mMask & ShiftLeft(ShiftRight(colour, caps.mShiftLeft), caps.mShiftRight);
    };
    return static_cast<uint16_t>(channel(_lbCurrentPixelCaps.mAlpha) |
                                 channel(_lbCurrentPixelCaps.mRed) |
                                 channel(_lbCurrentPixelCaps.mGreen) |
                                 channel(_lbCurrentPixelCaps.mBlue));
}

uint32_t ColourWord(TbColour colour) {
    if ((_lbCurrentDrawFlags & 0x18) == 0 && (_lbCurrentDrawFlags & 0x20) != 0) {
        const uint8_t index = _lbpCurrentFadeRow[colour.mIndex];
        const auto* palette = reinterpret_cast<const uint8_t*>(&_lbGlobalPalette) + uint32_t(index) * 4;
        colour.mAlpha = 0xff;
        colour.mRed = palette[0];
        colour.mGreen = palette[1];
        colour.mBlue = palette[2];
    }
    return uint32_t(colour.mAlpha) << 24 | uint32_t(colour.mRed) << 16 |
           uint32_t(colour.mGreen) << 8 | colour.mBlue;
}

template<unsigned BytesPerPixel, class Writer>
void ClearClipWindow(Writer write) {
    const int32_t width = _lbCurrentClipRect.right - _lbCurrentClipRect.left;
    int32_t height = _lbCurrentClipRect.bottom - _lbCurrentClipRect.top;
    int32_t offset = static_cast<int32_t>(uint32_t(_lbCurrentClipRect.left) * BytesPerPixel +
        uint32_t(_lbCurrentClipRect.top) * uint32_t(_lbpCurrentRA->mStride));
    uint8_t* row = _lbCurrentOriginPtr + offset;
    while (height-- > 0) {
        uint8_t* destination = row;
        for (int32_t column = 0; column < width; ++column) {
            write(destination);
            destination += BytesPerPixel;
        }
        row += _lbpCurrentRA->mStride;
    }
}

template<unsigned BytesPerPixel, class Writer>
void SpritePixels(int32_t x, int32_t y, const TbSprite& sprite, Writer write) {
    if (sprite.mWidth == 0 || sprite.mHeight == 0) return;
    const uint8_t* data = sprite.mData;
    const bool flip_x = (_lbCurrentDrawFlags & 1) != 0;
    const bool flip_y = (_lbCurrentDrawFlags & 2) != 0;
    for (uint32_t row = 0; row < sprite.mHeight; ++row) {
        int32_t column = 0;
        for (;;) {
            const int32_t count = static_cast<int8_t>(*data++);
            if (count == 0) break;
            if (count < 0) {
                column -= count;
                continue;
            }
            for (int32_t i = 0; i < count; ++i, ++column) {
                const uint8_t index = *data++;
                const int32_t px = flip_x ? x + int32_t(sprite.mWidth) - 1 - column : x + column;
                const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row) : y + int32_t(row);
                Pixel<BytesPerPixel>(px, py, [write, index](uint8_t* destination) { write(destination, index); });
            }
        }
    }
}

struct Writer8 {
    uint8_t index;
    uint8_t value;
    unsigned mode;
    explicit Writer8(TbColour colour) : index(colour.mIndex),
        value(((_lbCurrentDrawFlags & 0x18) == 0 && (_lbCurrentDrawFlags & 0x20) != 0)
                  ? _lbpCurrentFadeRow[colour.mIndex] : colour.mIndex),
        mode((_lbCurrentDrawFlags & 8) != 0 ? 1 : (_lbCurrentDrawFlags & 0x10) != 0 ? 2 : 0) {}
    void operator()(uint8_t* destination) const {
        if (mode == 1) *destination = _lbCurrentGhostTable[uint32_t(index) * 256 + *destination];
        else if (mode == 2) *destination = _lbCurrentGhostTable[index + uint32_t(*destination) * 256];
        else *destination = value;
    }
};

}

void RenderContext8::LbDraw_Pixel(int32_t x,int32_t y,TbColour colour) { Pixel<1>(x,y,Writer8(colour)); }
void RenderContext8::LbDraw_Rectangle(const TbRect& rect,TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_RectangleFilled(rect,colour); else LbDraw_RectangleOutline(rect,colour);
}
void RenderContext8::LbDraw_RectangleOutline(const TbRect& rect,TbColour colour) { RectangleOutline<1>(rect,Writer8(colour)); }
void RenderContext8::LbDraw_Line(int32_t x0,int32_t y0,int32_t x1,int32_t y1,TbColour colour) { Line<1>(x0,y0,x1,y1,Writer8(colour)); }
void RenderContext8::LbDraw_HorizontalLine(int32_t x,int32_t y,int32_t length,TbColour colour) { HorizontalLine<1>(x,y,length,Writer8(colour)); }
void RenderContext8::LbDraw_VerticalLine(int32_t x,int32_t y,int32_t length,TbColour colour) { VerticalLine<1>(x,y,length,Writer8(colour)); }
void RenderContext8::LbDraw_Circle(int32_t x,int32_t y,uint32_t radius,TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_CircleFilled(x,y,radius,colour); else LbDraw_CircleOutline(x,y,radius,colour);
}

// Unit w233: bullfrog RenderContext8::LbDraw_CircleOutline, 0x1002c7dc, 6892 B.

void RenderContext8::LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint32_t flags = _lbCurrentDrawFlags;
    const unsigned mode = flags & 8 ? 1 : flags & 0x10 ? 2 : 0;
    const uint8_t index = colour.mIndex;
    const uint8_t value = mode == 0 && (flags & 0x20) ? _lbpCurrentFadeRow[index] : index;
    const auto add = [](int32_t a, int32_t b) { return static_cast<int32_t>(uint32_t(a) + uint32_t(b)); };
    const auto sub = [](int32_t a, int32_t b) { return static_cast<int32_t>(uint32_t(a) - uint32_t(b)); };
    const auto pixel = [&](int32_t px, int32_t py) {
        if (px < _lbCurrentClipRect.left || px >= _lbCurrentClipRect.right ||
            py < _lbCurrentClipRect.top || py >= _lbCurrentClipRect.bottom) return;
        const int32_t offset = static_cast<int32_t>(uint32_t(px) + uint32_t(py) * uint32_t(_lbpCurrentRA->mStride));
        uint8_t* destination = _lbCurrentOriginPtr + offset;
        if (mode == 1) *destination = _lbCurrentGhostTable[uint32_t(index) * 256 + *destination];
        else if (mode == 2) *destination = _lbCurrentGhostTable[index + uint32_t(*destination) * 256];
        else *destination = value;
    };
    if (radius == 0) { pixel(x,y); return; }
    if (radius == 1) {
        pixel(sub(x,1),y); pixel(x,sub(y,1)); pixel(add(x,1),y);
        pixel(x,add(y,1)); pixel(x,y);
        return;
    }
    const auto octants = [&](int32_t a, int32_t b) {
        // Duplicate axis/diagonal visits are intentional: ghosting reads the
        // value written by the preceding visit and need not be idempotent.
        pixel(sub(x,a),sub(y,b)); pixel(add(x,a),sub(y,b));
        pixel(sub(x,a),add(y,b)); pixel(add(x,a),add(y,b));
        pixel(sub(x,b),sub(y,a)); pixel(add(x,b),sub(y,a));
        pixel(sub(x,b),add(y,a)); pixel(add(x,b),add(y,a));
    };
    int32_t a = 0, b = static_cast<int32_t>(radius);
    uint32_t decision = 3 - 2 * radius;
    for (; a < b; ++a) {
        octants(a,b);
        if (static_cast<int32_t>(decision) < 0) decision += uint32_t(a) * 4 + 6;
        else {
            decision += (uint32_t(a) - uint32_t(b)) * 4 + 10;
            --b;
        }
    }
    if (a == b) octants(a,b);
}

void RenderContext8::LbDraw_CircleFilled(int32_t x,int32_t y,uint32_t radius,TbColour colour) { CircleFilled<1>(x,y,radius,Writer8(colour)); }
void RenderContext8::LbDraw_Triangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,TbColour colour) {
    Triangle<1>(x0,y0,x1,y1,x2,y2,Writer8(colour));
}
void RenderContext8::LbDraw_ClearClipWindow(TbColour colour) {
    const uint8_t value=colour.mIndex; ClearClipWindow<1>([value](uint8_t*d){*d=value;});
}
void RenderContext8::LbDraw_RectangleFilled(const TbRect& rect,TbColour colour) { RectangleFilled<1>(rect,Writer8(colour)); }
void RenderContext8::LbDraw_PropText(int32_t x,int32_t y,const char* text,TbColour colour) {
    const uint8_t value=colour.mIndex; PropText<1>(x,y,text,[value](uint8_t*d){*d=value;});
}
void RenderContext8::LbDraw_UnicodePropText(int32_t x,int32_t y,const char16_t* text,TbColour colour) {
    const uint8_t value=colour.mIndex; PropText<1>(x,y,text,[value](uint8_t*d){*d=value;});
}

void RenderContext24::LbDraw_Pixel(int32_t x, int32_t y, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    Pixel<3>(x, y, [value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_Rectangle(const TbRect& rect, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_RectangleFilled(rect, colour);
    else LbDraw_RectangleOutline(rect, colour);
}
void RenderContext24::LbDraw_RectangleOutline(const TbRect& rect, TbColour colour) {
    const uint32_t v=ColourWord(colour); RectangleOutline<3>(rect,[v](uint8_t*d){d[0]=uint8_t(v);d[1]=uint8_t(v>>8);d[2]=uint8_t(v>>16);});
}
void RenderContext24::LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    Line<3>(x0, y0, x1, y1, [value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    HorizontalLine<3>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    VerticalLine<3>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_CircleFilled(x, y, radius, colour);
    else LbDraw_CircleOutline(x, y, radius, colour);
}

void RenderContext24::LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    CircleOutline<3>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value);
        destination[1] = static_cast<uint8_t>(value >> 8);
        destination[2] = static_cast<uint8_t>(value >> 16);
    });
}

void RenderContext24::LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    CircleFilled<3>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value);
        destination[1] = static_cast<uint8_t>(value >> 8);
        destination[2] = static_cast<uint8_t>(value >> 16);
    });
}

void RenderContext24::LbDraw_Triangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,TbColour colour) {
    const uint32_t v=ColourWord(colour); Triangle<3>(x0,y0,x1,y1,x2,y2,[v](uint8_t*d){d[0]=uint8_t(v);d[1]=uint8_t(v>>8);d[2]=uint8_t(v>>16);});
}
void RenderContext24::LbDraw_ClearClipWindow(TbColour colour) {
    const uint32_t value = uint32_t(colour.mAlpha) << 24 | uint32_t(colour.mRed) << 16 |
                           uint32_t(colour.mGreen) << 8 | colour.mBlue;
    ClearClipWindow<3>([value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_RectangleFilled(const TbRect& rect, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    RectangleFilled<3>(rect, [value](uint8_t* d) { d[0] = uint8_t(value); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value >> 16); });
}

void RenderContext24::LbDraw_PropText(int32_t x,int32_t y,const char* text,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    PropText<3>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v);d[1]=uint8_t(v>>8);d[2]=uint8_t(v>>16);});
}
void RenderContext24::LbDraw_UnicodePropText(int32_t x,int32_t y,const char16_t* text,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    PropText<3>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v);d[1]=uint8_t(v>>8);d[2]=uint8_t(v>>16);});
}

void RenderContext24::LbDraw_Sprite(int32_t x,int32_t y,const TbSprite* sprite) {
    SpritePixels<3>(x,y,*sprite,[](uint8_t*d,uint8_t index){
        const uint8_t*p=reinterpret_cast<const uint8_t*>(&_lbGlobalPalette)+uint32_t(index)*4;
        d[0]=p[2];d[1]=p[1];d[2]=p[0];
    });
}
void RenderContext24::LbDraw_OneColourSprite(int32_t x,int32_t y,const TbSprite* sprite,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    OneColourSpritePixels<3>(x,y,*sprite,[v](uint8_t*d){d[0]=uint8_t(v);d[1]=uint8_t(v>>8);d[2]=uint8_t(v>>16);});
}
void RenderContext32::LbDraw_Pixel(int32_t x, int32_t y, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    Pixel<4>(x, y, [value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_Rectangle(const TbRect& rect, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_RectangleFilled(rect, colour);
    else LbDraw_RectangleOutline(rect, colour);
}
void RenderContext32::LbDraw_RectangleOutline(const TbRect& rect, TbColour colour) {
    const uint32_t v=ColourWord(colour); RectangleOutline<4>(rect,[v](uint8_t*d){d[0]=uint8_t(v>>16);d[1]=uint8_t(v>>8);d[2]=uint8_t(v);d[3]=uint8_t(v>>24);});
}
void RenderContext32::LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    Line<4>(x0, y0, x1, y1, [value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    HorizontalLine<4>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    VerticalLine<4>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_CircleFilled(x, y, radius, colour);
    else LbDraw_CircleOutline(x, y, radius, colour);
}

void RenderContext32::LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    CircleOutline<4>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value >> 16);
        destination[1] = static_cast<uint8_t>(value >> 8);
        destination[2] = static_cast<uint8_t>(value);
        destination[3] = static_cast<uint8_t>(value >> 24);
    });
}

void RenderContext32::LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    CircleFilled<4>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value >> 16);
        destination[1] = static_cast<uint8_t>(value >> 8);
        destination[2] = static_cast<uint8_t>(value);
        destination[3] = static_cast<uint8_t>(value >> 24);
    });
}

void RenderContext32::LbDraw_Triangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,TbColour colour) {
    const uint32_t v=ColourWord(colour); Triangle<4>(x0,y0,x1,y1,x2,y2,[v](uint8_t*d){d[0]=uint8_t(v>>16);d[1]=uint8_t(v>>8);d[2]=uint8_t(v);d[3]=uint8_t(v>>24);});
}
void RenderContext32::LbDraw_ClearClipWindow(TbColour colour) {
    const uint32_t value = uint32_t(colour.mAlpha) << 24 | uint32_t(colour.mRed) << 16 |
                           uint32_t(colour.mGreen) << 8 | colour.mBlue;
    ClearClipWindow<4>([value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_RectangleFilled(const TbRect& rect, TbColour colour) {
    const uint32_t value = ColourWord(colour);
    RectangleFilled<4>(rect, [value](uint8_t* d) { d[0] = uint8_t(value >> 16); d[1] = uint8_t(value >> 8); d[2] = uint8_t(value); d[3] = uint8_t(value >> 24); });
}

void RenderContext32::LbDraw_PropText(int32_t x,int32_t y,const char* text,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    PropText<4>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v>>16);d[1]=uint8_t(v>>8);d[2]=uint8_t(v);d[3]=uint8_t(v>>24);});
}
void RenderContext32::LbDraw_UnicodePropText(int32_t x,int32_t y,const char16_t* text,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    PropText<4>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v>>16);d[1]=uint8_t(v>>8);d[2]=uint8_t(v);d[3]=uint8_t(v>>24);});
}

void RenderContext32::LbDraw_Sprite(int32_t x,int32_t y,const TbSprite* sprite) {
    SpritePixels<4>(x,y,*sprite,[](uint8_t*d,uint8_t index){
        const uint8_t*p=reinterpret_cast<const uint8_t*>(&_lbGlobalPalette)+uint32_t(index)*4;
        d[0]=p[0];d[1]=p[1];d[2]=p[2];d[3]=0xff;
    });
}
void RenderContext32::LbDraw_OneColourSprite(int32_t x,int32_t y,const TbSprite* sprite,TbColour colour) {
    const uint32_t v=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    OneColourSpritePixels<4>(x,y,*sprite,[v](uint8_t*d){d[0]=uint8_t(v>>16);d[1]=uint8_t(v>>8);d[2]=uint8_t(v);d[3]=uint8_t(v>>24);});
}
void RenderContext16::LbDraw_Pixel(int32_t x, int32_t y, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    Pixel<2>(x, y, [value](uint8_t* d) { d[0] = uint8_t(value >> 8); d[1] = uint8_t(value); });
}

void RenderContext16::LbDraw_Rectangle(const TbRect& rect, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_RectangleFilled(rect, colour);
    else LbDraw_RectangleOutline(rect, colour);
}
void RenderContext16::LbDraw_RectangleOutline(const TbRect& rect, TbColour colour) {
    const uint16_t v=Pack16(ColourWord(colour)); RectangleOutline<2>(rect,[v](uint8_t*d){d[0]=uint8_t(v>>8);d[1]=uint8_t(v);});
}
void RenderContext16::LbDraw_Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    Line<2>(x0, y0, x1, y1, [value](uint8_t* d) { d[0] = uint8_t(value >> 8); d[1] = uint8_t(value); });
}

void RenderContext16::LbDraw_HorizontalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    HorizontalLine<2>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value >> 8); d[1] = uint8_t(value); });
}

void RenderContext16::LbDraw_VerticalLine(int32_t x, int32_t y, int32_t length, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    VerticalLine<2>(x, y, length, [value](uint8_t* d) { d[0] = uint8_t(value >> 8); d[1] = uint8_t(value); });
}

void RenderContext16::LbDraw_Circle(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    if ((_lbCurrentDrawFlags & 4) == 0) LbDraw_CircleFilled(x, y, radius, colour);
    else LbDraw_CircleOutline(x, y, radius, colour);
}

void RenderContext16::LbDraw_CircleOutline(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    CircleOutline<2>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value >> 8);
        destination[1] = static_cast<uint8_t>(value);
    });
}

void RenderContext16::LbDraw_CircleFilled(int32_t x, int32_t y, uint32_t radius, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    CircleFilled<2>(x, y, radius, [value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value >> 8);
        destination[1] = static_cast<uint8_t>(value);
    });
}

void RenderContext16::LbDraw_Triangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,TbColour colour) {
    const uint16_t v=Pack16(ColourWord(colour)); Triangle<2>(x0,y0,x1,y1,x2,y2,[v](uint8_t*d){d[0]=uint8_t(v>>8);d[1]=uint8_t(v);});
}
void RenderContext16::LbDraw_ClearClipWindow(TbColour colour) {
    const uint16_t value = Pack16(uint32_t(colour.mAlpha) << 24 | uint32_t(colour.mRed) << 16 |
                                  uint32_t(colour.mGreen) << 8 | colour.mBlue);
    ClearClipWindow<2>([value](uint8_t* destination) {
        destination[0] = static_cast<uint8_t>(value >> 8);
        destination[1] = static_cast<uint8_t>(value);
    });
}

void RenderContext16::LbDraw_RectangleFilled(const TbRect& rect, TbColour colour) {
    const uint16_t value = Pack16(ColourWord(colour));
    RectangleFilled<2>(rect, [value](uint8_t* d) { d[0] = uint8_t(value >> 8); d[1] = uint8_t(value); });
}

void RenderContext16::LbDraw_PropText(int32_t x,int32_t y,const char* text,TbColour colour) {
    const uint32_t w=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    const uint16_t v=Pack16(w); PropText<2>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v>>8);d[1]=uint8_t(v);});
}
void RenderContext16::LbDraw_UnicodePropText(int32_t x,int32_t y,const char16_t* text,TbColour colour) {
    const uint32_t w=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    const uint16_t v=Pack16(w); PropText<2>(x,y,text,[v](uint8_t*d){d[0]=uint8_t(v>>8);d[1]=uint8_t(v);});
}

void RenderContext16::LbDraw_Sprite(int32_t x,int32_t y,const TbSprite* sprite) {
    SpritePixels<2>(x,y,*sprite,[](uint8_t*d,uint8_t index){
        const uint8_t*p=reinterpret_cast<const uint8_t*>(&_lbGlobalPalette)+uint32_t(index)*4;
        const uint16_t v=Pack16(0xff000000u|uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2]);
        d[0]=uint8_t(v>>8);d[1]=uint8_t(v);
    });
}
void RenderContext16::LbDraw_OneColourSprite(int32_t x,int32_t y,const TbSprite* sprite,TbColour colour) {
    const uint32_t w=uint32_t(colour.mAlpha)<<24|uint32_t(colour.mRed)<<16|uint32_t(colour.mGreen)<<8|colour.mBlue;
    const uint16_t v=Pack16(w); OneColourSpritePixels<2>(x,y,*sprite,[v](uint8_t*d){d[0]=uint8_t(v>>8);d[1]=uint8_t(v);});
}
