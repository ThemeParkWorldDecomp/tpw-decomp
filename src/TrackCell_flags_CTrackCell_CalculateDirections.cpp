#include <cstdint>
#include "TrackCell.h"
#include "MapCell.h"
#include "World.h"
#include "LLCellSystem.h"
#include "ObjectParts.h"

// Tile tables of the track cell modes (initial data of the Mac's data section).
#define TRACK_TILES(K) \
    {(K) + 2, 0, 0x11, {}}, {(K) + 2, 0x5a, 0x44, {}}, {(K) + 1, 0, 0x10, {}}, {(K) + 1, 0x5a, 0x40, {}}, \
    {(K) + 1, 0xb4, 0x01, {}}, {(K) + 1, 0x10e, 0x04, {}}, {(K) + 3, 0, 0x14, {}}, {(K) + 3, 0x5a, 0x50, {}}, \
    {(K) + 3, 0xb4, 0x41, {}}, {(K) + 3, 0x10e, 0x05, {}}, {(K) + 5, 0, 0x15, {}}, {(K) + 6, 0, 0x45, {}}, \
    {(K) + 7, 0, 0x51, {}}, {(K) + 8, 0, 0x54, {}}, {(K) + 4, 0, 0x55, {}}, {(K), 0, 0x00, {}}
CMapCellTileEntry gTrackTileTableA[16] = {TRACK_TILES(0x30000)};   // 0x1029d6e8, count gTrackTileCountA
int32_t gTrackTileCountA = 0x10;               // 0x1029d7a8
CMapCellTileEntry gTrackTileTableB[16] = {TRACK_TILES(0x40000)};   // 0x1029d7ac, count gTrackTileCountB
int32_t gTrackTileCountB = 0x10;               // 0x1029d86c
int32_t gTrackTileLast = 0;                    // 0x102e484c  the last random tile offset
int32_t gTrackTileRand = 0;                    // 0x102e4850  the next random value (0 to 0x1d)

// 0x100a12dc, 392 bytes.
void CTrackCell::CalculateDirections(int32_t a, int32_t b) {
    bool skip = false;
    if (gLLCC4818 != 0) {
        int32_t t = mType;
        if (t == 0xb || t == 0xc || t == 0x10 || t == 0xc || t == 0x11) skip = true;
        else if (GetMapCellEquiv()->mType == 0x15) skip = true;
    }
    if (skip) return;
    if (NumberOfHVNeighbours() > 2) {
        uint8_t d = GetDirection();
        if (a == -2) ModifyDirection(d | 0x40);
        if (a == 2) ModifyDirection(d | 4);
        if (b == -2) ModifyDirection(d | 1);
        if (b == 2) ModifyDirection(d | 0x10);
    } else {
        if (a == -2) ModifyDirection(0x40);
        if (a == 2) ModifyDirection(4);
        if (b == -2) ModifyDirection(1);
        if (b == 2) ModifyDirection(0x10);
    }
}

namespace {
// Jump table 0x1029e2dc: 0 default (table B), 1 plain track table, 2 straight-track table (0x1029d6e8), 3 the other table (0x1029d7ac), 4 hoarding.
const uint8_t kKind[26] = {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 4};
}

