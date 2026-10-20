#include <cstdint>
#include <cstring>
#include "analyser.h"
#include "Debug.h"
#include "MapCell.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "World.h"
#include "WorldSavvy.h"
#include "ppc_math.h"
#include "CellBase.h"
#include "ObjectParts.h"

// Nine halfwords the Mac keeps from 0x102a0d64, two bytes apart (initial data 0; names invented): the "no cell" ids SetRandomDestination compares
// against. [0] 0x102a0d64 patrol check, [1] 0x102a0d66 queue check, [2] 0x102a0d68 exit check, [3..6] 0x102a0d6a..0x102a0d70 the four neighbours
// in the final choice, [7] 0x102a0d72 the random pick, [8] 0x102a0d74 the assert.
uint16_t gPeepNoCell[9];

namespace {

// The texts at 0x102439f5 + 0x104, + 0x14e, + 0x171, + 0x1b5, + 0x1ef.
const char kTextInsidePatrol[] = "*** I'm inside my patrol area but can't get anywhere else within it! ***\n";
const char kTextNowhere[] = "Peep can't SetRandomDest anywhere\n";
const char kTextNavigation[] = "Serious person navigation problem: no adjacent cells are reachable\n";
const char kTextNoDirection[] = "\n*** Could not find a direction, but there should be one\n";
const char kTextProblem[] = "Problem in SetRandomDest\n";

typedef tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> CellId;

inline CellId Id(uint16_t cell) {
    CellId id;
    id.mIndex = cell;
    return id;
}

// The peep's cell: (y * 128 + 1 + x) in 16 bits, from the bytes at +4 and +6.
inline uint16_t PeepCell(const uint8_t* self) {
    return static_cast<uint16_t>(static_cast<uint32_t>(self[4]) + (static_cast<uint32_t>(self[6]) << 7) + 1u);
}

// The cell in the map grid (0x44 bytes each); the id 0 lands one before the grid.
inline CMapCell* CellAt(uint16_t cell) {
    return reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorldMap) + static_cast<int64_t>(static_cast<int32_t>(cell) - 1) * 0x44);
}

inline bool IsStaffType(uint8_t type) {
    return type == 4 || type == 5 || type == 6 || type == 8 || type == 7;
}

inline uint16_t Step(uint16_t cell, const MapDelta& d) {
    return static_cast<uint16_t>(static_cast<uint32_t>(cell) + (static_cast<uint32_t>(d.mY) << 7) + static_cast<uint32_t>(d.mX));
}

}  // namespace

