// VehicleCallbacks.cpp (main app, SimTheme Park 2.04b3; unit w218): the callbacks of the vehicles' walks and nav rules. callback_collision_candidates walks the
// things of the map squares around a vehicle and hands the function the first five vehicles near it (Separation's do_separation); callback_obstacle_candidates
// is an empty function on the Mac; map_local_to_global turns a map square into the 16.16 position of its corner (the other way round from map_global_to_local);
// implement_nav_rulesA2B answers whether a vehicle may step from one map square to another (the squares are numbered x + (y * 128 + 1), as map_fn numbers them):
// the squares may differ by one at most in either direction, and the walls (MAP_wall_in_way) on the sides the step crosses decide; a diagonal step is allowed when
// one of its two ways round the corner is free of walls. Adds, subtractions and shifts wrap as the PowerPC's do (done in uint32_t). Not translated here: the
// callers (CPeep and the staff walks) and the things' classes.
#include "VehicleSteer.h"

#include <cstddef>

#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "TPCamera.h"
#include "vector2d.h"
#include "World.h"

// ---- the two statics of implement_nav_rulesA2B (names invented): its first call clears the second, so that its assert is shown once
uint8_t gVehNavRulesInit = 0;       // 0x1034ca98 (BSS)
int32_t gVehNavRulesAsserted = 0;   // 0x1034ca9c (BSS)

namespace {

constexpr const char* kRowZero = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // the string pool at 0x10246ff4, +0
constexpr const char* kOffMap = "A peep has enquired as to the possibility of walking off the map.\nThis assert will not be repeated.  You can hit IGNORE";   // +0x43
constexpr const char* kTooFar = "A peep is walking so fast that they tried to cross > 1 cell in one step.\nThis assert will not be repeated.  You can hit IGNORE.";   // +0xbb
constexpr uint16_t kNoThing = 0;   // the halfword at 0x102a459c, read afresh on every pass of the Mac's walk (data, 0)
constexpr std::size_t kThingVehicle = 0xd4;   // where the Mac's thing keeps its Vehicle (CPeep, untranslated): the walk hands the function this address

// The Mac's add, subf and mullw wrap around; a signed overflow would be undefined on the host.
int32_t Add(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b)); }
int32_t Sub(int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)); }

// The map cell of the square numbered `square` (number - 1 cells on from the map's first cell, 0x44 bytes each; the number is a halfword promoted to 32 bits,
// so 0 reaches one cell back).
const CMapCell* CellOf(uint16_t square) {
    const intptr_t offset = static_cast<intptr_t>(static_cast<int32_t>(square) - 1) * static_cast<intptr_t>(sizeof(CMapCell));
    return reinterpret_cast<const CMapCell*>(static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(gGameWorldRef->GetMap()) + static_cast<uintptr_t>(offset)));
}

// Whether a thing of this kind is a vehicle the walk may hand on: the kinds 1, 4, 5, 6, 7, 8 and 0x12 (the code builds booleans from byte compares).
bool IsVehicleKind(uint8_t kind) { return kind == 1 || (kind >= 4 && kind <= 8) || kind == 0x12; }

// MAP_wall_in_way for the map square (x, y) and the side `dir` (0 = -y, 1 = +x, 2 = +y, 3 = -x) in the mode; any answer but 0 is a wall. The coordinates are
// bytes: the squares beside the first wrap in a byte.
bool Wall(uint32_t x, uint32_t y, uint8_t dir, NavMode mode) {
    return MAP_wall_in_way(static_cast<uint8_t>(x), static_cast<uint8_t>(y), dir, mode) != 0;
}

}  // namespace

// 0x101356b8 (636 bytes): for every map square within (radius + 0xffff) >> 16 squares of the one the vehicle stands on (the squares the map calls valid),
// walks the list of the things on it (the square's mWho, then each thing's next in the square, mCellNext) and calls the function with the vehicle and the
// vehicle of every thing whose kind is a vehicle's, that is not the vehicle itself and whose position, in the 8.8 halfwords of mCellX/mCellXSub and
// mCellY/mCellYSub against the vehicle's 16.16 position >> 8, lies within radius >> 8 (squared distance below the square of it, and not 0). Stops after the
// fifth call. Every thing visited first asserts that entry 0 of the thing table is free.
void callback_collision_candidates(const Vehicle& vehicle, int32_t radius, void (*function)(const Vehicle&, const Vehicle*)) {
    int32_t count = 0;
    const int32_t cells = Add(radius, 0xffff) >> 16;
    const int32_t px = vehicle.mPos.x;
    const int32_t py = vehicle.mPos.y;
    const int32_t cx = px >> 16;
    const int32_t cy = py >> 16;
    const int32_t vx = px >> 8;
    const int32_t vy = py >> 8;
    const int32_t shortRadius = radius >> 8;
    const uint32_t radiusSquared = static_cast<uint32_t>(shortRadius) * static_cast<uint32_t>(shortRadius);
    for (int32_t y = Sub(cy, cells); !(Add(cy, cells) < y); y = Add(y, 1)) {
        for (int32_t x = Sub(cx, cells); x <= Add(cx, cells); x = Add(x, 1)) {
            if (CMap::IsXYValid(x, y) == 0) continue;
            const uint16_t square = static_cast<uint16_t>(static_cast<uint32_t>(x) + ((static_cast<uint32_t>(y) << 7) + 1u));
            uint16_t id = CellOf(square)->mWho;
            while (id != kNoThing) {
                GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kRowZero);
                const CThing* thing = static_cast<const CThing*>(gThingArray[id].mThing);
                const Vehicle* other = reinterpret_cast<const Vehicle*>(reinterpret_cast<const uint8_t*>(thing) + kThingVehicle);
                if (IsVehicleKind(thing->mKind) && &vehicle != other) {
                    const int16_t tx = static_cast<int16_t>(static_cast<uint16_t>((static_cast<uint32_t>(thing->mCellX) << 8) | thing->mCellXSub));
                    const int16_t ty = static_cast<int16_t>(static_cast<uint16_t>((static_cast<uint32_t>(thing->mCellY) << 8) | thing->mCellYSub));
                    const uint32_t dx = static_cast<uint32_t>(Sub(tx, vx));
                    const uint32_t dy = static_cast<uint32_t>(Sub(ty, vy));
                    const uint32_t distanceSquared = dx * dx + dy * dy;   // mullw / mullw / add.
                    if (distanceSquared != 0 && static_cast<int32_t>(distanceSquared) < static_cast<int32_t>(radiusSquared)) {
                        function(vehicle, other);
                        count = Add(count, 1);
                        if (count >= 5) return;
                    }
                }
                id = thing->mCellNext;
            }
        }
    }
}

