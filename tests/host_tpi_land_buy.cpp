// host_tpi_land_buy.cpp: the TPI Worlds and Rides mod's land buying around the park gate (include/TpiLandBuy.h,
// TPW_MOD_HOOK tpi_worlds in LLCC_IsHoardingEditLegal and LLCC_FillInsideOfPark) on the host, without game data.
// A made-up 128 x 128 level has the park gate where Theme Park Inc's worlds have it (posts at (43,17) and (51,17), the
// land outside it freeform). A model of the edit check's freeform run (TallySideCell: two freeform dry cells in a row
// along a side make an edit illegal) shows that an original world refuses every rectangle round the gate by that run, and
// that in the mod's worlds, where a freeform cell does not count, only the gate check refuses it; land beside the park and
// past the gate is still allowed. A model of the fill's scans (a scan runs two cells a step until a locked cell, and
// clears the cell and the three after it) shows that a scan with nothing locked ahead leaves the level (the port's crash)
// and that the guard stops it at the edge, and that the guard never stops a scan that ends at the fence. Exits nonzero on
// the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiLandBuy.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

namespace {

constexpr int32_t kSide = tpw::kTpiLevelSide;

// The gate of the made-up level (the extents LLCC_DrawInitialHoarding sets from the entrance).
constexpr int32_t kLeft = 43, kTop = 17, kRight = 51, kBottom = 17;

struct Rect {
    int32_t x1, y1, x2, y2;
};

// Where a scan starts and the way it goes.
struct ScanFrom {
    int32_t x, y, dx, dy;
};

std::string Name(const Rect& r) {
    char text[64];
    std::snprintf(text, sizeof text, "(%d,%d)-(%d,%d)", r.x1, r.y1, r.x2, r.y2);
    return text;
}

// CMap::IsXYValid.
bool Valid(int32_t x, int32_t y) { return x >= 0 && y >= 0 && x < kSide && y < kSide; }

// The level's freeform dry cells: the road and pavement outside the gate, rows 0 to 16 from column 30 to 64.
struct Level {
    std::vector<uint8_t> freeform;
    Level() : freeform(kSide * kSide, 0) {
        for (int32_t y = 0; y <= 16; ++y)
            for (int32_t x = 30; x <= 64; ++x) freeform[x + y * kSide] = 1;
    }
    bool Freeform(int32_t x, int32_t y) const { return freeform[x + y * kSide] != 0; }
};

// The parts of LLCC_IsHoardingEditLegal the gate concerns, in its order: a corner out of the level, two cells wide or
// less, the gate check (the mod's worlds, with the fix), a side passing exactly through a gate post, and the freeform run
// along each side (which counts only in an original world). 1 when nothing refuses the rectangle.
int32_t EditLegal(const Level& level, const Rect& r, bool tpiWorld, bool gateCheck) {
    const int32_t lox = r.x2 < r.x1 ? r.x2 : r.x1;
    const int32_t loy = r.y2 < r.y1 ? r.y2 : r.y1;
    const int32_t hix = r.x1 < r.x2 ? r.x2 : r.x1;
    const int32_t hiy = r.y1 < r.y2 ? r.y2 : r.y1;
    if (!Valid(lox, loy) || !Valid(hix, hiy)) return 0;
    if (hix - lox <= 2 || hiy - loy <= 2) return 0;
    if (gateCheck && tpiWorld && tpw::TpiEditMeetsGate(lox, loy, hix, hiy, kLeft, kTop, kRight, kBottom)) return 0;
    bool bad = false;
    int32_t run[4] = {0, 0, 0, 0};
    auto tally = [&](int32_t x, int32_t y, int32_t side) {
        if (tpiWorld || !level.Freeform(x, y)) {
            run[side] = 0;
        } else if (++run[side] > 1) {
            run[side] = 0;
            bad = true;
        }
    };
    for (int32_t x = lox; x <= hix; x += 2) {
        if (x == kRight && (loy == kBottom || hiy == kBottom)) return 0;
        if (x == kLeft && (loy == kTop || hiy == kTop)) return 0;
        tally(x, loy, 3);
        tally(x, hiy, 1);
    }
    for (int32_t y = loy; y <= hiy; y += 2) {
        if ((lox == kRight || hix == kRight) && y == kBottom) return 0;
        if ((lox == kLeft || hix == kLeft) && y == kTop) return 0;
        tally(lox, y, 2);
        tally(hix, y, 0);
    }
    return bad ? 0 : 1;
}

// One of the fill's scans, as the game's: from the fence cell (x, y), two cells a step towards (dx, dy), clearing every
// cell it reaches up to and including the first locked one. `cleared` gets the cells cleared; the answer is whether a
// cleared cell's square of four left the level (the port reads a missing cell there and crashes). The step cap only ends
// the model's own run.
bool Scan(const std::vector<uint8_t>& locked, int32_t x, int32_t y, int32_t dx, int32_t dy, bool guard, std::vector<int32_t>* cleared) {
    bool offLevel = false;
    for (int32_t step = 1; step <= kSide; ++step) {
        const int32_t cx = x + 2 * dx * step;
        const int32_t cy = y + 2 * dy * step;
        if (guard && tpw::TpiFillCellLeavesLevel(cx, cy)) break;
        if (!Valid(cx, cy) || !Valid(cx + 1, cy) || !Valid(cx, cy + 1) || !Valid(cx + 1, cy + 1)) {
            offLevel = true;
            break;
        }
        cleared->push_back(cx + cy * kSide);
        if (locked[cx + cy * kSide] != 0) break;
    }
    return offLevel;
}

}  // namespace

