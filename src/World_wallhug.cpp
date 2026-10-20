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
#include <cstdint>

#include "TPCamera.h"
#include "platform/samsutils.h"

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

int32_t wall(uint8_t x, uint8_t y, int32_t dir) { return MAP_wall_in_way(x, y, static_cast<uint8_t>(dir), gWallhugNavMode); }

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
    if (wall(h.x, h.y, h.dir) != 0) {
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
        if (wall(h.x, h.y, turned & 3) == 0) {
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
        if (wall(curX, curY, dir) != 0) {
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
                if (wall(curX, curY, lastDir) == 0) {
                    corner = wall(static_cast<uint8_t>(curX + static_cast<uint8_t>(gWallhugStep[lastDir][0])),
                                  static_cast<uint8_t>(curY + static_cast<uint8_t>(gWallhugStep[lastDir][1])), dir) != 0;
                } else if (wall(prevX, prevY, dir) == 0) {
                    corner = wall(static_cast<uint8_t>(prevX + static_cast<uint8_t>(gWallhugStep[dir][0])),
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
