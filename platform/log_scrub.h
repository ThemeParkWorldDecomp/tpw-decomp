// log_scrub.h: takes what identifies the player or the computer out of a log's text, so that two computers doing the same
// thing write the same log and a log is safe to send. Every line is scrubbed as it is written to the log file (log_file.h),
// the Windows launcher.log is scrubbed the same way (host_win.cpp), and Export logs scrubs once more (log_export.h).
//
// What changes, in this order of preference (a path rule first, since a path says plainly what the text is):
//   the home folder          ~  (Windows: %USERPROFILE%); the system's spellings of it, and /Users/NAME, /home/NAME and
//                            C:\Users\NAME of any account, which also cover a name the system spells differently
//   other folders of NAME    a Windows folder moved out of the home (APPDATA, LOCALAPPDATA, TEMP, OneDrive) keeps its
//                            variable's name; /Volumes/NAME is /Volumes/<volume>; /var/folders/xx/hash is /var/folders/<id>;
//                            "OneDrive - Company" is OneDrive
//   the account name         <user>, only where it is plainly the name: after user=, username:, login=, owner: and the like
//   the computer name        <computer>, wherever it stands as a word of its own (at least 4 letters, and not a word such
//                            as "localhost" or "desktop")
//   Windows SIDs (S-1-5-21-...), MAC addresses and IPv4 addresses   <sid>, <mac>, <ip>
// What stays: the OS version, the CPU, the GPU, the build version and every relative path. Scrubbing the scrubbed text
// changes nothing, so the export can scrub again what the log file already holds.
//
// A name that is also an ordinary word ("Max", "Sam", "data") is therefore never replaced just for being that word: it
// goes only inside the home path, after a key such as user=, or (the computer name) when it is a word of its own and long
// enough. The text "Max has a max of 3" is left alone for a user called Max.
//
// The engine is inline and allocates nothing while it scrubs (it reads an identity built once), so the log file can use it in
// a signal handler and the platform files that include log_file.h need nothing linked. The identity of this computer is
// gathered by CollectScrubInputs (log_scrub.cpp); the tests build their own with MakeScrubIdentity.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace tpw {

// A folder (or a name) and what it becomes.
struct ScrubRoot {
    std::string text;
    std::string replacement;
};

// What to look for, as the system spells it: a pure description, so that the Mac's and Windows' rules are tested on every
// system.
struct ScrubInputs {
    bool windows = false;                 // case-blind paths, and %USERPROFILE% for the home
    std::vector<std::string> homes;       // the home folder, each way the system spells it
    std::vector<ScrubRoot> roots;         // other folders to replace as a whole ("/Volumes/My Disk", a moved APPDATA)
    std::vector<std::string> users;       // account names (and the home folder's own name)
    std::vector<std::string> hosts;       // computer names
};

struct ScrubName {
    std::string text;
    bool blind = false;   // match without regard to case
};

// ScrubInputs, ready: sorted longest first, the computer names widened to their short forms and the unsafe ones dropped.
struct ScrubIdentity {
    bool caseBlind = false;
    std::vector<ScrubRoot> roots;
    std::vector<ScrubName> users;
    std::vector<ScrubName> hosts;
};

// The identity every log write uses (null: the generic rules only). Set once by EnsureScrubIdentity (log_scrub.cpp); the
// tests set their own. One per program.
inline std::atomic<const ScrubIdentity*> gScrubIdentity{nullptr};

