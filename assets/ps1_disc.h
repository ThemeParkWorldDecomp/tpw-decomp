// The PlayStation import's content checks and FOLIO directory reader. A supported disc is identified by its own
// ISO directory, executable, symbolic string IDs and theme tables, with no edition-specific entry numbers.
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "source.h"

namespace tpw::assets {

enum class Ps1Status { NotPlayStation, Rejected, Supported };

struct Ps1RideEntry {
    size_t entry = 0;
    size_t themeAtlas = 0;
    size_t rideAtlas = 0;
};

struct Ps1Disc {
    std::string region;
    std::string boot;
    std::vector<std::vector<uint8_t>> entries;
    // The disc's own symbolic keys, for example HALLOW/JACK and JUNGLE/BUNGIE.
    std::map<std::string, Ps1RideEntry> rides;
    std::vector<std::array<size_t, 3>> themes;
};

// Reads the directory and copies its entries only when every offset and size is valid. Errors leave entries empty.
bool ReadPs1Folio(const std::vector<uint8_t>& bytes, std::vector<std::vector<uint8_t>>& entries, std::string& reason);

// NotPlayStation leaves the ordinary PC importer in charge. Rejected gives one line explaining why a PlayStation
// disc must not be converted. Raw images retain their sync, address and EDC checks; cooked images have none to check.
Ps1Status RecognisePs1(std::shared_ptr<Blob> blob, Ps1Disc& disc, std::string& reason);

}  // namespace tpw::assets
