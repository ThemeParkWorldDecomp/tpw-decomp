// host_ground_pool.cpp: the far ground's procedural texture sizes (the port's fix groundpool, include/PortGroundPool.h) on the
// host, without game data: the levels a ring size has, the pixels a cell covers at a distance, the size chosen for those
// pixels and when a tile still does, the step down that keeps a view's cells in the pool's budget, and the shrink of a tile.
// Exits nonzero on the failed checks' count.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "PortGroundPool.h"

using namespace tpw;

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

void Levels() {
    Check(PortGroundLevelCount(64) == 4, "64 has four levels");
    Check(PortGroundLevelCount(128) == 4, "128 has four levels, the smallest 16");
    Check(PortGroundLevelCount(32) == 3, "32 has three levels");
    Check(PortGroundLevelCount(16) == 2, "16 has two levels");
    Check(PortGroundLevelCount(8) == 1, "8 has one level");
    Check(PortGroundLevelCount(4) == 1, "4 has only its own");
    Check(PortGroundSize(64, 0) == 64 && PortGroundSize(64, 1) == 32 && PortGroundSize(64, 3) == 8, "64's sizes");
    Check(PortGroundBytes(64, 0, 2) == 8192 && PortGroundBytes(64, 0, 4) == 16384, "a full tile's bytes");
    Check(PortGroundBytes(64, 2, 4) == 1024, "a quarter tile's bytes");
}

void Pixels() {
    // A cell 1 wide at distance 10, a 90 degree field of view (cotangent 1) on a picture 1024 pixels wide: 51.2 pixels.
    Check(std::fabs(PortGroundPixels(1.0f, 10.0f, 1.0f, 512.0f) - 51.2f) < 1e-3f, "pixels at 10");
    Check(std::fabs(PortGroundPixels(1.0f, 20.0f, 1.0f, 512.0f) - 25.6f) < 1e-3f, "half the pixels at twice the distance");
    Check(PortGroundPixels(1.0f, 0.0f, 1.0f, 512.0f) > 1.0e8f, "a cell at the camera covers the picture");
    Check(PortGroundPixels(1.0f, -1.0f, 1.0f, 512.0f) > 1.0e8f, "a cell behind the camera covers the picture");
    Check(PortGroundPixels(0.0f, 0.0f, 1.0f, 512.0f) == 0.0f, "a cell of no width covers nothing");
}

void Choice() {
    Check(PortGroundLevel(1000.0f, 64) == 0, "a near cell is full size");
    Check(PortGroundLevel(64.0f, 64) == 0, "64 pixels need the full size");
    Check(PortGroundLevel(33.0f, 64) == 0, "33 pixels need more than a half");
    Check(PortGroundLevel(32.0f, 64) == 1, "32 pixels take a half");
    Check(PortGroundLevel(17.0f, 64) == 1, "17 pixels take a half");
    Check(PortGroundLevel(16.0f, 64) == 2, "16 pixels take a quarter");
    Check(PortGroundLevel(9.0f, 64) == 2, "9 pixels take a quarter");
    Check(PortGroundLevel(8.0f, 64) == 3, "8 pixels take an eighth");
    Check(PortGroundLevel(0.5f, 64) == 3, "under a pixel takes the smallest");
    Check(PortGroundLevel(0.5f, 16) == 1, "16's smallest is 8");
    Check(PortGroundLevel(0.5f, 8) == 0, "8 has only its own");
    // The size never falls as the pixels grow, and always has a texel per pixel unless it is the full size.
    int32_t previous = 3;
    for (float pixels = 0.25f; pixels < 300.0f; pixels += 0.25f) {
        const int32_t level = PortGroundLevel(pixels, 64);
        Check(level <= previous, "the size never falls as a cell grows, at " + std::to_string(pixels));
        Check(level == 0 || static_cast<float>(PortGroundSize(64, level)) >= pixels,
              "a texel per pixel at " + std::to_string(pixels));
        previous = level;
    }
    // A tile larger than asked is kept; a smaller one within the slack is kept, past it made again; a step down counts.
    Check(!PortGroundRemake(0, 5.0f, 0, 64), "a full tile is kept for a far cell");
    Check(!PortGroundRemake(1, 32.0f, 0, 64), "the asked level is kept");
    Check(!PortGroundRemake(1, 40.0f, 0, 64), "a half is kept for 40 pixels (within the slack)");
    Check(PortGroundRemake(1, 41.0f, 0, 64), "a half is made again for 41 pixels");
    Check(PortGroundRemake(3, 64.0f, 0, 64), "an eighth is made again for 64 pixels");
    Check(PortGroundRemake(1, 200.0f, 0, 64), "a half is made again for a near cell");
    Check(!PortGroundRemake(1, 200.0f, 1, 64), "a half is kept for a near cell when every tile steps down once");
    Check(!PortGroundRemake(2, 33.0f, 1, 64), "a quarter is kept for 33 pixels a step down (within the slack)");
    Check(PortGroundRemake(2, 41.0f, 1, 64), "a quarter is made again for 41 pixels a step down");
    Check(!PortGroundRemake(3, 64.0f, 3, 64), "nothing is made again at the smallest step");
    // Never made again at the level it is made at.
    for (int32_t shift = 0; shift < 4; ++shift)
        for (float pixels = 0.25f; pixels < 300.0f; pixels += 0.25f)
            Check(!PortGroundRemake(PortGroundShifted(PortGroundLevel(pixels, 64), shift, 64), pixels, shift, 64),
                  "a new tile is kept at " + std::to_string(pixels) + " shift " + std::to_string(shift));
    Check(PortGroundBudgetFor(int64_t{248} << 18) == int64_t{62} << 18, "a quarter of a 248 page cache");
    Check(PortGroundBudgetFor(int64_t{1} << 30) == kPortGroundBudget, "never more than the budget");
    Check(PortGroundBudgetFor(0) == kPortGroundBudget, "no cache size known: the budget");
}

