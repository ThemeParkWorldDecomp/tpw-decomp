// host_tpi_great_lake.cpp: the TPI Worlds and Rides mod's Great Lake (include/TpiGreatLake.h, TPW_MOD_HOOK tpi_worlds in
// LLCR_PlaceRideBlock, BMP_CreateRide, SetNavRoute, BMP_AI and BMP_DrawCarsForRide) on the host, without game data, on a
// made-up lake: TpiLake.sam's text is read whatever the locale; the ride is placed only where the cells behind
// its footprint are water, in every rotation; its chain of circles stays over water and the plot and reaches every
// circle; and boats that sail it for a long run (out to buoys, back to the plot) are never over land, never off the
// circles and never stuck; and which circles BMP_InsideArea lets a bump into (a water track's crossroad centre for its
// own bumps, a lake circle only for its own boats). Exits nonzero on the failed checks' count.
#include <clocale>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiGreatLake.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        if (gFailures < 40) std::printf("FAIL: %s\n", what.c_str());
    }
}

using namespace tpw;

// LLCR_PlaceRideBlock's map cell of the footprint's (col, row), for both values of gLLCC47C4.
struct Placer {
    int32_t x, y, rotation, width, height;
    bool centred;   // gLLCC47C4 == 0
    void operator()(int32_t col, int32_t row, int32_t& cx, int32_t& cy) const {
        const int32_t marginX = (5 - width) / 2, marginY = (5 - height) / 2;
        int32_t px = x, py = y, k7c, k78, k74, k70, k6c, k15;
        if (centred) {
            px -= width / 2;
            py -= height / 2;
            k7c = marginY - marginX;
            k78 = -1 - marginX - marginY;
            k74 = -1 - marginX - marginX;
            k70 = -1 - marginY - marginY;
            k6c = -1 - marginY - marginX;
            k15 = marginX - marginY;
        } else {
            k15 = 0;
            k7c = 0;
            k78 = -5;
            k74 = -5;
            k70 = -5;
            k6c = -1 - height;
        }
        if (rotation == 0xb4) {
            cx = k74 + ((px + 5) - col);
            cy = k70 + ((py + 5) - row);
        } else if (rotation == 0x5a) {
            cx = px + (row + k7c);
            cy = k78 + ((py + 5) - col);
        } else if (rotation == 0x10e) {
            cy = py + (col + k15);
            cx = k6c + ((px + 5) - row);
        } else {
            cx = px + col;
            cy = py + row;
        }
    }
};

// A lake behind a plot whose circle is on cell (64, 64), for the back direction (dx, dy): rows 2 to 20 cells behind, 12
// cells each side, less an island 8 to 10 behind and 2 each side, so the boats must go round it. Written as TpiLake.sam.
struct Frame {
    int32_t dx, dy;   // behind
    int32_t px, py;   // the side
    double cx(double u, double v) const { return 64.5 + u * px + v * dx; }
    double cy(double u, double v) const { return 64.5 + u * py + v * dy; }
};

static bool Island(int u, int v) { return v >= 8 && v <= 10 && u >= -2 && u <= 2; }

static std::string LakeText(const Frame& f, char point) {
    std::vector<uint8_t> water(kTpiLakeMap * kTpiLakeMap, 0);
    for (int v = 2; v <= 20; ++v)
        for (int u = -12; u <= 12; ++u)
            if (!Island(u, v)) water[static_cast<size_t>(int(f.cy(u, v)) * kTpiLakeMap + int(f.cx(u, v)))] = 1;
    std::string t = "# made up\nLake.RideId 7200\nLake.WaterLevel -2" + std::string(1, point) + "040\n";
    for (int y = 0; y < kTpiLakeMap; ++y)
        for (int x = 0; x < kTpiLakeMap; ++x)
            if (water[static_cast<size_t>(y * kTpiLakeMap + x)] && (x == 0 || !water[static_cast<size_t>(y * kTpiLakeMap + x - 1)])) {
                int e = x;
                while (e + 1 < kTpiLakeMap && water[static_cast<size_t>(y * kTpiLakeMap + e + 1)]) ++e;
                t += "Lake.Water " + std::to_string(y) + " " + std::to_string(x) + " " + std::to_string(e) + "\n";
            }
    auto num = [&](double v) {
        char b[32];
        std::snprintf(b, sizeof b, "%.1f", v);
        for (char* c = b; *c; ++c)
            if (*c == '.' || *c == ',') *c = point;
        return std::string(b);
    };
    // Theme Park Inc's circles (map units, 10 a cell): behind the island, at its side, one far too big, one at the far shore.
    const double circles[4][3] = {{0, 14, 3}, {-8, 6, 2}, {8, 16, 6}, {0, 19, 1}};
    for (const auto& c : circles) t += "Lake.Circle " + num(f.cx(c[0], c[1]) * 10) + " " + num(f.cy(c[0], c[1]) * 10) + " " + num(c[2] * 10) + "\n";
    // Its buoys: one in each circle, one on the island and one far off the lake (both left out).
    const double buoys[6][2] = {{0, 14}, {-8, 6}, {8, 16}, {0, 19.2}, {0, 9}, {30, 30}};
    for (const auto& b : buoys) t += "Lake.Buoy " + num(f.cx(b[0], b[1]) * 10) + " " + num(f.cy(b[0], b[1]) * 10) + "\n";
    return t;
}

