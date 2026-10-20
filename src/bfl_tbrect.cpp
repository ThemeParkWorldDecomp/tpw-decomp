// The Bullfrog library's rectangle class (bullfrog symbols shared, LbDraw.cpp range): TbRect::Intersection 0x10029f8c, which
// the ltms font render method calls through the library's export (unit w45a). TbRect::Bounding (0x1002a110, unit w58) is below it.
#include "TbGraphicFile.h"

// 0x10029f8c: each side takes the stricter of the two rectangles; a right or bottom that is then below its left or top is
// raised to it. The answer is never inverted, and unlike LbRect_Intersection (0x10029ad0) it cuts nothing in place and
// does not say whether anything is left (an empty answer has right == left or bottom == top).
TbRect TbRect::Intersection(const TbRect& other) const {
    const int32_t cutLeft = left > other.left ? left : other.left;
    const int32_t cutTop = top > other.top ? top : other.top;
    int32_t cutRight = right < other.right ? right : other.right;
    int32_t cutBottom = bottom < other.bottom ? bottom : other.bottom;
    if (cutRight < cutLeft) cutRight = cutLeft;
    if (cutBottom < cutTop) cutBottom = cutTop;
    return {cutLeft, cutTop, cutRight, cutBottom};
}

// 0x1002a110: the lower of the two lefts and of the two tops and the higher of the two rights and bottoms, whatever the
// rectangles are (an empty or inverted one counts by its numbers).
TbRect TbRect::Bounding(const TbRect& other) const {
    const int32_t boundLeft = left < other.left ? left : other.left;
    const int32_t boundTop = top < other.top ? top : other.top;
    const int32_t boundRight = right > other.right ? right : other.right;
    const int32_t boundBottom = bottom > other.bottom ? bottom : other.bottom;
    return {boundLeft, boundTop, boundRight, boundBottom};
}