void Budget() {
    int32_t counts[kPortGroundLevels] = {0, 0, 0, 0};
    // 1000 cells asking full size at 2 bytes a pixel: 8 MB, in a 16 MB budget.
    counts[0] = 1000;
    Check(PortGroundShift(counts, 64, 2, kPortGroundBudget) == 0, "8 MB fit as asked");
    // At 4 bytes, 16 MB minus a little: fits; 2000 full tiles (32 MB) step down once to 8 MB.
    Check(PortGroundShift(counts, 64, 4, kPortGroundBudget) == 0, "1000 full tiles at 4 bytes fit");
    counts[0] = 2000;
    Check(PortGroundShift(counts, 64, 4, kPortGroundBudget) == 1, "2000 full tiles at 4 bytes step down once");
    // A whole map of 128 x 128 cells asking full size: 256 MB at 4 bytes, stepped down to 16 MB.
    counts[0] = 16384;
    Check(PortGroundShift(counts, 64, 4, kPortGroundBudget) == 2, "a whole map steps down twice");
    // Nothing fits: the smallest level.
    Check(PortGroundShift(counts, 64, 4, 1) == 3, "nothing fits: the smallest");
    // A mixed view: the shift chosen is the fewest that fits, and the bytes after it are in the budget.
    const int32_t mixed[kPortGroundLevels] = {600, 1500, 4000, 10000};
    for (int32_t bpp : {2, 4}) {
        const int32_t shift = PortGroundShift(mixed, 64, bpp, kPortGroundBudget);
        int64_t bytes = 0;
        for (int32_t level = 0; level < kPortGroundLevels; ++level)
            bytes += mixed[level] * PortGroundBytes(64, PortGroundShifted(level, shift, 64), bpp);
        Check(bytes <= kPortGroundBudget, "the mixed view fits at " + std::to_string(bpp) + " bytes");
        if (shift > 0) {
            int64_t fewer = 0;
            for (int32_t level = 0; level < kPortGroundLevels; ++level)
                fewer += mixed[level] * PortGroundBytes(64, PortGroundShifted(level, shift - 1, 64), bpp);
            Check(fewer > kPortGroundBudget, "one step fewer would not fit at " + std::to_string(bpp) + " bytes");
        }
    }
    Check(PortGroundShifted(1, 1, 64) == 2 && PortGroundShifted(3, 1, 64) == 3 && PortGroundShifted(2, 5, 64) == 3,
          "a shift never goes past the smallest");
    Check(PortGroundShifted(0, 1, 16) == 1 && PortGroundShifted(1, 1, 16) == 1, "16's shift stops at 8");
}

void Shrink() {
    const int32_t full = 8;
    std::vector<uint8_t> in(full * full * 4);
    for (int32_t y = 0; y < full; ++y)
        for (int32_t x = 0; x < full; ++x) {
            uint8_t* pixel = &in[(y * full + x) * 4];
            pixel[0] = 255;
            pixel[1] = static_cast<uint8_t>(x * 10);
            pixel[2] = static_cast<uint8_t>(y * 20);
            pixel[3] = static_cast<uint8_t>((x + y) % 2 == 0 ? 0 : 255);
        }
    std::vector<uint8_t> same(full * full * 4);
    PortGroundShrink(in.data(), full, 0, same.data());
    Check(same == in, "level 0 copies");
    std::vector<uint8_t> half(4 * 4 * 4);
    PortGroundShrink(in.data(), full, 1, half.data());
    // Pixel (1, 2) of the half covers x 2..3 and y 4..5: x*10 means 25, y*20 means 90, the checks mean 127.5 rounded 128.
    const uint8_t* pixel = &half[(2 * 4 + 1) * 4];
    Check(pixel[0] == 255 && pixel[1] == 25 && pixel[2] == 90 && pixel[3] == 128, "a half's pixel is its block's mean");
    std::vector<uint8_t> one(4);
    PortGroundShrink(in.data(), full, 3, one.data());
    Check(one[0] == 255 && one[1] == 35 && one[2] == 70 && one[3] == 128, "a single texel is the whole tile's mean");
}

}  // namespace

int main() {
    Levels();
    Pixels();
    Choice();
    Budget();
    Shrink();
    if (gFailures == 0) std::printf("ground_pool: all checks passed\n");
    return gFailures;
}
