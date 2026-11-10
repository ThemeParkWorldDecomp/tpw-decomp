// host_ps1_save_repair.cpp: the ps1-rides mod's repair of parks saved with its 0.4.0 rides (TPW_MOD_HOOK ps1_save_repair,
// include/Ps1SaveRepair.h), on a stand-in for the map's cells. A ride's doors as its record gives them (DoorsFor, at each
// rotation); a ride whose saved doors match is left alone; the doors swapped on the same cells (Romper); a door moved
// along its side by one cell (Jack) and by three (Inca Pot), where a cell or more is laid to reach a path; a path in
// front of the new entrance that can become the queue and one that cannot; a new door with water in front of it (no
// queue, no exit path: the game's needs-a-queue state); cells that are not such a ride (nothing changes); and the repair
// run twice (the second changes nothing). After each repair the queue is walked as CObject::GetBackOfPhysicalQueue walks
// it and the exit's path is checked.
// Run by ctest ("ps1_save_repair") in a build with the port's fixes, which alone has the repair. Exits nonzero on a failure.
#include <cstdio>
#include <string>
#include <vector>

#include "Ps1SaveRepair.h"

using namespace ps1repair;

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

uint16_t Id(int32_t x, int32_t y) { return static_cast<uint16_t>(x + (y << 7) + 1); }

// The map's cells, 128 by 128 as the game's, with what the repair reads and writes.
struct Grid {
    struct Cell {
        int32_t type = kGrass;
        uint8_t links = 0;
        uint8_t dir = 0;
        uint16_t parent = 0;
        uint16_t flags = 0;
        bool operator==(const Cell& o) const {
            return type == o.type && links == o.links && dir == o.dir && parent == o.parent && flags == o.flags;
        }
    };
    std::vector<Cell> cells = std::vector<Cell>(0x4001);

    Cell& At(uint16_t c) { return cells[c]; }
    const Cell& At(uint16_t c) const { return cells[c]; }

    uint16_t Step(uint16_t cell, uint8_t dir) const {
        int32_t x = (cell - 1) & 0x7f, y = (cell - 1) >> 7;
        if (dir & (1 | 2 | 0x80)) --y;
        if (dir & (0x10 | 8 | 0x20)) ++y;
        if (dir & (4 | 2 | 8)) ++x;
        if (dir & (0x40 | 0x20 | 0x80)) --x;
        if (x < 0 || y < 0 || x > 127 || y > 127) return 0;
        return Id(x, y);
    }
    int32_t Type(uint16_t c) const { return At(c).type; }
    uint8_t Links(uint16_t c) const { return At(c).links; }
    uint8_t Direction(uint16_t c) const { return At(c).dir; }
    uint16_t Parent(uint16_t c) const { return At(c).parent; }
    bool Free(uint16_t c) const { return At(c).type == kGrass && At(c).parent == 0 && (At(c).flags & 0x160) == 0; }
    bool Plain(uint16_t) const { return true; }
    void SetDoor(uint16_t c, int32_t type, uint8_t out) { At(c).type = type; At(c).dir = out; At(c).links = 0; }
    void SetBody(uint16_t c) { At(c).type = kRide; At(c).dir = 0; At(c).links = 0; }
    void Unlink(uint16_t c, uint8_t mask) { At(c).links = static_cast<uint8_t>(At(c).links & ~mask); }
    void Link(uint16_t c, uint8_t mask) { At(c).links = static_cast<uint8_t>(At(c).links | mask); }
    void SetQueue(uint16_t c, uint8_t dir, uint16_t parent, bool front) {
        At(c) = Cell{kQueue, 0, dir, parent, static_cast<uint16_t>(front ? 0x20 : 0)};
    }
    // CalculateNeighbours of mode 1, straight neighbours only: the cell joins its path neighbours, an exit facing it and
    // an entrance whose direction points away from it.
    void MakePath(uint16_t c) {
        At(c).type = kPath;
        At(c).parent = 0;
        for (uint8_t d : kStraight) {
            const uint16_t n = Step(c, d);
            if (n == 0) continue;
            const bool join = Type(n) == kPath || (Type(n) == kExit && Direction(n) == Opposite(d)) ||
                              (Type(n) == kEntrance && Direction(n) == d);
            if (join) {
                Link(c, d);
                Link(n, Opposite(d));
            }
        }
    }
    void SetEdgeFlag(uint16_t c, bool on) { At(c).flags = static_cast<uint16_t>(on ? At(c).flags | 0x20 : At(c).flags & ~0x20); }
};

