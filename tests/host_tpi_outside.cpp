// host_tpi_outside.cpp: the TPI Worlds and Rides mod's outside features (include/TpiOutside.h, TPW_MOD_HOOK tpi_worlds)
// on the host, without game data: TpiOutside.sam's lines read back and the ones turned down, and which of a world's
// outside features a loaded park lacks: all of them in a park saved before converter 28 (none placed), none in a park
// that has them, and only the missing ones otherwise, so a second load places nothing. The ids are made up here. Exits
// nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiOutside.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

static bool Parse(const std::string& text, tpw::TpiOutsideTable& table) {
    return tpw::TpiParseOutside(text.data(), text.size(), table);
}

static std::vector<uint16_t> ToPlace(const tpw::TpiOutsideTable& table, const std::vector<uint16_t>& placed) {
    uint16_t out[tpw::kTpiMaxOutside] = {};
    const int32_t n = tpw::TpiOutsideToPlace(table, placed.data(), static_cast<int32_t>(placed.size()), out);
    return std::vector<uint16_t>(out, out + n);
}

int main() {
    using namespace tpw;

    // Reading: the converter's file (a tab after the word, its folder as a comment), in the file's order.
    TpiOutsideTable table;
    const std::string file =
        "# TPI Worlds and Rides: Theme Park Inc's outside features, placed when a park starts\n"
        "Outside\t5390\t\t# features/bunting\n"
        "Outside\t5440\t\t# features/helipad\n"
        "Outside 5450\r\n"
        "  Outside\t5650   # features/water\n";
    Check(Parse(file, table), "the converter's file: read");
    Check(table.count == 4 && table.ids[0] == 5390 && table.ids[1] == 5440 && table.ids[2] == 5450 && table.ids[3] == 5650,
          "the converter's file: four ids in order");
    // No last newline.
    Check(Parse("Outside 7640", table) && table.count == 1 && table.ids[0] == 7640, "no last newline: read");

    // Lines turned down: other words, no id, bad ids, ids out of range, more after the id, a repeat.
    const std::string bad =
        "Outsides 6000\n"
        "Outside\n"
        "Outside \n"
        "Outside x12\n"
        "Outside 12x\n"
        "Outside 0\n"
        "Outside -5\n"
        "Outside 65535\n"
        "Outside 6390 6450\n"
        "outside 6390\n"
        "# Outside 6390\n"
        "Outside 6650\n"
        "Outside 6650\n"
        "Outside 65534\n"
        "Outside 1\n";
    Check(Parse(bad, table), "bad lines among good ones: the good ones read");
    Check(table.count == 3 && table.ids[0] == 6650 && table.ids[1] == 65534 && table.ids[2] == 1,
          "only 6650 (once), 65534 and 1 kept");
    Check(!Parse("", table) && table.count == 0, "an empty file: nothing");
    Check(!Parse("# only a comment\n\n", table) && table.count == 0, "only a comment: nothing");
    Check(!tpw::TpiParseOutside(nullptr, 0, table) && table.count == 0, "no text: nothing");
    // A table is cleared before reading.
    Parse("Outside 9\n", table);
    Check(Parse("Outside 10\n", table) && table.count == 1 && table.ids[0] == 10, "read again: the old ids gone");
    // At most kTpiMaxOutside ids.
    std::string many;
    for (int i = 1; i <= kTpiMaxOutside + 4; ++i) many += "Outside " + std::to_string(100 + i) + "\n";
    Check(Parse(many, table) && table.count == kTpiMaxOutside && table.ids[kTpiMaxOutside - 1] == 100 + kTpiMaxOutside,
          "more than kTpiMaxOutside: the first ones kept");

    // Which a loaded park lacks. Arabian Nights' six features.
    Parse("Outside 5390\nOutside 5440\nOutside 5450\nOutside 5460\nOutside 5640\nOutside 5650\n", table);
    // A park saved before converter 28: its objects are rides and the gates, no outside feature.
    const std::vector<uint16_t> before = {5060, 5010, 5040, 5010, 1600};
    Check(ToPlace(table, before) == std::vector<uint16_t>({5390, 5440, 5450, 5460, 5640, 5650}),
          "a park saved before converter 28: all six placed");
    Check(ToPlace(table, {}) == std::vector<uint16_t>({5390, 5440, 5450, 5460, 5640, 5650}), "a park with no objects: all six");
    // A park started with converter 28, or loaded once since: it has them all, nothing is placed again.
    const std::vector<uint16_t> after = {5060, 5390, 5440, 5450, 5460, 5640, 5650, 5010};
    Check(ToPlace(table, after).empty(), "a park that has them all: none placed (a second load changes nothing)");
    // Some missing: only those, in the table's order.
    Check(ToPlace(table, {5650, 5390, 5060}) == std::vector<uint16_t>({5440, 5450, 5460, 5640}), "two of six there: the other four");
    // A world with no TpiOutside.sam (an original world, the mod off): nothing to place.
    Check(ToPlace(TpiOutsideTable{}, before).empty(), "no outside features: none placed");

    if (gFailures == 0) std::printf("tpi outside: all checks passed\n");
    return gFailures;
}
