// host_filmbox.cpp: checks include/platform/filmbox.h (where the intro films are drawn) on its own, with no game and
// no window. Run by ctest ("filmbox"); exits nonzero on the first failure.
#include <cstdio>
#include <cstdlib>

#include "platform/filmbox.h"

static int gChecks = 0;

static void Expect(const char* name, int32_t width, int32_t height, int32_t left, int32_t top, int32_t right,
                   int32_t bottom, bool filled) {
    const FilmBox box = FilmBoxFor(width, height);
    ++gChecks;
    if (box.left != left || box.top != top || box.right != right || box.bottom != bottom || box.filled != filled) {
        std::fprintf(stderr, "FAILED %s (%d x %d): got %d,%d to %d,%d %s, wanted %d,%d to %d,%d %s\n", name, width, height,
                     box.left, box.top, box.right, box.bottom, box.filled ? "FILL" : "FIT", left, top, right, bottom,
                     filled ? "FILL" : "FIT");
        std::exit(1);
    }
}

#define CHECK(condition)                                                                \
    do {                                                                                \
        ++gChecks;                                                                      \
        if (!(condition)) {                                                             \
            std::fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            std::exit(1);                                                               \
        }                                                                               \
    } while (0)

int main() {
    // The worked cases. The picture is 640 x 352 (1.818:1).
    Expect("4:3 as the original", 640, 480, 0, 64, 640, 416, false);           // the original: 64 black rows above and below
    Expect("16:9", 1920, 1080, -22, 0, 1942, 1080, true);                      // height fills; 2.2 percent cropped left and right
    Expect("16:10", 1920, 1200, -131, 0, 2051, 1200, true);                    // 12.0 percent cropped left and right
    Expect("21:9", 2560, 1080, 298, 0, 2261, 1080, false);                     // a 23 percent crop would be needed: fitted, pillar bars
    Expect("5:4", 1280, 1024, 0, 160, 1280, 864, false);                       // a 31 percent crop would be needed: fitted
    Expect("the picture's own size", 640, 352, 0, 0, 640, 352, true);          // no crop

    // The original game's placement (MOVIE_Play: the full width, height * 352 / 480 high, the top height * 64 / 480
    // down, unsigned arithmetic) on the 4:3 sizes the game offers: the width is the screen's and the rows are the
    // original's to within the one pixel its rounding down (against this centring) can differ.
    const int32_t sizes[][2] = {{512, 384}, {640, 480}, {800, 600}, {1024, 768}, {1280, 960}, {1600, 1200}, {2048, 1536}, {4096, 3072}};
    for (const auto& size : sizes) {
        const int32_t w = size[0], h = size[1];
        const int32_t high = static_cast<int32_t>(static_cast<uint32_t>(h * 0x160) / 0x1e0);
        const int32_t top = static_cast<int32_t>((static_cast<uint32_t>(h) << 6) / 480);
        const FilmBox b = FilmBoxFor(w, h);
        CHECK(b.left == 0 && b.right == w && !b.filled);
        CHECK(b.top - top >= 0 && b.top - top <= 1 && b.bottom - (top + high) >= 0 && b.bottom - (top + high) <= 1);
    }

    // The policy at its edges: the crop limit itself is still a fill, one pixel more is a fit.
    // At height 352 the picture is 640 wide; a screen 544 wide crops 96 of 640 = exactly 15 percent.
    Expect("exactly the crop limit", 544, 352, -48, 0, 592, 352, true);
    Expect("one pixel past the limit", 543, 352, 0, 27, 543, 325, false);
    // The same on the other dimension: width 640 gives a 352 high picture; 299 high crops 53 of 352 (15.06 percent).
    Expect("the limit in height", 640, 300, -0, -26, 640, 326, true);
    CHECK(FilmBoxFor(640, 299).filled == false);

    // Properties over a grid of screens: a fill covers the screen with at most the limit cropped, centred; a fit lies
    // inside the screen and touches it on one dimension; the aspect stays 640:352 (within a pixel); a degenerate
    // screen answers its own rectangle.
    for (int32_t w = 100; w <= 5000; w += 97) {
        for (int32_t h = 100; h <= 3000; h += 89) {
            const FilmBox b = FilmBoxFor(w, h);
            const int64_t bw = b.right - b.left, bh = b.bottom - b.top;
            CHECK(bw > 0 && bh > 0);
            CHECK(bw * 352 - bh * 640 <= 640 && bh * 640 - bw * 352 <= 640);   // aspect, to a pixel of rounding
            if (b.filled) {
                CHECK(b.left <= 0 && b.top <= 0 && b.right >= w && b.bottom >= h);
                CHECK((b.left == 0 && b.right == w) || (b.top == 0 && b.bottom == h));   // one dimension exactly covered
                const int64_t cropX = bw - w, cropY = bh - h;
                CHECK(cropX * 100 <= kFilmMaxCropPercent * bw && cropY * 100 <= kFilmMaxCropPercent * bh);
                CHECK(-b.left - (b.right - w) >= -1 && -b.left - (b.right - w) <= 1);   // equal crop on both sides
            } else {
                CHECK(b.left >= 0 && b.top >= 0 && b.right <= w && b.bottom <= h);
                CHECK((b.left == 0 && b.right == w) || (b.top == 0 && b.bottom == h));   // touches on one dimension
                CHECK(b.left - (w - b.right) >= -1 && b.left - (w - b.right) <= 1);      // centred
                CHECK(b.top - (h - b.bottom) >= -1 && b.top - (h - b.bottom) <= 1);
            }
        }
    }
    const FilmBox none = FilmBoxFor(0, 0);
    CHECK(none.left == 0 && none.top == 0 && none.right == 0 && none.bottom == 0 && !none.filled);

    std::printf("filmbox: %d checks passed\n", gChecks);
    return 0;
}