// A park: a ride of `w` by `h` cells from (rx, ry), parent id its first cell, inside a ring of path at x = 5 and 20,
// y = 5 and 20 (its cells joined).
struct Park {
    Grid g;
    uint16_t parent;
    Park(int32_t rx, int32_t ry, int32_t w, int32_t h) : parent(Id(rx, ry)) {
        for (int32_t y = ry; y < ry + h; ++y)
            for (int32_t x = rx; x < rx + w; ++x) g.At(Id(x, y)) = Grid::Cell{kRide, 0, 0, parent, 0};
        for (int32_t i = 5; i <= 20; ++i) {
            g.MakePath(Id(i, 5));
            g.MakePath(Id(i, 20));
            g.MakePath(Id(5, i));
            g.MakePath(Id(20, i));
        }
    }
    void Door(int32_t x, int32_t y, int32_t type, uint8_t out) { g.SetDoor(Id(x, y), type, out); }
    // A queue placed as the game places one: from the cell in front of the entrance (its first) on, each pointing to the one
    // before it and joined to it, the last joined to the path it reaches.
    void Queue(uint16_t entrance, const std::vector<uint16_t>& cells, uint16_t path) {
        uint16_t before = entrance;
        for (size_t i = 0; i < cells.size(); ++i) {
            uint8_t dir = 0;
            for (uint8_t d : kStraight)
                if (g.Step(cells[i], d) == before) dir = d;
            g.SetQueue(cells[i], dir, parent, i == 0);
            g.Link(cells[i], dir);
            g.Link(before, Opposite(dir));
            before = cells[i];
        }
        for (uint8_t d : kStraight) {
            if (g.Step(before, d) == path) {
                g.Link(before, d);
                g.Link(path, Opposite(d));
            }
        }
    }
    // An exit path as the game lays one (JoinQueueCell): each cell a path joined to the ones beside it, the first beside the exit.
    void ExitPath(const std::vector<uint16_t>& cells) {
        for (uint16_t c : cells) g.MakePath(c);
        g.SetEdgeFlag(cells[0], true);
    }
};

// The queue of the entrance `e` as the game walks it (GetFrontOfPhysicalQueue, GetQCellBehind): its length, 0 for none,
// and whether its back is joined to a path (IsBOQConnected, and the path joined back).
int32_t WalkQueue(const Grid& g, uint16_t e, bool* joined) {
    *joined = false;
    const uint8_t order[4] = {1, 0x10, 0x40, 4};
    uint16_t front = 0;
    for (uint8_t d : order) {
        if (g.Links(e) & d) {
            front = g.Step(e, d);
            break;
        }
    }
    if (front == 0 || g.Type(front) != kQueue) return 0;
    int32_t n = 1;
    uint16_t back = front;
    for (uint16_t c = detail::QueueCellBehind(g, front); c != 0 && n < 1000; c = detail::QueueCellBehind(g, c)) {
        back = c;
        ++n;
    }
    for (uint8_t d : kStraight) {
        const uint16_t p = g.Step(back, d);
        if (d != g.Direction(back) && (g.Links(back) & d) && p != 0 && g.Type(p) == kPath && (g.Links(p) & Opposite(d))) *joined = true;
    }
    return n;
}

// Whether the exit `x` opens onto a path joined to it.
bool ExitJoined(const Grid& g, uint16_t x) {
    const uint8_t out = g.Direction(x);
    const uint16_t p = g.Step(x, out);
    return g.Type(x) == kExit && (g.Links(x) & out) && p != 0 && g.Type(p) == kPath && (g.Links(p) & Opposite(out));
}