namespace scrub_detail {

inline bool IsDigit(unsigned char c) { return c >= '0' && c <= '9'; }
inline bool IsAlpha(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
inline bool IsAlnum(unsigned char c) { return IsDigit(c) || IsAlpha(c); }
inline bool IsHex(unsigned char c) { return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
inline bool IsSep(unsigned char c) { return c == '/' || c == '\\'; }
inline unsigned char Fold(unsigned char c) { return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + 32) : c; }
// A character of a name: letters, digits, _, -, and every byte of UTF-8 text beyond ASCII.
inline bool NameChar(unsigned char c) { return IsAlnum(c) || c == '_' || c == '-' || c >= 0x80; }

// Whether a name ends at in[j]: the next character is not one of a name, and not the dot of a longer name ("bob." ends a
// sentence, "bob.smith" is another name); a computer name's dot ends it ("pc.local").
inline bool NameEndsAt(const char* in, size_t n, size_t j, bool dotEnds = false) {
    if (j >= n) return true;
    const unsigned char c = static_cast<unsigned char>(in[j]);
    if (NameChar(c)) return false;
    if (c == '.' && j + 1 < n && !dotEnds) {
        const unsigned char d = static_cast<unsigned char>(in[j + 1]);
        if (NameChar(d)) return false;
    }
    return true;
}

// Whether a path may begin at in[i]: nothing before it that belongs to a longer path or a name.
inline bool PathStartsAt(const char* in, size_t i) {
    if (i == 0) return true;
    const unsigned char p = static_cast<unsigned char>(in[i - 1]);
    return !(NameChar(p) || p == '.' || IsSep(p) || p == '~');
}

// `text` at in[i...]; a / matches a \ (and the other way); blind: letters without regard to case.
inline bool PathMatches(const char* in, size_t n, size_t i, const std::string& text, bool blind) {
    if (i + text.size() > n) return false;
    for (size_t k = 0; k < text.size(); ++k) {
        const unsigned char a = static_cast<unsigned char>(in[i + k]), b = static_cast<unsigned char>(text[k]);
        if (a == b || (IsSep(a) && IsSep(b)) || (blind && Fold(a) == Fold(b))) continue;
        return false;
    }
    return true;
}

// `word` (lower case ASCII letters, spaces) at in[i...], without regard to case.
inline bool WordAt(const char* in, size_t n, size_t i, const char* word) {
    const size_t length = std::strlen(word);
    if (i + length > n) return false;
    for (size_t k = 0; k < length; ++k)
        if (Fold(static_cast<unsigned char>(in[i + k])) != static_cast<unsigned char>(word[k])) return false;
    return true;
}

inline bool IsSystemAccount(const char* in, size_t from, size_t to) {
    static const char* const kNames[] = {"shared", "public", "default", "all"};
    for (const char* name : kNames)
        if (to - from == std::strlen(name) && WordAt(in, to, from, name)) return true;
    return false;
}

// The end of a name that stands in a path, from `from`: at a separator, a quote, an angle bracket or other character a
// name cannot hold, and at a space too, unless the path is in quotes (then only the quote or a separator ends it, and
// the name may hold spaces). A final dot is the sentence's.
inline size_t SegmentEnd(const char* in, size_t n, size_t from, bool quoted) {
    size_t k = from;
    for (; k < n; ++k) {
        const unsigned char c = static_cast<unsigned char>(in[k]);
        if (c < 0x20 || IsSep(c) || c == '"' || c == '\'' || c == '<' || c == '>' || c == '|' || c == ':' || c == '*' ||
            c == '?' || c == ',' || c == ';' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}')
            break;
        if (c == ' ' && !quoted) break;
    }
    while (k > from + 1 && in[k - 1] == '.') --k;
    return k;
}

inline bool QuoteBefore(const char* in, size_t i) { return i > 0 && (in[i - 1] == '"' || in[i - 1] == '\''); }

// A rule's answer: copy `keep` bytes of the text as they are, drop the next `skip`, and write the piece.
struct Match {
    size_t keep = 0;
    size_t skip = 0;
    char piece[40] = {0};
    size_t pieceLength = 0;
    void Piece(const char* text) {
        pieceLength = std::strlen(text);
        std::memcpy(piece, text, pieceLength);
    }
};

// A home folder of any account: /Users/NAME, /home/NAME, C:\Users\NAME (and the other drive letters), at in[i] (the drive
// letter or the separator before Users or home).
inline bool MatchAnyHome(const char* in, size_t n, size_t i, Match& m) {
    size_t at = i;
    bool drive = false;
    if (i + 2 < n && IsAlpha(static_cast<unsigned char>(in[i])) && in[i + 1] == ':' && IsSep(static_cast<unsigned char>(in[i + 2])) &&
        (i == 0 || !NameChar(static_cast<unsigned char>(in[i - 1])))) {
        drive = true;
        at = i + 2;
    }
    if (!IsSep(static_cast<unsigned char>(in[at]))) return false;
    const bool users = WordAt(in, n, at + 1, "users");
    const bool home = !users && in[at] == '/' && !drive && at + 5 <= n && std::memcmp(in + at + 1, "home", 4) == 0;
    if (!users && !home) return false;
    const size_t wordEnd = at + 1 + (users ? 5 : 4);
    if (wordEnd >= n || !IsSep(static_cast<unsigned char>(in[wordEnd]))) return false;
    const bool start = drive || PathStartsAt(in, at);
    if (home && !start) return false;
    const size_t nameStart = wordEnd + 1;
    const size_t nameEnd = SegmentEnd(in, n, nameStart, QuoteBefore(in, i));
    if (nameEnd <= nameStart || IsSystemAccount(in, nameStart, nameEnd)) return false;
    if (start) {
        m.skip = nameEnd - i;
        m.Piece(drive || in[at] == '\\' ? "%USERPROFILE%" : "~");
    } else {
        m.keep = nameStart - i;   // "/Users/" stays, the name goes
        m.skip = nameEnd - nameStart;
        m.Piece("<user>");
    }
    return true;
}

// The rules that need no identity. in[i] is the first byte of a possible match.
inline bool MatchGeneric(const char* in, size_t n, size_t i, Match& m) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    if ((IsSep(c) || (IsAlpha(c) && i + 1 < n && in[i + 1] == ':')) && MatchAnyHome(in, n, i, m)) return true;
    if (c == '/') {
        // /Volumes/NAME, where a path begins
        if (i + 9 <= n && std::memcmp(in + i, "/Volumes/", 9) == 0 && PathStartsAt(in, i)) {
            const size_t nameEnd = SegmentEnd(in, n, i + 9, QuoteBefore(in, i));
            if (nameEnd > i + 9 && in[i + 9] != '<') {
                m.keep = 9;
                m.skip = nameEnd - (i + 9);
                m.Piece("<volume>");
                return true;
            }
        }
        // /var/folders/xx/hash: the per-user temporary folder's name, which names the account
        if (i + 13 <= n && std::memcmp(in + i, "/var/folders/", 13) == 0) {
            size_t k = i + 13;
            const size_t first = k;
            while (k < n && (IsAlnum(static_cast<unsigned char>(in[k])) || in[k] == '_')) ++k;
            if (k > first && k < n && in[k] == '/') {
                const size_t second = ++k;
                while (k < n && (IsAlnum(static_cast<unsigned char>(in[k])) || in[k] == '_')) ++k;
                if (k > second) {
                    m.keep = 13;
                    m.skip = k - (i + 13);
                    m.Piece("<id>");
                    return true;
                }
            }
        }
    }
    // OneDrive - Company: the company's name goes
    if ((c == 'O' || c == 'o') && i > 0 && IsSep(static_cast<unsigned char>(in[i - 1])) && WordAt(in, n, i, "onedrive - ")) {
        size_t k = i + 11;
        while (k < n && !IsSep(static_cast<unsigned char>(in[k])) && in[k] != '"' && in[k] != '\'' &&
               static_cast<unsigned char>(in[k]) >= 0x20)
            ++k;
        if (k > i + 11) {
            m.keep = 0;
            m.skip = k - i;
            m.Piece("OneDrive");
            return true;
        }
    }
    // S-1-5-21-a-b-c[-d]: a Windows account's SID
    if ((c == 'S' || c == 's') && (i == 0 || !NameChar(static_cast<unsigned char>(in[i - 1]))) && WordAt(in, n, i, "s-1-5-21-")) {
        size_t k = i + 9;
        int groups = 0;
        while (groups < 4) {
            const size_t digits = k;
            while (k < n && IsDigit(static_cast<unsigned char>(in[k]))) ++k;
            if (k == digits) break;
            ++groups;
            if (groups < 4 && k + 1 < n && in[k] == '-' && IsDigit(static_cast<unsigned char>(in[k + 1]))) ++k;
            else break;
        }
        if (groups >= 3) {
            m.skip = k - i;
            m.Piece("<sid>");
            return true;
        }
    }
    // hh:hh:hh:hh:hh:hh (or with -): a MAC address
    if (IsHex(c) && i + 17 <= n && (i == 0 || (!NameChar(static_cast<unsigned char>(in[i - 1])) && in[i - 1] != ':'))) {
        const char sep = in[i + 2];
        if (sep == ':' || sep == '-') {
            bool ok = true;
            for (size_t k = 0; k < 6 && ok; ++k) {
                ok = IsHex(static_cast<unsigned char>(in[i + 3 * k])) && IsHex(static_cast<unsigned char>(in[i + 3 * k + 1])) &&
                     (k == 5 || in[i + 3 * k + 2] == sep);
            }
            const size_t after = i + 17;
            if (ok && after < n) ok = !IsHex(static_cast<unsigned char>(in[after])) && !(in[after] == sep && after + 1 < n && IsHex(static_cast<unsigned char>(in[after + 1])));
            if (ok) {
                m.skip = 17;
                m.Piece("<mac>");
                return true;
            }
        }
    }
    // a.b.c.d with every part up to 255: an IPv4 address (not a loopback one, and not part of a longer number or a version)
    if (IsDigit(c) && (i == 0 || (!IsAlnum(static_cast<unsigned char>(in[i - 1])) && in[i - 1] != '.' && in[i - 1] != '-'))) {
        size_t k = i;
        int values[4];
        bool ok = true;
        for (int part = 0; part < 4 && ok; ++part) {
            const size_t digits = k;
            int value = 0;
            while (k < n && IsDigit(static_cast<unsigned char>(in[k])) && k - digits < 3) value = value * 10 + (in[k++] - '0');
            ok = k > digits && value <= 255 && (part == 3 || (k < n && in[k] == '.'));
            values[part] = value;
            if (ok && part < 3) ++k;
        }
        if (ok && k < n && (IsDigit(static_cast<unsigned char>(in[k])) || (in[k] == '.' && k + 1 < n && IsDigit(static_cast<unsigned char>(in[k + 1])))))
            ok = false;
        if (ok && values[0] != 127 && !(values[0] == 0 && values[1] == 0 && values[2] == 0 && values[3] == 0)) {
            m.skip = k - i;
            m.Piece("<ip>");
            return true;
        }
    }
    return false;
}

// user=NAME, "username": NAME, login: NAME ... with the account name after the key. in[i] starts the key.
inline bool MatchUserKey(const ScrubIdentity& id, const char* in, size_t n, size_t i, Match& m) {
    if (i > 0 && NameChar(static_cast<unsigned char>(in[i - 1]))) return false;
    static const char* const kKeys[] = {"username", "user name", "user", "login", "logname", "owner", "account"};
    for (const char* key : kKeys) {
        if (!WordAt(in, n, i, key)) continue;
        size_t k = i + std::strlen(key);
        if (k < n && IsAlnum(static_cast<unsigned char>(in[k]))) continue;
        if (k < n && (in[k] == '"' || in[k] == '\'')) ++k;
        size_t spaces = 0;
        while (k < n && (in[k] == ' ' || in[k] == '\t') && spaces++ < 4) ++k;
        if (k >= n || (in[k] != '=' && in[k] != ':')) continue;
        ++k;
        spaces = 0;
        while (k < n && (in[k] == ' ' || in[k] == '\t') && spaces++ < 4) ++k;
        if (k < n && (in[k] == '"' || in[k] == '\'')) ++k;
        for (const ScrubName& user : id.users) {
            if (k + user.text.size() > n || !PathMatches(in, n, k, user.text, user.blind) || !NameEndsAt(in, n, k + user.text.size())) continue;
            m.keep = k - i;
            m.skip = user.text.size();
            m.Piece("<user>");
            return true;
        }
    }
    return false;
}

inline bool MatchAt(const ScrubIdentity* id, const char* in, size_t n, size_t i, Match& m) {
    const unsigned char c = static_cast<unsigned char>(in[i]);
    if (id != nullptr) {
        for (const ScrubRoot& root : id->roots) {
            const unsigned char first = static_cast<unsigned char>(root.text[0]);
            if (!(c == first || (IsSep(c) && IsSep(first)) || (id->caseBlind && Fold(c) == Fold(first)))) continue;
            if (!PathMatches(in, n, i, root.text, id->caseBlind) || !NameEndsAt(in, n, i + root.text.size())) continue;
            if (IsSep(first) ? !PathStartsAt(in, i) : (i > 0 && NameChar(static_cast<unsigned char>(in[i - 1])))) continue;
            m.skip = root.text.size();
            m.Piece(root.replacement.c_str());
            return true;
        }
    }
    if (MatchGeneric(in, n, i, m)) return true;
    if (id != nullptr) {
        if (i == 0 || !NameChar(static_cast<unsigned char>(in[i - 1]))) {
            for (const ScrubName& host : id->hosts) {
                const unsigned char first = static_cast<unsigned char>(host.text[0]);
                if (!(c == first || (host.blind && Fold(c) == Fold(first)))) continue;
                if (!PathMatches(in, n, i, host.text, host.blind) || !NameEndsAt(in, n, i + host.text.size(), true)) continue;
                m.skip = host.text.size();
                m.Piece("<computer>");
                return true;
            }
        }
        if (IsAlpha(c) && MatchUserKey(*id, in, n, i, m)) return true;
    }
    return false;
}

}  // namespace scrub_detail