// A boat's centre over (x, y) is allowed: the map's water or the plot.
static bool CellOk(const TpiLake& lake, const TpiLakeCircle& head, double x, double y) {
    if (x < 0 || y < 0) return false;
    return TpiLakeCellOk(lake, head.x, head.y, int32_t(x / kTpiLakeCell), int32_t(y / kTpiLakeCell));
}

static uint32_t gSeed = 12345;
static uint32_t Rand() {
    gSeed = gSeed * 1103515245u + 12345u;
    return (gSeed >> 8) & 0xffffff;
}

int main() {
    // The parser, under the user's own locale (one with a decimal comma changes nothing).
    std::setlocale(LC_ALL, "");
    const Frame frames[4] = {{0, 1, 1, 0}, {0, -1, -1, 0}, {1, 0, 0, -1}, {-1, 0, 0, 1}};
    const int32_t rotKinds[4] = {0, 6, 5, 1};
    {
        TpiLake dot;
        Check(TpiParseLake(LakeText(frames[0], '.').c_str(), LakeText(frames[0], '.').size(), dot), "the lake reads");
        Check(dot.rideId == 7200 && std::fabs(dot.waterLevel + 2.04f) < 1e-4f, "the ride id and the water level");
        Check(dot.circles.size() == 4 && dot.buoys.size() == 6, "four circles and six buoys");
        Check(dot.circles[0].x == int32_t(std::lround(645 * 307.2)) && dot.circles[0].r == int32_t(std::lround(30 * 307.2)),
              "a circle in a bump's units");
        Check(dot.Water(64, 66) && !dot.Water(64, 65) && !dot.Water(64, 73) && !dot.Water(-1, 66) && !dot.Water(200, 66), "the water cells");
        TpiLake none;
        Check(!TpiParseLake("Lake.RideId 7200\n", 17, none) && !none.loaded, "no water: not loaded");
        Check(!TpiParseLake("", 0, none), "an empty file: not loaded");
    }

    // Placement, on rotation 0's lake: in each rotation, either way of centring, a spot is allowed exactly when every
    // cell next to its footprint on the side away from its front row (the entrance and exit) is water.
    {
        const std::string text = LakeText(frames[0], '.');
        TpiLake lake;
        TpiParseLake(text.c_str(), text.size(), lake);
        const int32_t rotations[4] = {0, 0x5a, 0xb4, 0x10e};
        for (int32_t rotation : rotations) {
            for (bool centred : {true, false}) {
                int accepted = 0, refused = 0, wrong = 0;
                for (int32_t y = 40; y < 90; ++y) {
                    for (int32_t x = 40; x < 90; ++x) {
                        const Placer place{x, y, rotation, 6, 4, centred};
                        std::vector<std::pair<int, int>> foot, front;
                        double fx = 0, fy = 0, gx = 0, gy = 0;
                        for (int r = 0; r < 4; ++r)
                            for (int c = 0; c < 6; ++c) {
                                int cx, cy;
                                place(c, r, cx, cy);
                                foot.push_back({cx, cy});
                                gx += cx;
                                gy += cy;
                                if (r == 0) {
                                    fx += cx;
                                    fy += cy;
                                }
                            }
                        gx /= 24;
                        gy /= 24;
                        fx /= 6;
                        fy /= 6;
                        const int ddx = std::fabs(gx - fx) > std::fabs(gy - fy) ? (gx > fx ? 1 : -1) : 0;
                        const int ddy = ddx == 0 ? (gy > fy ? 1 : -1) : 0;
                        bool expect = true;
                        int behind = 0;
                        for (const auto& c : foot) {
                            const std::pair<int, int> b{c.first + ddx, c.second + ddy};
                            bool inside = false;
                            for (const auto& o : foot) inside = inside || o == b;
                            if (inside) continue;
                            ++behind;
                            if (!lake.Water(b.first, b.second)) expect = false;
                        }
                        if (behind != 6) ++wrong;
                        const bool got = TpiLakeBackOnWater(lake, 6, 4, place);
                        if (got != expect) ++wrong;
                        (got ? accepted : refused)++;
                    }
                }
                const std::string name = "rotation " + std::to_string(rotation) + (centred ? " centred" : " corner");
                Check(wrong == 0, name + ": allowed exactly where the back touches water (" + std::to_string(wrong) + " wrong)");
                Check(refused > 2000, name + ": most spots refused");
                // Only rotation 0 faces this lake with its back (the lake is behind the plot's south side).
                if (rotation == 0) Check(accepted >= 15, name + ": spots along the shore allowed (" + std::to_string(accepted) + ")");
            }
        }
        // Rotation 0, centred: the click (64, 64) has its back row on y 66, the lake's first row: allowed; one row
        // further from the lake, refused; with the back over the island's row, refused.
        Check(TpiLakeBackOnWater(lake, 6, 4, Placer{64, 64, 0, 6, 4, true}), "the spot in front of the lake allowed");
        Check(!TpiLakeBackOnWater(lake, 6, 4, Placer{64, 63, 0, 6, 4, true}), "a row inland refused");
        Check(!TpiLakeBackOnWater(lake, 6, 4, Placer{64, 70, 0, 6, 4, true}), "the back on the island refused");
        Check(!TpiLakeBackOnWater(lake, 6, 4, Placer{76, 64, 0, 6, 4, true}), "the back half over the shore refused");
    }

    // The chain and the boats, behind the plot in each rotation.
    for (int k = 0; k < 4; ++k) {
        const Frame& f = frames[k];
        const std::string text = LakeText(f, '.');
        TpiLake lake;
        TpiParseLake(text.c_str(), text.size(), lake);
        const std::string name = "rotation kind " + std::to_string(rotKinds[k]);
        const TpiLakeCircle head{64 * kTpiLakeCell + 0x600, 64 * kTpiLakeCell + 0x600, 0x1100};
        const TpiLakeNet net = TpiLakeBuild(lake, head, rotKinds[k]);
        Check(!net.Empty(), name + ": a chain");
        if (net.Empty()) continue;
        const size_t n = net.nodes.size();
        Check(net.nodes[0].x == head.x && net.nodes[0].y == head.y && net.nodes[0].r == head.r, name + ": node 0 is the plot's");
        Check(net.buoys.size() == 4, name + ": the four buoys over water kept (" + std::to_string(net.buoys.size()) + ")");
        // A wrong way round: no chain.
        Check(TpiLakeBuild(lake, head, rotKinds[(k + 1) % 4]).Empty(), name + ": no chain when the back faces land");
        Check(TpiLakeBuild(lake, head, 3).Empty(), name + ": no chain for an unknown rotation");
        // Every circle over water or the plot, at 64 points round its edge and its centre.
        int over = 0;
        for (const TpiLakeCircle& c : net.nodes) {
            if (!CellOk(lake, head, c.x, c.y)) ++over;
            for (int a = 0; a < 64; ++a) {
                const double t = a * 6.283185307 / 64;
                if (!CellOk(lake, head, c.x + (c.r - 1) * std::cos(t), c.y + (c.r - 1) * std::sin(t))) ++over;
            }
        }
        Check(over == 0, name + ": every circle over water and the plot (" + std::to_string(over) + " points over land)");
        // The route from every node to every other: each hop to an overlapping node, the target reached.
        int bad = 0;
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j) {
                size_t at = i;
                for (size_t s = 0; s < n && at != j; ++s) {
                    const size_t nx = static_cast<size_t>(net.Next(int32_t(at), int32_t(j)));
                    if (nx == at || !detail::LakeOverlap(net.nodes[at], net.nodes[nx])) break;
                    at = nx;
                }
                if (at != j) ++bad;
            }
        Check(bad == 0, name + ": every node reaches every node (" + std::to_string(bad) + " not)");

        // The boats: launched at random in the plot's circle, each sails to 3 random buoys, then back into the plot's
        // circle; 400 rides of 3 boats. A boat steers as SetNavRoute does with the hook, moves at most 90 units a turn
        // with a little sideways wobble, and like BMP_InsideArea is kept inside the circles (it stays put and its
        // wobble changes when a move would leave them).
        int land = 0, off = 0, stuck = 0, legs = 0;
        long turns = 0;
        for (int ride = 0; ride < 400; ++ride) {
            for (int boat = 0; boat < 3; ++boat) {
                const double a = (Rand() % 512) * 6.283185307 / 512, d = Rand() % (head.r - 200);
                double x = head.x + d * std::cos(a), y = head.y + d * std::sin(a);
                int32_t col = 0;
                for (int leg = 0; leg < 4; ++leg) {
                    TpiLakePoint target;
                    if (leg < 3) target = net.buoys[Rand() % net.buoys.size()];
                    else target = {head.x, head.y};
                    ++legs;
                    int t = 0;
                    double wob = 0;
                    for (; t < 20000; ++t, ++turns) {
                        if (leg == 3 ? detail::LakeInside(net.nodes[0], x, y)
                                     : detail::LakeDistance(x - target.x, y - target.y) < 0x68 * 4)
                            break;
                        int32_t ax = target.x, ay = target.y;
                        const int32_t to = TpiLakeNodeAt(net, target.x, target.y);
                        TpiLakeAim(net, col, int32_t(x), int32_t(y), to, ax, ay);
                        double vx = ax - x, vy = ay - y;
                        const double len = detail::LakeDistance(vx, vy);
                        if (len > 90) {
                            vx = vx * 90 / len;
                            vy = vy * 90 / len;
                        }
                        const double nx = x + vx - vy * wob, ny = y + vy + vx * wob;
                        if (TpiLakeNodeAt(net, int32_t(nx), int32_t(ny)) >= 0) {
                            x = nx;
                            y = ny;
                        } else {
                            wob = ((Rand() % 200) - 100) / 100.0;
                        }
                        if (Rand() % 50 == 0) wob = ((Rand() % 60) - 30) / 100.0;
                        if (!detail::LakeInside(net.nodes[size_t(col)], x, y)) col = TpiLakeNodeAt(net, int32_t(x), int32_t(y));
                        if (col < 0) {
                            ++off;
                            col = 0;
                        }
                        if (!CellOk(lake, head, x, y)) ++land;
                    }
                    if (t >= 20000) ++stuck;
                }
            }
        }
        Check(land == 0, name + ": no boat over land (" + std::to_string(land) + " turns)");
        Check(off == 0, name + ": no boat off the circles (" + std::to_string(off) + " turns)");
        Check(stuck == 0, name + ": no boat stuck (" + std::to_string(stuck) + " of " + std::to_string(legs) + " legs)");
        Check(turns > 100000, name + ": a long run (" + std::to_string(turns) + " turns)");
    }

    // A lake that is not behind the plot, and a lake with no path round: no chain, the Mac's ride.
    {
        TpiLake lake;
        const std::string text = LakeText(frames[0], '.');
        TpiParseLake(text.c_str(), text.size(), lake);
        const TpiLakeCircle far{20 * kTpiLakeCell + 0x600, 20 * kTpiLakeCell + 0x600, 0x1100};
        Check(TpiLakeBuild(lake, far, 0).Empty(), "a plot far from the lake: no chain");
        TpiLake off = lake;
        off.loaded = false;
        Check(TpiLakeBuild(off, TpiLakeCircle{64 * kTpiLakeCell + 0x600, 64 * kTpiLakeCell + 0x600, 0x1100}, 0).Empty(), "not loaded: no chain");
    }

    // Which circles BMP_InsideArea moves a bump into in the mod's worlds: a water track's crossroad centre (the Mac's 0x80,
    // kinds -5, -8, -12 and -13) is entered by its own ride's bumps, a lake chain's circle only by its own ride's boats,
    // and the lake's circles keep the Mac's 0x80 (no bump to bump collision) as before.
    {
        struct Obj {
            uint32_t flags;
            int32_t x, y, r;
            bool chainOfBump;   // on the chain of the bump's own ride
        };
        // BMP_InsideArea's walk with the hook: the first live object, not an outside one (0x40), with the point inside it.
        const auto inside = [](const std::vector<Obj>& objs, int32_t x, int32_t y) {
            for (size_t i = 0; i < objs.size(); ++i) {
                const Obj& o = objs[i];
                if ((o.flags & 1) == 0) continue;
                if (TpiLakeCol(o.flags) && !TpiLakeColEnterable(o.flags, o.chainOfBump)) continue;
                const double dx = double(x) - o.x, dy = double(y) - o.y;
                if ((o.flags & 0x40) == 0 && dx * dx + dy * dy < double(o.r) * o.r) return int32_t(i);
            }
            return int32_t(-1);
        };
        // A crossroad section (BMP_TrackSectionPlaceSection, case 7) at (ux, uy): the centre circle, then its four corners.
        const auto crossroad = [](int32_t ux, int32_t uy, bool centre80) {
            std::vector<Obj> objs;
            objs.push_back({0x1fu | (centre80 ? 0x80u : 0u), ux + 0xc00, uy + 0xc00, 0xc20, false});
            objs.push_back({(0x1fu | 0x40u) & 0xffffffebu, ux + 0x1800, uy, 0x500, false});
            objs.push_back({(0x1fu | 0x40u) & 0xffffffedu, ux, uy, 0x500, false});
            objs.push_back({(0x1fu | 0x40u) & 0xfffffff5u, ux, uy + 0x1800, 0x500, false});
            objs.push_back({(0x1fu | 0x40u) & 0xfffffff3u, ux + 0x1800, uy + 0x1800, 0x500, false});
            return objs;
        };
        for (int32_t kind : {-5, -8, -12, -13, -6}) {
            const bool centre80 = kind == -5 || kind == -8 || kind == -12 || kind == -13;
            const std::vector<Obj> objs = crossroad(0x24000, 0x18000, centre80);
            int entered = 0;
            for (int32_t dy = -0x800; dy <= 0x800; dy += 0x100)
                for (int32_t dx = -0x800; dx <= 0x800; dx += 0x100)
                    if (inside(objs, 0x24000 + 0xc00 + dx, 0x18000 + 0xc00 + dy) == 0) ++entered;
            Check(entered == 17 * 17, "kind " + std::to_string(kind) + ": a bump enters the crossroad's centre (" +
                                          std::to_string(entered) + " of 289 points)");
            Check(!TpiLakeCol(objs[0].flags), "kind " + std::to_string(kind) + ": a crossroad's centre is no lake circle");
        }

        // A lake chain of three circles (the plot's own first, unmarked, as TpiLakeJoin leaves it).
        std::vector<Obj> lake;
        lake.push_back({0x1fu, 0x10000, 0x10000, 0x1100, true});
        lake.push_back({0x1fu | kTpiLakeColFlags, 0x11800, 0x10000, 0x1000, true});
        lake.push_back({0x1fu | kTpiLakeColFlags, 0x13000, 0x10000, 0x1000, true});
        Check(inside(lake, 0x11a00, 0x10000) == 1 && inside(lake, 0x13200, 0x10000) == 2, "the lake's own boat enters its circles");
        for (Obj& o : lake) o.chainOfBump = false;
        Check(inside(lake, 0x13200, 0x10000) == -1, "another ride's bump does not enter a lake circle");
        Check(inside(lake, 0x10000, 0x10000) == 0, "the plot's own circle is the Mac's, entered by any bump");
        Check((lake[1].flags & 0x80u) != 0 && (lake[2].flags & 0x80u) != 0, "the lake's circles keep the Mac's 0x80");

        // No flags the Mac gives a collision object (0x1f, then 0x20, 0x40, 0x80 and 0x100 on, corner bits off) mark a lake.
        int marked = 0;
        for (uint32_t on = 0; on < 16; ++on)
            for (uint32_t off = 0; off < 16; ++off) {
                uint32_t f = 0x1fu;
                if (on & 1) f |= 0x20u;
                if (on & 2) f |= 0x40u;
                if (on & 4) f |= 0x80u;
                if (on & 8) f |= 0x100u;
                f &= ~(off << 1);
                if (TpiLakeCol(f)) ++marked;
            }
        Check(marked == 0, "no Mac collision object is taken for a lake circle (" + std::to_string(marked) + ")");
        Check((kTpiLakeColFlag & 0x1ffu) == 0, "the lake mark is none of the Mac's bits");
    }

    if (gFailures == 0) std::printf("tpi_great_lake: all checks passed\n");
    return gFailures;
}
