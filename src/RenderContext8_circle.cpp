// Unit w233: bullfrog RenderContext8::LbDraw_CircleOutline, 0x1002c7dc, 6892 B.
#include "RenderContext8.h"
#include "LbDraw.h"
#include "LbSurface.h"

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
