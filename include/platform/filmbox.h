// filmbox.h: where the intro films are drawn on a screen of any size. Pure integer arithmetic (no window, no game
// state), so tests/host_filmbox.cpp checks it on its own; src/game/Movie.cpp uses it in MOVIE_Play for a screen wider than 4:3.
//
// The films (Movies/bf.mov, bfj.mov, grav.mov, roll.mov and the others) are QuickTime SVQ1 stored at half width
// (320 x 352): the frame is a whole picture whose intended display box is 640 x 352. The Mac game played it in its
// 640 x 480 window with 64 black rows above and below (GameSys.cpp MovieTop(): the screen's height * 64 / 480).
#ifndef TPW_PLATFORM_FILMBOX_H
#define TPW_PLATFORM_FILMBOX_H

#include <cstdint>

// The picture's display box in the original game, and so its aspect.
constexpr int32_t kFilmPictureWidth = 640;
constexpr int32_t kFilmPictureHeight = 352;
// The most of the picture's overflowing dimension (percent) that filling the screen may crop; beyond it the picture is
// fitted inside the screen with bars instead.
constexpr int32_t kFilmMaxCropPercent = 15;

// The film's box in screen pixels; right and bottom are exclusive, and a side may lie outside the screen (0 .. width,
// 0 .. height) when the picture is cropped. `filled` is true when the picture covers the whole screen.
struct FilmBox {
    int32_t left, top, right, bottom;
    bool filled;
};

// The box for a screen of width x height pixels. FILL scales the picture to cover the screen, centred, and crops the
// overflow equally on both sides; when that crop is more than kFilmMaxCropPercent of the overflowing dimension, FIT
// scales it to touch the screen from inside, centred, with bars on the other dimension. On a 4:3 screen this is the
// original game's placement (the full width, 352/640 of it high, centred).
inline FilmBox FilmBoxFor(int32_t width, int32_t height) {
    FilmBox box = {0, 0, width, height, false};
    if (width <= 0 || height <= 0) return box;
    const int64_t w = width, h = height;
    // The picture is wider than the screen when w * 352 < h * 640: then its height fills and its width overflows.
    const bool widthOverflows = w * kFilmPictureHeight < h * kFilmPictureWidth;
    int64_t boxWidth, boxHeight;
    bool crop = false;
    if (widthOverflows) {   // FILL: height h, width h * 640 / 352 (rounded up so the box covers the screen)
        boxHeight = h;
        boxWidth = (h * kFilmPictureWidth + kFilmPictureHeight - 1) / kFilmPictureHeight;
        crop = (boxWidth - w) * 100 <= kFilmMaxCropPercent * boxWidth;
    } else {                // FILL: width w, height w * 352 / 640 (rounded up)
        boxWidth = w;
        boxHeight = (w * kFilmPictureHeight + kFilmPictureWidth - 1) / kFilmPictureWidth;
        crop = (boxHeight - h) * 100 <= kFilmMaxCropPercent * boxHeight;
    }
    if (!crop) {            // FIT: the larger dimension of the screen is not covered
        if (widthOverflows) {   // the screen is narrower than the picture: touch the sides
            boxWidth = w;
            boxHeight = w * kFilmPictureHeight / kFilmPictureWidth;
        } else {
            boxHeight = h;
            boxWidth = h * kFilmPictureWidth / kFilmPictureHeight;
        }
    }
    // Centred: a negative origin is the crop on the left or top, (box - screen) / 2 rounded down.
    const int64_t left = boxWidth >= w ? -((boxWidth - w) / 2) : (w - boxWidth) / 2;
    const int64_t top = boxHeight >= h ? -((boxHeight - h) / 2) : (h - boxHeight) / 2;
    box.left = static_cast<int32_t>(left);
    box.top = static_cast<int32_t>(top);
    box.right = static_cast<int32_t>(left + boxWidth);
    box.bottom = static_cast<int32_t>(top + boxHeight);
    box.filled = crop;
    return box;
}

#endif
