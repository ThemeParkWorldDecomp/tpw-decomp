// tpi_names.h: the names Theme Park Inc shows for its rides, shops, sideshows, features and upgrades. Each item's own
// .sam carries an Info.Name, but that is the developers' working name, often in small letters or with an acronym half
// capitalised; the game shows the names of its object names string bank (Data/Language/<language>/OBJECT_NAMES.str, a
// BFST bank of 556 strings, its id 0x3fe) instead, two strings an item joined the way Theme Park World's own name table
// joins them (RideDB.h gRideNameTable, CRideNameDB::GetName). Which two strings name which item is in Theme Park Inc's
// program only; the converter (tpi_convert.h) keeps that binding as string numbers (tpi_names.cpp kTpiNameRows: world,
// Info.Id, first and second string) and reads the strings themselves from the player's own copy at convert time, so no
// name is held here. An item without a row, or a bank that is missing or not of that shape, gets its Info.Name in title
// case instead (TpiTitleCase), except the fixed items, which get the port's own names (TpiFixtureName).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

// The object names bank the binding is for.
constexpr uint32_t kTpiObjectNamesId = 0x3fe;
constexpr size_t kTpiObjectNamesCount = 556;

// A decoded BFST string bank: its id and its strings, each as ASCII text. A string with a character above 0x7f, a
// variable (BFST op 2), a byte the character map has no entry for, or an unknown op is unusable: its flag is false and
// its text empty. The game's parser of the item files takes Info.Name byte by byte (TpiWorlds.h TpiCopyAscii), so only
// ASCII names are written.
struct TpiStrings {
    uint32_t id = 0;
    std::vector<std::string> text;
    std::vector<bool> usable;
};

// Decodes a BFST bank with its BFMU character map (MBToUni.dat: a byte b of a string is the map's character b - 1).
// False with a message when either file is not of that form.
bool DecodeTpiStrings(const std::vector<uint8_t>& bank, const std::vector<uint8_t>& charMap, TpiStrings& out,
                      std::string& err);

// True when `strings` is Theme Park Inc's object names bank as the binding knows it (id and count); else `why` says
// what differs.
bool IsTpiObjectNames(const TpiStrings& strings, std::string& why);

// The binding: the first and second string of a world's item (the Theme Park Inc folder under Data/levels and the
// item's own Info.Id); second is 0 for a name of one string. False when the item has no row.
bool TpiNameRows(const std::string& world, int32_t id, uint32_t& first, uint32_t& second);

// Two strings joined as CRideNameDB::GetName joins them: with a blank between, none when the second is empty, and
// without the blank (the hyphen cut off) when the first ends with a hyphen.
std::string JoinTpiName(const std::string& first, const std::string& second);

// Title case, for an item the bank does not name: each word (between blanks) starts with a capital; a word of two or
// more capitals and no small letter (an acronym: "TV") stays as it is; a word with a capital second letter followed by
// small ones ("SLide", a shift key held too long) gets that second letter small; nothing else changes, so "Sam's"
// and "Pick-a-card" keep their small letters after the apostrophe and the hyphens.
std::string TpiTitleCase(const std::string& name);

// The port's own name for a fixed item (Info.WhichUIType 4: the helipad, the ticket booth, the bunting...), by its item
// folder in any case, or null. The bank has no string for them and most of their .sam files say "heli", so without
// this they would all show as "Heli" when the pointer is over them. The names describe the item; they are not from
// either game.
const char* TpiFixtureName(const std::string& folder);

// The name an item is converted with. `bank` (the decoded object names bank, or null) names the item when the binding
// has its row and both strings are usable and the first is not empty; otherwise, for an item without a row, its
// fixture name (TpiFixtureName, from `folder`); otherwise `infoName` in title case. `fromBank` says whether the bank
// named it.
std::string TpiShownName(const TpiStrings* bank, const std::string& world, int32_t id, const std::string& folder,
                         const std::string& infoName, bool& fromBank);

}  // namespace tpw::assets
