// The line of sight and the path smoothing of the walk planner (main app, SimTheme Park 2.04b3; World.cpp; unit w195):
// bresenham_start 0x101360ac, line_of_sight 0x1013623c, line_of_sight_cleanup 0x10136498, wallhug_cleanup 0x101365cc and
// wallhug_tricky 0x10136e7c. wallhug_continue_trivial and wallhug_trivial (the planner itself) are unit w196 (src/World_wallhug.cpp).
//
// The squares are bytes and wrap (uint8_t arithmetic). A wallhug_waypoint travels by value in the high half of a register on the Mac
// (x in bits 24 to 31, y in bits 16 to 23); line_of_sight_cleanup passes a whole word at the waypoint, of which line_of_sight reads
// only the top half, so the translation passes the waypoint. The eight globals' names are invented, the addresses are the original's.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "TPCamera.h"
#include "platform/samsutils.h"

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
