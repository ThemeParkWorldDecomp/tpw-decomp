#include "TbGraphicFile.h"

// 0x1002a06c: signed inclusive overlap test.
bool TbRect::Intersects(const TbRect& other) const {
    return left <= other.right && right >= other.left && top <= other.bottom && bottom >= other.top;
}

// 0x1002a1d4 (unit v5L16): the Mac swaps each pair with three xors through memory, the same as a swap.
void TbRect::Normalise() {
    if (left > right) {
        left ^= right;
        right ^= left;
        left ^= right;
    }
    if (top > bottom) {
        top ^= bottom;
        bottom ^= top;
        top ^= bottom;
    }
}
