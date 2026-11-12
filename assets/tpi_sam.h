// tpi_sam.h: Theme Park Inc's .sam settings files for Theme Park World's parser. The two games share the balance file
// format (sam_check.h) but not the tables: a Theme Park Inc file holds statements Theme Park World's table does not
// know, and the game's parser stops at the first of them, so everything after it would keep its zero default. The
// converter (tpi_convert.h) keeps a statement when the table it will be read with has its group and field, drops the
// rest and reports them, renames the few that Theme Park World spells differently, and remaps the item ids.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "sam_check.h"

namespace tpw::assets {

struct TpiSamOptions {
    // Statements spelled differently in Theme Park World: "Group.Field" or "Group[].Field" (any index) to the
    // "Group.Field" written in its place, before the table decides. The array index is dropped.
    std::vector<std::pair<std::string, std::string>> rename;
    // Statements dropped whatever the table says ("Group.Field", or "Group.*" for a whole group).
    std::vector<std::string> drop;
    // Integer statements whose value is an item id ("Group.Field"): the value is replaced through `ids`. An id that is
    // not in the map is left as it is and reported.
    std::vector<std::string> idFields;
    const std::map<int32_t, int32_t>* ids = nullptr;
    // Statements appended at the end ("Group.Field value"), after dropping any statement they replace.
    std::vector<std::string> append;
};

struct TpiSamReport {
    int kept = 0;
    std::vector<std::string> dropped;      // "Group.Field" once each, in file order
    std::vector<std::string> unmappedIds;  // "Group.Field old" for ids not in the map
};

// Rewrites `text` for `table` (ending with its type 12 row; empty is an error: the converter never writes a file it
// cannot check). Comments and blank lines stay. False only for a table-less call.
bool FilterTpiSam(const std::vector<SamRow>& table, const std::string& text, const TpiSamOptions& options,
                  std::string& out, TpiSamReport& report, std::string& err);

// The first integer value of the first "Group.Field" statement in `text`; false when there is none.
bool TpiSamInteger(const std::string& text, const std::string& group, const std::string& field, int32_t& value);
// The first string value (between quotes) of the first "Group.Field" statement; false when there is none.
bool TpiSamString(const std::string& text, const std::string& group, const std::string& field, std::string& value);

}  // namespace tpw::assets