// 0x100a2a0c, 1728 bytes.
uint32_t CTrackCell::GetTile(int32_t* rotation) const {
    uint8_t nb = GetNeighbours();
    int32_t mode = mType;
    uint8_t dir = GetDirection();
    GetMapCellEquiv();
    int32_t rot = 0;
    *rotation = 0;
    int32_t m = mode < 0 ? -mode : mode;
    if (m == 5 || m == 7 || m == 4 || m == 0xc || m == 0x11 || m == 9 || m == 10) return 8;

    const CMapCellTileEntry* table;
    int32_t count;
    int32_t tile = -1;
    int32_t x = 0, y = 0;
    int kind = (static_cast<uint32_t>(m) <= 0x19) ? kKind[m] : 0;
    switch (kind) {
        default:
            table = gTileTableB; count = gTileCountB;
            break;
        case 1:
            table = gTrackTileTableA; count = gTrackTileCountA;
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            break;
        case 3:
            table = gTrackTileTableB; count = gTrackTileCountB;
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            break;
        case 4: {
            table = gTrackTileTableB; count = gTrackTileCountB;
            uint32_t id = mCellId;
            x = static_cast<int32_t>((id - 1) & 0x7f);
            y = static_cast<int32_t>((id - 1) >> 7);
            if (IsStraight() && (dir == 0x40 || dir == 0x10)) rot = 0xb4;
            if ((x == gHoardingRight && y == gHoardingBottom) || (x == gHoardingLeft && y == gHoardingTop))
                nb = nb | OPPOSITE(nb);
            break;
        }
    }
    bool found = false;
    for (int32_t i = 0; i < count && !found; i++) {
        if (nb == table[i].mMask) {
            found = true;
            tile = static_cast<int32_t>(table[i].mTile);
            *rotation = rot + table[i].mRotation;
        }
    }
    if (tile == -1) {
        found = false;
        for (int32_t i = 0; i < count && !found; i++) {
            uint8_t mk = table[i].mMask;
            if (mk == (nb & mk)) {
                found = true;
                tile = static_cast<int32_t>(table[i].mTile);
                *rotation = rot + table[i].mRotation;
            }
        }
    }
    if (!IsCorner() && !IsCrossRoad()) {
        uint16_t f = mFlags;
        if (f & 1) tile += 9;
        if (f & 2) tile += 0xc;
        if (f & 8) tile += 0xf;
        if (f & 4) tile = static_cast<int32_t>((static_cast<uint32_t>(tile) & 0xffff0000u) + 0x12);
    }
    if ((mode == 0x10 || mode == 0xb) && IsCorner()) {
        uint8_t d = GetDirection();
        uint8_t n = GetNeighbours();
        if ((d == 0x40 && n == 0x50) || (d == 0x10 && n == 0x14) || (d == 1 && n == 0x41) || (d == 4 && n == 5))
            tile += 0x10;
    }
    if (mode == 0x10 && IsCrossRoad()) {
        uint8_t d = GetDirection();
        if (d == 0x14) *rotation = 0x5a;
        if (d == 0x41) tile += 0x10;
        if (d == 0x50) {
            tile += 0x10;
            *rotation = 0x10e;
        }
    }
    if (mode == 0x19) {
        if (IsCorner()) {
            uint8_t d = GetDirection();
            uint8_t n = GetNeighbours();
            if ((d == 1 && n == 0x41) || (d == 0x10 && n == 0x14) || (d == 4 && n == 5) || (d == 0x40 && n == 0x50) ||
                (d == 0x40 && n == 0x14) || (d == 1 && n == 0x50) || (d == 4 && n == 0x41) || (d == 0x10 && n == 5)) {
                tile += 0x10;
                if (IsBridge() != 0) tile = 0x1b;
            } else if (IsBridge() != 0) {
                tile = 0x1a;
            }
        }
        if (IsStraight() && IsBridge() == 0) {
            // r20 is the caller's register when the counter is 0x1e or more (never: it is a remainder of 0x1e).
            int32_t r20 = 0;
            int32_t c = gTrackTileRand;
            if (c < 0x12) r20 = 0;
            else if (c < 0x18) r20 = 0x14;
            else if (c < 0x1e) r20 = 0x17;
            if (gTrackTileLast != r20) tile += r20;
            gTrackTileLast = r20;
            gTrackTileRand = static_cast<int32_t>(gGameWorld->GetRand() % 0x1e);
        }
        tile = static_cast<int32_t>((static_cast<uint32_t>(tile) & 0xffff) | 0x50000);
    }
    return static_cast<uint32_t>(tile);
}