// Whether the path cells of the park are all reachable from (5, 5) along joined paths.
bool PathsJoined(const Grid& g) {
    std::vector<uint8_t> seen(0x4001, 0);
    std::vector<uint16_t> todo = {Id(5, 5)};
    seen[Id(5, 5)] = 1;
    while (!todo.empty()) {
        const uint16_t c = todo.back();
        todo.pop_back();
        for (uint8_t d : kStraight) {
            const uint16_t n = g.Step(c, d);
            if (n == 0 || seen[n] || g.Type(n) != kPath || !(g.Links(c) & d)) continue;
            seen[n] = 1;
            todo.push_back(n);
        }
    }
    for (uint16_t c = 1; c < 0x4001; ++c)
        if (g.Type(c) == kPath && !seen[c]) return false;
    return true;
}

// The doors the ride whose record gives its shape's doors at `entry` (`entryFlag`) and `exit` has at rotation `rot`, at
// the offset cell (ox, oy).
Doors DoorsAt(int32_t ox, int32_t oy, int32_t rot, int32_t ex, int32_t ey, uint8_t ef, int32_t xx, int32_t xy, uint8_t xf) {
    Doors doors;
    Check(DoorsFor(Id(ox, oy), rot, ex, ey, ef, xx, xy, xf, &doors), "DoorsFor answers at " + std::to_string(rot));
    return doors;
}

void TestDoorsFor() {
    // An entrance on the shape's row 0 with direction byte 1 (the door's inward side as the shape gives it): at rotation 0
    // the cell is the offset cell plus (x, y), the entrance opens on +y (the queue's side) and the exit on -y.
    Doors d = DoorsAt(40, 40, 0, 1, 0, 1, 2, 2, 1);
    Check(d.entrance.cell == Id(41, 40) && d.entrance.out == 0x10, "rotation 0: entrance cell and side");
    Check(d.exit.cell == Id(42, 42) && d.exit.out == 1, "rotation 0: exit cell and side");
    d = DoorsAt(40, 40, 180, 1, 0, 1, 2, 2, 1);
    Check(d.entrance.cell == Id(39, 40) && d.entrance.out == 1, "rotation 180: (-x, -y), sides turned");
    Check(d.exit.cell == Id(38, 38) && d.exit.out == 0x10, "rotation 180: exit");
    d = DoorsAt(40, 40, 90, 1, 0, 1, 2, 2, 1);
    Check(d.entrance.cell == Id(40, 39) && d.entrance.out == 4, "rotation 90: (y, -x), entrance opens on +x");
    Check(d.exit.cell == Id(42, 38) && d.exit.out == 0x40, "rotation 90: exit opens on -x");
    d = DoorsAt(40, 40, 270, 1, 0, 1, 2, 2, 1);
    Check(d.entrance.cell == Id(40, 41) && d.entrance.out == 0x40, "rotation 270: (-y, x), entrance opens on -x");
    Check(d.exit.cell == Id(38, 42) && d.exit.out == 4, "rotation 270: exit opens on +x");
    Doors none;
    Check(!DoorsFor(Id(40, 40), 45, 0, 0, 1, 1, 1, 1, &none), "no doors at rotation 45");
    Check(!DoorsFor(Id(40, 40), 0, 0, 0, 3, 1, 1, 1, &none), "no doors for a direction byte that is not straight");
}

