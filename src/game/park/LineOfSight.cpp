// LineOfSight.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "platform/samsutils.h"
#include "TPCamera.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>

// The line of sight and the path smoothing of the walk planner (main app, SimTheme Park 2.04b3; World.cpp; unit w195):
// bresenham_start 0x101360ac, line_of_sight 0x1013623c, line_of_sight_cleanup 0x10136498, wallhug_cleanup 0x101365cc and
// wallhug_tricky 0x10136e7c. wallhug_continue_trivial and wallhug_trivial (the planner itself) are unit w196 (src/World_wallhug.cpp).
//
// The squares are bytes and wrap (uint8_t arithmetic). A wallhug_waypoint travels by value in the high half of a register on the Mac
// (x in bits 24 to 31, y in bits 16 to 23); line_of_sight_cleanup passes a whole word at the waypoint, of which line_of_sight reads
// only the top half, so the translation passes the waypoint. The eight globals' names are invented, the addresses are the original's.

int32_t gBresenhamXMajor = 0;            // 0x1034cab8
int32_t gBresenhamXDir = 0;              // 0x1034cabc
int32_t gBresenhamYDir = 0;              // 0x1034cac0
int32_t gBresenhamError = 0;             // 0x1034cac4
int32_t gBresenhamDY = 0;                // 0x1034cac8
int32_t gBresenhamDX = 0;                // 0x1034cacc
NavMode gWallhugNavMode = NavMode(0);    // 0x1034cad0
const int8_t gWallhugStep[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};   // 0x102a45a4 (words 0x00ff0100 0x0001ff00)

// 0x101360ac, 324 bytes.
void bresenham_start(wallhug_waypoint from, wallhug_waypoint to) {
    const int32_t dy = static_cast<int32_t>(to.mY) - static_cast<int32_t>(from.mY);
    const int32_t dx = static_cast<int32_t>(to.mX) - static_cast<int32_t>(from.mX);
    // The listing's r25 is the Y difference and r26 the X difference (the decompiler's __x names are swapped).
    const int32_t absDy = std::abs(dy);
    const int32_t absDx = std::abs(dx);
    gBresenhamXMajor = absDx > absDy ? 1 : 0;
    gBresenhamXDir = dx > 0 ? 1 : 3;
    gBresenhamYDir = dy > 0 ? 2 : 0;
    if (from.mX == to.mX) gBresenhamXDir = gBresenhamYDir;
    if (from.mY == to.mY) gBresenhamYDir = gBresenhamXDir;
    gBresenhamDX = std::abs(dx);
    gBresenhamDY = std::abs(dy);
    // Signed halvings (srawi, addze) of two non-negative numbers.
    gBresenhamError = gBresenhamDX / 2 + gBresenhamDY / 2;
}

// 0x1013623c, 532 bytes: 1 when no wall stands between the two squares, else 0 at the first wall.
int32_t line_of_sight(wallhug_waypoint from, wallhug_waypoint to) {
    bool first = true;
    uint8_t curX = from.mX;
    uint8_t curY = from.mY;
    uint8_t prevX = 0;   // the square before this one (r17 and r30; set at the end of each step)
    uint8_t prevY = 0;
    int32_t dir = -1;
    bresenham_start(from, to);
    while (curX != to.mX || curY != to.mY) {
        const int32_t lastDir = dir;
        if (gBresenhamError < gBresenhamDX) {
            gBresenhamError += gBresenhamDY;
            dir = gBresenhamXDir;
        } else {
            gBresenhamError -= gBresenhamDX;
            dir = gBresenhamYDir;
        }
        const uint8_t y = curY;
        if (MAP_wall_in_way(curX, y, static_cast<uint8_t>(dir), gWallhugNavMode) != 0) return 0;
        if (first) {
            first = false;
        } else if (dir != lastDir) {
            // The direction turned: the corner is cut when a wall stands on either side of it.
            if (MAP_wall_in_way(curX, y, static_cast<uint8_t>(lastDir), gWallhugNavMode) == 0) {
                const uint8_t sx = static_cast<uint8_t>(gWallhugStep[lastDir][0]);
                const uint8_t sy = static_cast<uint8_t>(gWallhugStep[lastDir][1]);
                if (MAP_wall_in_way(static_cast<uint8_t>(curX + sx), static_cast<uint8_t>(y + sy), static_cast<uint8_t>(dir),
                                    gWallhugNavMode) != 0) {
                    return 0;
                }
            }
            if (MAP_wall_in_way(prevX, prevY, static_cast<uint8_t>(dir), gWallhugNavMode) == 0) {
                const uint8_t sx = static_cast<uint8_t>(gWallhugStep[dir][0]);
                const uint8_t sy = static_cast<uint8_t>(gWallhugStep[dir][1]);
                if (MAP_wall_in_way(static_cast<uint8_t>(prevX + sx), static_cast<uint8_t>(prevY + sy),
                                    static_cast<uint8_t>(lastDir), gWallhugNavMode) != 0) {
                    return 0;
                }
            }
        }
        prevX = curX;
        prevY = y;
        if (dir != 0 && dir != 1 && dir != 2 && dir != 3) SamsUtilities::UDebug("Step in which direction?");   // unreachable
        curX = static_cast<uint8_t>(curX + static_cast<uint8_t>(gWallhugStep[dir][0]));
        curY = static_cast<uint8_t>(curY + static_cast<uint8_t>(gWallhugStep[dir][1]));
    }
    return 1;
}