// The bytes of room ScrubChunk needs in `out` before it takes another step (the longest copied prefix plus the longest piece).
inline constexpr size_t kScrubRoom = 128;

// Scrubs in[start...n) into out[0...cap), stopping when out is nearly full; *written is the bytes put in out and the answer
// is where to go on from (n when done). `cap` must be at least kScrubRoom. Allocates nothing and locks nothing, so it may
// run in a signal handler. `id` may be null: then only the rules that need no identity apply.
inline size_t ScrubChunk(const ScrubIdentity* id, const char* in, size_t n, size_t start, char* out, size_t cap, size_t* written) {
    size_t i = start, w = 0;
    while (i < n && cap - w >= kScrubRoom) {
        scrub_detail::Match m;
        if (scrub_detail::MatchAt(id, in, n, i, m)) {
            std::memcpy(out + w, in + i, m.keep);
            w += m.keep;
            std::memcpy(out + w, m.piece, m.pieceLength);
            w += m.pieceLength;
            i += m.keep + m.skip;
        } else {
            out[w++] = in[i++];
        }
    }
    *written = w;
    return i;
}

// The same for a whole text.
inline std::string Scrub(const ScrubIdentity* id, std::string_view text) {
    std::string result;
    result.reserve(text.size());
    char out[4096];
    size_t at = 0;
    while (at < text.size()) {
        size_t written = 0;
        at = ScrubChunk(id, text.data(), text.size(), at, out, sizeof out, &written);
        result.append(out, written);
    }
    return result;
}

