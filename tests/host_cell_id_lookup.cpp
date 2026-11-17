// host_cell_id_lookup.cpp: the lookup of a map or track cell by its 1 based id (include/CellIdLookup.h), as MapCellOf and
// TrackCellOf in src/game/ui/ui_main.cpp, PointerCell in src/game/Mouse.cpp and the ticket queue use it, on the host,
// without game data. A made-up world laid out as CWorld (the map cells at 0x2d8, 0x44 bytes each, then the track cells,
// 0x28 bytes each) checks that the id 0, a cell that belongs to nothing, lands where the Mac's 32 bit sum puts it, one cell
// before the grid and inside the world, not 4 GB past it, and that map cell 0's thing list is empty as the big-endian Mac
// reads it, though a little-endian host finds the park's year there. Then the 0.5.5 crash's chain: a TPI Worlds hoarding
// cell (track mode 0xc) whose post was reset (mode 7, parent 0). Exits nonzero on the failed checks' count.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CellIdLookup.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

namespace {
constexpr uint32_t kMapCells = 0x2d8, kMapSize = 0x44, kTrackCells = 0x1102d8, kTrackSize = 0x28, kCells = 0x4000;
constexpr uint32_t kYear = 0x2b8;          // CParkTime (0x2a0) + 0x18: the raw year
constexpr uint32_t kWho = 0x24, kType = 8, kParent = 0x10;

bool Inside(const std::vector<uint8_t>& world, const void* p, size_t bytes) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p), b = reinterpret_cast<uintptr_t>(world.data());
    return a >= b && a + bytes <= b + world.size();
}
uint16_t Read16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}
void Write16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, 2); }
void Write32(uint8_t* p, int32_t v) { std::memcpy(p, &v, 4); }
}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // the checks before a fault still print
    using namespace tpw;
    // The offsets.
    Check(CellIdOffset(1, kMapSize) == 0, "map id 1: the first cell");
    Check(CellIdOffset(2, kMapSize) == 0x44, "map id 2: the second cell");
    Check(CellIdOffset(kCells, kMapSize) == static_cast<std::ptrdiff_t>((kCells - 1) * kMapSize), "map id 0x4000: the last cell");
    Check(CellIdOffset(0, kMapSize) == -0x44, "map id 0: one cell before the first");
    Check(CellIdOffset(0, kTrackSize) == -0x28, "track id 0: one cell before the first");
    Check(CellIdOffset(0xffff, kMapSize) == static_cast<std::ptrdiff_t>(0xfffe * kMapSize), "the largest uint16 id: forward");

    std::vector<uint8_t> world(kTrackCells + kCells * kTrackSize, 0);
    uint8_t* const map = world.data() + kMapCells;
    uint8_t* const tracks = world.data() + kTrackCells;

    // The id 0 stays inside the world: map cell 0 is CWorld + 0x294, track cell 0 the tail of the last map cell.
    uint8_t* const mapZero = CellIdAt<uint8_t>(map, 0, kMapSize);
    uint8_t* const trackZero = CellIdAt<uint8_t>(tracks, 0, kTrackSize);
    Check(Inside(world, mapZero, kMapSize), "map cell 0 lies inside the world (not 4 GB past the cells)");
    Check(mapZero == world.data() + 0x294, "map cell 0 is CWorld + 0x294");
    Check(Inside(world, trackZero, kTrackSize), "track cell 0 lies inside the world");
    Check(trackZero == map + (kCells - 1) * kMapSize + (kMapSize - kTrackSize), "track cell 0 is the last map cell's last 0x28 bytes");
    Check(CellIdAt<uint8_t>(map, 0x1234, kMapSize) == map + 0x1233 * kMapSize, "an ordinary id: its cell");
    Check(CellIdAt<uint8_t>(tracks, 0x1234, kTrackSize) == tracks + 0x1233 * kTrackSize, "an ordinary track id: its cell");

    // Map cell 0's thing list head is the high half of the year on the Mac (big-endian), the low half on this host.
    const int32_t year = 2000;
    Write32(world.data() + kYear, year);
    Check(mapZero + kWho == world.data() + kYear, "map cell 0's list head is the year's word");
    const uint8_t bigEndianYear[4] = {0, 0, 0x07, 0xd0};
    Check(((bigEndianYear[0] << 8) | bigEndianYear[1]) == 0, "the Mac reads 0 there: an empty list");
    const uint16_t hostWho = Read16(mapZero + kWho);
    Check(CellIdWho(0, hostWho) == 0, "the port answers an empty list for map cell 0, whatever the host reads there");
    Check(CellIdWho(0x1234, 0x55) == 0x55, "an ordinary cell keeps its list head");
    Check(IsMapCellZero(mapZero, map), "map cell 0 is recognised");
    Check(!IsMapCellZero(map, map), "the first map cell is not map cell 0");
    Check(!IsMapCellZero(map + kMapSize, map), "the second map cell is not map cell 0");
    Check(!IsMapCellZero(trackZero, map), "track cell 0 is not map cell 0");

    // The 0.5.5 crash's chain (ContextMouse, kFA8): a hoarding cell (mode 0xc) names its post; the post was reset by the
    // walkway repair (mode 7, parent 0); the owner is then looked up through the post's parent, the id 0.
    const uint32_t post = 1 + 49 + (41u << 7), child = post + 1;
    uint8_t* const postTrack = CellIdAt<uint8_t>(tracks, post, kTrackSize);
    uint8_t* const childTrack = CellIdAt<uint8_t>(tracks, child, kTrackSize);
    Write32(childTrack + kType, 0xc);
    Write16(childTrack + kParent, static_cast<uint16_t>(post));
    Write32(postTrack + kType, 7);
    Write16(postTrack + kParent, 0);
    const uint32_t parentOfChild = Read16(childTrack + kParent);
    uint8_t* const named = CellIdAt<uint8_t>(tracks, parentOfChild, kTrackSize);
    Check(named == postTrack, "the hoarding cell names its post");
    const uint32_t owner = Read16(named + kParent);
    uint8_t* const ownerCell = CellIdAt<uint8_t>(map, owner, kMapSize);
    Check(Inside(world, ownerCell, kMapSize), "the post's parent cell (id 0) lies inside the world");
    const bool none = IsMapCellZero(ownerCell, map) || CellIdWho(owner, Read16(ownerCell + kWho)) == 0;
    Check(none, "and has no thing: ContextMouse returns as the Mac does");

    if (gFailures == 0) std::printf("host_cell_id_lookup: all checks passed\n");
    return gFailures;
}