// 0x10136498, 248 bytes: drops the waypoints a straight line can skip, from waypoint `first` on; 1 when any was skipped.
int32_t line_of_sight_cleanup(wallhug_path* path, uint32_t first) {
    int32_t skipped = 0;
    bool jumped;
    do {
        // The square the walk stands on: the start for first 0, else the waypoint before `first`.
        wallhug_waypoint cur = first == 0 ? wallhug_waypoint{path->mStartX, path->mStartY} : path->mPoints[first - 1];
        jumped = false;
        uint32_t read = first;
        uint32_t write = first;
        while (read < path->mCount) {
            bool found = false;
            for (uint32_t ahead = 4; ahead != 0; --ahead) {
                if (read + ahead < path->mCount) {
                    if (line_of_sight(cur, path->mPoints[read + ahead]) != 0) {
                        read += ahead;
                        jumped = true;
                        skipped = 1;
                        found = true;
                        break;
                    }
                }
            }
            if (found) continue;
            path->mPoints[write] = path->mPoints[read];
            cur = path->mPoints[write];
            ++write;
            ++read;
        }
        path->mCount = static_cast<uint8_t>(write);
    } while (jumped);
    return skipped;
}

namespace {

// The wall test of the diagonal pass, always with the stored nav mode.
int32_t wall(int32_t x, int32_t y, uint8_t dir) {
    return MAP_wall_in_way(static_cast<uint8_t>(x), static_cast<uint8_t>(y), dir, gWallhugNavMode);
}

// One quadrant of the diagonal pass: (sx, sy) is the step's direction, (x0, y0) the leg's start. Looks at the square the step reaches
// (x0 + sx, y0 + sy) with the two directions (a, b) that lead back along its sides, then at the start square with (c, d); the first
// pair that has a wall on one side only picks the corner (cx, cy). `ex`, `ey` are the offsets of the four picks in the listing's order:
// the pick of the square's first test, its second, the start's first and its second. Returns whether a corner was chosen.
struct QuadrantRule {
    int32_t sx;        // the step in x
    int32_t sy;        // the step in y
    uint8_t a;         // the square's first direction
    uint8_t b;         // the square's second direction
    uint8_t c;         // the start's first direction
    uint8_t d;         // the start's second direction
    int8_t pick1[2];   // chosen when wall(a) and not wall(b) at the square
    int8_t pick2[2];   // chosen when wall(b) and not wall(a) at the square
    int8_t pick3[2];   // chosen when wall(c) and not wall(d) at the start
    int8_t pick4[2];   // chosen when wall(d) and not wall(c) at the start
};

// The listing's four blocks, each the same shape with its own operands.
const QuadrantRule kQuadrants[4] = {
    {1, 1, 0, 3, 2, 1, {-1, 0}, {0, -1}, {0, -1}, {-1, 0}},      // 0x10136858
    {-1, 1, 0, 1, 2, 3, {1, 0}, {0, -1}, {0, -1}, {1, 0}},       // 0x101369a8
    {1, -1, 2, 3, 0, 1, {-1, 0}, {0, 1}, {0, 1}, {-1, 0}},       // 0x10136b00
    {-1, -1, 2, 1, 0, 3, {1, 0}, {0, 1}, {0, 1}, {1, 0}},        // 0x10136c58
};

bool pickCorner(const QuadrantRule& q, int32_t x0, int32_t y0, int32_t* cx, int32_t* cy) {
    const int32_t x = x0 + q.sx;
    const int32_t y = y0 + q.sy;
    if (wall(x, y, q.a) != 0 && wall(x, y, q.b) == 0) {
        *cx = q.pick1[0];
        *cy = q.pick1[1];
        return true;
    }
    if (wall(x, y, q.b) != 0 && wall(x, y, q.a) == 0) {
        *cx = q.pick2[0];
        *cy = q.pick2[1];
        return true;
    }
    if (wall(x0, y0, q.c) != 0 && wall(x0, y0, q.d) == 0) {
        *cx = q.pick3[0];
        *cy = q.pick3[1];
        return true;
    }
    if (wall(x0, y0, q.d) != 0 && wall(x0, y0, q.c) == 0) {
        *cx = q.pick4[0];
        *cy = q.pick4[1];
        return true;
    }
    return false;
}

}  // namespace