// ScrubInputs, made ready (see ScrubIdentity).
inline ScrubIdentity MakeScrubIdentity(const ScrubInputs& inputs) {
    ScrubIdentity id;
    id.caseBlind = inputs.windows;
    const auto trimmed = [](std::string text) {
        while (text.size() > 1 && scrub_detail::IsSep(static_cast<unsigned char>(text.back()))) text.pop_back();
        return text;
    };
    const auto lower = [](std::string text) {
        for (char& ch : text) ch = static_cast<char>(scrub_detail::Fold(static_cast<unsigned char>(ch)));
        return text;
    };
    const auto addUser = [&](const std::string& name) {
        if (name.empty()) return;
        for (const ScrubName& have : id.users)
            if (lower(have.text) == lower(name)) return;
        id.users.push_back({name, inputs.windows});
    };
    for (const std::string& raw : inputs.homes) {
        const std::string home = trimmed(raw);
        if (home.size() < 3) continue;   // "/" and the like would replace every path
        id.roots.push_back({home, inputs.windows ? "%USERPROFILE%" : "~"});
        const size_t slash = home.find_last_of("/\\");
        if (slash != std::string::npos && slash + 1 < home.size()) addUser(home.substr(slash + 1));
    }
    for (const ScrubRoot& raw : inputs.roots) {
        const std::string text = trimmed(raw.text);
        if (text.size() >= 3 && !raw.replacement.empty()) id.roots.push_back({text, raw.replacement});
    }
    // Longest first, so that a folder inside the home is found before the home; one entry per spelling.
    std::stable_sort(id.roots.begin(), id.roots.end(),
                     [](const ScrubRoot& a, const ScrubRoot& b) { return a.text.size() > b.text.size(); });
    for (size_t a = 0; a < id.roots.size(); ++a)
        for (size_t b = id.roots.size(); b-- > a + 1;)
            if (lower(id.roots[a].text) == lower(id.roots[b].text)) id.roots.erase(id.roots.begin() + static_cast<long>(b));
    for (const std::string& name : inputs.users) addUser(name);
    std::stable_sort(id.users.begin(), id.users.end(),
                     [](const ScrubName& a, const ScrubName& b) { return a.text.size() > b.text.size(); });

    // A computer name as typed, without its domain ("pc.example.com": pc) and without ".local". Not a name that is a word,
    // or too short to be told from text.
    static const char* const kWords[] = {"localhost", "localdomain", "local", "host", "computer", "desktop", "laptop", "server",
                                         "macbook", "imac", "windows", "unknown", "default", "user", "admin"};
    std::vector<std::string> names;
    for (const std::string& host : inputs.hosts) {
        names.push_back(host);
        const size_t dot = host.find('.');
        if (dot != std::string::npos && dot > 0) names.push_back(host.substr(0, dot));
    }
    for (const std::string& name : names) {
        if (name.size() < 4) continue;
        const std::string low = lower(name);
        bool word = false;
        for (const char* w : kWords) word = word || low == w;
        bool have = false;
        for (const ScrubName& other : id.hosts) have = have || lower(other.text) == low;
        if (word || have) continue;
        // A short plain name could be an ordinary word in the text: it must match in the case it was given.
        const bool distinctive = name.size() >= 6;
        id.hosts.push_back({name, distinctive});
    }
    std::stable_sort(id.hosts.begin(), id.hosts.end(),
                     [](const ScrubName& a, const ScrubName& b) { return a.text.size() > b.text.size(); });
    return id;
}

// The inputs of this computer (log_scrub.cpp): the environment, the account database, the volumes, the computer's name.
ScrubInputs CollectScrubInputs();
// Builds this computer's identity the first time and makes it gScrubIdentity (never freed: a signal handler may be reading it).
// Later calls change nothing. Call it before the first log line is written.
void EnsureScrubIdentity();

}  // namespace tpw
