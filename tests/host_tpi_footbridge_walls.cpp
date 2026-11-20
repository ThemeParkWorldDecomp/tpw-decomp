// TPW_MOD_HOOK tpi_worlds: a footbridge's sides are walls (tpw::TpiFootbridgeWall, MAP_wall_in_way). A made-up tunnel
// shaped like a TPI one: a two-cell floor seventeen cells long, two canyons across it, open land beside it at both
// mouths and between the canyons, and a short path at each mouth. No game data.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "TpiFootbridge.h"

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what); }
}

// '.' open land (mode 0), '#' a canyon (mode 7, no-go), '=' the floor (mode 0x1e), 'P' a path (mode 1).
constexpr int width = 24, height = 10;
const char* const kTunnel[height] = {
    "........###......####...",
    "........###......###....",
    ".......####......####...",
    ".......####......##.....",
    ".PPP=================PPP",
    ".PPP=================PPP",
    "......####.....####.....",
    "......####....####......",
    "......###.....###.......",
    ".....####.....###.......",
};

tpw::TpiFootbridgeCell CellAt(int x, int y) {
    const char c = kTunnel[y][x];
    return {c == '=' ? 0x1e : c == 'P' ? 1 : c == '#' ? 7 : 0, 0};
}

// MAP_wall_in_way's answer for the cell kinds above (no queue, ride, track or entrance among them), first the mod's
// wall, then the Mac's rules in the Mac's order.
bool Wall(tpw::TpiFootbridgeCell here, tpw::TpiFootbridgeCell that, int32_t mode) {
    if (tpw::TpiFootbridgeWall(here, that, mode)) return true;
    if (that.mode == 1) return false;
    if (that.mode == 0x1e && mode != 2) return false;
    if (that.mode == 7) return true;
    if (here.mode != 1) return false;
    if (mode == 1 && that.mode != 1) return false;
    return true;
}

// The cells a walker in nav mode `mode` reaches from (sx, sy), one straight step at a time (a diagonal step needs two
// straight ones open, implement_nav_rulesA2B).
void Reach(int sx, int sy, int32_t mode, bool reached[height][width]) {
    std::memset(reached, 0, sizeof(bool) * width * height);
    int queue[width * height][2];
    int head = 0, tail = 0;
    reached[sy][sx] = true;
    queue[tail][0] = sx, queue[tail][1] = sy, ++tail;
    constexpr int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    while (head < tail) {
        const int x = queue[head][0], y = queue[head][1];
        ++head;
        for (int d = 0; d < 4; ++d) {
            const int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height || reached[ny][nx]) continue;
            if (Wall(CellAt(x, y), CellAt(nx, ny), mode)) continue;
            reached[ny][nx] = true;
            queue[tail][0] = nx, queue[tail][1] = ny, ++tail;
        }
    }
}

// Whether the walk from the west mouth's path in nav mode 0 (a guest's) reaches exactly the floor and the two paths.
bool ExactlyTheStrip() {
    bool reached[height][width];
    Reach(1, 4, 0, reached);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const char c = kTunnel[y][x];
            if (reached[y][x] != (c == '=' || c == 'P')) return false;
        }
    return true;
}

void Tunnel() {
    Check(ExactlyTheStrip(), "a guest's walk through the tunnel reaches the floor and both mouths' paths, nothing else");
    // Both mouths join, each way, in nav modes 0 and 1.
    for (int32_t mode : {0, 1}) {
        bool reached[height][width];
        Reach(1, 5, mode, reached);
        Check(reached[4][23] && reached[5][23], "the west mouth's path reaches the east mouth's");
        Reach(23, 4, mode, reached);
        Check(reached[4][1] && reached[5][1], "the east mouth's path reaches the west mouth's");
    }
    // From every floor cell the only open sides lead to the floor or a path.
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            if (kTunnel[y][x] != '=') continue;
            for (int32_t mode : {0, 1}) {
                bool reached[height][width];
                Reach(x, y, mode, reached);
                bool onlyStrip = true;
                for (int yy = 0; yy < height; ++yy)
                    for (int xx = 0; xx < width; ++xx)
                        if (reached[yy][xx] && kTunnel[yy][xx] == '.' && mode == 0) onlyStrip = false;
                Check(onlyStrip, "a guest on the floor never reaches the land beside it");
                for (int d = 0; d < 4; ++d) {
                    const int nx = x + (d == 1) - (d == 3), ny = y + (d == 2) - (d == 0);
                    const char c = kTunnel[ny][nx];
                    Check(Wall(CellAt(x, y), CellAt(nx, ny), mode) == (c != '=' && c != 'P'),
                          "a floor cell's side is open exactly toward the floor or a path");
                }
            }
        }
    // A guest who is on the land beside the floor still gets back onto it, and onto a path from there.
    bool reached[height][width];
    Reach(5, 3, 0, reached);
    Check(reached[3][5] && reached[4][5] && reached[4][1] && reached[4][23], "a stray guest walks back onto the floor");
}
}  // namespace

int main() {
    using namespace tpw;
    const TpiFootbridgeCell floor{0x1e, 0}, pavement{0x1e, 0x80}, path{1, 0}, land{0, 0}, canyon{7, 0};
    for (bool enabled : {false, true}) {
        gModTpiWorlds = enabled;
        for (const char* theme : {"arabian", "water", "science", "jungle", "hallow", "space", "fantasy", ""}) {
            TpiNoteTheme(theme);
            const bool active = enabled && (std::strcmp(theme, "arabian") == 0 || std::strcmp(theme, "water") == 0 ||
                                            std::strcmp(theme, "science") == 0);
            for (int32_t mode : {0, 1}) {
                Check(TpiFootbridgeWall(floor, land, mode) == active, "a floor's side onto land is a wall in the mod's worlds only");
                Check(TpiFootbridgeWall(floor, canyon, mode) == active, "and onto any other cell");
                Check(!TpiFootbridgeWall(floor, floor, mode), "the floor goes on");
                Check(!TpiFootbridgeWall(floor, path, mode), "the floor meets a path");
                Check(!TpiFootbridgeWall(floor, pavement, mode), "the floor meets the gate's pavement");
                Check(!TpiFootbridgeWall(pavement, land, mode), "the gate's pavement keeps the Mac's walls");
                Check(!TpiFootbridgeWall(path, land, mode), "a path keeps the Mac's walls");
                Check(!TpiFootbridgeWall(land, floor, mode), "land keeps the Mac's walls");
            }
            Check(!TpiFootbridgeWall(floor, land, 2), "first person keeps the Mac's walls");
        }
    }
    gModTpiWorlds = true;
    TpiNoteTheme("arabian");
    Tunnel();
    // The Mac's walls: off the floor a guest walks onto the land beside it, which is the bug.
    gModTpiWorlds = false;
    TpiNoteTheme(nullptr);
    Check(!ExactlyTheStrip(), "without the walls a guest's walk leaves the floor (the Mac's rules)");
    std::printf("tpi_footbridge_walls: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
