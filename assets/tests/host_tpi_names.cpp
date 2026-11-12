// Host tests of Theme Park Inc's shown item names (assets/tpi_names.h): the BFST string bank and BFMU character map
// decoder, the binding's shape, the join and the title case rule. Every bank and map is built here, in code, with
// made-up strings; no game data, no files, no clock.
#include <cstdio>
#include <string>
#include <vector>

#include "../tpi_names.h"

using namespace tpw::assets;

namespace {

int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (cond) {                                                     \
            g_pass++;                                                   \
        } else {                                                        \
            g_fail++;                                                   \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                                                   \
    do {                                                                                                     \
        std::string va = (a), vb = (b);                                                                      \
        if (va == vb) {                                                                                      \
            g_pass++;                                                                                        \
        } else {                                                                                             \
            g_fail++;                                                                                        \
            std::printf("FAIL %s:%d: %s: \"%s\" != \"%s\"\n", __FILE__, __LINE__, #a, va.c_str(), vb.c_str()); \
        }                                                                                                    \
    } while (0)

using Bytes = std::vector<uint8_t>;

void Put32(Bytes& d, uint32_t v) {
    for (int i = 0; i < 4; ++i) d.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

// A character map whose byte b is the character b (the map's entry b - 1), and byte 0xc9 an accented letter.
Bytes CharMap() {
    Bytes d = {'B', 'F', 'M', 'U', 0, 0};
    const uint16_t count = 0xff;
    d.push_back(count & 0xff);
    d.push_back(count >> 8);
    for (uint16_t i = 1; i <= count; ++i) {
        const uint16_t c = i == 0xc9 ? 0xe9 : i;
        d.push_back(static_cast<uint8_t>(c));
        d.push_back(static_cast<uint8_t>(c >> 8));
    }
    return d;
}

// One string's words: a text run (op 1), padded to a whole word.
void Text(Bytes& s, const std::string& text) {
    Put32(s, 1 | (static_cast<uint32_t>(text.size()) << 8));
    for (char c : text) s.push_back(static_cast<uint8_t>(c));
    while (s.size() % 4) s.push_back(0);
}

// A bank of `count` strings, each empty unless `strings` gives its words (without the end word, which is added).
Bytes Bank(uint32_t id, uint32_t count, const std::vector<std::pair<uint32_t, Bytes>>& strings) {
    std::vector<Bytes> bodies(count);
    for (const auto& [row, body] : strings) bodies[row] = body;
    Bytes d = {'B', 'F', 'S', 'T'};
    Put32(d, id);
    Put32(d, count);
    uint32_t offset = 4 * count;
    for (uint32_t i = 0; i < count; ++i) {
        Put32(d, offset);
        offset += static_cast<uint32_t>(bodies[i].size()) + 4;
    }
    for (uint32_t i = 0; i < count; ++i) {
        d.insert(d.end(), bodies[i].begin(), bodies[i].end());
        Put32(d, 0);
    }
    return d;
}

Bytes Words(const std::string& text) {
    Bytes s;
    Text(s, text);
    return s;
}

void TestDecode() {
    // Two text runs with a format and a number's flags between them (ops 3 and 4 add no text).
    Bytes mixed;
    Text(mixed, "Ab");
    Put32(mixed, 3 | (1u << 8));
    Put32(mixed, 0x12345678);
    Put32(mixed, 4 | (7u << 8));
    Text(mixed, "cde");
    Bytes variable;
    Text(variable, "x");
    Put32(variable, 2 | (5u << 8));
    Bytes accented;
    Text(accented, std::string("caf") + static_cast<char>(0xc9));
    Bytes unknown;
    Put32(unknown, 9);
    Bytes zero;
    Text(zero, std::string("a") + '\0');
    const Bytes bank = Bank(0x1234, 7, {{1, mixed}, {2, variable}, {3, accented}, {4, unknown}, {5, zero}, {6, Words("Plain")}});
    TpiStrings out;
    std::string err;
    CHECK(DecodeTpiStrings(bank, CharMap(), out, err));
    CHECK(out.id == 0x1234);
    CHECK(out.text.size() == 7 && out.usable.size() == 7);
    if (out.text.size() == 7) {
        CHECK(out.usable[0] && out.text[0].empty());
        CHECK(out.usable[1]);
        CHECK_EQ_STR(out.text[1], "Abcde");
        CHECK(!out.usable[2] && out.text[2].empty());   // a variable
        CHECK(!out.usable[3] && out.text[3].empty());   // above 0x7f
        CHECK(!out.usable[4]);                          // an op the reader stops at
        CHECK(!out.usable[5]);                          // a byte the map has no entry for
        CHECK(out.usable[6]);
        CHECK_EQ_STR(out.text[6], "Plain");
    }
    // Decoding twice gives the same.
    TpiStrings again;
    CHECK(DecodeTpiStrings(bank, CharMap(), again, err));
    CHECK(again.text == out.text && again.usable == out.usable);

    // Files of the wrong form.
    Bytes badMap = CharMap();
    badMap[0] = 'X';
    CHECK(!DecodeTpiStrings(bank, badMap, out, err));
    Bytes shortMap = CharMap();
    shortMap.resize(100);
    CHECK(!DecodeTpiStrings(bank, shortMap, out, err));
    Bytes badBank = bank;
    badBank[3] = 'X';
    CHECK(!DecodeTpiStrings(badBank, CharMap(), out, err));
    Bytes cut = bank;
    cut.resize(20);
    CHECK(!DecodeTpiStrings(cut, CharMap(), out, err));
    // A string whose text run says it is longer than the file is unusable, not read past the end.
    Bytes runaway = Bank(1, 1, {});
    runaway.resize(runaway.size() - 4);
    Put32(runaway, 1 | (1000u << 8));
    CHECK(DecodeTpiStrings(runaway, CharMap(), out, err));
    CHECK(out.usable.size() == 1 && !out.usable[0]);
    // A string without its end word is unusable.
    Bytes open = Bank(1, 1, {});
    open.resize(open.size() - 4);
    Text(open, "abc");
    CHECK(DecodeTpiStrings(open, CharMap(), out, err));
    CHECK(out.usable.size() == 1 && !out.usable[0]);
}

void TestShape() {
    TpiStrings s;
    std::string why;
    s.id = kTpiObjectNamesId;
    s.text.resize(kTpiObjectNamesCount);
    s.usable.assign(kTpiObjectNamesCount, true);
    CHECK(IsTpiObjectNames(s, why));
    s.id = kTpiObjectNamesId + 1;
    CHECK(!IsTpiObjectNames(s, why));
    CHECK(!why.empty());
    s.id = kTpiObjectNamesId;
    s.text.resize(kTpiObjectNamesCount - 1);
    CHECK(!IsTpiObjectNames(s, why));
}

void TestBinding() {
    // Every row names strings inside the bank, a first string that is not the bank's empty first one, and a second
    // one only after the first.
    int rows = 0;
    bool inside = true;
    for (const char* world : {"science", "water", "arabian"}) {
        for (int32_t id = 0; id < 10000; ++id) {
            uint32_t first = 0, second = 0;
            if (!TpiNameRows(world, id, first, second)) continue;
            ++rows;
            inside = inside && first > 0 && first < kTpiObjectNamesCount && second < kTpiObjectNamesCount &&
                     (second == 0 || second > first);
        }
    }
    CHECK(inside);
    CHECK(rows > 150);
    uint32_t first = 0, second = 0;
    CHECK(!TpiNameRows("science", 1601, first, second));   // the gate is named after its world
    CHECK(!TpiNameRows("jungle", 1150, first, second));    // only Theme Park Inc's worlds
    CHECK(!TpiNameRows("", 1150, first, second));
    // The same id in two worlds is two items.
    uint32_t a1 = 0, b1 = 0, a2 = 0, b2 = 0;
    CHECK(TpiNameRows("science", 3417, a1, b1));
    CHECK(TpiNameRows("arabian", 3417, a2, b2));
    CHECK(a1 != a2);
}

void TestJoin() {
    CHECK_EQ_STR(JoinTpiName("Made", "Up"), "Made Up");
    CHECK_EQ_STR(JoinTpiName("Solo", ""), "Solo");
    CHECK_EQ_STR(JoinTpiName("Two Words", "And More"), "Two Words And More");
    CHECK_EQ_STR(JoinTpiName("Hyph-", "en"), "Hyphen");
    CHECK_EQ_STR(JoinTpiName("Hyph-", ""), "Hyph");
    CHECK_EQ_STR(JoinTpiName("Not - ", "Joined"), "Not -  Joined");
    CHECK_EQ_STR(JoinTpiName("", ""), "");
}

void TestTitleCase() {
    CHECK_EQ_STR(TpiTitleCase("pumps"), "Pumps");
    CHECK_EQ_STR(TpiTitleCase("TV show"), "TV Show");
    CHECK_EQ_STR(TpiTitleCase("big TV show"), "Big TV Show");
    CHECK_EQ_STR(TpiTitleCase("Big SLide"), "Big Slide");
    CHECK_EQ_STR(TpiTitleCase("SLIDE"), "SLIDE");
    CHECK_EQ_STR(TpiTitleCase("sam's drop"), "Sam's Drop");
    CHECK_EQ_STR(TpiTitleCase("pick-a-card"), "Pick-a-card");
    CHECK_EQ_STR(TpiTitleCase("MegaWheel"), "MegaWheel");
    CHECK_EQ_STR(TpiTitleCase("x"), "X");
    CHECK_EQ_STR(TpiTitleCase("A"), "A");
    CHECK_EQ_STR(TpiTitleCase("ab  cd "), "Ab  Cd ");
    CHECK_EQ_STR(TpiTitleCase(" lead"), " Lead");
    CHECK_EQ_STR(TpiTitleCase("2nd go"), "2nd Go");
    CHECK_EQ_STR(TpiTitleCase(""), "");
    CHECK_EQ_STR(TpiTitleCase("Already Right"), "Already Right");
    // The rule is settled: title case of its own output is that output.
    for (const char* s : {"pumps", "TV show", "Big SLide", "sam's drop", "ab  cd "})
        CHECK_EQ_STR(TpiTitleCase(TpiTitleCase(s)), TpiTitleCase(s));
}

void TestShownName() {
    // A made-up bank of the binding's shape: the science world's 1150 is strings 4 and 5, its 1108 string 14 alone, its
    // 1502 strings 58 and 59, its 1100 strings 20 and 21, its 1101 strings 22 and 23, its 1104 strings 24 and 25.
    Bytes accented;
    Text(accented, std::string("Caf") + static_cast<char>(0xc9));
    const Bytes bank = Bank(kTpiObjectNamesId, kTpiObjectNamesCount,
                            {{4, Words("Made Up")}, {5, Words("Name")}, {14, Words("Solo")}, {58, Words("Synth-")},
                             {59, Words("Ride")}, {20, accented}, {21, Words("Bar")}, {23, Words("Orphan")},
                             {24, Words("Quote\"d")}, {25, Words("Name")}});
    TpiStrings names;
    std::string err, why;
    CHECK(DecodeTpiStrings(bank, CharMap(), names, err));
    CHECK(IsTpiObjectNames(names, why));
    bool fromBank = false;
    CHECK_EQ_STR(TpiShownName(&names, "science", 1150, "made up name", fromBank), "Made Up Name");
    CHECK(fromBank);
    CHECK_EQ_STR(TpiShownName(&names, "science", 1108, "x", fromBank), "Solo");
    CHECK(fromBank);
    CHECK_EQ_STR(TpiShownName(&names, "science", 1502, "x", fromBank), "SynthRide");
    CHECK(fromBank);
    // A string with a character above 0x7f: the working name in title case.
    CHECK_EQ_STR(TpiShownName(&names, "science", 1100, "pumps", fromBank), "Pumps");
    CHECK(!fromBank);
    // An empty first string, though the second has text.
    CHECK_EQ_STR(TpiShownName(&names, "science", 1101, "metal swing", fromBank), "Metal Swing");
    CHECK(!fromBank);
    // A quote would end the .sam string early.
    CHECK_EQ_STR(TpiShownName(&names, "science", 1104, "TV show", fromBank), "TV Show");
    CHECK(!fromBank);
    // No row (the same id in another world, or an id the binding does not have).
    CHECK_EQ_STR(TpiShownName(&names, "water", 1150, "crab laps", fromBank), "Crab Laps");
    CHECK(!fromBank);
    CHECK_EQ_STR(TpiShownName(&names, "science", 9999, "TV show", fromBank), "TV Show");
    CHECK(!fromBank);
    // No bank.
    CHECK_EQ_STR(TpiShownName(nullptr, "science", 1150, "TV show", fromBank), "TV Show");
    CHECK(!fromBank);
    // A bank shorter than the row: the working name.
    const Bytes small = Bank(kTpiObjectNamesId, 10, {{4, Words("Made Up")}, {5, Words("Name")}});
    TpiStrings few;
    CHECK(DecodeTpiStrings(small, CharMap(), few, err));
    CHECK(!IsTpiObjectNames(few, why));
    CHECK_EQ_STR(TpiShownName(&few, "science", 1185, "space coaster", fromBank), "Space Coaster");
    CHECK(!fromBank);
}

}  // namespace

int main() {
    TestDecode();
    TestShape();
    TestBinding();
    TestJoin();
    TestTitleCase();
    TestShownName();
    std::printf("tpi_names: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
