// gold_ride_table.h: where each of the 35 Gold rides goes in the game folder.
//
// Approved exception (the maintainer, 2026-11-07), the only game data in this file: the 35 wad-to-folder rows of the Gold installer's
// RideConfig.txt, nothing else of it. Some downloads of the Gold rides hold the 35 .wad files but not RideConfig.txt (the
// list exists only inside the installer's allitems.exe), so the Gold Edition does not need that file: platform/gold_rides.cpp
// places the rides by this table when the file is absent, and by the file when it is there. The rows are in the file's
// order (by theme, then folder, then name), the folder in the form platform/gold_rides.h gives (lower case, '/'). The disc
// test in assets/tests/host_importer.cpp (TestGold) checks them against the real RideConfig.txt row for row; the host test in
// tests/host_manifest.cpp checks that every Gold .wad of the manifest has exactly one row.
#pragma once

#include <cstddef>

namespace tpw::assets {

struct GoldRideRow {
    const char* wad;      // the .wad's name, lower case
    const char* folder;   // the game folder it belongs in
};

inline constexpr GoldRideRow kGoldRideTable[] = {
    {"_cards_19.wad", "data/levels/fantasy/features"},
    {"_coin_20.wad", "data/levels/fantasy/features"},
    {"_crayon_21.wad", "data/levels/fantasy/features"},
    {"_sweet_22.wad", "data/levels/fantasy/features"},
    {"_acorn_23.wad", "data/levels/fantasy/rides"},
    {"_bumpers_25.wad", "data/levels/fantasy/rides"},
    {"_flyfoun_39.wad", "data/levels/fantasy/rides"},
    {"_puzzle_24.wad", "data/levels/fantasy/sideshow"},
    {"_finger_15.wad", "data/levels/hallow/features"},
    {"_monhand_16.wad", "data/levels/hallow/features"},
    {"_tentacl_14.wad", "data/levels/hallow/features"},
    {"_trident_17.wad", "data/levels/hallow/features"},
    {"_bellt_10.wad", "data/levels/hallow/rides"},
    {"_devil_18.wad", "data/levels/hallow/rides"},
    {"_hhouse_11.wad", "data/levels/hallow/rides"},
    {"_shakey_12.wad", "data/levels/hallow/rides"},
    {"_shooter_13.wad", "data/levels/hallow/sideshow"},
    {"_bushiii_2.wad", "data/levels/jungle/features"},
    {"_bushiv_3.wad", "data/levels/jungle/features"},
    {"_gidol_5.wad", "data/levels/jungle/features"},
    {"_rattle_6.wad", "data/levels/jungle/features"},
    {"_dizzyd_8.wad", "data/levels/jungle/rides"},
    {"_king_7.wad", "data/levels/jungle/rides"},
    {"_manic_9.wad", "data/levels/jungle/rides"},
    {"_snake_1.wad", "data/levels/jungle/rides"},
    {"_morpher_27.wad", "data/levels/space/features"},
    {"_obelisk_28.wad", "data/levels/space/features"},
    {"_pulsar_29.wad", "data/levels/space/features"},
    {"_robosta_30.wad", "data/levels/space/features"},
    {"_tower_31.wad", "data/levels/space/features"},
    {"_gforce_35.wad", "data/levels/space/rides"},
    {"_whirli_36.wad", "data/levels/space/rides"},
    {"_wormhol_33.wad", "data/levels/space/rides"},
    {"_rshoot_37.wad", "data/levels/space/sideshow"},
    {"_strengt_34.wad", "data/levels/space/sideshow"},
};
inline constexpr size_t kGoldRideTableSize = sizeof(kGoldRideTable) / sizeof(kGoldRideTable[0]);

}  // namespace tpw::assets