// Romper: the doors swap on the same cells (case 2). The old queue becomes the exit's path; the old exit's path, a
// dead end off the ring, becomes the queue.
void TestSwapSameCells() {
    Park p(10, 10, 4, 3);
    p.Door(11, 12, kEntrance, 0x10);
    p.Door(12, 10, kExit, 1);
    p.Queue(Id(11, 12), {Id(11, 13), Id(11, 14), Id(11, 15), Id(11, 16), Id(11, 17), Id(11, 18), Id(11, 19)}, Id(11, 20));
    p.ExitPath({Id(12, 9), Id(12, 8), Id(12, 7), Id(12, 6)});
    bool joined = false;
    Check(WalkQueue(p.g, Id(11, 12), &joined) == 7 && joined, "Romper before: a queue of 7 joined to the ring");
    const Doors want{{Id(12, 10), 1}, {Id(11, 12), 0x10}};
    const Result r = RepairPs1RideDoors(p.g, p.parent, Id(11, 12), Id(12, 10), want);
    Check(r.outcome == Outcome::kRepaired && r.oldQueueCells == 7 && r.laidCells == 0, "Romper: repaired, 7 queue cells now paths");
    const int32_t n = WalkQueue(p.g, Id(12, 10), &joined);
    Check(r.queueJoined && n == 1 && joined, "Romper: the old exit path's first cell is a queue of 1 joined to the path");
    Check(r.exitJoined && ExitJoined(p.g, Id(11, 12)), "Romper: the new exit opens on the old queue, now a path");
    Check(p.g.Type(Id(11, 13)) == kPath && p.g.Type(Id(11, 19)) == kPath, "Romper: the old queue is a path");
    Check(PathsJoined(p.g), "Romper: every path is joined to the ring");
}

// Jack: each door moves one cell along its side and the roles swap (case 3). The cell in front of the new entrance is
// grass beside the old exit's path, so one queue cell is laid; the new exit opens on grass beside the old queue, so one
// path cell is laid.
void TestShiftOne() {
    Park p(10, 10, 4, 3);
    p.Door(11, 10, kExit, 1);
    p.Door(12, 12, kEntrance, 0x10);
    p.ExitPath({Id(11, 9), Id(11, 8), Id(11, 7), Id(11, 6)});
    p.Queue(Id(12, 12), {Id(12, 13), Id(12, 14), Id(12, 15), Id(12, 16), Id(12, 17), Id(12, 18), Id(12, 19)}, Id(12, 20));
    const Doors want{{Id(12, 10), 1}, {Id(11, 12), 0x10}};
    const Result r = RepairPs1RideDoors(p.g, p.parent, Id(12, 12), Id(11, 10), want);
    Check(r.outcome == Outcome::kRepaired && r.laidCells == 2, "Jack: repaired, 2 cells laid");
    bool joined = false;
    Check(r.queueJoined && WalkQueue(p.g, Id(12, 10), &joined) == 1 && joined, "Jack: a queue of 1 joined to the old exit's path");
    Check(r.exitJoined && ExitJoined(p.g, Id(11, 12)), "Jack: the new exit opens on a laid path");
    Check(p.g.Type(Id(11, 10)) == kRide && p.g.Type(Id(12, 12)) == kRide, "Jack: the old doors are the ride's body");
    Check(p.g.Links(Id(11, 10)) == 0 && p.g.Links(Id(12, 12)) == 0, "Jack: the old doors join nothing");
    Check((p.g.At(Id(11, 9)).flags & 0x20) == 0, "Jack: the old exit's path is no longer a cell beside a door");
    Check((p.g.At(Id(11, 13)).flags & 0x20) != 0 && (p.g.At(Id(12, 9)).flags & 0x20) != 0, "Jack: the cells in front of the new doors are");
    Check(PathsJoined(p.g), "Jack: every path is joined to the ring");
}

// Inca Pot: the doors move three cells, to the far corners (case 3); the new entrance's queue and the new exit's path
// are laid over grass to the nearest path.
void TestShiftThree() {
    Park p(10, 10, 4, 3);
    p.Door(10, 10, kExit, 1);
    p.Door(13, 12, kEntrance, 0x10);
    p.ExitPath({Id(10, 9), Id(10, 8), Id(10, 7), Id(10, 6)});
    p.Queue(Id(13, 12), {Id(13, 13), Id(13, 14), Id(13, 15), Id(13, 16), Id(13, 17), Id(13, 18), Id(13, 19)}, Id(13, 20));
    const Doors want{{Id(13, 10), 1}, {Id(10, 12), 0x10}};
    const Result r = RepairPs1RideDoors(p.g, p.parent, Id(13, 12), Id(10, 10), want);
    bool joined = false;
    const int32_t n = WalkQueue(p.g, Id(13, 10), &joined);
    Check(r.outcome == Outcome::kRepaired && r.queueJoined && n >= 1 && joined, "Inca Pot: a laid queue of " + std::to_string(n) + " joined to a path");
    Check(r.exitJoined && ExitJoined(p.g, Id(10, 12)), "Inca Pot: the new exit's path is laid to a path");
    Check(PathsJoined(p.g), "Inca Pot: every path is joined to the ring");
}

