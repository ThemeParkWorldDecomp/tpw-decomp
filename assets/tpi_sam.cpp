// tpi_sam.cpp: Theme Park Inc .sam statements for Theme Park World's tables (tpi_sam.h). The statement grammar is the
// game's (CBalanceTable::Parse, mirrored in sam_check.cpp): optional white space, '#' for a comment line, else a group
// of up to 64 letters and digits, then '.' or "[n].", then up to 16 field names separated by '.', then the values to the
// end of the line; a map field's value is the rest of its line, a "---" line, the rows, and a "---" line.
#include "tpi_sam.h"

#include <cstdlib>
#include <cstring>

namespace tpw::assets {

namespace {

bool IsSpace(char c) { return c == ' ' || (c >= 9 && c <= 13); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsAlnum(char c) { return IsDigit(c) || IsAlpha(c); }

const char kSymbolsA[] = ".*Q@+#8642NESWD><OX";
const char kSymbolsB[] = ".^_[]JF7L=HCUn3O";

struct Statement {
    std::string group;
    bool inArray = false;
    int32_t index = 0;
    std::vector<std::string> fields;
    size_t valuesAt = 0;   // offset in the line of the first value
};

// Parses the head of a statement line (no leading white space); false when the game's parser would stop here.
bool ParseHead(const std::string& line, Statement& s) {
    size_t p = 0;
    while (p < line.size() && IsAlnum(line[p]) && p < 64) ++p;
    if (p == 0 || p >= line.size() || (line[p] != '.' && line[p] != '[')) return false;
    s.group = line.substr(0, p);
    if (line[p] == '[') {
        ++p;
        if (p >= line.size() || !IsDigit(line[p])) return false;
        int32_t index = 0;
        while (p < line.size() && IsDigit(line[p])) index = index * 10 + (line[p++] - '0');
        if (p + 1 >= line.size() || line[p] != ']' || line[p + 1] != '.') return false;
        p += 2;
        s.inArray = true;
        s.index = index;
    } else {
        ++p;
    }
    for (;;) {
        if (p >= line.size() || !IsAlpha(line[p])) return false;
        size_t start = p;
        while (p < line.size() && IsAlnum(line[p]) && p - start < 64) ++p;
        s.fields.push_back(line.substr(start, p - start));
        // A name ends at '.' or white space; the line's end counts as white space (the file has '\r' or '\n' there).
        if (p >= line.size()) break;
        const char after = line[p];
        if (after != '.' && !IsSpace(after)) return false;
        ++p;
        if (after == '.') {
            if (s.fields.size() == 16) return false;
            continue;
        }
        break;
    }
    s.valuesAt = p;
    return true;
}

// LocateBalanceTableRow, as sam_check.cpp has it.
int32_t Locate(const std::vector<SamRow>& rows, int32_t rowCount, const std::string& group, const std::string& field,
               bool inArray, int32_t index) {
    int32_t i = rowCount;
    while (i >= 0) {
        if ((rows[i].type == 1 || rows[i].type == 3) && group == rows[i].name) break;
        --i;
    }
    if (i < 0) return -1;
    if (inArray && rows[i].type == 1) return -1;
    if (!inArray && rows[i].type == 3) return -1;
    if (inArray && index >= rows[i].arrayCount) return -1;
    for (--i; i >= 0; --i) {
        if (rows[i].type == 0 || rows[i].type == 2) break;
        if (std::strncmp(rows[i].name, field.c_str(), 64) == 0) return i;
    }
    return -1;
}

std::string Key(const Statement& s, bool anyIndex) {
    std::string key = s.group;
    if (s.inArray) key += anyIndex ? "[]" : "[" + std::to_string(s.index) + "]";
    key += ".";
    for (size_t i = 0; i < s.fields.size(); ++i) key += (i ? "." : "") + s.fields[i];
    return key;
}

bool IsDelimit(const std::string& line) { return line.compare(0, 3, "---") == 0; }

// Splits text into lines without their '\n' (and without '\r', as the game drops them). The text ends at a NUL.
std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text) {
        if (c == 0) break;
        if (c == '\r') continue;
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void Remember(std::vector<std::string>& list, const std::string& item) {
    for (const auto& have : list)
        if (have == item) return;
    list.push_back(item);
}

bool ParseInteger(const std::string& line, size_t at, int32_t& value, size_t& begin, size_t& end) {
    size_t p = at;
    while (p < line.size() && IsSpace(line[p])) ++p;
    begin = p;
    bool negative = false;
    if (p < line.size() && line[p] == '-') {
        negative = true;
        ++p;
    }
    if (p >= line.size() || !IsDigit(line[p])) return false;
    int64_t v = 0;
    while (p < line.size() && IsDigit(line[p])) {
        v = v * 10 + (line[p] - '0');
        if (v > 0x7fffffff) v = 0x7fffffff;
        ++p;
    }
    if (p < line.size() && !IsSpace(line[p])) return false;
    value = static_cast<int32_t>(negative ? -v : v);
    end = p;
    return true;
}

bool ParseFloat(const std::string& line, size_t at, float& value, size_t& end) {
    size_t p = at;
    while (p < line.size() && IsSpace(line[p])) ++p;
    size_t start = p;
    int dots = 0;
    if (p < line.size() && line[p] == '-') ++p;
    while (p < line.size() && (IsDigit(line[p]) || (line[p] == '.' && dots < 2))) {
        if (line[p] == '.') ++dots;
        ++p;
    }
    if (p < line.size() && !IsSpace(line[p])) return false;
    if (dots > 1) return false;
    value = std::strtof(line.substr(start, p - start).c_str(), nullptr);
    end = p;
    return true;
}

bool ParseString(const std::string& line, size_t at, size_t& end) {
    size_t p = at;
    while (p < line.size() && IsSpace(line[p])) ++p;
    if (p >= line.size() || line[p] != '"') return false;
    size_t close = line.find('"', p + 1);
    if (close == std::string::npos) return false;
    end = close + 1;
    return true;
}

// Would the game's parser take the values of a (non-map) statement? Its bounds are the table's: type 5 and 8 not
// negative, 6 and 9 inside [min, max), 10 a quoted string.
bool ValuesTaken(const std::vector<SamRow>& rows, const std::vector<int32_t>& fieldRows, const std::string& body,
                 size_t at) {
    size_t p = at;
    for (const int32_t row : fieldRows) {
        const SamRow& d = rows[row];
        if (d.type == 10) {
            if (!ParseString(body, p, p)) return false;
        } else if (d.type == 7 || d.type == 8 || d.type == 9) {
            float v = 0;
            if (!ParseFloat(body, p, v, p)) return false;
            if (d.type == 8 && v < 0.0f) return false;
            if (d.type == 9 && (v < d.minFloat || v >= d.maxFloat)) return false;
        } else {
            int32_t v = 0;
            size_t begin = 0;
            if (!ParseInteger(body, p, v, begin, p)) return false;
            if (d.type == 5 && v < 0) return false;
            if (d.type == 6 && (v < d.minInteger || v >= d.maxInteger)) return false;
        }
    }
    return true;
}

}  // namespace

bool FilterTpiSam(const std::vector<SamRow>& table, const std::string& text, const TpiSamOptions& options,
                  std::string& out, TpiSamReport& report, std::string& err) {
    if (table.empty()) {
        err = "no settings table registered for this .sam file";
        return false;
    }
    std::vector<SamRow> rows = table;
    int32_t rowCount = 0;
    while (rowCount < static_cast<int32_t>(rows.size()) && rows[rowCount].type != 12) ++rowCount;
    if (rowCount == static_cast<int32_t>(rows.size())) {
        SamRow end{};
        end.type = 12;
        rows.push_back(end);
    }
    // Statements the appended lines replace.
    std::vector<std::string> replaced;
    for (const auto& line : options.append) {
        Statement s;
        if (ParseHead(line, s)) replaced.push_back(Key(s, false));
    }
    out.clear();
    report = TpiSamReport{};
    const std::vector<std::string> lines = Lines(text);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string line = lines[i];
        size_t p = 0;
        while (p < line.size() && IsSpace(line[p])) ++p;
        if (p >= line.size() || line[p] == '#') {
            out += line + "\n";
            continue;
        }
        const std::string indent = line.substr(0, p);
        std::string body = line.substr(p);
        Statement s;
        if (!ParseHead(body, s)) {
            // The game's parser would stop here (Theme Park World's too): the line goes, and is reported as it is.
            Remember(report.dropped, "(unreadable) " + body.substr(0, 40));
            continue;
        }
        // Renames, before anything else.
        if (s.fields.size() == 1) {
            const std::string exact = Key(s, false), any = Key(s, true);
            for (const auto& [from, to] : options.rename) {
                if (from == exact || from == any) {
                    Statement renamed;
                    if (ParseHead(to + " ", renamed)) {
                        body = to + body.substr(s.valuesAt - 1);
                        renamed.valuesAt = to.size() + 1;
                        s = renamed;
                    }
                    break;
                }
            }
        }
        const std::string key = Key(s, false);
        const std::string groupKey = s.group + ".*";
        bool drop = false;
        for (const auto& d : options.drop)
            if (d == key || d == Key(s, true) || d == groupKey) drop = true;
        for (const auto& r : replaced)
            if (r == key) drop = true;
        // The table: every field must be there.
        int32_t mapRow = -1;
        std::vector<int32_t> fieldRows;
        if (!drop) {
            for (const auto& field : s.fields) {
                const int32_t row = Locate(rows, rowCount, s.group, field, s.inArray, s.index);
                if (row < 0) {
                    drop = true;
                    break;
                }
                if (rows[row].type == 11) mapRow = row;
                fieldRows.push_back(row);
            }
        }
        if (!drop && mapRow < 0 && !ValuesTaken(rows, fieldRows, body, s.valuesAt)) {
            Remember(report.dropped, key + " (value)");
            continue;
        }
        if (drop && mapRow < 0 && i + 1 < lines.size() && IsDelimit(lines[i + 1])) {
            // A map Theme Park World has no row for: its lines go with it, through the closing "---" (or to the end).
            Remember(report.dropped, key);
            size_t j = i + 2;
            while (j < lines.size() && !IsDelimit(lines[j])) ++j;
            i = j < lines.size() ? j : lines.size();
            continue;
        }
        if (mapRow >= 0) {
            // A map: the statement line, a "---" line, the rows, a "---" line. Kept or dropped whole, and only kept when
            // Theme Park World's parser reads it: one field, a known symbol set, every symbol in it, at most 20 by 20.
            size_t j = i + 1;
            bool wellFormed = s.fields.size() == 1 && j < lines.size() && IsDelimit(lines[j]);
            const char* symbols = rows[mapRow].flags == 1 ? kSymbolsA : rows[mapRow].flags == 2 ? kSymbolsB : nullptr;
            if (symbols == nullptr) wellFormed = false;
            size_t endLine = j;
            if (wellFormed) {
                ++j;
                int32_t height = 0;
                for (;; ++j) {
                    if (j >= lines.size()) {
                        wellFormed = false;
                        break;
                    }
                    if (IsDelimit(lines[j])) break;
                    int32_t columns = 0;
                    for (const char c : lines[j]) {
                        if (c == ' ') continue;
                        if (height == 20 || columns == 20 || std::strchr(symbols, c) == nullptr) wellFormed = false;
                        ++columns;
                    }
                    ++height;
                }
                endLine = j;
            }
            if (!wellFormed) {
                // Without a closing line the game stops: drop what we can see of it (to the next statement-looking line).
                Remember(report.dropped, key);
                if (j >= lines.size()) {
                    i = lines.size();
                } else {
                    i = endLine;
                }
                continue;
            }
            if (drop) {
                Remember(report.dropped, key);
                i = endLine;
                continue;
            }
            for (size_t k = i; k <= endLine; ++k) out += (k == i ? indent + body : lines[k]) + "\n";
            ++report.kept;
            i = endLine;
            continue;
        }
        if (drop) {
            Remember(report.dropped, key);
            continue;
        }
        // Id fields.
        if (options.ids != nullptr && s.fields.size() == 1) {
            for (const auto& idField : options.idFields) {
                if (idField != key) continue;
                int32_t value = 0;
                size_t begin = 0, end = 0;
                if (ParseInteger(body, s.valuesAt, value, begin, end)) {
                    auto it = options.ids->find(value);
                    if (it == options.ids->end()) {
                        Remember(report.unmappedIds, key + " " + std::to_string(value));
                    } else {
                        body = body.substr(0, begin) + std::to_string(it->second) + body.substr(end);
                    }
                }
                break;
            }
        }
        out += indent + body + "\n";
        ++report.kept;
    }
    if (!options.append.empty()) {
        out += "\n# Written by the Theme Park Inc converter.\n";
        for (const auto& line : options.append) {
            out += line + "\n";
            ++report.kept;
        }
    }
    return true;
}

namespace {

bool FindStatement(const std::string& text, const std::string& group, const std::string& field, std::string& body,
                   Statement& s) {
    for (const auto& line : Lines(text)) {
        size_t p = 0;
        while (p < line.size() && IsSpace(line[p])) ++p;
        if (p >= line.size() || line[p] == '#') continue;
        body = line.substr(p);
        s = Statement{};
        if (!ParseHead(body, s)) continue;
        if (s.group == group && s.fields.size() == 1 && s.fields[0] == field) return true;
    }
    return false;
}

}  // namespace

bool TpiSamInteger(const std::string& text, const std::string& group, const std::string& field, int32_t& value) {
    std::string body;
    Statement s;
    if (!FindStatement(text, group, field, body, s)) return false;
    size_t end = 0;
    size_t begin = 0;
    return ParseInteger(body, s.valuesAt, value, begin, end);
}

bool TpiSamString(const std::string& text, const std::string& group, const std::string& field, std::string& value) {
    std::string body;
    Statement s;
    if (!FindStatement(text, group, field, body, s)) return false;
    size_t p = s.valuesAt;
    while (p < body.size() && IsSpace(body[p])) ++p;
    if (p >= body.size() || body[p] != '"') return false;
    size_t close = body.find('"', p + 1);
    if (close == std::string::npos) return false;
    value = body.substr(p + 1, close - p - 1);
    return true;
}

}  // namespace tpw::assets