int main() {
    const Level level;

    // The gate check alone: either post, on an edge or inside, and the opening between them.
    for (const Rect& r : {Rect{39, 9, 55, 23}, Rect{41, 9, 53, 19}, Rect{39, 11, 55, 23}, Rect{55, 23, 39, 9}, Rect{51, 17, 60, 30},
                          Rect{30, 5, 43, 17}, Rect{45, 5, 49, 19}, Rect{45, 17, 49, 30}, Rect{0, 0, 127, 127}, Rect{20, 17, 43, 40}}) {
        const int32_t lox = r.x1 < r.x2 ? r.x1 : r.x2, hix = r.x1 < r.x2 ? r.x2 : r.x1;
        const int32_t loy = r.y1 < r.y2 ? r.y1 : r.y2, hiy = r.y1 < r.y2 ? r.y2 : r.y1;
        Check(tpw::TpiEditMeetsGate(lox, loy, hix, hiy, kLeft, kTop, kRight, kBottom), "meets the gate: " + Name(r));
    }
    for (const Rect& r : {Rect{53, 17, 70, 30}, Rect{53, 5, 70, 40}, Rect{20, 5, 41, 40}, Rect{39, 19, 55, 40}, Rect{39, 0, 55, 15},
                          Rect{60, 60, 90, 90}}) {
        Check(!tpw::TpiEditMeetsGate(r.x1, r.y1, r.x2, r.y2, kLeft, kTop, kRight, kBottom), "does not meet the gate: " + Name(r));
    }
    Check(!tpw::TpiEditMeetsGate(0, 0, 127, 127, -1, -1, -1, -1), "no hoarding meets nothing");
    // A gate on a column, posts given either way round.
    Check(tpw::TpiEditMeetsGate(10, 50, 20, 60, 15, 62, 15, 55), "a column gate: the rectangle holds a post");
    Check(tpw::TpiEditMeetsGate(10, 58, 20, 59, 15, 62, 15, 55), "a column gate: the rectangle crosses the opening");
    Check(!tpw::TpiEditMeetsGate(17, 50, 20, 70, 15, 62, 15, 55), "a column gate: the rectangle beside it");

    // The rectangles slappy060 drew round Arabian Nights' gate. An original world refuses each by the freeform run; in the
    // mod's worlds the run does not count, so without the gate check each was accepted (the crash), and with it each is
    // refused.
    for (const Rect& r : {Rect{39, 9, 55, 23}, Rect{41, 9, 53, 19}, Rect{39, 11, 55, 23}, Rect{45, 5, 49, 19}}) {
        Check(EditLegal(level, r, false, true) == 0, "an original world refuses " + Name(r));
        Check(EditLegal(level, r, true, false) == 1, "before the fix the mod's worlds accepted " + Name(r));
        Check(EditLegal(level, r, true, true) == 0, "the mod's worlds refuse " + Name(r));
    }
    // Land the 0.5.1 change was for stays allowed in the mod's worlds: beside the park, past a canyon or a bridge (a side
    // across freeform cells away from the gate), and inside the level's far corner.
    for (const Rect& r : {Rect{53, 19, 70, 40}, Rect{20, 19, 41, 40}, Rect{55, 5, 70, 30}, Rect{20, 5, 41, 30}, Rect{60, 60, 90, 90},
                          Rect{100, 100, 126, 126}}) {
        Check(EditLegal(level, r, true, true) == 1, "the mod's worlds still allow " + Name(r));
        Check(EditLegal(level, r, true, true) == EditLegal(level, r, true, false), "the gate check leaves alone " + Name(r));
    }
    // An original world is the Mac's whatever the gate check: it is only made in the mod's worlds.
    for (int32_t y1 = 1; y1 < 40; y1 += 3)
        for (int32_t x1 = 25; x1 < 70; x1 += 4)
            for (const int32_t size : {4, 9, 16}) {
                const Rect r{x1, y1, x1 + size, y1 + size};
                Check(EditLegal(level, r, false, true) == EditLegal(level, r, false, false), "an original world is unchanged: " + Name(r));
            }

    // The guard against the square of four that the game's neighbour rule gives (MapDelta, CMap::IsXYValid), on every
    // cell of the level and a ring outside it.
    for (int32_t y = -3; y < kSide + 3; ++y)
        for (int32_t x = -3; x < kSide + 3; ++x) {
            const bool square = Valid(x, y) && Valid(x, y + 1) && Valid(x + 1, y + 1) && Valid(x + 1, y);
            if (tpw::TpiFillCellLeavesLevel(x, y) == square) {
                Check(false, "the guard is the square's rule at (" + std::to_string(x) + "," + std::to_string(y) + ")");
            }
        }

    // The fill's scans. A park fenced from (39,17) round to (55,17) through row 61 (the fence's cells locked): every scan
    // from the fence inwards ends at the fence, the guard changes nothing.
    std::vector<uint8_t> locked(kSide * kSide, 0);
    for (int32_t x = 39; x <= 55; x += 2) locked[x + 61 * kSide] = 1;
    for (int32_t y = 17; y <= 61; y += 2) {
        locked[39 + y * kSide] = 1;
        locked[55 + y * kSide] = 1;
    }
    for (int32_t y = 19; y <= 59; y += 2) {
        std::vector<int32_t> plain, guarded;
        Check(!Scan(locked, 39, y, 1, 0, false, &plain), "a scan across the park stays inside it");
        Check(!Scan(locked, 39, y, 1, 0, true, &guarded) && plain == guarded, "the guard leaves a scan across the park alone");
    }
    for (int32_t x = 41; x <= 53; x += 2) {
        std::vector<int32_t> plain, guarded;
        Check(!Scan(locked, x, 17, 0, 1, false, &plain) && !Scan(locked, x, 17, 0, 1, true, &guarded) && plain == guarded,
              "a scan down the park ends at the fence, guard or not");
    }
    // A fence that became a loop round the gate alone: the scans out of the gate's side find nothing locked. Without the
    // guard they leave the level (the crash); with it they stop at the edge, having cleared the same cells up to it.
    for (const int32_t dir : {-1, 1}) {
        std::vector<int32_t> plain, guarded;
        Check(Scan(locked, 51, 7, dir, 0, false, &plain), "before the guard a scan with nothing ahead leaves the level");
        Check(!Scan(locked, 51, 7, dir, 0, true, &guarded) && guarded == plain, "the guard stops it at the level's edge, the same cells cleared");
        Check(!guarded.empty() && !tpw::TpiFillCellLeavesLevel(guarded.back() % kSide, guarded.back() / kSide),
              "the guarded scan clears cells up to the edge");
    }
    for (const ScanFrom& from : {ScanFrom{47, 7, 0, -1}, ScanFrom{60, 70, 0, 1}}) {
        std::vector<int32_t> plain, guarded;
        Check(Scan(locked, from.x, from.y, from.dx, from.dy, false, &plain), "before the guard a scan up or down with nothing ahead leaves the level");
        Check(!Scan(locked, from.x, from.y, from.dx, from.dy, true, &guarded) && guarded == plain, "the guard stops it at the level's edge");
    }

    if (gFailures != 0) {
        std::printf("%d check(s) failed\n", gFailures);
        return 1;
    }
    std::printf("tpi_land_buy: all checks passed\n");
    return 0;
}