// The new entrance faces a path of the ring. In a ring it can become the queue (the ring stays joined around); in a
// single line it cannot (the line would be cut), and the entrance is left without a queue.
void TestPathInFront(bool ring) {
    const std::string what = ring ? "a ring's cell" : "a line's cell";
    Park p(10, 6, 4, 3);   // the ride's top row is y = 6: the cells in front of it are the ring's, y = 5
    if (!ring) {           // the ring cut at (20, 12): the top of the ring and its sides are one line, (12, 5) in it
        p.g.Unlink(Id(20, 11), 0x10);
        p.g.Unlink(Id(20, 13), 1);
        p.g.At(Id(20, 12)) = Grid::Cell{};
        p.g.Unlink(Id(5, 11), 0x10);
        p.g.Unlink(Id(5, 13), 1);
        p.g.At(Id(5, 12)) = Grid::Cell{};
    }
    p.Door(11, 6, kExit, 1);
    p.Door(12, 8, kEntrance, 0x10);
    p.g.MakePath(Id(11, 5));
    p.g.SetEdgeFlag(Id(11, 5), true);
    p.Queue(Id(12, 8), {Id(12, 9), Id(12, 10)}, Id(12, 11));
    p.g.MakePath(Id(12, 11));
    p.g.MakePath(Id(12, 12));
    const Doors want{{Id(12, 6), 1}, {Id(11, 8), 0x10}};
    const Result r = RepairPs1RideDoors(p.g, p.parent, Id(12, 8), Id(11, 6), want);
    bool joined = false;
    const int32_t n = WalkQueue(p.g, Id(12, 6), &joined);
    if (ring) {
        Check(r.queueJoined && n == 1 && joined, "path in front, " + what + ": it becomes a queue of 1");
    } else {
        Check(!r.queueJoined && n == 0 && p.g.Links(Id(12, 6)) == 0, "path in front, " + what + ": no queue, the entrance joins nothing");
        Check(p.g.Type(Id(12, 5)) == kPath, "path in front, " + what + ": the path stays");
    }
    Check(r.exitJoined && ExitJoined(p.g, Id(11, 8)), "path in front, " + what + ": the exit opens on the old queue");
}

// Water in front of both new doors: the doors move, but neither joins anything (the game's state of a ride that needs
// a queue and an exit path built); nothing is laid.
void TestBlocked() {
    Park p(10, 10, 4, 3);
    p.Door(11, 10, kExit, 1);
    p.Door(12, 12, kEntrance, 0x10);
    p.ExitPath({Id(11, 9), Id(11, 8), Id(11, 7), Id(11, 6)});
    p.Queue(Id(12, 12), {Id(12, 13), Id(12, 14), Id(12, 15), Id(12, 16), Id(12, 17), Id(12, 18), Id(12, 19)}, Id(12, 20));
    for (int32_t x = 6; x <= 19; ++x) {
        if (x == 11 || x == 12) continue;
        p.g.At(Id(x, 9)).type = 2;
        p.g.At(Id(x, 13)).type = 2;
    }
    p.g.At(Id(12, 9)).type = 2;
    p.g.At(Id(11, 13)).type = 2;
    const Doors want{{Id(12, 10), 1}, {Id(11, 12), 0x10}};
    const Result r = RepairPs1RideDoors(p.g, p.parent, Id(12, 12), Id(11, 10), want);
    bool joined = false;
    Check(r.outcome == Outcome::kRepaired && !r.queueJoined && !r.exitJoined && r.laidCells == 0, "blocked: doors moved, nothing joined or laid");
    Check(WalkQueue(p.g, Id(12, 10), &joined) == 0 && p.g.Links(Id(12, 10)) == 0, "blocked: the entrance has no queue");
    Check(p.g.Type(Id(12, 10)) == kEntrance && p.g.Type(Id(11, 12)) == kExit, "blocked: the doors are on their new cells");
    Check(PathsJoined(p.g), "blocked: every path is still joined to the ring");
}

