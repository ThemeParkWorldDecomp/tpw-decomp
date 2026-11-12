// tpi_names.cpp: Theme Park Inc's shown item names (tpi_names.h).
#include "tpi_names.h"

#include <cstring>

namespace tpw::assets {

namespace {

uint32_t Get32(const std::vector<uint8_t>& d, size_t at) {
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<uint32_t>(d[at + 3]) << 24);
}

uint16_t Get16(const std::vector<uint8_t>& d, size_t at) { return static_cast<uint16_t>(d[at] | (d[at + 1] << 8)); }

// One row of the binding: the Theme Park Inc world folder, the item's own Info.Id, its two strings in the object names
// bank (second 0: a name of one string). The rows follow the bank's own order, which is the order of Theme Park World's
// name table (RideDB.h gRideNameTable) with Theme Park Inc's items put in: the bank's previous edition beside it
// (OLD_OBJECT_NAMES.str, the same rows) still holds each item's working name, which is how each row was matched to an
// Info.Id. The information kiosks, the research labs, the training rooms, the science world's staff room and the
// obstacles are not converted (tpi_convert.cpp), so they have no row; nor have the fixed items the build menu does not
// show (Info.WhichUIType 4: the bus, the monorail, the helicopter and the like) or the entrance gate, which the
// converter names after its world. The photo kiosk upgrade has one string for all three worlds.
struct NameRow {
    const char* world;
    int32_t id;
    uint16_t first;
    uint16_t second;
};

const NameRow kTpiNameRows[] = {
    // Land of Invention
    {"science", 1500, 2, 0},     {"science", 1150, 4, 5},     {"science", 1300, 6, 0},     {"science", 1190, 8, 9},
    {"science", 1180, 10, 11},   {"science", 1170, 12, 13},   {"science", 1108, 14, 0},    {"science", 1106, 16, 0},
    {"science", 1185, 18, 19},   {"science", 1100, 20, 21},   {"science", 1101, 22, 23},   {"science", 1104, 24, 25},
    {"science", 1103, 26, 27},   {"science", 1105, 28, 0},    {"science", 1102, 30, 0},    {"science", 1109, 32, 33},
    {"science", 1110, 34, 35},   {"science", 1112, 36, 0},    {"science", 1160, 38, 39},   {"science", 1140, 40, 41},
    {"science", 1505, 48, 49},   {"science", 1201, 50, 51},   {"science", 1501, 54, 55},   {"science", 1502, 58, 59},
    {"science", 1207, 60, 61},   {"science", 1206, 64, 65},   {"science", 1203, 66, 67},   {"science", 1202, 68, 69},
    {"science", 1209, 70, 71},   {"science", 1211, 72, 73},   {"science", 1208, 74, 0},    {"science", 1303, 76, 77},
    {"science", 1304, 78, 0},    {"science", 1305, 80, 81},   {"science", 1309, 82, 83},   {"science", 1307, 84, 85},
    {"science", 1401, 86, 0},    {"science", 1418, 87, 0},    {"science", 1425, 88, 89},   {"science", 1419, 93, 0},
    {"science", 1406, 94, 95},   {"science", 1417, 96, 0},    {"science", 3417, 98, 0},    {"science", 1439, 100, 0},
    {"science", 1411, 102, 0},   {"science", 1403, 103, 0},   {"science", 1421, 105, 106}, {"science", 1405, 107, 108},
    {"science", 1407, 109, 0},   {"science", 1414, 110, 0},   {"science", 1413, 111, 0},   {"science", 1420, 113, 0},
    {"science", 1424, 116, 117}, {"science", 1423, 118, 0},   {"science", 1422, 120, 121}, {"science", 4150, 307, 0},
    {"science", 4109, 317, 0},   {"science", 4500, 347, 0},   {"science", 4208, 350, 0},   {"science", 4422, 367, 0},
    {"science", 4427, 392, 0},   {"science", 4428, 393, 0},   {"science", 4429, 394, 0},
    // Polar Zone
    {"water", 2150, 158, 159},   {"water", 2180, 160, 161},   {"water", 2190, 162, 0},     {"water", 2170, 164, 165},
    {"water", 2102, 166, 167},   {"water", 2111, 168, 169},   {"water", 2160, 170, 0},     {"water", 2110, 174, 175},
    {"water", 2117, 176, 0},     {"water", 2140, 178, 179},   {"water", 2100, 180, 181},   {"water", 2113, 182, 183},
    {"water", 2103, 184, 185},   {"water", 2112, 186, 187},   {"water", 2116, 190, 191},   {"water", 2118, 192, 0},
    {"water", 2500, 2, 0},       {"water", 2503, 198, 0},     {"water", 2501, 199, 0},     {"water", 2502, 202, 0},
    {"water", 2203, 204, 205},   {"water", 2204, 206, 207},   {"water", 2202, 208, 0},     {"water", 2201, 209, 210},
    {"water", 2200, 211, 0},     {"water", 2206, 214, 0},     {"water", 2207, 215, 216},   {"water", 2304, 217, 0},
    {"water", 2305, 218, 219},   {"water", 2303, 220, 0},     {"water", 2301, 223, 0},     {"water", 2302, 225, 0},
    {"water", 2300, 227, 228},   {"water", 2412, 229, 0},     {"water", 2413, 230, 231},   {"water", 2414, 232, 0},
    {"water", 2410, 234, 235},   {"water", 2404, 237, 238},   {"water", 2406, 239, 240},   {"water", 2411, 241, 0},
    {"water", 2415, 242, 243},   {"water", 2409, 244, 0},     {"water", 2408, 246, 0},     {"water", 2400, 248, 0},
    {"water", 2401, 249, 0},     {"water", 2402, 250, 0},     {"water", 2407, 251, 0},     {"water", 2403, 253, 0},
    {"water", 2416, 254, 0},     {"water", 2417, 256, 0},     {"water", 2418, 258, 0},     {"water", 2420, 261, 0},
    {"water", 2421, 262, 0},     {"water", 2424, 265, 266},   {"water", 2426, 268, 0},     {"water", 2428, 272, 273},
    {"water", 2429, 274, 275},   {"water", 2123, 277, 278},   {"water", 2194, 279, 280},   {"water", 2195, 281, 282},
    {"water", 2499, 287, 288},   {"water", 4185, 309, 0},     {"water", 4160, 327, 328},   {"water", 4201, 353, 0},
    // Arabian Nights
    {"arabian", 1402, 91, 92},   {"arabian", 1433, 132, 133}, {"arabian", 1434, 134, 135}, {"arabian", 1435, 136, 137},
    {"arabian", 4190, 311, 312}, {"arabian", 4170, 313, 314}, {"arabian", 4110, 325, 0},   {"arabian", 4300, 362, 363},
    {"arabian", 3150, 401, 402}, {"arabian", 3185, 403, 404}, {"arabian", 3180, 405, 406}, {"arabian", 3170, 407, 408},
    {"arabian", 3140, 409, 410}, {"arabian", 3101, 411, 412}, {"arabian", 3107, 413, 414}, {"arabian", 3100, 415, 416},
    {"arabian", 3102, 417, 418}, {"arabian", 3106, 419, 420}, {"arabian", 3160, 421, 422}, {"arabian", 3104, 423, 424},
    {"arabian", 3105, 425, 0},   {"arabian", 3109, 427, 428}, {"arabian", 3111, 429, 430}, {"arabian", 3108, 431, 0},
    {"arabian", 3114, 435, 436}, {"arabian", 3190, 441, 442}, {"arabian", 3305, 445, 446}, {"arabian", 3501, 449, 450},
    {"arabian", 3502, 451, 0},   {"arabian", 3500, 2, 0},     {"arabian", 3206, 452, 453}, {"arabian", 3200, 454, 0},
    {"arabian", 3205, 455, 456}, {"arabian", 3207, 457, 458}, {"arabian", 3201, 459, 0},   {"arabian", 3202, 461, 462},
    {"arabian", 3203, 463, 464}, {"arabian", 3300, 466, 0},   {"arabian", 3301, 468, 469}, {"arabian", 3302, 470, 0},
    {"arabian", 3303, 471, 0},   {"arabian", 3304, 472, 0},   {"arabian", 3409, 473, 0},   {"arabian", 3408, 475, 0},
    {"arabian", 3404, 477, 0},   {"arabian", 3412, 478, 0},   {"arabian", 3402, 480, 0},   {"arabian", 3403, 481, 0},
    {"arabian", 3415, 482, 483}, {"arabian", 3416, 484, 0},   {"arabian", 3405, 488, 0},   {"arabian", 3400, 489, 0},
    {"arabian", 3410, 491, 0},   {"arabian", 3411, 493, 0},   {"arabian", 3417, 495, 0},   {"arabian", 3418, 497, 0},
    {"arabian", 3413, 498, 0},   {"arabian", 3401, 500, 0},   {"arabian", 3406, 502, 0},   {"arabian", 3407, 504, 0},
    {"arabian", 3421, 507, 0},   {"arabian", 3423, 511, 0},   {"arabian", 3424, 513, 514}, {"arabian", 3503, 523, 0},
};

bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

// One word of TpiTitleCase.
std::string TitleWord(std::string word) {
    bool lower = false;
    size_t upper = 0;
    for (char c : word) {
        lower = lower || IsLower(c);
        upper += IsUpper(c);
    }
    if (!lower && upper >= 2)
        return word;
    if (IsLower(word[0]))
        word[0] = static_cast<char>(word[0] - 'a' + 'A');
    if (word.size() >= 3 && IsUpper(word[0]) && IsUpper(word[1])) {
        bool rest = true;
        for (size_t i = 2; i < word.size(); ++i)
            rest = rest && IsLower(word[i]);
        if (rest)
            word[1] = static_cast<char>(word[1] - 'A' + 'a');
    }
    return word;
}

}  // namespace

