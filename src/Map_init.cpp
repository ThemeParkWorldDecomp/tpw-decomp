// CMap::Init (main app, SimTheme Park 2.04b3, Map.cpp; unit w139), 0x10100a8c, 980 bytes. `this` is the map, CWorld + 0x2d8.
//
// It clears the map cells (0x110000 bytes), the track cells (0xa0000 bytes at +0x110000) and the 0x1104 bytes after them (at +0x1b0000),
// then numbers every cell of the 128 x 128 level: id = x + (y << 7) + 1, kind 1 for the map cell and kind 2 for the track cell with that
// index (the id less one). Then it fills the eight region effect records (0xc bytes each, at +0x1d9104 = CWorld + 0x1d93dc) from the
// balance table: the radius byte first (low byte of the word at Mac offset 0x5d0 + 0x18 i), then the five effect halfwords (the next five
// words, each cut to 16 bits) copied as ten bytes. The pad byte of each record (+0xb) is not written. The Mac also copies an 80 byte zero
// image from 0x102a0aa0 to the stack first; that image is overwritten before use (dead), and is not reproduced.
#include "World.h"

#include <cstdint>
#include <cstring>

#include "CellBase.h"
#include "MainBalance.h"

namespace {

constexpr uintptr_t kTrackCellsOffset = 0x110000;   // CWorld + 0x1102d8 less the map's 0x2d8
constexpr uintptr_t kRunOffset = 0x1b0000;
constexpr uintptr_t kRecordsOffset = 0x1d9104;
constexpr int kBalanceFirstSlot = 370;               // slot = (Mac offset - 8) / 4; 0x5d0 is slot 370

}  // namespace

void CMap::Init() {
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    memset(base, 0, 0x110000);
    memset(base + kTrackCellsOffset, 0, 0xa0000);
    memset(base + kRunOffset, 0, 0x1104);

    for (int32_t x = 0; x < 128; ++x) {
        for (int32_t y = 0; y < 128; ++y) {
            const uint16_t id = static_cast<uint16_t>(x + (y << 7) + 1);
            CCellBase* const mapCell = reinterpret_cast<CCellBase*>(base + 0x44 * static_cast<uint32_t>(id - 1));
            mapCell->mCellId = id;
            mapCell->mKind = 1;
            CCellBase* const trackCell = reinterpret_cast<CCellBase*>(base + kTrackCellsOffset + 0x28 * static_cast<uint32_t>(id - 1));
            trackCell->mCellId = id;
            trackCell->mKind = 2;
        }
    }

    RegionEffectInfo* const records = reinterpret_cast<RegionEffectInfo*>(base + kRecordsOffset);
    for (int32_t i = 0; i < 8; ++i) {
        records[i].mRadius = static_cast<uint8_t>(gMainBalance.mValues[kBalanceFirstSlot + 6 * i].mInteger);
    }
    for (int32_t i = 0; i < 8; ++i) {
        uint16_t image[5];
        for (int32_t k = 0; k < 5; ++k) {
            image[k] = static_cast<uint16_t>(gMainBalance.mValues[kBalanceFirstSlot + 1 + 6 * i + k].mInteger);
        }
        memcpy(&records[i], image, 10);
    }
}