// 0x10113870: a random walk target. A stranded peep whose surroundings are unchanged only shows its bubble. Otherwise it follows up to 1-5
// cell steps along the paths (never straight back, skipping queue/exit/outside-patrol cells), then asks for a random spot in the last cell; a
// peep with no neighbours tries the eight rings around it, then five random cells within 5 of it.
int32_t CPeep::SetRandomDestination() {
    uint8_t* self = reinterpret_cast<uint8_t*>(this);
    CStaff* staffThis = reinterpret_cast<CStaff*>(this);
    uint32_t prev = 9999;
    uint16_t cur = PeepCell(self);
    int32_t outside = 0;

    if (IsStranded() && IsLocalAreaUnchangedSinceBecomingStranded()) {
        reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x11), false);
        return 0;
    }
    SetStranded(0);
    uint32_t rnd = gWorld->GetRand();
    const bool staff = IsStaffType(self[2]);
    int32_t counter = static_cast<int32_t>(rnd % 5u) + 1;
    CMapCell* here = CellAt(PeepCell(self));
    if (staff) {
        if (staffThis->InsidePatrolArea(Id(PeepCell(self))) == 0) {
            if (staffThis->FindDestInPatrolArea() != 0) return 1;
            outside = 1;
        }
    }

    if (here->NumberOfHVNeighbours() > 0) {
        while (true) {
            const int32_t old = counter;
            counter = old - 1;
            if (old == 0) break;

            CMapCell* cell = CellAt(cur);
            uint32_t nb = static_cast<uint32_t>(cell->GetNeighbours()) & 0x55u;
            int32_t n = cell->NumberOfHVNeighbours();
            uint16_t cand[4];
            cand[0] = (nb & 0x10) ? Step(cur, gMapDeltaPlusY) : 0;
            cand[1] = (nb & 0x04) ? Step(cur, gMapDeltaPlusX) : 0;
            cand[2] = (nb & 0x01) ? Step(cur, gMapDeltaMinusY) : 0;
            cand[3] = (nb & 0x40) ? Step(cur, gMapDeltaMinusX) : 0;

            if (staff && outside == 0) {
                for (int32_t i = 0; i < 4; ++i) {
                    if (cand[i] != gPeepNoCell[0] && staffThis->InsidePatrolArea(Id(cand[i])) == 0) {
                        --n;
                        cand[i] = 0;
                        if (n == 0) {
                            if (staffThis->FindDestInPatrolArea() != 0) return 1;
                            CTRACE(2, kTextInsidePatrol);
                            return 0;
                        }
                    }
                }
            }
            if (cell->IsPath()) {
                for (int32_t i = 0; i < 4; ++i) {
                    if (cand[i] != gPeepNoCell[1] && CellAt(cand[i])->IsQueue() != 0) {
                        --n;
                        cand[i] = 0;
                    }
                }
            }
            if (cell->IsQueue() != 0 && cell->IsEntrance() == 0) {
                if (cell->GetDirection() == 1) {
                    --n;
                    cand[2] = 0;
                } else if (cell->GetDirection() == 0x10) {
                    --n;
                    cand[0] = 0;
                } else if (cell->GetDirection() == 4) {
                    --n;
                    cand[1] = 0;
                } else if (cell->GetDirection() == 0x40) {
                    --n;
                    cand[3] = 0;
                }
            }
            for (int32_t i = 0; i < 4; ++i) {
                const uint16_t v = cand[i];
                CMapCell* exitCell = reinterpret_cast<CMapCell*>(reinterpret_cast<uint8_t*>(gWorld) + 0x2d8 +
                                                                 static_cast<int64_t>(static_cast<int32_t>(v) - 1) * 0x44);
                if (v != gPeepNoCell[2] && exitCell->IsExit() != 0) {
                    --n;
                    cand[i] = 0;
                }
            }

            uint32_t dir;
            if (n < 2) {
                if (cand[0] != gPeepNoCell[3]) {
                    dir = 0;
                } else if (cand[1] != gPeepNoCell[4]) {
                    dir = 1;
                } else if (cand[2] != gPeepNoCell[5]) {
                    dir = 2;
                } else if (cand[3] != gPeepNoCell[6]) {
                    dir = 3;
                } else {
                    CTRACE(2, kTextNowhere);
                    if (staff) {
                        if (staffThis->FindDestInPatrolArea() != 0) return 1;
                        CTRACE(2, kTextInsidePatrol);
                        return 0;
                    }
                    CTRACE(2, kTextNavigation);
                    reinterpret_cast<CThoughts*>(self + 0x30)->ShowThoughtBubble(static_cast<CThoughts::ThoughtType>(0x11), false);
                    SetStranded(1);
                    return 0;
                }
            } else {
                const uint32_t start = gWorld->GetRand() & 3u;
                uint32_t i = start;
                dir = start;
                while (i < start + 4 && (cand[dir] == gPeepNoCell[7] || dir == prev)) {
                    ++i;
                    dir = i & 3u;
                }
                if (i == start + 4) CTRACE(2, kTextNoDirection);
            }
            cur = cand[dir];
            GAME_ASSERTMSG(static_cast<int32_t>(gPeepNoCell[8] != cur), kTextProblem);
            prev = (dir + 2) & 3u;
            if (cur == PeepCell(self) && counter == 0) ++counter;
        }

        uint32_t ra = gWorld->GetRand() & 0x7fu;
        uint32_t rb = gWorld->GetRand() & 0x7fu;
        uint8_t a = static_cast<uint8_t>(ra);
        uint8_t b = static_cast<uint8_t>(rb);
        if (a < 5) a = 5; else if (a > 0x7b) a = 0x7b;
        if (b < 5) b = 5; else if (b > 0x7b) b = 0x7b;
        const uint32_t m = static_cast<uint32_t>(cur) - 1u;
        MapPos px;
        MapPos py;
        px.mCell = static_cast<uint8_t>(m & 0x7fu);
        px.mOffset = a;
        py.mCell = static_cast<uint8_t>(m >> 7);
        py.mOffset = b;
        int32_t result = SetDest(px, py);
        if (result != 0) *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
        return result;
    }

    // No neighbour: the eight rings (cell itself, then the compass directions) of up to 3 cells, then five random cells near the peep.
    const uint16_t start = PeepCell(self);
    for (int32_t dir = 0; dir < 8; ++dir) {
        for (int32_t j = 0; j < 4; ++j) {
            int32_t dx = 0;
            int32_t dy = 0;
            switch (dir) {
                case 0: break;
                case 1: dx = j; dy = j; break;
                case 2: dx = j; break;
                case 3: dx = j; dy = -j; break;
                case 4: dy = -j; break;
                case 5: dx = -j; dy = -j; break;
                case 6: dx = -j; break;
                case 7: dx = -j; dy = j; break;
                default: break;
            }
            const uint16_t c = static_cast<uint16_t>(static_cast<uint32_t>(start) + (static_cast<uint32_t>(dy) << 7) + static_cast<uint32_t>(dx));
            const uint32_t m = static_cast<uint32_t>(c) - 1u;
            if (CMap::IsXYValid(static_cast<int32_t>(m & 0x7fu), static_cast<int32_t>(m >> 7)) != 0 && CellAt(c)->IsPath() != 0 &&
                SetDest(Id(c)) != 0) {
                *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
                return 1;
            }
        }
    }
    uint32_t tries;
    for (tries = 0; tries < 5; ++tries) {
        const uint32_t r1 = gWorld->GetRand();
        const int32_t offX = static_cast<int32_t>(r1 % 11u) - 5;
        const uint32_t r2 = gWorld->GetRand();
        const int32_t offY = static_cast<int32_t>(r2 % 11u) - 5;
        const uint32_t m = static_cast<uint32_t>(PeepCell(self)) - 1u;
        const int32_t x = offX + static_cast<int32_t>(m & 0x7fu);
        const int32_t y = offY + static_cast<int32_t>(m >> 7);
        if (CMap::IsXYValid(x, y) != 0) {
            const uint16_t c = static_cast<uint16_t>((static_cast<uint32_t>(y) << 7) + 1u + static_cast<uint32_t>(x));
            if (SetDest(Id(c)) != 0) break;
        }
    }
    if (tries == 5) return 0;
    *reinterpret_cast<int32_t*>(self + 0xd0) = 1;
    return 1;
}