// 0x101365cc, 2168 bytes: smooths the path (line_of_sight_cleanup, then each leg replanned with wallhug_trivial and spliced in when
// the new leg is shorter), then adds a corner square to every one-square diagonal step a wall would cut. Answers `result`.
uint32_t wallhug_cleanup(wallhug_path* path, uint32_t result) {
    int32_t rounds = 10;
    uint32_t from = 0;
    for (;;) {
        line_of_sight_cleanup(path, from);
        --rounds;
        if (rounds == 0) return result;
        bool spliced = false;
        for (uint32_t k = 0; k + 1 < path->mCount; ++k) {
            wallhug_path leg;
            std::memset(&leg, 0, sizeof(leg));
            // The leg runs from the square before point k (the start for k 0) to point k + 1.
            const wallhug_waypoint legStart = k == 0 ? wallhug_waypoint{path->mStartX, path->mStartY} : path->mPoints[k - 1];
            leg.mStartX = legStart.mX;
            leg.mStartY = legStart.mY;
            leg.mEndX = path->mPoints[k + 1].mX;
            leg.mEndY = path->mPoints[k + 1].mY;
            if (wallhug_trivial(&leg, gWallhugNavMode) == 0x70000000) continue;
            bool through = false;
            for (int32_t i = 0; static_cast<uint32_t>(i) + 1 < leg.mCount; ++i) {
                if (leg.mPoints[i].mX == path->mPoints[k].mX && leg.mPoints[i].mY == path->mPoints[k].mY) {
                    through = true;
                    break;
                }
            }
            if (through) continue;
            if (static_cast<int32_t>(leg.mCount) + (static_cast<int32_t>(path->mCount) - 2) > 0xfc) continue;
            std::memmove(&path->mPoints[k + leg.mCount], &path->mPoints[k + 2], (static_cast<int32_t>(path->mCount) - static_cast<int32_t>(k) - 2) * 2);
            std::memcpy(&path->mPoints[k], leg.mPoints, static_cast<size_t>(leg.mCount) << 1);
            path->mCount = static_cast<uint8_t>(leg.mCount + path->mCount - 2);
            from = k < 4 ? 0 : k - 4;
            spliced = true;
            break;
        }
        if (spliced) continue;

        // Nothing to splice: the diagonal pass.
        for (int32_t i = 0; i < static_cast<int32_t>(path->mCount); ++i) {
            const wallhug_waypoint a = i == 0 ? wallhug_waypoint{path->mStartX, path->mStartY} : path->mPoints[i - 1];
            const wallhug_waypoint b = path->mPoints[i];
            int32_t dx = static_cast<int32_t>(b.mX) - static_cast<int32_t>(a.mX);
            int32_t dy = static_cast<int32_t>(b.mY) - static_cast<int32_t>(a.mY);
            if (dx == 0 || dy == 0) continue;
            const int32_t signX = dx < 0 ? -1 : 1;
            const int32_t signY = dy < 0 ? -1 : 1;
            const int32_t absDx = std::abs(dx);
            const int32_t absDy = std::abs(dy);
            int32_t p = absDx;
            int32_t q = absDy;
            while (p != 0 && q != 0) {
                if (p > q) p = p - (p / q) * q;
                else q = q - (q / p) * p;
            }
            const int32_t gcd = p == 0 ? q : p;
            if (((absDx / gcd) & 1) == 0 || ((absDy / gcd) & 1) == 0) continue;
            int32_t cx = 0;
            int32_t cy = 0;
            bool chosen = false;
            for (const QuadrantRule& rule : kQuadrants) {
                if (rule.sx == signX && rule.sy == signY) {
                    chosen = pickCorner(rule, a.mX, a.mY, &cx, &cy);
                    break;
                }
            }
            if (chosen && absDx == 1 && absDy == 1 && path->mCount + 1 < 0xfc) {
                std::memmove(&path->mPoints[i + 1], &path->mPoints[i], (static_cast<int32_t>(path->mCount) - i) * 2);
                ++i;
                path->mPoints[i - 1].mX = static_cast<uint8_t>(path->mPoints[i - 1].mX + cx);
                path->mPoints[i - 1].mY = static_cast<uint8_t>(path->mPoints[i - 1].mY + cy);
                path->mCount = static_cast<uint8_t>(path->mCount + 1);
            }
        }
        return result;
    }
}