bool DecodeTpiStrings(const std::vector<uint8_t>& bank, const std::vector<uint8_t>& charMap, TpiStrings& out,
                      std::string& err) {
    out = TpiStrings{};
    if (charMap.size() < 8 || std::memcmp(charMap.data(), "BFMU", 4) != 0) {
        err = "the character map is not a BFMU file";
        return false;
    }
    const size_t characters = Get16(charMap, 6);
    if (8 + 2 * characters > charMap.size()) {
        err = "the character map is cut short";
        return false;
    }
    if (bank.size() < 12 || std::memcmp(bank.data(), "BFST", 4) != 0) {
        err = "the string bank is not a BFST file";
        return false;
    }
    out.id = Get32(bank, 4);
    const uint32_t count = Get32(bank, 8);
    if (count > (bank.size() - 12) / 4) {
        err = "the string bank's offsets run past its end";
        return false;
    }
    out.text.resize(count);
    out.usable.assign(count, false);
    for (uint32_t i = 0; i < count; ++i) {
        size_t p = 12 + static_cast<size_t>(Get32(bank, 12 + 4 * static_cast<size_t>(i)));
        std::string text;
        bool usable = true, ended = false;
        while (usable && p + 4 <= bank.size()) {
            const uint32_t word = Get32(bank, p);
            const uint32_t op = word & 0xff, arg = word >> 8;
            p += 4;
            if (op == 0) {
                ended = true;
                break;
            }
            if (op == 1) {
                if (arg > bank.size() - p) {
                    usable = false;
                    break;
                }
                for (uint32_t k = 0; k < arg; ++k) {
                    const uint8_t b = bank[p + k];
                    const uint16_t c = b >= 1 && b <= characters ? Get16(charMap, 8 + 2 * static_cast<size_t>(b - 1)) : 0;
                    if (c == 0 || c > 0x7f)
                        usable = false;
                    else
                        text.push_back(static_cast<char>(c));
                }
                p += (static_cast<size_t>(arg) + 3) & ~static_cast<size_t>(3);
            } else if (op == 3) {
                p += 4 * static_cast<size_t>(arg);
            } else if (op == 4 || op == 5 || op == 6) {
                // a number's format: no text of its own
            } else {
                // a variable (2) or an op the game's reader stops at
                usable = false;
            }
        }
        if (usable && ended) {
            out.text[i] = text;
            out.usable[i] = true;
        }
    }
    return true;
}