// A ride whose doors are its record's (every PC and Gold ride, and a PS1 ride already repaired) and cells that are not
// a ride as the repair knows it: nothing changes.
void TestNothingToDo() {
    Park p(10, 10, 4, 3);
    p.Door(11, 10, kExit, 1);
    p.Door(12, 12, kEntrance, 0x10);
    p.ExitPath({Id(11, 9), Id(11, 8), Id(11, 7), Id(11, 6)});
    p.Queue(Id(12, 12), {Id(12, 13), Id(12, 14)}, Id(12, 15));
    p.g.MakePath(Id(12, 15));
    const std::vector<Grid::Cell> before = p.g.cells;
    Result r = RepairPs1RideDoors(p.g, p.parent, Id(12, 12), Id(11, 10), Doors{{Id(12, 12), 0x10}, {Id(11, 10), 1}});
    Check(r.outcome == Outcome::kMatches && p.g.cells == before, "matching doors: nothing changes");
    r = RepairPs1RideDoors(p.g, p.parent, Id(12, 13), Id(11, 10), Doors{{Id(12, 10), 1}, {Id(11, 12), 0x10}});
    Check(r.outcome == Outcome::kSkipped && p.g.cells == before, "a saved entry that is not an entrance: nothing changes");
    r = RepairPs1RideDoors(p.g, p.parent, Id(12, 12), Id(11, 10), Doors{{Id(12, 9), 1}, {Id(11, 12), 0x10}});
    Check(r.outcome == Outcome::kSkipped && p.g.cells == before, "a new door outside the ride: nothing changes");
    r = RepairPs1RideDoors(p.g, static_cast<uint16_t>(p.parent + 1), Id(12, 12), Id(11, 10), Doors{{Id(12, 10), 1}, {Id(11, 12), 0x10}});
    Check(r.outcome == Outcome::kSkipped && p.g.cells == before, "another ride's parent: nothing changes");
}

// The repair run on the same park twice, as a park loaded twice (or saved after the repair and loaded again): the
// second finds the doors its record's and changes nothing.
void TestTwice() {
    Park p(10, 10, 4, 3);
    p.Door(10, 10, kExit, 1);
    p.Door(13, 12, kEntrance, 0x10);
    p.ExitPath({Id(10, 9), Id(10, 8), Id(10, 7), Id(10, 6)});
    p.Queue(Id(13, 12), {Id(13, 13), Id(13, 14), Id(13, 15), Id(13, 16), Id(13, 17), Id(13, 18), Id(13, 19)}, Id(13, 20));
    const Doors want{{Id(13, 10), 1}, {Id(10, 12), 0x10}};
    Check(RepairPs1RideDoors(p.g, p.parent, Id(13, 12), Id(10, 10), want).outcome == Outcome::kRepaired, "twice: the first repairs");
    const std::vector<Grid::Cell> after = p.g.cells;
    const Result r = RepairPs1RideDoors(p.g, p.parent, want.entrance.cell, want.exit.cell, want);
    Check(r.outcome == Outcome::kMatches && p.g.cells == after, "twice: the second changes nothing");
}

}  // namespace

int main() {
    TestDoorsFor();
    TestSwapSameCells();
    TestShiftOne();
    TestShiftThree();
    TestPathInFront(true);
    TestPathInFront(false);
    TestBlocked();
    TestNothingToDo();
    TestTwice();
    std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
