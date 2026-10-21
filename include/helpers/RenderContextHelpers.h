// RenderContextHelpers.h: file-local helpers that src/bullfrog/RenderContext.cpp shares with LbTime.cpp. Before the
// regroup, the functions using them shared a source file (1 file), which had one copy of each; that copy now lives
// here, static inline, so each file that includes this header keeps its own internal copy.
#pragma once

#include "LbDraw.h"
#include "LbDrawText.h"
#include "LbSurface.h"
#include "TpwPropFont.h"

static inline int32_t Add(int32_t a, int32_t b) {
    return static_cast<int32_t>(uint32_t(a) + uint32_t(b));
}

static inline int32_t Subtract(int32_t a, int32_t b) {
    return static_cast<int32_t>(uint32_t(a) - uint32_t(b));
}

static inline int32_t Fixed16(int32_t value) {
    return static_cast<int32_t>(uint32_t(value) << 16);
}

static inline int32_t FixedSlope(int32_t from, int32_t to, int32_t height) {
    const int32_t delta = Subtract(to, from);
    return static_cast<int32_t>(uint32_t(delta) * 0x10000u) / height;
}

template<unsigned BytesPerPixel, class Writer>
static inline void Pixel(int32_t x, int32_t y, Writer write) {
    if (x < _lbCurrentClipRect.left || x >= _lbCurrentClipRect.right ||
        y < _lbCurrentClipRect.top || y >= _lbCurrentClipRect.bottom) return;
    const int32_t offset = static_cast<int32_t>(uint32_t(x) * BytesPerPixel +
        uint32_t(y) * uint32_t(_lbpCurrentRA->mStride));
    write(_lbCurrentOriginPtr + offset);
}

template<unsigned BytesPerPixel, class Writer>
static inline void HorizontalLine(int32_t x, int32_t y, int32_t length, Writer write);

template<unsigned BytesPerPixel, class Writer>
static inline void CircleOutline(int32_t x, int32_t y, uint32_t radius, Writer write) {
    const auto pixel = [&](int32_t px, int32_t py) {
        Pixel<BytesPerPixel>(px, py, write);
    };

    if (radius == 0) {
        pixel(x, y);
        return;
    }
    if (radius == 1) {
        pixel(Subtract(x, 1), y);
        pixel(x, Subtract(y, 1));
        pixel(Add(x, 1), y);
        pixel(x, Add(y, 1));
        pixel(x, y);
        return;
    }

    const auto octants = [&](int32_t a, int32_t b) {
        pixel(Subtract(x, a), Subtract(y, b));
        pixel(Add(x, a), Subtract(y, b));
        pixel(Subtract(x, a), Add(y, b));
        pixel(Add(x, a), Add(y, b));
        pixel(Subtract(x, b), Subtract(y, a));
        pixel(Add(x, b), Subtract(y, a));
        pixel(Subtract(x, b), Add(y, a));
        pixel(Add(x, b), Add(y, a));
    };

    int32_t a = 0;
    int32_t b = static_cast<int32_t>(radius);
    uint32_t decision = 3 - 2 * radius;
    for (; a < b; ++a) {
        octants(a, b);
        if (static_cast<int32_t>(decision) < 0) decision += uint32_t(a) * 4 + 6;
        else {
            decision += (uint32_t(a) - uint32_t(b)) * 4 + 10;
            --b;
        }
    }
    if (a == b) octants(a, b);
}