// 0x1013598c (4 bytes): an empty function (blr), so the original never repels or avoids an obstacle.
void callback_obstacle_candidates(const Vehicle&, int32_t, void (*)(const Vehicle&, const Obstacle*)) {}

// 0x10135a2c (20 bytes): the position of a map square's corner: each coordinate shifted left by 16 (plain 32-bit shifts).
Vector2d map_local_to_global(uint32_t x, uint32_t y) {
    Vector2d global;
    global.x = static_cast<Fixed>(x << 16);
    global.y = static_cast<Fixed>(y << 16);
    return global;
}

// 0x10135a70 (1324 bytes). Only the low 16 bits of a number count; the square is the number - 1, x its low 7 bits and y the rest (a logical shift of the
// 32-bit word, so the number 0 is the square (0x7f, 0x1ffffff)). The first call clears the assert-once word. Refused (0) without asking the map: y of `from` 0
// with y of `to` 0x3ff (a halfword never gives it; the off-map assert, once), squares two or more apart in either direction (the too-fast assert, once) and `to`
// equal to -1 (the whole word). Then 1 for the same square, or a neighbour with no wall on the side between them; for a diagonal neighbour, with either way round
// free of walls (the side of the first square then the side of the square beside it, or the other way); else 0.
int32_t implement_nav_rulesA2B(int32_t from, int32_t to, NavMode mode) {
    const uint32_t a = static_cast<uint16_t>(from);
    const uint32_t b = static_cast<uint16_t>(to);
    const uint32_t xa = (a - 1) & 0x7f;
    const uint32_t ya = (a - 1) >> 7;
    const uint32_t xb = (b - 1) & 0x7f;
    const uint32_t yb = (b - 1) >> 7;
    if (gVehNavRulesInit == 0) {
        gVehNavRulesAsserted = 0;
        gVehNavRulesInit = 1;
    }
    if (ya == 0 && yb == 0x3ff) {
        if (gVehNavRulesAsserted == 0) {
            gVehNavRulesAsserted = 1;
            GAME_ASSERTMSG(0, kOffMap);
        }
        return 0;
    }
    const int32_t dx = static_cast<int32_t>(xa - xb);
    const int32_t dy = static_cast<int32_t>(ya - yb);
    if (!((dx < 0 ? -dx : dx) < 2 && (dy < 0 ? -dy : dy) < 2)) {
        if (gVehNavRulesAsserted == 0) {
            gVehNavRulesAsserted = 1;
            GAME_ASSERTMSG(0, kTooFar);
        }
        return 0;
    }
    if (to == -1) return 0;
    const bool north = ya - yb == 1;
    const bool east = xb - xa == 1;
    const bool south = yb - ya == 1;
    const bool west = xa - xb == 1;
    const uint8_t sides = static_cast<uint8_t>(north + east + south + west);
    if (sides != 2) {
        return (!north || !Wall(xa, ya, 0, mode)) && (!east || !Wall(xa, ya, 1, mode)) && (!south || !Wall(xa, ya, 2, mode)) && (!west || !Wall(xa, ya, 3, mode)) ? 1 : 0;
    }
    if (north && east) {
        if (!Wall(xa, ya, 0, mode) && !Wall(xa, ya - 1, 1, mode)) return 1;
        if (!Wall(xa, ya, 1, mode) && !Wall(xa + 1, ya, 0, mode)) return 1;
    } else if (east && south) {
        if (!Wall(xa, ya, 1, mode) && !Wall(xa + 1, ya, 2, mode)) return 1;
        if (!Wall(xa, ya, 2, mode) && !Wall(xa, ya + 1, 1, mode)) return 1;
    } else if (south && west) {
        if (!Wall(xa, ya, 2, mode) && !Wall(xa, ya + 1, 3, mode)) return 1;
        if (!Wall(xa, ya, 3, mode) && !Wall(xa - 1, ya, 2, mode)) return 1;
    } else if (north && west) {
        if (!Wall(xa, ya, 0, mode) && !Wall(xa, ya - 1, 3, mode)) return 1;
        if (!Wall(xa, ya, 3, mode) && !Wall(xa - 1, ya, 0, mode)) return 1;
    }
    return 0;
}
