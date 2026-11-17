// host_tpi_hoarding.cpp: the TPI Worlds and Rides mod's repair of a park's fence where it crosses a tunnel's or a bridge's
// walkway (include/TpiSaveRepair.h, TPW_MOD_HOOK tpi_worlds) on the host, without game data. A made-up 128 x 128 track
// grid gets a fence laid as LLCC_BuildHoardingDB lays one (posts two cells apart, type 0x19, linked in the hoarding
// mask, each heading on to the next, its three block cells type 0xc naming it), with a gate gap, and a run of footbridge
// cells across it like the Arabian tunnel's corridor. A model of LLCC_BuildHoardingDB's walk (src/game/park/LLCellRide.cpp,
// the walk every land purchase makes from one end of the fence to the other) with a step cap shows the freeze: after
// 0.5.5's walkway repair (which reset every parentless track cell there, posts too) the walk stands still at the hole,
// with the repair's rule now (tpw::TpiWalkwayTrackToReset) and after the load step that puts lost posts back
// (tpw::TpiRestoreLostHoardingPosts) it reaches the fence's other end. Exits nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiSaveRepair.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

namespace {

using tpw::TpiHoardingCell;
constexpr int32_t kSide = tpw::kTpiHoardingGrid;

struct Point {
    int32_t x, y;
};

struct Grid {
    std::vector<TpiHoardingCell> cells;
    std::vector<Point> posts;   // the fence in walk order, from one gate end to the other
    Grid() : cells(kSide * kSide, TpiHoardingCell{7, 0, 0, 0, 0, 0, false}) {}
    TpiHoardingCell& At(int32_t x, int32_t y) { return cells[x + (y << 7)]; }
};

uint8_t HeadingTo(Point from, Point to) {
    if (to.x > from.x) return 4;
    if (to.x < from.x) return 0x40;
    if (to.y > from.y) return 0x10;
    return 1;
}

void Footbridge(Grid* grid, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    for (int32_t y = y0; y <= y1; ++y)
        for (int32_t x = x0; x <= x1; ++x) grid->At(x, y).footbridge = true;
}

// Lays the fence through the waypoints (corners, odd cells), two cells a step, as LLCC_BuildHoardingDB leaves it: the two
// end posts linked one way (the gate), every post heading to the next (the last keeps the heading it came with), the
// bridge flag where its block touches a footbridge (Modify 0x85), the overlap counter of its shape, and its block cells.
void LayFence(Grid* grid, const std::vector<Point>& waypoints) {
    grid->posts.clear();
    grid->posts.push_back(waypoints[0]);
    for (size_t i = 1; i < waypoints.size(); ++i) {
        Point p = grid->posts.back();
        const Point to = waypoints[i];
        while (p.x != to.x || p.y != to.y) {
            p.x += (to.x > p.x) ? 2 : (to.x < p.x) ? -2 : 0;
            p.y += (to.y > p.y) ? 2 : (to.y < p.y) ? -2 : 0;
            grid->posts.push_back(p);
        }
    }
    const size_t n = grid->posts.size();
    for (size_t i = 0; i < n; ++i) {
        const Point p = grid->posts[i];
        uint8_t links = 0;
        if (i > 0) links |= HeadingTo(p, grid->posts[i - 1]);
        if (i + 1 < n) links |= HeadingTo(p, grid->posts[i + 1]);
        TpiHoardingCell& post = grid->At(p.x, p.y);
        post.type = 0x19;
        post.parent = 0;
        post.hoarding = links;
        post.direction = (i + 1 < n) ? HeadingTo(p, grid->posts[i + 1]) : HeadingTo(grid->posts[i - 1], p);
        const bool bridge = post.footbridge || grid->At(p.x + 1, p.y).footbridge || grid->At(p.x, p.y + 1).footbridge ||
                            grid->At(p.x + 1, p.y + 1).footbridge;
        post.flags = bridge ? 1 : 0;
        post.overlap = tpw::TpiPostOverlap(links);
        const uint16_t id = static_cast<uint16_t>(1 + p.x + (p.y << 7));
        const Point block[3] = {{p.x + 1, p.y}, {p.x, p.y + 1}, {p.x + 1, p.y + 1}};
        for (Point b : block) {
            TpiHoardingCell& cell = grid->At(b.x, b.y);
            cell.type = 0xc;
            cell.parent = id;
            cell.overlap = 1;
        }
    }
}

// The walkway repair's track step on every footbridge cell (SaveLoad.cpp RepairTpiWalkways): a cell the rule picks is
// reset as CTrackCell::Reset resets it (a hidden quad: mode 7, no links, heading, flags or overlap).
template <class Rule>
int32_t WalkwayRepair(Grid* grid, Rule rule) {
    int32_t reset = 0;
    for (TpiHoardingCell& cell : grid->cells) {
        if (!cell.footbridge || !rule(cell.type, cell.parent)) continue;
        cell = TpiHoardingCell{7, 0, 0, 0, 0, 0, true};
        ++reset;
    }
    return reset;
}

bool OldRule(int32_t type, uint16_t parent) { return parent == 0 && type != 7; }   // 0.5.5

bool IsCorner(uint8_t links) {
    const uint8_t hv = links & 0x55;
    return hv == 0x05 || hv == 0x14 || hv == 0x50 || hv == 0x41;
}

// LLCC_BuildHoardingDB's walk with the hoarding mask read (gLLCC4818 set): from (x, y) along `heading`, a post or a linked
// cell turns at a corner, any other cell gives heading 0 (no move), until it has stood on (endX, endY). The number of
// cells it stood on, or -1 when it has not got there in `cap` of them; `stuck` gets the cell it stood on last.
int32_t Walk(Grid* grid, Point start, uint8_t heading, Point end, int32_t cap, Point* stuck) {
    Point p = start;
    uint8_t dir = heading;
    for (int32_t steps = 1; steps <= cap; ++steps) {
        const TpiHoardingCell& cell = grid->At(p.x, p.y);
        if (cell.type == 0x19 || cell.hoarding != 0) {
            if (IsCorner(cell.hoarding)) {
                uint8_t other = tpw::TpiOppositeHeading(dir);
                if ((other & cell.hoarding) == 0) other = dir;
                dir = static_cast<uint8_t>(other ^ cell.hoarding);
            }
        } else {
            dir = 0;
        }
        if (p.x == end.x && p.y == end.y) return steps;
        switch (dir) {
            case 1: p.y -= 2; break;
            case 4: p.x += 2; break;
            case 0x10: p.y += 2; break;
            case 0x40: p.x -= 2; break;
            default: break;
        }
        if (stuck != nullptr) *stuck = p;
        if (p.x < 0 || p.y < 0 || p.x >= kSide || p.y >= kSide) return -1;
    }
    return -1;
}

int32_t WalkFence(Grid* grid, Point* stuck) {
    const Point start = grid->posts.front(), end = grid->posts.back();
    return Walk(grid, start, grid->At(start.x, start.y).direction, end, 4 * kSide * kSide, stuck);
}

bool Same(const TpiHoardingCell& a, const TpiHoardingCell& b) {
    return a.type == b.type && a.parent == b.parent && a.hoarding == b.hoarding && a.direction == b.direction && a.flags == b.flags &&
           a.overlap == b.overlap && a.footbridge == b.footbridge;
}

std::string Where(Point p) { return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")"; }

// a player's Arabian Nights park, made up: the fence's gate gap on row 17 between (51,17) and (43,17), its east side at x 71 over the
// tunnel's corridor (footbridge rows 65 and 66, x 60 to 76), and the Arabian bridge (x 31 to 34, rows 21 and 22) inside.
Grid ArabianLike() {
    Grid grid;
    Footbridge(&grid, 60, 65, 76, 66);
    Footbridge(&grid, 31, 21, 34, 22);
    LayFence(&grid, {{51, 17}, {71, 17}, {71, 81}, {11, 81}, {11, 17}, {43, 17}});
    return grid;
}

// A fence whose south side runs along the corridor (row 65, x 61 to 75 on footbridge cells), its south-east corner on it.
Grid AlongCorridor() {
    Grid grid;
    Footbridge(&grid, 60, 65, 76, 66);
    LayFence(&grid, {{51, 17}, {75, 17}, {75, 65}, {11, 65}, {11, 17}, {43, 17}});
    return grid;
}

void CheckFence(const char* name, Grid (*make)()) {
    const std::string tag = name;
    Point stuck = {0, 0};

    // The fence as laid: the walk reaches the gate's other end, one cell per post.
    Grid laid = make();
    const int32_t whole = WalkFence(&laid, &stuck);
    Check(whole == static_cast<int32_t>(laid.posts.size()), tag + ": the laid fence's walk ends at the gate after every post (got " +
                                                                 std::to_string(whole) + " of " + std::to_string(laid.posts.size()) + ")");

    // 0.5.5's walkway repair: the posts over the corridor are reset, and the walk stands still at the first (the freeze).
    Grid broken = make();
    const int32_t lostPosts = WalkwayRepair(&broken, OldRule);
    Check(lostPosts > 0, tag + ": 0.5.5's rule resets posts over the walkway");
    Check(WalkFence(&broken, &stuck) == -1, tag + ": after 0.5.5's rule the walk never ends (the freeze), stuck at " + Where(stuck));

    // The rule now keeps the posts: nothing of the fence changes, and the walk ends.
    Grid kept = make();
    Check(WalkwayRepair(&kept, tpw::TpiWalkwayTrackToReset) == 0, tag + ": the rule now resets no post");
    bool same = true;
    for (int32_t i = 0; i < kSide * kSide; ++i) same = same && Same(kept.cells[i], laid.cells[i]);
    Check(same, tag + ": the rule now leaves the fence as laid");
    Check(WalkFence(&kept, &stuck) == static_cast<int32_t>(laid.posts.size()), tag + ": with the rule now the walk ends at the gate");

    // A park saved after 0.5.5's reset: the load step puts every lost post back as it was laid, and the walk ends.
    std::vector<int32_t> restored;
    const int32_t count = tpw::TpiRestoreLostHoardingPosts(broken.cells.data(), kSide, kSide, &restored);
    Check(count == lostPosts && static_cast<int32_t>(restored.size()) == lostPosts,
          tag + ": every lost post restored (" + std::to_string(count) + " of " + std::to_string(lostPosts) + ")");
    for (int32_t i = 0; i < kSide * kSide; ++i) {
        if (Same(broken.cells[i], laid.cells[i])) continue;
        const TpiHoardingCell& got = broken.cells[i];
        const TpiHoardingCell& want = laid.cells[i];
        char line[200];
        std::snprintf(line, sizeof line, "%s: cell (%d,%d) restored as laid: type %#x/%#x links %#x/%#x heading %#x/%#x flags %#x/%#x overlap %d/%d",
                      name, i & 0x7f, i >> 7, got.type, want.type, got.hoarding, want.hoarding, got.direction, want.direction, got.flags,
                      want.flags, got.overlap, want.overlap);
        Check(false, line);
    }
    Check(WalkFence(&broken, &stuck) == static_cast<int32_t>(laid.posts.size()), tag + ": after the restore the walk ends at the gate");
    // A second load changes nothing.
    Check(tpw::TpiRestoreLostHoardingPosts(broken.cells.data(), kSide, kSide, nullptr) == 0, tag + ": a second load restores nothing");
    // A park the rule now loads (posts kept) has nothing to restore.
    Check(tpw::TpiRestoreLostHoardingPosts(kept.cells.data(), kSide, kSide, nullptr) == 0, tag + ": a fence kept whole restores nothing");
}

}  // namespace

