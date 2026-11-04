// sam_check.h: will the game read a .sam settings file to its end? The launcher keeps a .sam file the player edited
// (tpw-port #94) and the game reads it as it is, any value its parser accepts, as the original did. This check runs the
// game's own balance parser rules (CBalanceTable::Parse, src/game/balance/BalanceLoader.cpp) over the text without
// storing anything and gives the first line the parser stops at: the game reads nothing after it, so the later
// settings keep their zero defaults. It never limits a value the parser takes: a bounded field (description type 6
// or 9) fails only outside the bounds the game's own table gives it, as in the game.
//
// The parser needs the file's description table (which groups and fields exist, their types and bounds). The tables
// are the game's (gMainBalanceDescriptions and the others); the launcher registers them by file name at startup
// (SetSamTable), so this library needs nothing of the game's. A file with no table registered (any name in a test) gets
// the parts of the check that hold for every table: each line must be "Group.Field..." or "Group[n].Field..." as the
// parser reads it; field names and values are not checked.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

// One row of a description table, laid out as the game's BalanceDescription (CBalanceTable.h): type 0 blank, 1 group
// heading (after its fields), 2 array start, 3 array end (name: the group, arrayCount: its capacity), 4 integer, 5
// integer not negative, 6 integer in [min, max), 7 float, 8 float not negative, 9 float in [min, max), 10 string, 11
// map (flags 1 or 2: its symbol set), 12 the end of the table.
struct SamRow {
    int32_t type;
    char name[32];
    int32_t minInteger;
    int32_t maxInteger;
    float minFloat;
    float maxFloat;
    int32_t arrayCount;
    uint32_t flags;
};
static_assert(sizeof(SamRow) == 0x3c, "a SamRow is a BalanceDescription");

// Registers the table the game reads the .sam file `fileName` with (its base name, any case). `rows` ends with its type
// 12 row. A later call for the same name replaces it. Thread-safe.
void SetSamTable(const std::string& fileName, const SamRow* rows, size_t count);
// The table registered for the base name of `path` (any case), empty if none.
std::vector<SamRow> SamTableFor(const std::string& path);

// The 1-based line of `text` (a .sam file's bytes) the game's parser stops at, 0 when it reads the whole file. `rows`
// is the file's table (ending with type 12); empty: the check that holds for every table (see above). Line numbers
// count '\n' as the file has them (the parser takes out every '\r' first).
int SamUnreadableLine(const std::vector<SamRow>& rows, const std::string& text);
// The same with the table registered for `path`'s base name.
int SamUnreadableLine(const std::string& path, const std::string& text);

}  // namespace tpw::assets
