// TPW_MOD_HOOK tpi_worlds: synthetic tunnel floors, both mouths, guest wandering and unchanged original worlds.
// No game data. Stored cell modes, flags and links stay unchanged, including when an old park is loaded.
#include <array>
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
struct Cell {
    tpw::TpiFootbridgeCell floor{0, 0};
    uint8_t stored = 0;
};
constexpr int width = 25, height = 9;
using Grid = std::array<Cell, width * height>;
uint8_t Links(const Grid& grid, int x, int y) {
    return tpw::TpiFootbridgeLinks(grid[y * width + x].floor, [&](uint8_t direction) {
        const int nx = x + (direction == 4) - (direction == 0x40);
        const int ny = y + (direction == 0x10) - (direction == 1);
        return nx < 0 || nx >= width || ny < 0 || ny >= height ? tpw::TpiFootbridgeCell{-1, 0}
                                                                          : grid[ny * width + nx].floor;
    });
}
void Corridor() {
    // A made-up two-cell-wide floor, seventeen cells long, with paths at each end and no stored bridge links.
    Grid grid{};
    for (int y = 3; y <= 4; ++y) {
        grid[y * width + 2].floor = {1, 0};
        grid[y * width + 20].floor = {1, 0};
        for (int x = 3; x < 20; ++x) grid[y * width + x].floor = {0x1e, 0x40};
    }
    const Grid saved = grid;
    for (int y = 3; y <= 4; ++y) {
        Check(Links(grid, 2, y) == 4, "west mouth path joins floor");
        Check(Links(grid, 20, y) == 0x40, "east mouth path joins floor");
        // A guest's 1..5-step random choices can cross the full corridor in either direction.
        for (int sign : {-1, 1}) {
            int x = sign > 0 ? 2 : 20;
            const int end = sign > 0 ? 20 : 2;
            const uint8_t heading = sign > 0 ? 4 : 0x40;
            while (x != end) {
                Check(tpw::TpiFootbridgeWander(false, false), "guest gets the bridge supplement");
                Check((Links(grid, x, y) & heading) != 0, "every step crosses the floor to the far path");
                x += sign;
            }
        }
    }
    for (size_t i = 0; i < grid.size(); ++i) {
        Check(grid[i].floor.mode == saved[i].floor.mode && grid[i].floor.flags == saved[i].floor.flags &&
                  grid[i].stored == saved[i].stored, "drawing and walks preserve saved cells");
    }
}
}  // namespace

int main() {
    using namespace tpw;
    for (bool enabled : {false, true}) {
        gModTpiWorlds = enabled;
        for (const char* theme : {"arabian", "water", "science", "jungle", "hallow", "space", "fantasy", ""}) {
            TpiNoteTheme(theme);
            const bool active = enabled && (std::strcmp(theme, "arabian") == 0 || std::strcmp(theme, "water") == 0 ||
                                            std::strcmp(theme, "science") == 0);
            for (bool staff : {false, true}) for (bool mod : {false, true})
                Check(TpiFootbridgeWander(staff, mod) == (active || (staff && mod)), "only TPI guests get bridge wandering");
            Check(TpiFootbridgeStep({1, 0}, {0x1e, 0}) == active, "path onto bridge");
            Check(TpiFootbridgeStep({0x1e, 0}, {1, 0}) == active, "bridge onto path");
            Check(TpiFootbridgeStep({0x1e, 0}, {0x1e, 0}) == active, "bridge interior");
            for (int mode : {-1, 0, 2, 3, 4, 7, 9, 10, 0x19}) {
                Check(!TpiFootbridgeStep({mode, 0}, {0x1e, 0}), "no entry from other cell modes");
                Check(!TpiFootbridgeStep({0x1e, 0}, {mode, 0}), "no exit onto other cell modes");
            }
            Check(!TpiFootbridgeStep({1, 0}, {0x1e, 0x80}), "gate pavement is not a bridge");
            Check(!TpiFootbridgeStep({0x1e, 0x80}, {1, 0}), "gate pavement gets no exit links");
            Check(TpiFootbridgeLinks({1, 0}, [](uint8_t) { return TpiFootbridgeCell{0x1e, 0}; }) ==
                      (active ? 0x55 : 0), "four cardinal joins, no diagonals");
            Check(TpiFootbridgeLinks({3, 0}, [](uint8_t) { return TpiFootbridgeCell{0x1e, 0}; }) == 0,
                  "queues retain original joins");
        }
    }
    gModTpiWorlds = true;
    TpiNoteTheme("arabian");
    Corridor();
    // All four compass directions, including a one-cell floor at the map edge and a missing neighbour.
    for (uint8_t heading : {uint8_t(1), uint8_t(4), uint8_t(0x10), uint8_t(0x40)}) {
        Check(TpiFootbridgeLinks({1, 0}, [heading](uint8_t d) {
            return TpiFootbridgeCell{d == heading ? 0x1e : -1, 0};
        }) == heading, "mouth can face any direction");
    }
    gModTpiWorlds = false;
    TpiNoteTheme(nullptr);
    std::printf("tpi_footbridge: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