// 0x10136e7c, 76 bytes: plans the walk and smooths it; answers wallhug_cleanup's answer.
uint32_t wallhug_tricky(wallhug_path* path, NavMode mode) {
    gWallhugNavMode = mode;
    const int32_t planned = wallhug_trivial(path, gWallhugNavMode);
    return wallhug_cleanup(path, static_cast<uint32_t>(planned));
}

// The walk planner (main app, SimTheme Park 2.04b3; World.cpp; unit w196): wallhug_continue_trivial 0x10136f04 and wallhug_trivial
// 0x10137d04. The line of sight, the Bresenham set-up and the path smoothing it calls are unit w195 (src/World_sight.cpp).
//
// The walk follows the Bresenham line from the start square to the path's end. At a wall it sends two wall huggers round the obstacle at
// once (the first turns one way, the second the other, each with its own buffer of squares) until one of them crosses the line on the far
// side; that hugger's squares are spliced into the path and the walk goes on from the path's last point. The original unrolls the two
// huggers; here one struct and its functions serve both (the calls, their order and the stores are the listing's). The stack slots that
// only the original reads (the hit square copies at 0x42 and 0x24e) are not kept.
//
// Waypoints travel by value in the high half of a register on the Mac (x in bits 24 to 31, y in bits 16 to 23); the planner passes the
// word at path + 2 (start and the end's x) and the word at path + 4 and the callees read only the top half.
// The six globals' names are invented, the addresses are the original's.

int32_t gWallhugRecordHits = 0;    // 0x1034caa8
uint32_t gWallhugHitStep = 0;      // 0x1034caac
uint8_t gWallhugHitDir = 0;        // 0x1034cab0
uint8_t gWallhugHitSide = 0;       // 0x1034cab1
uint8_t gWallhugHitCount = 0;      // 0x1034cab2
uint32_t gWallhugSteps = 0;        // 0x1034cab4

