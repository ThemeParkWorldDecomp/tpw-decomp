#include "sam_defaults.h"

#include <cstring>

namespace tpw::assets {
namespace {
// The launcher's built-in original .sam files (launcher/sam_originals.cpp), none until it registers them.
const SamDefault* gDefaults = nullptr;
size_t gDefaultCount = 0;

std::string Lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c == '\\') c = '/';
    }
    return s;
}

bool BlankOrComment(const std::string& line) {
    size_t p = line.find_first_not_of(" \t\r\n\v\f");
    return p == std::string::npos || line[p] == '#';
}

bool Delimiter(const std::string& line) { return line.compare(0, 3, "---") == 0; }

bool TextLine(const std::string& line) {
    for (unsigned char c : line)
        if (c == 0 || (c < 32 && c != '\t' && c != '\r' && c != '\n' && c != '\v' && c != '\f')) return false;
    return true;
}

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> lines;
    for (size_t start = 0; start < text.size();) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        else ++end;
        lines.push_back(text.substr(start, end - start));
        start = end;
    }
    return lines;
}

bool Readable(const std::vector<SamRow>& table, const std::string& text) {
    return TextLine(text) && SamUnreadableLine(table, text) == 0;
}
}  // namespace

void SetSamDefaults(const SamDefault* rows, size_t count) {
    gDefaults = rows;
    gDefaultCount = rows ? count : 0;
}
const SamDefault* SamDefaults() { return gDefaults; }
size_t SamDefaultCount() { return gDefaultCount; }

const SamDefault* FindSamDefault(const std::string& edition, const std::string& dest) {
    const std::string ed = Lower(edition), path = Lower(dest);
    for (size_t i = 0; i < gDefaultCount; ++i)
        if (ed == gDefaults[i].edition && path == Lower(gDefaults[i].dest)) return &gDefaults[i];
    return nullptr;
}

bool RepairSam(const std::vector<SamRow>& table, const std::string& original,
               const std::string& player, std::string& out, SamRepairResult& result, std::string& error) {
    result = {};
    error.clear();
    if (table.empty()) {
        error = "no settings table registered for this .sam file";
        return false;
    }
    if (player.size() > 8 * 1024 * 1024 || original.empty() || !Readable(table, original)) {
        error = "cannot safely repair this .sam file from its original defaults";
        return false;
    }
    if (Readable(table, player)) {
        out = player;
        return true;
    }
    std::string fixed = original;
    if (fixed.back() != '\n') fixed += '\n';
    fixed += "\n# Readable settings retained from your imported file.\n";
    const auto lines = Lines(player);
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string statement = lines[i];
        if (!TextLine(statement)) {
            ++result.discarded;
            continue;
        }
        // A map is one statement. Stop an unclosed damaged map at the next
        // readable statement so later player settings can still be recovered.
        if (!BlankOrComment(statement) && i + 1 < lines.size() && Delimiter(lines[i + 1])) {
            size_t end = i + 2;
            for (; end < lines.size() && !Delimiter(lines[end]); ++end) {
                if (!BlankOrComment(lines[end]) && Readable(table, lines[end])) break;
            }
            const bool closed = end < lines.size() && Delimiter(lines[end]);
            size_t past = closed ? end + 1 : end;
            for (size_t j = i + 1; j < past; ++j) statement += lines[j];
            i = past - 1;
            if (!closed) {
                ++result.discarded;
                continue;
            }
        }
        // A final line needs a terminator for the game's field-name reader.
        if (statement.empty() || statement.back() != '\n') statement += '\n';
        if (Readable(table, statement)) {
            fixed += statement;
            if (!BlankOrComment(statement)) ++result.kept;
        } else {
            ++result.discarded;
        }
    }
    if (!Readable(table, fixed)) {
        error = "repaired .sam file did not pass the game's read check";
        return false;
    }
    result.repaired = true;
    out = std::move(fixed);
    return true;
}

bool RepairSam(const std::string& edition, const std::string& dest, const std::string& player,
               std::string& out, SamRepairResult& result, std::string& error) {
    const SamDefault* d = FindSamDefault(edition, dest);
    if (!d) {
        result = {};
        error = "no original defaults for this edition and .sam file";
        return false;
    }
    return RepairSam(SamTableFor(dest), std::string(d->text, d->size), player, out, result, error);
}
}  // namespace tpw::assets