int main() {
    using namespace tpw;

    // The walkway repair's track rule: a parentless cell is reset unless it is a hidden quad already or a hoarding post.
    Check(TpiWalkwayTrackToReset(0, 0), "plain land track cell: reset");
    Check(TpiWalkwayTrackToReset(0xc, 0), "a parentless block cell: reset");
    Check(!TpiWalkwayTrackToReset(7, 0), "already a hidden quad: kept");
    Check(!TpiWalkwayTrackToReset(0x19, 0), "a hoarding post: kept");
    Check(!TpiWalkwayTrackToReset(0xc, 8386), "a post's block cell: kept");
    Check(!TpiWalkwayTrackToReset(0xb, 12), "track of a ride: kept");

    // The overlap counter of a post's shape.
    Check(TpiPostOverlap(0x11) == 0 && TpiPostOverlap(0x44) == 0, "straight posts: 0");
    Check(TpiPostOverlap(0x01) == 0 && TpiPostOverlap(0x40) == 0, "a gate end: 0");
    Check(TpiPostOverlap(0x14) == 1 && TpiPostOverlap(0x41) == 1 && TpiPostOverlap(0x05) == 1 && TpiPostOverlap(0x50) == 1, "corners: 1");
    Check(TpiPostOverlap(0x15) == 1 && TpiPostOverlap(0x55) == 1, "crossroads: 1");

    // The steps and headings.
    Check(TpiOppositeHeading(1) == 0x10 && TpiOppositeHeading(4) == 0x40 && TpiOppositeHeading(0x10) == 1 && TpiOppositeHeading(0x40) == 4,
          "opposite headings");
    Check(TpiHoardingStep(1 + (1 << 7), 1, kSide, kSide) == -1, "a step off the top: none");
    Check(TpiHoardingStep(126 + (5 << 7), 4, 127, kSide) == -1, "a step past a narrower level's edge: none");
    Check(TpiHoardingStep(5 + (5 << 7), 0x10, kSide, kSide) == 5 + (7 << 7), "a step down");

    CheckFence("east side over the corridor", ArabianLike);
    CheckFence("south side along the corridor", AlongCorridor);

    // What is not a lost post.
    {
        Grid grid = ArabianLike();
        // A footbridge cell with no block (every corridor cell off the fence): nothing.
        Check(!TpiLostHoardingPost(grid.cells.data(), 62 + (65 << 7), kSide, kSide), "a footbridge cell with no block: not lost");
        // A post's block cell on a footbridge (parent set): nothing.
        Check(!TpiLostHoardingPost(grid.cells.data(), 72 + (65 << 7), kSide, kSide), "a block cell: not lost");
        // A post that is there: nothing.
        Check(!TpiLostHoardingPost(grid.cells.data(), 71 + (65 << 7), kSide, kSide), "a post: not lost");
        // A post's cell made something else off the walkways: not the walkway repair's, left alone.
        TpiHoardingCell& inland = grid.At(71, 41);
        inland = TpiHoardingCell{7, 0, 0, 0, 0, 0, false};
        Check(!TpiLostHoardingPost(grid.cells.data(), 71 + (41 << 7), kSide, kSide), "a cell off the walkways: not lost");
        // A block with one cell naming another post: not lost.
        Grid other = ArabianLike();
        WalkwayRepair(&other, OldRule);
        other.At(72, 66).parent = 1;
        Check(!TpiLostHoardingPost(other.cells.data(), 71 + (65 << 7), kSide, kSide), "a block cell naming another post: not lost");
        // A lone lost post with no post beside it linked back: kept as it is.
        Grid lone;
        Footbridge(&lone, 60, 65, 76, 66);
        LayFence(&lone, {{71, 63}, {71, 65}});
        WalkwayRepair(&lone, OldRule);
        lone.At(71, 63) = TpiHoardingCell{7, 0, 0, 0, 0, 0, false};
        Check(TpiLostHoardingPost(lone.cells.data(), 71 + (65 << 7), kSide, kSide), "a lone reset post is lost");
        Check(TpiRestoreLostHoardingPosts(lone.cells.data(), kSide, kSide, nullptr) == 0, "a lost post with no link is not restored");
        Check(lone.At(71, 65).type == 7, "the lone cell stays a hidden quad");
        // A block at the level's last column or row: not looked past.
        Grid edge;
        Footbridge(&edge, 120, 120, 127, 127);
        Check(!TpiLostHoardingPost(edge.cells.data(), 127 + (121 << 7), kSide, kSide), "the last column: not lost");
        Check(!TpiLostHoardingPost(edge.cells.data(), 121 + (127 << 7), kSide, kSide), "the last row: not lost");
    }

    if (gFailures != 0) {
        std::printf("%d check(s) failed\n", gFailures);
        return 1;
    }
    std::printf("tpi hoarding: all checks passed\n");
    return 0;
}