bool IsTpiObjectNames(const TpiStrings& strings, std::string& why) {
    if (strings.id != kTpiObjectNamesId) {
        why = "its id is " + std::to_string(strings.id) + ", not " + std::to_string(kTpiObjectNamesId);
        return false;
    }
    if (strings.text.size() != kTpiObjectNamesCount) {
        why = "it has " + std::to_string(strings.text.size()) + " strings, not " + std::to_string(kTpiObjectNamesCount);
        return false;
    }
    return true;
}

bool TpiNameRows(const std::string& world, int32_t id, uint32_t& first, uint32_t& second) {
    for (const NameRow& row : kTpiNameRows) {
        if (row.id == id && world == row.world) {
            first = row.first;
            second = row.second;
            return true;
        }
    }
    return false;
}

std::string JoinTpiName(const std::string& first, const std::string& second) {
    if (!first.empty() && first.back() == '-')
        return first.substr(0, first.size() - 1) + second;
    return second.empty() ? first : first + " " + second;
}

std::string TpiTitleCase(const std::string& name) {
    std::string out;
    size_t i = 0;
    while (i < name.size()) {
        if (name[i] == ' ') {
            out.push_back(' ');
            ++i;
            continue;
        }
        const size_t end = name.find(' ', i);
        const size_t stop = end == std::string::npos ? name.size() : end;
        out += TitleWord(name.substr(i, stop - i));
        i = stop;
    }
    return out;
}

std::string TpiShownName(const TpiStrings* bank, const std::string& world, int32_t id, const std::string& infoName,
                         bool& fromBank) {
    fromBank = false;
    uint32_t first = 0, second = 0;
    if (bank != nullptr && TpiNameRows(world, id, first, second) && first < bank->text.size() &&
        second < bank->text.size() && bank->usable[first] && bank->usable[second] && !bank->text[first].empty()) {
        const std::string name = JoinTpiName(bank->text[first], second == 0 ? std::string() : bank->text[second]);
        if (name.find('"') == std::string::npos && name.size() < 0x40) {
            fromBank = true;
            return name;
        }
    }
    return TpiTitleCase(infoName);
}

}  // namespace tpw::assets