namespace {

constexpr uint32_t kNoWalk = 0x70000000;
constexpr int32_t kCrossed = 0x32;   // a hugger's direction once it has crossed the line
constexpr int32_t kFull = 0x33;      // a hugger's direction once its buffer is full

// One wall hugger: where it stands, which way it faces and turns (+1 or -1), the square it set out from and the squares it has passed.
struct Hugger {
    int32_t dir;
    int32_t turn;
    uint8_t x;
    uint8_t y;
    uint8_t startX;
    uint8_t startY;
    uint8_t count;
    wallhug_waypoint squares[253];
};

int32_t wall_2(uint8_t x, uint8_t y, int32_t dir) { return MAP_wall_in_way(x, y, static_cast<uint8_t>(dir), gWallhugNavMode); }

// The direction check the original makes before every step table lookup (unreachable: the directions are 0 to 3 there).
void checkDirection(int32_t dir) {
    if (dir != 0 && dir != 1 && dir != 2 && dir != 3) SamsUtilities::UDebug("Step in which direction?");
}

void appendSquare(wallhug_path* path, uint8_t x, uint8_t y) {
    path->mPoints[path->mCount].mX = x;
    path->mPoints[path->mCount].mY = y;
    path->mCount = static_cast<uint8_t>(path->mCount + 1);
}

// One move of a hugger (0x101370cc for the first, 0x101374a8 for the second): turn back at a wall, else step and try to turn; store the
// square; then test whether it crossed the line between its start and the end on the side it now faces. Sets dir to kFull or kCrossed.
void moveHugger(Hugger& h, wallhug_waypoint end) {
    if (h.dir == kFull) return;
    if (wall_2(h.x, h.y, h.dir) != 0) {
        h.dir = (h.dir - h.turn) & 3;
        h.squares[h.count] = wallhug_waypoint{h.x, h.y};
        h.count = static_cast<uint8_t>(h.count + 1);
        if (h.count > 0xfb) {
            h.dir = kFull;
            return;
        }
    } else {
        checkDirection(h.dir);
        h.x = static_cast<uint8_t>(h.x + static_cast<uint8_t>(gWallhugStep[h.dir][0]));
        h.y = static_cast<uint8_t>(h.y + static_cast<uint8_t>(gWallhugStep[h.dir][1]));
        const int32_t turned = h.dir + h.turn;
        if (wall_2(h.x, h.y, turned & 3) == 0) {
            h.dir = turned & 3;
            h.squares[h.count] = wallhug_waypoint{h.x, h.y};
            h.count = static_cast<uint8_t>(h.count + 1);
            if (h.count > 0xfb) {
                h.dir = kFull;
                return;
            }
        }
    }
    // The side it now faces must be on the end's side of it, and so must the way it is going.
    bool beyond = false;
    switch ((h.dir + h.turn) & 3) {
        case 0: beyond = end.mY >= h.y; break;
        case 1: beyond = end.mX <= h.x; break;
        case 2: beyond = end.mY <= h.y; break;
        default: beyond = end.mX >= h.x; break;
    }
    if (!beyond) return;
    switch (h.dir) {
        case 0: beyond = end.mY <= h.y; break;
        case 1: beyond = end.mX >= h.x; break;
        case 2: beyond = end.mY >= h.y; break;
        case 3: beyond = end.mX <= h.x; break;
        default: break;   // (not reachable) the listing falls through to the box test
    }
    if (!beyond) return;
    // It stands inside the box of its start square and the end, and is not on the start square.
    if (!(h.startX > end.mX)) {
        if (h.x < h.startX || h.x > end.mX) return;
    }
    if (!(h.startX < end.mX)) {
        if (h.x > h.startX || h.x < end.mX) return;
    }
    if (!(h.startY > end.mY)) {
        if (h.y < h.startY || h.y > end.mY) return;
    }
    if (!(h.startY < end.mY)) {
        if (h.y > h.startY || h.y < end.mY) return;
    }
    if (h.startX == h.x && h.startY == h.y) return;
    h.dir = kCrossed;
}

// The meeting test (0x101373dc, again at 0x101377b4): the first hugger's next square is the second one's and they face each other.
bool huggersMeet(const Hugger& a, const Hugger& b) {
    if (a.dir == kFull || a.dir == kCrossed) return false;
    checkDirection(a.dir);
    const uint8_t nextX = static_cast<uint8_t>(a.x + static_cast<uint8_t>(gWallhugStep[a.dir][0]));
    const uint8_t nextY = static_cast<uint8_t>(a.y + static_cast<uint8_t>(gWallhugStep[a.dir][1]));
    return b.x == nextX && b.y == nextY && static_cast<uint32_t>((a.dir + 2) & 3) == static_cast<uint32_t>(b.dir);
}

// Appends a crossed hugger's squares and its last square to the path when they fit (0x10137880 and 0x10137930).
bool spliceHugger(wallhug_path* path, const Hugger& h) {
    if (static_cast<int32_t>(path->mCount) + 2 + static_cast<int32_t>(h.count) > 0xfc) return false;
    for (uint32_t i = 0; i < h.count; ++i) appendSquare(path, h.squares[i].mX, h.squares[i].mY);
    appendSquare(path, h.x, h.y);
    return true;
}

}  // namespace