template<unsigned BytesPerPixel, class Writer>
static inline void VerticalLine(int32_t x, int32_t y, int32_t length, Writer write) {
    if (length < 0) {
        y = Add(y, length);
        length = -length;
    }
    if (x < _lbCurrentClipRect.left || x >= _lbCurrentClipRect.right) return;
    int32_t end = Add(y, length);
    if (end < _lbCurrentClipRect.top || y > _lbCurrentClipRect.bottom) return;
    if (y < _lbCurrentClipRect.top) y = _lbCurrentClipRect.top;
    if (end > _lbCurrentClipRect.bottom) end = _lbCurrentClipRect.bottom;
    if (y >= end) return;
    int32_t offset = static_cast<int32_t>(uint32_t(x) * BytesPerPixel +
        uint32_t(y) * uint32_t(_lbpCurrentRA->mStride));
    uint8_t* destination = _lbCurrentOriginPtr + offset;
    while (y++ < end) {
        write(destination);
        destination += _lbpCurrentRA->mStride;
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void Line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, Writer write) {
    if (x0 == x1) {
        VerticalLine<BytesPerPixel>(x0, y0, y1 - y0, write);
        return;
    }
    if (y0 == y1) {
        HorizontalLine<BytesPerPixel>(x0, y0, x1 - x0, write);
        return;
    }

    if (x0 < _lbCurrentClipRect.left) {
        if (x1 < _lbCurrentClipRect.left) return;
        y0 -= ((y0 - y1) * (x0 - _lbCurrentClipRect.left)) / (x0 - x1);
        x0 = _lbCurrentClipRect.left;
        if (x1 >= _lbCurrentClipRect.right) {
            y1 -= ((y1 - y0) * (x1 - (_lbCurrentClipRect.right - 1))) /
                  (x1 - _lbCurrentClipRect.left);
            x1 = _lbCurrentClipRect.right - 1;
        }
    } else if (x0 < _lbCurrentClipRect.right) {
        if (x1 < _lbCurrentClipRect.left) {
            y1 -= ((y1 - y0) * (x1 - _lbCurrentClipRect.left)) / (x1 - x0);
            x1 = _lbCurrentClipRect.left;
        } else if (x1 >= _lbCurrentClipRect.right) {
            const int32_t old_x1 = x1;
            x1 = _lbCurrentClipRect.right - 1;
            y1 -= ((y1 - y0) * (old_x1 - x1)) / (old_x1 - x0);
        }
    } else {
        if (x1 >= _lbCurrentClipRect.right) return;
        const int32_t clipped = _lbCurrentClipRect.right - 1;
        y0 -= ((y0 - y1) * (x0 - clipped)) / (x0 - x1);
        x0 = clipped;
        if (x1 < _lbCurrentClipRect.left) {
            y1 -= ((y1 - y0) * (x1 - _lbCurrentClipRect.left)) / (x1 - x0);
            x1 = _lbCurrentClipRect.left;
        }
    }

    if (y0 < _lbCurrentClipRect.top) {
        if (y1 < _lbCurrentClipRect.top) return;
        x0 -= ((x0 - x1) * (y0 - _lbCurrentClipRect.top)) / (y0 - y1);
        y0 = _lbCurrentClipRect.top;
        if (y1 >= _lbCurrentClipRect.bottom) {
            x1 -= ((x1 - x0) * (y1 - (_lbCurrentClipRect.bottom - 1))) /
                  (y1 - _lbCurrentClipRect.top);
            y1 = _lbCurrentClipRect.bottom - 1;
        }
    } else if (y0 < _lbCurrentClipRect.bottom) {
        if (y1 < _lbCurrentClipRect.top) {
            x1 -= ((x1 - x0) * (y1 - _lbCurrentClipRect.top)) / (y1 - y0);
            y1 = _lbCurrentClipRect.top;
        } else if (y1 >= _lbCurrentClipRect.bottom) {
            const int32_t old_y1 = y1;
            y1 = _lbCurrentClipRect.bottom - 1;
            x1 -= ((x1 - x0) * (old_y1 - y1)) / (old_y1 - y0);
        }
    } else {
        if (y1 >= _lbCurrentClipRect.bottom) return;
        const int32_t clipped = _lbCurrentClipRect.bottom - 1;
        x0 -= ((x0 - x1) * (y0 - clipped)) / (y0 - y1);
        y0 = clipped;
        if (y1 < _lbCurrentClipRect.top) {
            x1 -= ((x1 - x0) * (y1 - _lbCurrentClipRect.top)) / (y1 - y0);
            y1 = _lbCurrentClipRect.top;
        }
    }

    const int32_t dx = x1 >= x0 ? x1 - x0 : x0 - x1;
    const int32_t dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    const int32_t sx = x1 - x0 < 1 ? -1 : 1;
    const int32_t sy = y1 - y0 < 1 ? -1 : 1;
    if (dy < dx) {
        int32_t error = -dx;
        for (;;) {
            error += dy * 2;
            Pixel<BytesPerPixel>(x0, y0, write);
            if (x0 == x1) break;
            if (error >= 0) {
                y0 += sy;
                error -= dx * 2;
            }
            x0 += sx;
        }
    } else {
        int32_t error = -dy;
        for (;;) {
            error += dx * 2;
            Pixel<BytesPerPixel>(x0, y0, write);
            if (y0 == y1) break;
            if (error >= 0) {
                x0 += sx;
                error -= dy * 2;
            }
            y0 += sy;
        }
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void HorizontalLine(int32_t x, int32_t y, int32_t length, Writer write) {
    if (length < 0) {
        x = Add(x, length);
        length = -length;
    }
    if (y < _lbCurrentClipRect.top || y >= _lbCurrentClipRect.bottom || length <= 0) return;

    int32_t end = Add(x, length);
    if (x < _lbCurrentClipRect.left) x = _lbCurrentClipRect.left;
    if (end > _lbCurrentClipRect.right) end = _lbCurrentClipRect.right;
    if (x >= end) return;

    int32_t offset = static_cast<int32_t>(uint32_t(x) * BytesPerPixel +
        uint32_t(y) * uint32_t(_lbpCurrentRA->mStride));
    uint8_t* destination = _lbCurrentOriginPtr + offset;
    while (x++ < end) {
        write(destination);
        destination += BytesPerPixel;
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void CircleFilled(int32_t x, int32_t y, uint32_t radius, Writer write) {
    if (radius < 2) {
        CircleOutline<BytesPerPixel>(x, y, radius, write);
        return;
    }

    int32_t b = static_cast<int32_t>(radius);
    int32_t decision = static_cast<int32_t>(uint32_t(radius) * uint32_t(-2) + 3);
    HorizontalLine<BytesPerPixel>(Subtract(x, b), y, Add(b, b) + 1, write);
    if (decision < 0) {
        decision = static_cast<int32_t>(uint32_t(radius) * uint32_t(-2) + 9);
    } else {
        HorizontalLine<BytesPerPixel>(x, Subtract(y, b), 1, write);
        HorizontalLine<BytesPerPixel>(x, Add(y, b), 1, write);
        --b;
        decision = Add(decision, static_cast<int32_t>(uint32_t(radius) * uint32_t(-4) + 10));
    }

    int32_t span = Add(b, b) + 1;
    int32_t step = 4;
    int32_t a = 1;
    for (; a < b; ++a) {
        HorizontalLine<BytesPerPixel>(Subtract(x, b), Subtract(y, a), span, write);
        HorizontalLine<BytesPerPixel>(Subtract(x, b), Add(y, a), span, write);
        int32_t delta;
        if (decision < 0) delta = step + 6;
        else {
            const int32_t narrow = Add(a, a) + 1;
            HorizontalLine<BytesPerPixel>(Subtract(x, a), Subtract(y, b), narrow, write);
            HorizontalLine<BytesPerPixel>(Subtract(x, a), Add(y, b), narrow, write);
            delta = static_cast<int32_t>((uint32_t(a) - uint32_t(b)) * 4 + 10);
            span -= 2;
            --b;
        }
        decision = Add(decision, delta);
        step += 4;
    }
    if (a == b) {
        const int32_t final_span = Add(b, b) + 1;
        HorizontalLine<BytesPerPixel>(Subtract(x, b), Subtract(y, a), final_span, write);
        HorizontalLine<BytesPerPixel>(Subtract(x, b), Add(y, a), final_span, write);
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void RectangleFilled(const TbRect& rect, Writer write) {
    TbRect inside = {
        _lbCurrentClipRect.left > rect.left ? _lbCurrentClipRect.left : rect.left,
        _lbCurrentClipRect.top > rect.top ? _lbCurrentClipRect.top : rect.top,
        _lbCurrentClipRect.right < rect.right ? _lbCurrentClipRect.right : rect.right,
        _lbCurrentClipRect.bottom < rect.bottom ? _lbCurrentClipRect.bottom : rect.bottom,
    };
    if (inside.right < inside.left) inside.right = inside.left;
    if (inside.bottom < inside.top) inside.bottom = inside.top;
    const int32_t width = inside.right - inside.left;
    const int32_t height = inside.bottom - inside.top;
    int32_t offset = static_cast<int32_t>(uint32_t(inside.left) * BytesPerPixel +
        uint32_t(inside.top) * uint32_t(_lbpCurrentRA->mStride));
    uint8_t* row = _lbCurrentOriginPtr + offset;
    for (int32_t y = 0; y < height; ++y) {
        uint8_t* destination = row;
        for (int32_t x = 0; x < width; ++x) {
            write(destination);
            destination += BytesPerPixel;
        }
        row += _lbpCurrentRA->mStride;
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void RectangleOutline(const TbRect& rect, Writer write) {
    const int32_t width = rect.right - rect.left;
    const int32_t height = rect.bottom - rect.top;
    HorizontalLine<BytesPerPixel>(rect.left, rect.top, width - 1, write);
    VerticalLine<BytesPerPixel>(rect.right - 1, rect.top, height - 1, write);
    HorizontalLine<BytesPerPixel>(rect.left + 1, rect.bottom - 1, width - 1, write);
    VerticalLine<BytesPerPixel>(rect.left, rect.top + 1, height - 1, write);
}

template<unsigned BytesPerPixel, class Writer>
static inline void Triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
              int32_t x2, int32_t y2, Writer write) {
    if ((_lbCurrentDrawFlags & 4) != 0) {
        Line<BytesPerPixel>(x0, y0, x1, y1, write);
        Line<BytesPerPixel>(x1, y1, x2, y2, write);
        Line<BytesPerPixel>(x2, y2, x0, y0, write);
        return;
    }
    if (y1 < y0) { int32_t tx=x0; x0=x1; x1=tx; int32_t ty=y0; y0=y1; y1=ty; }
    if (y2 < y1) { int32_t tx=x1; x1=x2; x2=tx; int32_t ty=y1; y1=y2; y2=ty; }
    if (y1 < y0) { int32_t tx=x0; x0=x1; x1=tx; int32_t ty=y0; y0=y1; y1=ty; }
    if (y0 == y1 && y1 == y2) return;

    int32_t long_step;
    int32_t long_x;
    int32_t short_x;
    int32_t short_step;
    if (y0 == y1) {
        if (x0 < x1) {
            long_step = FixedSlope(x0, x2, y2 - y0);
            long_x = Fixed16(x0); short_x = Fixed16(x1);
            short_step = FixedSlope(x1, x2, y2 - y1);
        } else {
            long_step = FixedSlope(x1, x2, y2 - y1);
            long_x = Fixed16(x1); short_x = Fixed16(x0);
            short_step = FixedSlope(x0, x2, y2 - y0);
        }
    } else {
        long_step = FixedSlope(x0, x2, y2 - y0);
        long_x = Fixed16(x0);
        const int32_t upper_step = FixedSlope(x0, x1, y1 - y0);
        short_x = long_x;
        if (upper_step <= long_step) {
            for (; y0 < y1; ++y0) {
                HorizontalLine<BytesPerPixel>(short_x >> 16, y0,
                    (long_x >> 16) - (short_x >> 16), write);
                long_x = Add(long_x, long_step); short_x = Add(short_x, upper_step);
            }
            if (y1 == y2) return;
            short_x = Fixed16(x1);
            short_step = FixedSlope(x1, x2, y2 - y1);
            for (; y0 < y2; ++y0) {
                HorizontalLine<BytesPerPixel>(short_x >> 16, y0,
                    (long_x >> 16) - (short_x >> 16), write);
                long_x = Add(long_x, long_step); short_x = Add(short_x, short_step);
            }
            return;
        }
        for (; y0 < y1; ++y0) {
            HorizontalLine<BytesPerPixel>(long_x >> 16, y0,
                (short_x >> 16) - (long_x >> 16), write);
            long_x = Add(long_x, long_step); short_x = Add(short_x, upper_step);
        }
        if (y1 == y2) return;
        short_x = Fixed16(x1);
        short_step = FixedSlope(x1, x2, y2 - y1);
    }
    for (; y0 < y2; ++y0) {
        HorizontalLine<BytesPerPixel>(long_x >> 16, y0,
            (short_x >> 16) - (long_x >> 16), write);
        long_x = Add(long_x, long_step); short_x = Add(short_x, short_step);
    }
}

template<unsigned BytesPerPixel, class Char, class Writer>
static inline void PropText(int32_t x, int32_t y, const Char* text, Writer write) {
    if (y < _lbCurrentClipRect.top || y + 6 > _lbCurrentClipRect.bottom ||
        x + 6 > _lbCurrentClipRect.right || text == nullptr || *text == 0) return;
    int32_t cursor = x;
    while (*text != 0) {
        int32_t ch;
        if constexpr (sizeof(Char) == 1) ch = static_cast<int8_t>(*text++);
        else ch = static_cast<uint16_t>(*text++);
        if (ch == 10) {
            y += 8; cursor = x;
            if (y + 6 > _lbCurrentClipRect.bottom) return;
            continue;
        }
        if (ch == 9) {
            cursor = (cursor + 32) & ~31;
            continue;
        }
        uint32_t glyph = uint32_t(ch - 0x20);
        if (glyph > 0x88) glyph = 0;
        const uint8_t* rows = kTpwPropFont8 + glyph * 8;
        const uint32_t advance = rows[6];
        if (cursor >= _lbCurrentClipRect.left && cursor + 6 <= _lbCurrentClipRect.right) {
            for (int32_t row = 0; row < 6; ++row) {
                const uint8_t bits = rows[row];
                for (int32_t column = 0; column < 5; ++column) {
                    if ((bits & (0x40 >> column)) != 0) {
                        const int32_t offset = static_cast<int32_t>(uint32_t(cursor + column) * BytesPerPixel +
                            uint32_t(y + row) * uint32_t(_lbpCurrentRA->mStride));
                        write(_lbCurrentOriginPtr + offset);
                    }
                }
            }
        }
        cursor += static_cast<int32_t>(advance);
    }
}

template<unsigned BytesPerPixel, class Writer>
static inline void OneColourSpritePixels(int32_t x, int32_t y, const TbSprite& sprite, Writer write) {
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
                ++data;
                const int32_t px = flip_x ? x + int32_t(sprite.mWidth) - 1 - column : x + column;
                const int32_t py = flip_y ? y + int32_t(sprite.mHeight) - 1 - int32_t(row) : y + int32_t(row);
                Pixel<BytesPerPixel>(px, py, write);
            }
        }
    }
}
