// sam_check.cpp: the game's balance parser (CBalanceTable::Parse and its line readers, BalanceLoader.cpp) run over a
// .sam file's text without storing anything, to find the line it stops at. Each reader below follows the game's one
// step for step; where the game stores a value this only checks what the game's AssignValue checks.

#include "sam_check.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

namespace tpw::assets {
namespace {

// The game's character tests: ASCII only.
bool IsSpace(char c) { return c == ' ' || (c >= 9 && c <= 13); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsAlnum(char c) { return IsDigit(c) || IsAlpha(c); }

// The symbols a map may use: description flags 1 and 2 (kSymbolKeysA and kSymbolKeysB).
const char kSymbolsA[] = ".*Q@+#8642NESWD><OX";
const char kSymbolsB[] = ".^_[]JF7L=HCUn3O";

int32_t Atoi(const char* text) {
    int64_t value = 0;
    for (; IsDigit(*text); ++text) {
        value = value * 10 + (*text - '0');
        if (value > INT32_MAX) value = INT32_MAX;
    }
    return static_cast<int32_t>(value);
}

// read_array_row: "[12]." at the cursor.
bool ReadArrayRow(const char** cursor, int32_t* index) {
    const char* p = *cursor + 1;
    if (!IsDigit(*p)) return false;
    *index = Atoi(p);
    while (IsDigit(*p)) ++p;
    if (*p != ']') return false;
    if (p[1] != '.') return false;
    *cursor = p + 2;
    return true;
}

// read_field_names: up to 16 names of up to 64 letters and digits, separated by '.' or white space.
bool ReadFieldNames(const char** cursor, char (*names)[64], int32_t* count) {
    const char* p = *cursor;
    int32_t n = 0;
    std::memset(names, 0, 16 * 64);
    for (;;) {
        if (!IsAlpha(*p)) return false;
        int32_t i = 0;
        while (i < 64 && IsAlnum(*p)) {
            names[n][i] = *p;
            ++p;
            ++i;
        }
        const char after = *p;
        ++n;
        if (after != '.' && !IsSpace(after)) return false;
        const char separator = *p;
        ++p;
        if (separator == '.' && n < 16) continue;
        break;
    }
    if (n == 16 && *p == '.') return false;
    *cursor = p;
    *count = n;
    return true;
}

// read_integer_value: optional '-' and digits, ended by white space or the end; any number is taken (it saturates).
bool ReadInteger(const char** cursor, int32_t* value) {
    const char* start = *cursor;
    while (IsSpace(*start)) ++start;
    const char* end = start;
    const bool negative = *end == '-';
    if (negative) ++end;
    while (IsDigit(*end)) ++end;
    if (!IsSpace(*end) && *end != 0) return false;
    *cursor = end;
    uint32_t magnitude = 0;
    bool overflow = false;
    for (const char* digit = start + (negative ? 1 : 0); IsDigit(*digit); ++digit) {
        const uint64_t next = static_cast<uint64_t>(magnitude) * 10 + static_cast<uint64_t>(*digit - '0');
        if (next > 0xffffffffull) {
            overflow = true;
        } else {
            magnitude = static_cast<uint32_t>(next);
        }
    }
    if (overflow) magnitude = 0xffffffffu;
    *value = static_cast<int32_t>(negative ? 0u - magnitude : magnitude);
    return true;
}

// read_float_value: optional '-', digits and at most one '.', under 100 characters.
bool ReadFloat(const char** cursor, float* value) {
    const char* start = *cursor;
    while (IsSpace(*start)) ++start;
    const char* end = start;
    int32_t dots = 0;
    if (*end == '-') ++end;
    while (IsDigit(*end) || (*end == '.' && dots < 2)) {
        if (*end == '.') ++dots;
        ++end;
    }
    if (!IsSpace(*end) && *end != 0) return false;
    if (dots > 1 && *end != '.') return false;
    *cursor = end;
    if (end - start >= 100) return false;
    *value = static_cast<float>(std::atof(start));
    return true;
}

// read_string: a string in double quotes on one line.
bool ReadString(const char** cursor) {
    const char* p = *cursor;
    while (IsSpace(*p)) ++p;
    if (*p != '"') return false;
    const char* q = p + 1;
    while (*q != '"' && *q != '\n' && *q != 0) ++q;
    if (*q != '"') return false;
    *cursor = q + 1;
    return true;
}

// recognise_delimit: a line beginning "---"; the cursor goes past it.
bool RecogniseDelimit(const char** cursor) {
    const char* p = *cursor;
    if (p[0] != '-' || p[1] != '-' || p[2] != '-') return false;
    p += 3;
    while (*p != 0 && *p != '\n') ++p;
    if (*p == '\n') ++p;
    *cursor = p;
    return true;
}

// read_map_internal: the rest of the current line, a "---" line, up to 20 rows of up to 20 symbols, a "---" line.
// `symbols` null: any symbol and any size (the check for a file with no table). On failure `stop` is where it stopped
// (a map never closed: its opening "---" line, not the end of the file).
bool ReadMap(const char** cursor, const char* symbols, const char** stop) {
    const char* p = *cursor;
    while (IsSpace(*p)) ++p;
    while (*p != 0 && *p != '\n') ++p;
    if (*p == 0) {
        *stop = p;
        return false;
    }
    ++p;
    const char* opening = p;
    if (!RecogniseDelimit(&p)) {
        *stop = p;
        return false;
    }
    int32_t height = 0;
    while (!RecogniseDelimit(&p)) {
        if (*p == 0) {
            *stop = opening;
            return false;
        }
        int32_t columns = 0;
        while (*p != 0 && *p != '\n') {
            const char symbol = *p;
            if (symbol != ' ' && symbols != nullptr) {
                if (height == 20 || std::strchr(symbols, symbol) == nullptr || columns == 20) {
                    *stop = p;
                    return false;
                }
                ++columns;
            }
            ++p;
        }
        ++height;
        while (*p != 0 && *p != '\n') ++p;
        if (*p == '\n') ++p;
    }
    *cursor = p;
    return true;
}

// LocateBalanceTableRow: the group's heading (type 1, or type 3 for an array group) from the end of the table, then
// the field among the rows above it, down to a blank row or an array start. -1 if the parser would stop.
int32_t Locate(const std::vector<SamRow>& rows, int32_t rowCount, const char* group, const char* field, bool inArray,
               int32_t index) {
    int32_t i = rowCount;
    while (i >= 0) {
        if ((rows[i].type == 1 || rows[i].type == 3) && std::strcmp(rows[i].name, group) == 0) break;
        --i;
    }
    if (i < 0) return -1;
    if (inArray && rows[i].type == 1) return -1;
    if (!inArray && rows[i].type == 3) return -1;
    if (inArray && index >= rows[i].arrayCount) return -1;
    for (--i; i >= 0; --i) {
        if (rows[i].type == 0 || rows[i].type == 2) break;
        // (The game compares a 64-character name with no terminator; a table name is shorter, so it never matches.)
        if (std::strncmp(rows[i].name, field, 64) == 0) return i;
    }
    return -1;
}

// AssignValue: what the game refuses to store.
bool IntegerTaken(const SamRow& row, int32_t value) {
    if (row.type == 5) return value >= 0;
    if (row.type == 6) return value >= row.minInteger && value < row.maxInteger;
    return true;
}

bool FloatTaken(const SamRow& row, float value) {
    if (row.type == 8) return !(value < 0.0f);
    if (row.type == 9) return !(value < row.minFloat || value >= row.maxFloat);
    return true;
}

int LineOf(const std::string& text, const char* at) {
    const size_t end = static_cast<size_t>(at - text.c_str());
    int line = 1;
    for (size_t i = 0; i < end && i < text.size(); ++i) {
        if (text[i] == '\n') ++line;
    }
    return line;
}

std::string BaseNameLower(const std::string& path) {
    const size_t slash = path.find_last_of("/\\:");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return name;
}

std::mutex gTablesMutex;
std::map<std::string, std::vector<SamRow>>& Tables() {
    static std::map<std::string, std::vector<SamRow>> tables;
    return tables;
}

}  // namespace

void SetSamTable(const std::string& fileName, const SamRow* rows, size_t count) {
    std::lock_guard<std::mutex> lock(gTablesMutex);
    Tables()[BaseNameLower(fileName)] = std::vector<SamRow>(rows, rows + count);
}

std::vector<SamRow> SamTableFor(const std::string& path) {
    std::lock_guard<std::mutex> lock(gTablesMutex);
    const auto it = Tables().find(BaseNameLower(path));
    return it == Tables().end() ? std::vector<SamRow>() : it->second;
}

int SamUnreadableLine(const std::string& path, const std::string& text) {
    return SamUnreadableLine(SamTableFor(path), text);
}

// CBalanceTable::Parse step for step.
int SamUnreadableLine(const std::vector<SamRow>& table, const std::string& file) {
    const bool typed = !table.empty();
    std::vector<SamRow> rows = table;
    int32_t rowCount = 0;
    while (rowCount < static_cast<int32_t>(rows.size()) && rows[rowCount].type != 12) ++rowCount;
    if (typed && rowCount == static_cast<int32_t>(rows.size())) {
        SamRow end{};
        end.type = 12;
        rows.push_back(end);
    }
    std::string text;
    text.reserve(file.size());
    for (const char c : file) {
        if (c == 0) break;   // the game's text ends at its first NUL
        if (c != '\r') text.push_back(c);
    }
    const char* cursor = text.c_str();
    while (*cursor != 0) {
        const char* p = cursor;
        while (IsSpace(*p)) ++p;
        const char* const statement = p;
        if (*p != 0 && *p != '#' && *p != '\n') {
            char group[65];
            std::memset(group, 0, sizeof group);
            int32_t length = 0;
            while (IsAlnum(*p) && length < 64) group[length++] = *p++;
            if (length == 0 || (*p != '.' && *p != '[')) return LineOf(text, statement);
            int32_t index = 0;
            bool inArray = false;
            if (*p == '[') {
                if (!ReadArrayRow(&p, &index)) return LineOf(text, statement);
                inArray = true;
            } else {
                ++p;
            }
            char names[16][64];
            int32_t fieldCount = 0;
            if (!ReadFieldNames(&p, names, &fieldCount)) return LineOf(text, statement);
            if (!typed) {
                // No table: a field alone on its line followed by a "---" line is taken for a map.
                if (fieldCount == 1) {
                    const char* q = cursor;
                    while (IsSpace(*q)) ++q;
                    while (*q != 0 && *q != '\n') ++q;
                    if (*q == '\n' && std::strncmp(q + 1, "---", 3) == 0) {
                        const char* stop = nullptr;
                        // a map or not, the game stops here: an unclosed map, or a "---" line that is no statement
                        if (!ReadMap(&cursor, nullptr, &stop)) return LineOf(text, stop);
                    }
                }
            } else {
                for (int32_t k = 0; k < fieldCount; ++k) {
                    const int32_t row = Locate(rows, rowCount, group, names[k], inArray, index);
                    if (row < 0) return LineOf(text, statement);
                    const SamRow& description = rows[row];
                    if (description.type == 11) {
                        if (fieldCount != 1) return LineOf(text, statement);
                        const char* symbols = description.flags == 1 ? kSymbolsA
                                              : description.flags == 2 ? kSymbolsB
                                                                       : nullptr;
                        if (symbols == nullptr) return LineOf(text, statement);
                        const char* stop = nullptr;
                        if (!ReadMap(&cursor, symbols, &stop)) return LineOf(text, stop);
                    } else if (description.type == 10) {
                        if (!ReadString(&p)) return LineOf(text, statement);
                    } else if (description.type == 7 || description.type == 8 || description.type == 9) {
                        float value = 0;
                        if (!ReadFloat(&p, &value) || !FloatTaken(description, value)) return LineOf(text, statement);
                    } else {
                        int32_t value = 0;
                        if (!ReadInteger(&p, &value) || !IntegerTaken(description, value)) {
                            return LineOf(text, statement);
                        }
                    }
                }
            }
        }
        // To the end of the line the reading of this one began on.
        while (*cursor != 0 && *cursor != '\n') ++cursor;
        if (*cursor == '\n') ++cursor;
    }
    return 0;
}

}  // namespace tpw::assets