// 0x10136f04, 3500 bytes: answers the step counter when the end is reached (with the end appended), else 0x70000000.
uint32_t wallhug_continue_trivial(wallhug_path* path, wallhug_waypoint start, uint32_t limit) {
    uint8_t curX = start.mX;
    uint8_t curY = start.mY;
    int32_t dir = 0;
    bool first = true;
    bresenham_start(start, wallhug_waypoint{path->mEndX, path->mEndY});
    for (;;) {
        // The loop test (0x10137be8): the step limit, then the end.
        if (gWallhugSteps >= limit || (curX == path->mEndX && curY == path->mEndY)) break;

        // One Bresenham step.
        const int32_t lastDir = dir;
        if (gBresenhamError < gBresenhamDX) {
            gBresenhamError += gBresenhamDY;
            dir = gBresenhamXDir;
        } else {
            gBresenhamError -= gBresenhamDX;
            dir = gBresenhamYDir;
        }
        if (wall_2(curX, curY, dir) != 0) {
            // A wall: two huggers go round it (the first turns -1, the second +1).
            if (gWallhugRecordHits != 0) {
                gWallhugHitCount = path->mCount;
                gWallhugHitStep = gWallhugSteps;
                gWallhugHitDir = static_cast<uint8_t>(dir);
            }
            appendSquare(path, curX, curY);
            const wallhug_waypoint end{path->mEndX, path->mEndY};
            Hugger a;
            Hugger b;
            a.dir = (dir + 1) & 3;
            a.turn = -1;
            b.dir = (dir - 1) & 3;
            b.turn = 1;
            a.x = a.startX = b.x = b.startX = curX;
            a.y = a.startY = b.y = b.startY = curY;
            a.count = 0;
            b.count = 0;
            // LAB_101379fc: the hug loop; it leaves with a splice, a full pair of buffers (abort) or the step limit.
            bool aborted = false;
            while (gWallhugSteps < limit) {
                moveHugger(a, end);
                if (huggersMeet(a, b)) {
                    aborted = true;
                    break;
                }
                moveHugger(b, end);
                if (huggersMeet(a, b)) {
                    aborted = true;
                    break;
                }
                if (a.dir == kCrossed) {
                    if (!spliceHugger(path, a)) {
                        aborted = true;
                        break;
                    }
                    if (gWallhugRecordHits != 0) gWallhugHitSide = 0xff;
                    break;
                }
                if (b.dir == kCrossed) {
                    if (!spliceHugger(path, b)) {
                        aborted = true;
                        break;
                    }
                    if (gWallhugRecordHits != 0) gWallhugHitSide = 1;
                    break;
                }
                if (a.dir == kFull && b.dir == kFull) {
                    aborted = true;
                    break;
                }
                gWallhugSteps = gWallhugSteps + 1;
            }
            // LAB_10137a0c
            if (aborted || gWallhugSteps == limit) {
                appendSquare(path, curX, curY);
                return kNoWalk;
            }
            line_of_sight_cleanup(path, 0);
            // Restart from the path's last point (the halfword at path + 4 + 2 * mCount; the end itself when the path is empty).
            const uint8_t* raw = reinterpret_cast<const uint8_t*>(path);
            curX = raw[4 + 2 * path->mCount];
            curY = raw[5 + 2 * path->mCount];
            bresenham_start(wallhug_waypoint{curX, curY}, end);
            first = true;
        } else {
            const uint8_t prevX = curX;
            const uint8_t prevY = curY;
            checkDirection(dir);
            curX = static_cast<uint8_t>(curX + static_cast<uint8_t>(gWallhugStep[dir][0]));
            curY = static_cast<uint8_t>(curY + static_cast<uint8_t>(gWallhugStep[dir][1]));
            if (first) {
                first = false;
            } else if (dir != lastDir) {
                // The direction turned: a corner square is added where a wall cuts the corner.
                bool corner = false;
                if (wall_2(curX, curY, lastDir) == 0) {
                    corner = wall_2(static_cast<uint8_t>(curX + static_cast<uint8_t>(gWallhugStep[lastDir][0])),
                                  static_cast<uint8_t>(curY + static_cast<uint8_t>(gWallhugStep[lastDir][1])), dir) != 0;
                } else if (wall_2(prevX, prevY, dir) == 0) {
                    corner = wall_2(static_cast<uint8_t>(prevX + static_cast<uint8_t>(gWallhugStep[dir][0])),
                                  static_cast<uint8_t>(prevY + static_cast<uint8_t>(gWallhugStep[dir][1])), lastDir) != 0;
                }
                if (corner) {
                    appendSquare(path, curX, curY);
                    if (path->mCount > 0xfa) {
                        appendSquare(path, curX, curY);
                        return kNoWalk;
                    }
                }
            }
        }
        gWallhugSteps = gWallhugSteps + 1;
    }
    // The walk ended: the limit, or the end square.
    if (path->mCount >= 0xfa) return kNoWalk;
    if (curX == path->mEndX && curY == path->mEndY) {
        appendSquare(path, curX, curY);
        return gWallhugSteps;
    }
    return kNoWalk;
}

// 0x10137d04, 96 bytes: plans the whole walk (the limit is 60000 steps).
int32_t wallhug_trivial(wallhug_path* path, NavMode mode) {
    gWallhugNavMode = mode;
    path->mCount = 0;
    gWallhugSteps = 0;
    if (gWallhugRecordHits != 0) gWallhugHitCount = 0xff;
    return static_cast<int32_t>(wallhug_continue_trivial(path, wallhug_waypoint{path->mStartX, path->mStartY}, 60000));
}
