// InterfaceWindow's text offset and size query (main app, unit w93).
#include <cstring>

#include "InterfaceWindow.h"
#include "ui_DrawObject.h"

// 0x101da484 (168 bytes). The Change record is made on the stack with only its text-style pointer set; the style holds the two
// offsets (flags 0x4000 and 0x8000 read them) and nothing else. The draw object goes first, then the client draw object.
void InterfaceWindow::SetTextOffset(int32_t x, int32_t y) {
    if (mDrawObject == nullptr && mClientDrawObject == nullptr) return;
    DOTextStyle style{};
    const int16_t offsetX = static_cast<int16_t>(x);
    const int16_t offsetY = static_cast<int16_t>(y);
    std::memcpy(&style.mF0A[0], &offsetX, sizeof offsetX);
    std::memcpy(&style.mF0A[2], &offsetY, sizeof offsetY);
    DOChange change{};
    change.mText = &style;
    if (mDrawObject != nullptr) mDrawObject->Change(0xc000, &change);
    if (mClientDrawObject != nullptr) mClientDrawObject->Change(0xc000, &change);
}

// 0x101dbeb0 (164 bytes).
int32_t InterfaceWindow::GetDims(int16_t* width, int16_t* height, int16_t adjust) {
    mRect.GetDims(width, height);
    if (adjust != 0) {
        if (width != nullptr) *width = static_cast<int16_t>(*width - (mAnimOffset[2] - mAnimOffset[0]));
        if (height != nullptr) *height = static_cast<int16_t>(*height - (mAnimOffset[3] - mAnimOffset[1]));
    }
    return 1;
}
