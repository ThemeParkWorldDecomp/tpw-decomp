// Host tests of the Theme Park Inc converter (assets/tpi_convert.h and its helpers tpi_wad.h, tpi_fsh.h, tpi_sam.h,
// tpi_sfx.h).
// Every fixture is built here, in code: a wad, a SHPI texture, .sam texts with a made-up description table, and a small
// Data tree with three worlds; no game data. With TPW_TPI_DATA (a folder holding a Theme Park Inc Data folder),
// TPW_TPI_TABLES (a folder of <file>.bin description tables, 0x3c bytes a row), TPW_TPW_ASSETS (the assets/ folder of
// a Theme Park World data folder, for the donor items) and TPW_TEST_SCRATCH set, the real conversion runs as well and
// its summary is printed.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../../platform/path_text.h"
#include "../sam_check.h"
#include "../source.h"
#include "../tpi_convert.h"
#include "../tpi_disc.h"
#include "../tpi_fsh.h"
#include "../tpi_island_md2.h"
#include "../tpi_sam.h"
#include "../tpi_sfx.h"
#include "../tpi_wad.h"

using namespace tpw::assets;
namespace fs = std::filesystem;

namespace {

int g_fail = 0, g_pass = 0, g_skip = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (cond) {                                                                    \
            g_pass++;                                                                  \
        } else {                                                                       \
            g_fail++;                                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
        }                                                                              \
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

void Skip(const std::string& what) {
    g_skip++;
    std::printf("SKIP %s\n", what.c_str());
}

std::string Env(const char* name) {
    const char* v = std::getenv(name);
    return v ? v : "";
}

using Bytes = std::vector<uint8_t>;

Bytes B(const std::string& s) { return Bytes(s.begin(), s.end()); }
std::string S(const Bytes& b) { return std::string(b.begin(), b.end()); }

uint32_t Get32(const Bytes& d, size_t at) {
    return d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | (static_cast<uint32_t>(d[at + 3]) << 24);
}

void Put32(Bytes& d, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) d[at + i] = static_cast<uint8_t>(v >> (8 * i));
}

// ---- wads

void TestWadHash() {
    CHECK(TpiWadNameHash("A") == 0x41u);
    CHECK(TpiWadNameHash("a") == 0x41u);                     // case folded
    CHECK(TpiWadNameHash("AB") == 0x41u + 0x42u);            // leftovers added plainly
    CHECK(TpiWadNameHash("abcd") == 65u + (66u << 1) + (67u << 2) + (68u << 3));
    CHECK(TpiWadNameHash("abcde") == 65u + (66u << 1) + (67u << 2) + (68u << 3) + 69u);
    CHECK(TpiWadNameHash("dir\\x.fsh") != TpiWadNameHash("x.fsh"));
}

void TestWadRoundTrip() {
    std::vector<TpiWadMember> in = {
        {"dir/b.fsh", B("BBBB")}, {"a.txt", B("A")}, {"dir/c.fsh", B("CC")}, {"other/qickload.txt", B("x\n")},
        {"dir/qickload.txt", B("y\n")}, {"z.dat", {}}};
    const Bytes wad = BuildTpiWad(in);
    CHECK(wad.size() >= 0x58 + 6 * 40);
    CHECK(std::memcmp(wad.data(), "DWFB", 4) == 0);
    CHECK(Get32(wad, 4) == 2);
    CHECK(Get32(wad, 0x48) == 6);
    // The root's members come first, then each folder together; the name block gives the folder on a run's first name.
    const uint32_t nameOff = Get32(wad, 0x4c), nameLen = Get32(wad, 0x50);
    const std::string names(reinterpret_cast<const char*>(wad.data() + nameOff), nameLen);
    std::vector<std::string> list;
    for (size_t p = 0; p < names.size();) {
        list.push_back(names.c_str() + p);
        p += list.back().size() + 1;
    }
    CHECK(list.size() == 6);
    if (list.size() == 6) {
        CHECK_EQ_STR(list[0], "a.txt");
        CHECK_EQ_STR(list[1], "z.dat");
        CHECK_EQ_STR(list[2], "dir\\b.fsh");
        CHECK_EQ_STR(list[3], "c.fsh");
        CHECK_EQ_STR(list[4], "qickload.txt");
        CHECK_EQ_STR(list[5], "other\\qickload.txt");
    }
    // Each entry's hash is of its full path.
    CHECK(Get32(wad, 0x58 + 3 * 40) == TpiWadNameHash("dir\\c.fsh"));
    CHECK(Get32(wad, 0x58 + 4 * 40) == TpiWadNameHash("dir\\qickload.txt"));
    CHECK(Get32(wad, 0x58 + 0 * 40) == TpiWadNameHash("a.txt"));
    // Read back with the full names.
    std::vector<TpiWadMember> out;
    std::string err;
    CHECK(ReadTpiWad(wad, out, err));
    CHECK(out.size() == 6);
    if (out.size() == 6) {
        CHECK_EQ_STR(out[0].name, "a.txt");
        CHECK_EQ_STR(out[1].name, "z.dat");
        CHECK_EQ_STR(out[2].name, "dir/b.fsh");
        CHECK_EQ_STR(out[3].name, "dir/c.fsh");
        CHECK_EQ_STR(out[4].name, "dir/qickload.txt");
        CHECK_EQ_STR(out[5].name, "other/qickload.txt");
        CHECK_EQ_STR(S(out[3].data), "CC");
        CHECK_EQ_STR(S(out[4].data), "y\n");
        CHECK(out[1].data.empty());
    }
    // The importer's own wad reader opens it too (it lists the names as the block holds them).
    auto source = OpenWad(MakeMemoryBlob(wad), err);
    CHECK(source != nullptr);
    if (source) {
        CHECK(source->Entries().size() == 6);
        for (const Entry& e : source->Entries()) {
            if (e.path == "dir/b.fsh") {
                auto stream = source->Open(e, err);
                Bytes data;
                CHECK(stream && ReadAll(*stream, 1 << 20, data, err));
                CHECK_EQ_STR(S(data), "BBBB");
            }
        }
    }
    // Damage is refused, not read past the end.
    Bytes cut(wad.begin(), wad.begin() + 0x58 + 2 * 40);
    CHECK(!ReadTpiWad(cut, out, err));
    CHECK(!ReadTpiWad(B("not a wad at all, not even close to one"), out, err));
    CHECK(!IsTpiWad(B("DWF")));
    // An empty wad is a wad.
    const Bytes empty = BuildTpiWad({});
    CHECK(ReadTpiWad(empty, out, err) && out.empty());
}

// ---- textures

// A SHPI file with one 8-bit image (code 0x7b, or 0xfb with a hand-made RefPack stream of literals) and a palette of
// `paletteCode` (0x24 24-bit, 0x2a 32-bit, 0x2d 16-bit), the palette entries as given (BGRA; 0x24 drops A and stores R, G,
// B as the game's files do, 0x2d packs 1555), followed by a text attachment, as the game's files are laid out.
Bytes MakeFsh(uint32_t w, uint32_t h, const Bytes& pixels, uint8_t paletteCode, const std::vector<std::array<uint8_t, 4>>& palette,
              bool packed) {
    Bytes body;
    if (packed) {
        // RefPack: 10 FB, the unpacked size (3 bytes, big-endian), literal runs of 4n+4 bytes (E0 + n - 1), a stop code
        // FC + the number of trailing literals (0 to 3).
        body = {0x10, 0xfb, static_cast<uint8_t>(pixels.size() >> 16), static_cast<uint8_t>(pixels.size() >> 8),
                static_cast<uint8_t>(pixels.size())};
        size_t p = 0;
        while (pixels.size() - p >= 4) {
            size_t run = std::min<size_t>((pixels.size() - p) / 4 * 4, 112);
            body.push_back(static_cast<uint8_t>(0xe0 + run / 4 - 1));
            body.insert(body.end(), pixels.begin() + static_cast<ptrdiff_t>(p), pixels.begin() + static_cast<ptrdiff_t>(p + run));
            p += run;
        }
        body.push_back(static_cast<uint8_t>(0xfc + (pixels.size() - p)));
        body.insert(body.end(), pixels.begin() + static_cast<ptrdiff_t>(p), pixels.end());
    } else {
        body = pixels;
    }
    Bytes entry(16, 0);
    entry[0] = packed ? 0xfb : 0x7b;
    const uint32_t blockSize = 16 + static_cast<uint32_t>(body.size());
    entry[1] = static_cast<uint8_t>(blockSize);
    entry[2] = static_cast<uint8_t>(blockSize >> 8);
    entry[3] = static_cast<uint8_t>(blockSize >> 16);
    entry[4] = static_cast<uint8_t>(w);
    entry[5] = static_cast<uint8_t>(w >> 8);
    entry[6] = static_cast<uint8_t>(h);
    entry[7] = static_cast<uint8_t>(h >> 8);
    entry.insert(entry.end(), body.begin(), body.end());
    Bytes pal(16, 0);
    const size_t per = paletteCode == 0x24 ? 3 : paletteCode == 0x2a ? 4 : 2;
    const uint32_t palSize = 16 + static_cast<uint32_t>(palette.size() * per);
    pal[0] = paletteCode;
    pal[1] = static_cast<uint8_t>(palSize);
    pal[2] = static_cast<uint8_t>(palSize >> 8);
    pal[3] = static_cast<uint8_t>(palSize >> 16);
    pal[4] = static_cast<uint8_t>(palette.size());
    pal[5] = static_cast<uint8_t>(palette.size() >> 8);
    pal[6] = 1;
    for (const auto& c : palette) {
        if (paletteCode == 0x2d) {
            const uint32_t v = ((c[3] ? 1u : 0u) << 15) | ((c[2] >> 3) << 10) | ((c[1] >> 3) << 5) | (c[0] >> 3);
            pal.push_back(static_cast<uint8_t>(v));
            pal.push_back(static_cast<uint8_t>(v >> 8));
        } else if (paletteCode == 0x24) {
            pal.push_back(c[2]);
            pal.push_back(c[1]);
            pal.push_back(c[0]);
        } else {
            pal.insert(pal.end(), c.begin(), c.begin() + static_cast<ptrdiff_t>(per));
        }
    }
    const std::string text = "TPW test";
    Bytes attach(16, 0);
    attach[0] = 0x70;
    const uint32_t attachSize = 16 + static_cast<uint32_t>(text.size()) + 1;
    attach[1] = static_cast<uint8_t>(attachSize);
    attach[2] = static_cast<uint8_t>(attachSize >> 8);
    attach.insert(attach.end(), text.begin(), text.end());
    attach.push_back(0);
    Bytes fsh(16 + 8, 0);
    std::memcpy(fsh.data(), "SHPI", 4);
    Put32(fsh, 8, 1);
    std::memcpy(fsh.data() + 12, "G231", 4);
    std::memcpy(fsh.data() + 16, "test", 4);
    Put32(fsh, 20, 24);
    fsh.insert(fsh.end(), entry.begin(), entry.end());
    fsh.insert(fsh.end(), pal.begin(), pal.end());
    fsh.insert(fsh.end(), attach.begin(), attach.end());
    Put32(fsh, 4, static_cast<uint32_t>(fsh.size()));
    return fsh;
}

const std::vector<std::array<uint8_t, 4>> kPalette = {{10, 20, 30, 255}, {40, 50, 60, 0}, {70, 80, 90, 128}, {100, 110, 120, 255}};

void TestFsh() {
    // 2 by 2, the four palette entries in reading order (row 0 at the top).
    const Bytes pixels = {0, 1, 2, 3};
    for (const bool packed : {false, true}) {
        const Bytes fsh = MakeFsh(2, 2, pixels, 0x2a, kPalette, packed);
        CHECK(IsTpiFsh(fsh));
        TpiImage image;
        std::string err;
        CHECK(DecodeTpiFsh(fsh, image, err));
        if (!err.empty()) std::printf("  fsh: %s\n", err.c_str());
        CHECK(image.width == 2 && image.height == 2);
        CHECK(image.bgra.size() == 16);
        if (image.bgra.size() == 16) {
            CHECK(image.bgra[0] == 10 && image.bgra[1] == 20 && image.bgra[2] == 30 && image.bgra[3] == 255);
            CHECK(image.bgra[4] == 40 && image.bgra[7] == 0);
            CHECK(image.bgra[12] == 100 && image.bgra[15] == 255);
        }
        Bytes tga;
        CHECK(TpiFshToTga(fsh, tga, err));
        CHECK(tga.size() == 18 + 16);
        if (tga.size() == 34) {
            CHECK(tga[2] == 2 && tga[12] == 2 && tga[14] == 2 && tga[16] == 32 && tga[17] == 8);
            // Bottom-up: the first stored row is the image's last.
            CHECK(tga[18] == 70 && tga[19] == 80 && tga[20] == 90 && tga[21] == 128);
            CHECK(tga[22] == 100);
            CHECK(tga[26] == 10 && tga[30] == 40);
        }
    }
    // 24-bit palette: opaque.
    {
        const Bytes fsh = MakeFsh(2, 2, pixels, 0x24, kPalette, false);
        TpiImage image;
        std::string err;
        CHECK(DecodeTpiFsh(fsh, image, err));
        CHECK(image.bgra.size() == 16 && image.bgra[7] == 255 && image.bgra[4] == 40 && image.bgra[6] == 60);
    }
    // The byte order on disc: a 0x24 entry is R, G, B (a red flag's 255, 78, 0 is TGA bytes 0, 78, 255); a 0x2a entry is
    // B, G, R, A and is copied as it is.
    {
        Bytes fsh = MakeFsh(1, 1, {0}, 0x24, {{0, 0, 0, 255}}, false);
        const size_t entry = fsh.size() - (16 + 9) - 3;   // the one palette entry sits before the text attachment
        fsh[entry] = 255;
        fsh[entry + 1] = 78;
        fsh[entry + 2] = 0;
        Bytes tga;
        std::string err;
        CHECK(TpiFshToTga(fsh, tga, err) && tga.size() == 18 + 4);
        CHECK(tga.size() == 22 && tga[18] == 0 && tga[19] == 78 && tga[20] == 255 && tga[21] == 255);
        const Bytes quad = MakeFsh(1, 1, {0}, 0x2a, {{1, 163, 217, 153}}, false);
        CHECK(TpiFshToTga(quad, tga, err) && tga.size() == 22);
        CHECK(tga.size() == 22 && tga[18] == 1 && tga[19] == 163 && tga[20] == 217 && tga[21] == 153);
    }
    // 16-bit palette: 1555, the alpha bit.
    {
        const Bytes fsh = MakeFsh(2, 2, pixels, 0x2d, kPalette, false);
        TpiImage image;
        std::string err;
        CHECK(DecodeTpiFsh(fsh, image, err));
        CHECK(image.bgra.size() == 16 && image.bgra[3] == 255 && image.bgra[7] == 0);
        CHECK(image.bgra.size() == 16 && image.bgra[12] == ((100 >> 3) << 3 | (100 >> 3) >> 2));
    }
    // A big one through RefPack (several literal runs), 16 by 8.
    {
        Bytes big(128);
        for (size_t i = 0; i < big.size(); ++i) big[i] = static_cast<uint8_t>(i % 4);
        const Bytes fsh = MakeFsh(16, 8, big, 0x2a, kPalette, true);
        TpiImage image;
        std::string err;
        CHECK(DecodeTpiFsh(fsh, image, err));
        CHECK(image.width == 16 && image.height == 8 && image.bgra.size() == 512);
        CHECK(image.bgra.size() == 512 && image.bgra[4 * 127] == 100);
    }
    // A packed image whose size is not a multiple of 16 unpacks to the next multiple (3 by 3 to 16 bytes); past that,
    // the stream is refused.
    {
        Bytes padded(16, 0);
        padded[8] = 1;
        TpiImage image;
        std::string err;
        CHECK(DecodeTpiFsh(MakeFsh(3, 3, padded, 0x2a, kPalette, true), image, err));
        CHECK(image.width == 3 && image.height == 3 && image.bgra.size() == 36 && image.bgra[4 * 8] == kPalette[1][0]);
        CHECK(!DecodeTpiFsh(MakeFsh(3, 3, Bytes(32, 0), 0x2a, kPalette, true), image, err));
    }
    // Refusals: not SHPI, a cut file, an unknown code, an index past the palette (clamped, not read past the end).
    {
        TpiImage image;
        std::string err;
        CHECK(!DecodeTpiFsh(B("TGA?"), image, err));
        Bytes fsh = MakeFsh(2, 2, pixels, 0x2a, kPalette, false);
        Bytes cut(fsh.begin(), fsh.begin() + 30);
        CHECK(!DecodeTpiFsh(cut, image, err));
        Bytes odd = fsh;
        odd[24] = 0x7d;
        CHECK(!DecodeTpiFsh(odd, image, err));
        Bytes high = MakeFsh(2, 2, {0, 9, 9, 9}, 0x2a, kPalette, false);
        CHECK(DecodeTpiFsh(high, image, err) && image.bgra[4] == 100);
        Bytes noPalette = MakeFsh(2, 2, pixels, 0x24, {}, false);
        CHECK(!DecodeTpiFsh(noPalette, image, err));
    }
}

// ---- sam

SamRow Row(int32_t type, const char* name, int32_t minI = 0, int32_t maxI = 0, int32_t count = 0, uint32_t flags = 0) {
    SamRow r{};
    r.type = type;
    std::snprintf(r.name, sizeof r.name, "%s", name);
    r.minInteger = minI;
    r.maxInteger = maxI;
    r.arrayCount = count;
    r.flags = flags;
    return r;
}

// A table laid out as the game's: a group's fields above its heading (type 1), an array group between a type 2 and its
// type 3 row (name: the group, arrayCount: its capacity), blank rows between groups, type 12 at the end.
const std::vector<SamRow> kItemTable = {
    Row(0, ""),      Row(5, "Id"),       Row(10, "Name"),        Row(11, "Shape", 0, 0, 0, 1), Row(6, "WhichUIType", 0, 4),
    Row(7, "Price"), Row(1, "Info"),     Row(0, ""),             Row(2, ""),                   Row(4, "NewBonus"),
    Row(3, "Attraction", 0, 0, 3), Row(0, ""), Row(5, "UpgradesId"), Row(1, "AddOn"),        Row(0, ""),
    Row(5, "CostToEnter"), Row(1, "Keys"), Row(0, ""), Row(5, "GateObjectId"), Row(1, "ParkName"), Row(0, ""),
    Row(5, "TicketBoothAPosX"), Row(5, "TicketBoothBPosY"), Row(1, "FixedItemInfo"), Row(0, ""),
    Row(5, "Size"), Row(1, "StaffPoolInfo"), Row(0, ""), Row(5, "GoldenTicketCost"), Row(5, "InitPricePerUse"), Row(1, "UsageInfo"), Row(0, ""),
    Row(2, ""), Row(5, "CostOfUpgrade"), Row(3, "Upgrades", 0, 0, 3), Row(12, "")};

void TestSamFilter() {
    const std::string text =
        "# a comment\r\n"
        "Info.Id\t\t3112\t\tthe id\n"
        "Info.Name\t\"Magic Carpet\"\n"
        "Info.WorkingLife\t12\n"
        "Info.WhichUIType\t\t7\n"
        "Info.Price 2.5\n"
        "\n"
        "Attraction[0].NewBonus\t10\n"
        "Attraction[3].NewBonus\t1\n"
        "Buoys[0].X 1\n"
        "AddOn.UpgradesId 3200\n"
        "AddOn.UpgradeDistance 4\n"
        "Info.Shape\n"
        "---\n"
        "..Q..\n"
        ".***.\n"
        "---\n"
        "Info.Hoarding\n"
        "---\n"
        "..^..\n"
        "---\n"
        "TicketBooth[0].PosX 40\n"
        "TicketBooth[1].PosY 41\n"
        "StaffPoolInfo.Size 3\n"
        "Keys.CostToEnter 100\n"
        "ParkName.GateObjectId 3601\n"
        "garbage line without a dot\n"
        "Info.Id 9\n";
    std::map<int32_t, int32_t> ids = {{3112, 5000}, {3601, 5010}};
    TpiSamOptions o;
    o.rename = {{"TicketBooth[0].PosX", "FixedItemInfo.TicketBoothAPosX"}, {"TicketBooth[1].PosY", "FixedItemInfo.TicketBoothBPosY"}};
    o.drop = {"StaffPoolInfo.*"};
    o.idFields = {"Info.Id", "AddOn.UpgradesId", "ParkName.GateObjectId"};
    o.ids = &ids;
    o.append = {"Keys.CostToEnter\t0"};
    std::string out, err;
    TpiSamReport report;
    CHECK(FilterTpiSam(kItemTable, text, o, out, report, err));
    const std::string expected =
        "# a comment\n"
        "Info.Id\t\t5000\t\tthe id\n"
        "Info.Name\t\"Magic Carpet\"\n"
        "Info.Price 2.5\n"
        "\n"
        "Attraction[0].NewBonus\t10\n"
        "AddOn.UpgradesId 3200\n"
        "Info.Shape\n"
        "---\n"
        "..Q..\n"
        ".***.\n"
        "---\n"
        "FixedItemInfo.TicketBoothAPosX 40\n"
        "FixedItemInfo.TicketBoothBPosY 41\n"
        "ParkName.GateObjectId 5010\n"
        "Info.Id 9\n"
        "\n# Written by the Theme Park Inc converter.\n"
        "Keys.CostToEnter\t0\n";
    CHECK_EQ_STR(out, expected);
    CHECK(report.kept == 11);
    std::string dropped;
    for (const auto& d : report.dropped) dropped += d + ";";
    CHECK_EQ_STR(dropped,
                 "Info.WorkingLife;Info.WhichUIType (value);Attraction[3].NewBonus;Buoys[0].X;AddOn.UpgradeDistance;"
                 "Info.Hoarding;StaffPoolInfo.Size;Keys.CostToEnter;(unreadable) garbage line without a dot;");
    CHECK(report.unmappedIds.size() == 2);
    if (report.unmappedIds.size() == 2) {
        CHECK_EQ_STR(report.unmappedIds[0], "AddOn.UpgradesId 3200");
        CHECK_EQ_STR(report.unmappedIds[1], "Info.Id 9");
    }
    // The result reads to its end with the game's parser rules.
    CHECK(SamUnreadableLine(kItemTable, out) == 0);
    CHECK(SamUnreadableLine(kItemTable, text) != 0);
    // A map with a symbol outside the set goes; an unclosed map goes to the end.
    {
        TpiSamOptions none;
        std::string out2;
        TpiSamReport r2;
        CHECK(FilterTpiSam(kItemTable, "Info.Shape\n---\n..Z..\n---\nInfo.Id 1\n", none, out2, r2, err));
        CHECK_EQ_STR(out2, "Info.Id 1\n");
        CHECK(FilterTpiSam(kItemTable, "Info.Id 2\nInfo.Shape\n---\n.....\n", none, out2, r2, err));
        CHECK_EQ_STR(out2, "Info.Id 2\n");
    }
    // No table: an error, never a file.
    {
        std::string out3;
        TpiSamReport r3;
        CHECK(!FilterTpiSam({}, "Info.Id 1\n", TpiSamOptions{}, out3, r3, err));
        CHECK(!err.empty());
    }
    // Readers.
    int32_t v = 0;
    CHECK(TpiSamInteger(text, "Info", "Id", v) && v == 3112);
    CHECK(TpiSamInteger(text, "AddOn", "UpgradesId", v) && v == 3200);
    CHECK(!TpiSamInteger(text, "Info", "Missing", v));
    std::string s;
    CHECK(TpiSamString(text, "Info", "Name", s) && s == "Magic Carpet");
    CHECK(!TpiSamString(text, "Info", "Id", s));
}

// ---- a Data tree

void WriteFile(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

Bytes ReadFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), {});
}

// An uncompressed 8-bit grey TGA, 2 by 1, levels 10 and 200, top-left origin, with a one-byte id field.
const Bytes kGreyTga = {1, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 8, 0x20, 'i', 10, 200};

// A 2 by 2 texture whose every texel is opaque (a 0x24 palette: R, G, B): it is written as a 24-bit TGA.
const Bytes kOpaqueFsh = MakeFsh(2, 2, {0, 3, 3, 0}, 0x24, kPalette, false);

Bytes Item(const std::string& name, int32_t id, const std::string& extra = "", const Bytes& mesh = B("mesh")) {
    const Bytes fsh = MakeFsh(2, 2, {0, 1, 2, 3}, 0x2a, kPalette, true);
    return BuildTpiWad({{name + ".sam", B("Info.Id\t" + std::to_string(id) + "\nInfo.Name\t\"" + name + "\"\nInfo.WorkingLife 3\n" + extra)},
                        {name + ".MD2", mesh},
                        {"flag.fsh", fsh},
                        {"flag.tga", B("old tga")},
                        {"wood.tga", B("plain tga")},
                        {"stexture/grey.tga", kGreyTga},
                        {"qickload.txt", B("flag.fsh\r\nwood.tga\r\n")},
                        {"textures/" + name + "t.fsh", fsh},
                        {"textures/qickload.txt", B(name + "t.fsh\nsign1.tga\n")},
                        {"textures/blink.ifl", B("x.tga\r\n" + name + "t.fsh\r\n" + name + "t.fsh\r\n")},
                        {"textures/" + name + "m.MD2", B("mesh")},
                        {"stexture/" + name + "t.fsh", fsh}});
}

// The gate's mesh is a real one (a box 40 wide, 30 tall): the island builder (tpi_island.h) reads it for the lobby's
// island, which the converter builds together with the world.
Bytes GateMesh() {
    namespace md2 = tpw::assets::tpi_island;
    md2::Md2Model gate;
    gate.name = "Gates.MD2";
    md2::Md2Texture flag;
    flag.frames.push_back("flag.tga");
    gate.textures.push_back(flag);
    md2::NodeBuilder b;
    b.Box(md2::Vec3{-20, 0, -5}, md2::Vec3{20, 30, 5}, 4.0f, 0);
    gate.nodes.push_back(b.Build("root"));
    md2::RecomputeBoxes(gate);
    return md2::WriteMd2(gate);
}

// A terrain mesh with a 2 x 1 height field of shown quads, flat at height 0, and a lake obstacle whose mesh lies on
// quad (1,0), which its shape blocks: the converter merges the lake into the terrain (tpi_terrain.h) and hides the quad.
Bytes TerrainMesh() {
    namespace md2 = tpw::assets::tpi_island;
    md2::Md2Model t;
    t.name = "base.MD2";
    t.flags = 3;
    md2::Md2Texture grass;
    grass.frames.push_back("grass.tga");
    t.textures.push_back(grass);
    md2::NodeBuilder b;
    b.Quad(md2::Vec3{0, 0, 0}, md2::Vec3{10, 0, 0}, md2::Vec3{10, 0, 10}, md2::Vec3{0, 0, 10}, 0, 0, 1, 0, 1, 1, 0, 1, 0);
    t.nodes.push_back(b.Build("root"));
    md2::RecomputeBoxes(t);
    t.heightField.assign(0x30, 0);
    Put32(t.heightField, 4, 6 | (2u << 16));
    const float cell = 10.0f;
    std::memcpy(t.heightField.data() + 0x10, &cell, 4);
    std::memcpy(t.heightField.data() + 0x14, &cell, 4);
    Put32(t.heightField, 0x18, 2);
    Put32(t.heightField, 0x1c, 1);
    t.heightField.resize(0x30 + 6 * 4, 0);
    for (int q = 0; q < 2; ++q) t.heightField.insert(t.heightField.end(), {0x42, 0, 0, 0});
    return md2::WriteMd2(t);
}

Bytes LakeObstacle() {
    namespace md2 = tpw::assets::tpi_island;
    md2::Md2Model lake;
    lake.name = "lake.MD2";
    md2::Md2Texture water;
    water.frames.push_back("water.tga");
    lake.textures.push_back(water);
    md2::NodeBuilder b;
    b.Quad(md2::Vec3{10, 0, 0}, md2::Vec3{20, 0, 0}, md2::Vec3{20, 0, 10}, md2::Vec3{10, 0, 10}, 0, 0, 1, 0, 1, 1, 0, 1, 0);
    lake.nodes.push_back(b.Build("lake"));
    md2::RecomputeBoxes(lake);
    const Bytes fsh = MakeFsh(2, 2, {0, 1, 2, 3}, 0x2a, kPalette, true);
    return BuildTpiWad({{"lake.sam", B("Info.Id 1700\r\nInfo.Shape\r\n---\r\nH\r\n---\r\nInfo.IsObstacle 1\r\n"
                                       "Obstacle.ShapeOffsetX 1\r\nObstacle.ShapeOffsetY 0\r\n")},
                        {"lake.MD2", md2::WriteMd2(lake)},
                        {"textures/water.fsh", fsh}});
}

void MakeWorld(const fs::path& levels, const std::string& world, const std::string& tct) {
    const fs::path w = levels / world;
    WriteFile(w / "Standard.sam", B("MapInfo.HeightfieldWidth 96\nTicketBooth[0].PosX 40\nTicketBooth[1].PosY 41\nNumTicketBooths 2\n"));
    WriteFile(w / "global.sam", B("#Keys.CostToEnter 0\nParkInfo.AvailableFromStart 0\nParkName.GateObjectId 3601\nStaffPoolInfo.Size 3\n"));
    WriteFile(w / "Hoardings.sam", B("HoardingClicks[0].X 0\nHoardingClicks[0].Y 0\n"));
    WriteFile(w / "EndHoardings.sam", B("HoardingClicks[0].X 2\n"));
    WriteFile(w / "cem_Hoardings.sam", B("HoardingClicks[0].X 4\n"));
    WriteFile(w / "Online_Standard.sam", B("Online.X 1\n"));
    const Bytes fsh = MakeFsh(2, 2, {3, 2, 1, 0}, 0x24, kPalette, false);
    std::vector<TpiWadMember> terrain = {{tct, B("tct")}, {"base.map", B("map")}, {"qickload.txt", B("pathtex\\p.fsh\n")},
                                         {"pathtex/p.fsh", MakeFsh(2, 2, {0, 1, 2, 3}, 0x2a, kPalette, false)},
                                         {"pathtex/q.fsh", kOpaqueFsh}};
    if (world == "water") {
        terrain.push_back({"base.MD2", TerrainMesh()});
        WriteFile(w / "obstacles" / "lake.wad", LakeObstacle());
    }
    WriteFile(w / "terrain.wad", BuildTpiWad(terrain));
    WriteFile(w / "Sharetex.wad", BuildTpiWad({{"black.tga", B("tga")}, {"s.fsh", fsh}}));
    WriteFile(w / "Queue.wad", BuildTpiWad({{"que.hmp", B("h")}}));
    // Theme Park Inc's advisor, left out: the donor world's Theme Park World advisor is copied instead.
    WriteFile(w / "advisor.wad",
              BuildTpiWad({{"Advisor_WAT.md2", B("m")}, {"advisor_watm21.MD2", B("m")}, {"qickload.txt", B("advisor_WATm21.md2\n")},
                           {"textures/advis1.fsh", fsh}}));
    WriteFile(w / "2dmap.tga", B("2d"));
    WriteFile(w / "scape.omp", B("omp"));
    WriteFile(w / "hoardings.map", B("hm"));
    WriteFile(w / "sky" / "sky.tga", B("sky"));
    WriteFile(w / "ssky" / "sky.tga", B("ssky"));
    WriteFile(w / "Music" / "MusicHD.sdt", B("m"));
    WriteFile(w / "rides" / "Rides.sam", B("Info.WhichUIType 0\nInfo.ExpectedPrice 3\nUsageInfo.InitPricePerUse 20\n"));
    WriteFile(w / "rides" / "Online_Rides.sam", B("Info.WhichUIType 0\n"));
    WriteFile(w / "rides" / "carpet.wad", Item("carpet", 3112, "UsageInfo.GoldenTicketCost 1\nUpgrades[0].CostOfUpgrade 0\nUsageInfo.InitPricePerUse 25\n"));
    // A coaster: its coaster.sam is read with the coaster table (Info.Id would be dropped from it; Track.* kept).
    WriteFile(w / "rides" / "loop.wad",
              BuildTpiWad({{"loop.sam", B("Info.Id 3114\n")}, {"coaster.sam", B("Info.Id 7\nTrack.pcTextureFilename \"t\"\nTrack.fScrollRate 0.5\nTrack.Bogus 1\n")}}));
    WriteFile(w / "rides" / "babylon.wad", Item("babylon", 3113));
    WriteFile(w / "shops" / "Shops.sam", B("Info.WhichUIType 1\n"));
    WriteFile(w / "shops" / "burger.wad", Item("burger", 3300, "UsageInfo.InitPricePerUse 30\n"));
    WriteFile(w / "features" / "Gates.wad", Item("Gates", 3601, "", GateMesh()));
    WriteFile(w / "features" / "bus.wad", Item("bus", 1600));
    WriteFile(w / "features" / "end.wad", Item("end", 1605));
    WriteFile(w / "features" / "noid.wad", BuildTpiWad({{"noid.sam", B("Info.Name \"x\"\n")}}));
    WriteFile(w / "upgrades" / "fotobth.wad", Item("fotobth", 3700, "AddOn.UpgradesId 3112\nInfo.WhichUIType 1\nUsageInfo.InitPricePerUse 40\n"));
    WriteFile(w / "facilities" / "ftoilet.wad", Item("ftoilet", 3800));
    WriteFile(w / "facilities" / "info.wad", Item("info", 3801));
    WriteFile(w / "facilities" / "litbin.wad", Item("litbin", 3802));
    WriteFile(w / "facilities" / "Facilities.sam", B("Info.WhichUIType 5\n"));
    WriteFile(w / "obstacles" / "Rock.wad", Item("Rock", 3900));
    // An unpacked item folder is taken like a wad.
    WriteFile(w / "sideshow" / "hoopla" / "hoopla.sam", B("Info.Id 3500\n"));
    WriteFile(w / "sideshow" / "hoopla" / "textures" / "h.fsh", fsh);
}

// ---- sound event maps

// A sound of a made-up map: `samples` samples, `branches` branches, the volume bytes, the rest made up.
struct SfxSound {
    uint32_t samples;
    uint32_t branches;
    uint8_t volMin, volMax;
};
struct SfxEvent {
    uint32_t id;
    std::vector<SfxSound> sounds;
};
// A map in the layout tpi_sfx.h describes (version 1, as Theme Park Inc's): the categories, then their events, then the
// events' sounds, then each sound's samples and branches.
Bytes SfxMap(const std::vector<std::vector<SfxEvent>>& categories) {
    Bytes d(28, 0);
    for (int i = 0; i < 16; ++i) d[i] = static_cast<uint8_t>(0xa0 + i);
    Put32(d, 16, 1);
    Put32(d, 24, static_cast<uint32_t>(categories.size()));
    auto add = [&d](size_t n) { d.resize(d.size() + n, 0); return d.size() - n; };
    for (const auto& events : categories) {
        const size_t c = add(24);
        Put32(d, c, static_cast<uint32_t>(events.size()));
        d[c + 10] = 0x7f;
    }
    for (const auto& events : categories) {
        for (const SfxEvent& e : events) {
            const size_t at = add(20);
            Put32(d, at, e.id);
            Put32(d, at + 4, static_cast<uint32_t>(e.sounds.size()));
            Put32(d, at + 12, 4500);
            d[at + 16] = 6;
        }
        for (const SfxEvent& e : events) {
            for (const SfxSound& s : e.sounds) {
                const size_t at = add(42);
                Put32(d, at, 0x10000 | s.samples);   // high bits are not part of the count
                Put32(d, at + 4, s.branches);
                d[at + 12] = s.volMin;
                d[at + 13] = s.volMax;
                d[at + 16] = 0x33;
                Put32(d, at + 30, 65535);
            }
            for (const SfxSound& s : e.sounds) {
                for (uint32_t i = 0; i < s.samples; ++i) {
                    const size_t at = add(16);
                    Put32(d, at, 489 + i);
                    d[at + 12] = 10;
                }
                for (uint32_t i = 0; i < s.branches; ++i) Put32(d, add(8), i);
            }
        }
    }
    return d;
}

void TestSfxPatch() {
    const std::vector<std::vector<SfxEvent>> layout = {
        {{114, {{3, 1, 0x05, 0x05}}}, {224, {{9, 0, 0x64, 0x64}, {2, 2, 0x64, 0x64}}}},
        {},
        {{225, {{1, 0, 0x64, 0x64}}}, {226, {}}}};
    const Bytes original = SfxMap(layout);
    const TpiSfxVolumePatch patch{224, 0x64, 0x06};
    std::string reason;
    // The event's sounds, and only their two volume bytes, change.
    Bytes map = original;
    CHECK(PatchTpiSfxVolume(map, patch, reason));
    CHECK(reason.empty());
    std::vector<std::vector<SfxEvent>> want = layout;
    for (SfxSound& s : want[0][1].sounds) s.volMin = s.volMax = 0x06;
    CHECK(map == SfxMap(want));
    size_t changed = 0;
    for (size_t i = 0; i < map.size(); ++i) changed += map[i] != original[i];
    CHECK(changed == 4);
    // A second run finds the new volumes, not the expected ones, and leaves the map alone.
    const Bytes once = map;
    CHECK(!PatchTpiSfxVolume(map, patch, reason) && reason.find("volumes 6..6") != std::string::npos && map == once);
    // An event that is not there, or has no sounds.
    map = original;
    CHECK(!PatchTpiSfxVolume(map, {999, 0x64, 0x06}, reason) && reason.find("not in the map") != std::string::npos);
    CHECK(!PatchTpiSfxVolume(map, {226, 0x64, 0x06}, reason) && reason.find("no sounds") != std::string::npos);
    CHECK(map == original);
    // A copy whose volumes differ in one sound (or in one of the two bytes) is left whole.
    std::vector<std::vector<SfxEvent>> other = layout;
    other[0][1].sounds[1].volMax = 0x50;
    map = SfxMap(other);
    CHECK(!PatchTpiSfxVolume(map, patch, reason) && map == SfxMap(other));
    // An event listed twice is not guessed at.
    std::vector<std::vector<SfxEvent>> twice = layout;
    twice[1].push_back({224, {{1, 0, 0x64, 0x64}}});
    map = SfxMap(twice);
    CHECK(!PatchTpiSfxVolume(map, patch, reason) && reason.find("2 times") != std::string::npos && map == SfxMap(twice));
    // Every cut of the map, and a map with bytes after its end, fails cleanly and changes nothing.
    for (size_t n = 0; n < original.size(); ++n) {
        Bytes cut(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(n));
        const Bytes before = cut;
        CHECK(!PatchTpiSfxVolume(cut, patch, reason) && cut == before);
    }
    map = original;
    map.push_back(0);
    CHECK(!PatchTpiSfxVolume(map, patch, reason) && reason.find("1 bytes follow") != std::string::npos);
    // Counts too large for the file are refused, not wrapped: a category count, a sound count, a sample count.
    map = original;
    Put32(map, 24, 0xffffffffu);
    CHECK(!PatchTpiSfxVolume(map, patch, reason));
    map = original;
    Put32(map, 28 + 3 * 24 + 4, 0x40000000u);   // the first event's sound count
    CHECK(!PatchTpiSfxVolume(map, patch, reason));
    map = original;
    Put32(map, 28 + 3 * 24 + 2 * 20, 0xffffu);   // the first sound's sample count
    CHECK(!PatchTpiSfxVolume(map, patch, reason));
}

void RegisterTestTables() {
    const std::vector<SamRow> main = {Row(0, ""), Row(5, "HeightfieldWidth"), Row(1, "MapInfo"), Row(0, ""),
                                      Row(5, "TicketBoothAPosX"), Row(5, "TicketBoothBPosY"), Row(1, "FixedItemInfo"), Row(12, "")};
    const std::vector<SamRow> global = {Row(0, ""), Row(5, "CostToEnter"), Row(1, "Keys"), Row(0, ""),
                                        Row(5, "CanEarnTicketsInTheme"), Row(5, "CanSpendTicketsInTheme"), Row(1, "Tickets"), Row(0, ""),
                                        Row(5, "GateObjectId"), Row(1, "ParkName"), Row(12, "")};
    const std::vector<SamRow> hoardings = {Row(0, ""), Row(2, ""), Row(4, "X"), Row(4, "Y"), Row(3, "HoardingClicks", 0, 0, 20), Row(12, "")};
    const std::vector<SamRow> coaster = {Row(0, ""), Row(10, "pcTextureFilename"), Row(7, "fScrollRate"), Row(1, "Track"), Row(12, "")};
    SetSamTable("rides.sam", kItemTable.data(), kItemTable.size());
    SetSamTable("coaster.sam", coaster.data(), coaster.size());
    SetSamTable("standard.sam", main.data(), main.size());
    SetSamTable("global.sam", global.data(), global.size());
    SetSamTable("hoardings.sam", hoardings.data(), hoardings.size());
    SetSamTable("endhoardings.sam", hoardings.data(), hoardings.size());
}

// The player's Theme Park World data the converter takes the donor items from: the first edition's game folder with
// the four wads per donor world (synthetic wads, built here; the advisor spelt "Advisor.wad", so the lookup ignores case).
std::vector<uint8_t> DonorWad(const std::string& donor, const std::string& item) {
    return BuildTpiWad({{item + ".sam", B("Info.Id 100\nInfo.Name \"" + item + " of " + donor + "\"\n")},
                        {"textures\\black.wct", B("w")}});
}

// A sprite description: the signature, the data file's name, the first person flag, the rest zeros (350 bytes, as the
// originals). `tag` makes each one's bytes its own.
Bytes Esp(const std::string& tag, bool firstPerson) {
    Bytes d(350, 0);
    std::memcpy(d.data(), "ESP_FILE2.00", 12);
    std::memcpy(d.data() + 12, tag.data(), tag.size());
    d[12 + 0x100] = firstPerson ? 1 : 0;
    return d;
}

// One sprite of a group: the description, its .TPC, and its .FPC when it has a first person view.
void AddSprite(std::vector<TpiWadMember>& wad, const std::string& folder, const std::string& stem, bool firstPerson = true) {
    wad.push_back({folder + "\\" + stem + ".ESP", Esp(folder + stem, firstPerson)});
    if (firstPerson) wad.push_back({folder + "\\" + stem + ".FPC", B("fpc " + folder + stem)});
    wad.push_back({folder + "\\" + stem + ".TPC", B("tpc " + folder + stem)});
}

// The player's Theme Park World esprites.wad: generic groups and the four themes, Halloween and Fantasy without costume
// heads, as the original.
Bytes TpwSpriteWad() {
    std::vector<TpiWadMember> wad;
    AddSprite(wad, "Generic\\Kids", "SPR_K1");
    for (const char* theme : {"Fantasy", "Hallow", "Jungle", "Space"}) {
        const std::string t = theme;
        AddSprite(wad, t + "\\Costumes", "SPR_C" + t.substr(0, 1));
        AddSprite(wad, t + "\\Entertainers", "SPR_E" + t.substr(0, 1));
        if (t == "Jungle" || t == "Space") AddSprite(wad, t + "\\Costumeheads", "SPR_H" + t.substr(0, 1), false);
    }
    return BuildTpiWad(wad);
}

void MakeDonorItems(const fs::path& dataRoot, const char* game) {
    WriteFile(dataRoot / "assets" / game / "Theme Park Data" / "data" / "esprites.wad", TpwSpriteWad());
    for (const char* donor : {"jungle", "hallow", "space"}) {
        const fs::path levels = dataRoot / "assets" / game / "Theme Park Data" / "data" / "levels" / donor;
        WriteFile(levels / "rides" / "mystery.wad", DonorWad(donor, "mystery"));
        WriteFile(levels / "features" / "ground.wad", DonorWad(donor, "ground"));
        WriteFile(levels / "features" / "groundc.wad", DonorWad(donor, "groundc"));
        WriteFile(levels / "Advisor.wad", DonorWad(donor, "advisor"));
    }
}

void TestConvertTree(const fs::path& scratch) {
    const fs::path tree = scratch / "tpi-copy";
    const fs::path dataRoot = scratch / "data-root";
    std::error_code ec;
    fs::remove_all(tree, ec);
    fs::remove_all(dataRoot, ec);
    const fs::path levels = tree / "Data" / "levels";
    MakeWorld(levels, "arabian", "Arabian.tct");
    MakeWorld(levels, "water", "polar.tct");
    MakeWorld(levels, "science", "science.tct");
    WriteFile(levels / "Standard.sam", B("MapInfo.HeightfieldWidth 1\n"));
    // Build animations (an item's "c" mesh): Arabian's bus has one and no script, its end has one and a script of its
    // own, and its carpet (a ride) has one and no script. Only the bus is given the feature script.
    const auto addMembers = [&levels](const std::string& rel, const std::vector<TpiWadMember>& extra) {
        std::vector<TpiWadMember> members;
        std::string err;
        CHECK(ReadTpiWad(ReadFile(levels / "arabian" / rel), members, err));
        members.insert(members.end(), extra.begin(), extra.end());
        WriteFile(levels / "arabian" / rel, BuildTpiWad(members));
    };
    addMembers("features/bus.wad", {{"busC.MD2", B("mesh")}, {"busm1.MD2", B("mesh")}});
    addMembers("features/end.wad", {{"endc.MD2", B("mesh")}, {"end.RSE", B("its own")}});
    addMembers("rides/carpet.wad", {{"carpetc.MD2", B("mesh")}});
    // Land of Invention's ambient map with the gate's loop at volume 100 (lowered: tpi_convert.cpp kSfxPatches), the
    // same map in Polar Zone (left as it is), and another of Invention's maps (copied as it is).
    const Bytes ambient = SfxMap({{{114, {{3, 0, 0x05, 0x05}}}, {224, {{9, 0, 0x64, 0x64}}}}});
    WriteFile(levels / "science" / "sound" / "cat_ambientSFX.map", ambient);
    WriteFile(levels / "science" / "sound" / "cat_ridesSFX.map", ambient);
    WriteFile(levels / "water" / "sound" / "cat_ambientSFX.map", ambient);
    RegisterTestTables();
    MakeDonorItems(dataRoot, "eur");
    // A second edition with the donor items spelt differently; the first game folder that has them is used.
    MakeDonorItems(dataRoot, "us");
    fs::rename(dataRoot / "assets" / "us" / "Theme Park Data" / "data" / "levels" / "hallow" / "rides" / "mystery.wad",
               dataRoot / "assets" / "us" / "Theme Park Data" / "data" / "levels" / "hallow" / "rides" / "Mystery.WAD", ec);

    std::string err;
    auto source = OpenFolder(tree.string(), err);
    CHECK(source != nullptr);
    if (!source) return;
    TpiDisc disc;
    disc.edition = "Theme Park Inc";
    disc.path = tree.string();
    disc.source = std::move(source);
    disc.root = "";
    // A leftover staging folder from an interrupted run is replaced, not an obstacle.
    WriteFile(dataRoot / "assets" / ".staging" / "tpi-0" / "data" / "levels" / "arabian" / "old.txt", B("old"));
    std::string reason;
    const bool converted = ConvertTpi(disc, dataRoot, reason);
    CHECK(converted);
    if (!reason.empty()) std::printf("  convert: %s\n", reason.c_str());
    if (!converted) return;
    const fs::path out = dataRoot / "assets" / "tpi" / "data" / "levels";
    CHECK(ReadTpiConverterStamp(dataRoot) == kTpiConverterVersion);
    for (const char* world : {"arabian", "water", "science"}) CHECK(fs::is_directory(out / world / "rides" / "carpet"));
    // The lobby's islands came with the worlds, in the same swap (tpi_island.h): a descriptor and a folder per world.
    const fs::path lobby = dataRoot / "assets" / "tpi" / "data" / "lobby";
    for (const char* world : {"arabian", "water", "science"}) {
        CHECK(fs::is_regular_file(lobby / (std::string(world) + ".txt")));
        CHECK(fs::is_directory(lobby / world));
    }
    CHECK(fs::is_regular_file(lobby / "arabian" / "ara_gate.md2"));
    CHECK(fs::is_regular_file(lobby / "arabian" / "ara_isle.md2"));
    const fs::path a = out / "arabian";
    // Items: unpacked, ids moved into the block in folder then name order (bus and end keep theirs), references
    // following.
    const std::string carpet = S(ReadFile(a / "rides" / "carpet" / "carpet.sam"));
    int32_t id = 0;
    CHECK(TpiSamInteger(carpet, "Info", "Id", id) && id == 5010);
    CHECK(carpet.find("WorkingLife") == std::string::npos);
    // A golden ticket ride keeps its ticket cost and its free cash price, as in Theme Park World.
    CHECK(TpiSamInteger(carpet, "UsageInfo", "GoldenTicketCost", id) && id == 1);
    CHECK(TpiSamInteger(carpet, "Upgrades", "CostOfUpgrade", id) && id == 0);
    // Rides charge nothing per use, as Theme Park World's (a price turns every visitor away at the head of the queue):
    // not in the ride's .sam nor the rides' folder .sam. A shop and an upgrade that is a shop by its own UI type keep
    // their prices.
    CHECK(carpet.find("InitPricePerUse") == std::string::npos);
    const std::string ridesFolderSam = S(ReadFile(a / "rides" / "Rides.sam"));
    CHECK(ridesFolderSam.find("InitPricePerUse") == std::string::npos && TpiSamInteger(ridesFolderSam, "Info", "WhichUIType", id) && id == 0);
    CHECK(TpiSamInteger(S(ReadFile(a / "shops" / "burger" / "burger.sam")), "UsageInfo", "InitPricePerUse", id) && id == 30);
    CHECK(TpiSamInteger(S(ReadFile(a / "upgrades" / "fotobth" / "fotobth.sam")), "UsageInfo", "InitPricePerUse", id) && id == 40);
    // The donor items, copied whole from the player's game data: the donor world's own wads, bytes as they are.
    for (const auto& [world, donor] : {std::pair{"arabian", "jungle"}, std::pair{"water", "hallow"}, std::pair{"science", "space"}}) {
        CHECK(ReadFile(out / world / "rides" / "mystery.wad") == DonorWad(donor, "mystery"));
        CHECK(ReadFile(out / world / "features" / "ground.wad") == DonorWad(donor, "ground"));
        CHECK(ReadFile(out / world / "features" / "groundc.wad") == DonorWad(donor, "groundc"));
        CHECK(ReadFile(out / world / "advisor.wad") == DonorWad(donor, "advisor"));
    }
    CHECK(TpiSamInteger(S(ReadFile(a / "rides" / "babylon" / "babylon.sam")), "Info", "Id", id) && id == 5000);
    CHECK(TpiSamInteger(S(ReadFile(a / "shops" / "burger" / "burger.sam")), "Info", "Id", id) && id == 5030);
    CHECK(TpiSamInteger(S(ReadFile(a / "sideshow" / "hoopla" / "hoopla.sam")), "Info", "Id", id) && id == 5040);
    CHECK(TpiSamInteger(S(ReadFile(a / "features" / "Gates" / "Gates.sam")), "Info", "Id", id) && id == 5050);
    // The gate (global.sam's ParkName.GateObjectId) carries the world's name, which its board shows.
    const std::string gates = S(ReadFile(a / "features" / "Gates" / "Gates.sam"));
    CHECK(gates.find("Info.Name\t\t\t\"Arabian Nights\"") != std::string::npos && gates.find("\"Gates\"") == std::string::npos);
    CHECK(S(ReadFile(out / "water" / "features" / "Gates" / "Gates.sam")).find("\"Polar Zone\"") != std::string::npos);
    CHECK(S(ReadFile(out / "science" / "features" / "Gates" / "Gates.sam")).find("\"Land of Invention\"") != std::string::npos);
    // No object names bank in this copy: an item is named by its working name in title case (tpi_names.h).
    CHECK(S(ReadFile(a / "rides" / "babylon" / "babylon.sam")).find("Info.Name\t\t\t\"Babylon\"") != std::string::npos);
    CHECK(TpiSamInteger(S(ReadFile(a / "features" / "bus" / "bus.sam")), "Info", "Id", id) && id == 1600);
    CHECK(TpiSamInteger(S(ReadFile(a / "features" / "end" / "end.sam")), "Info", "Id", id) && id == 1605);
    // The feature script (tpi_convert.cpp FeatureScript): a feature with a build animation and no script gets the one
    // that plays it once, word by word as RSSE_InitScript reads it, the item's name as its text.
    {
        const Bytes script = ReadFile(a / "features" / "bus" / "bus.RSE");
        CHECK(script.size() == 0x58 + 4);
        if (script.size() == 0x58 + 4) {
            CHECK_EQ_STR(S(Bytes(script.begin(), script.begin() + 4)), "RSSE");
            const uint32_t header[] = {0x45535352, 0x00010f51, 0, 0, 50, 0, 0, 0};
            for (size_t i = 0; i < std::size(header); ++i) CHECK(Get32(script, i * 4) == header[i]);
            CHECK_EQ_STR(S(Bytes(script.begin() + 0x20, script.begin() + 0x30)), "Pad Pad Pad Pad ");
            const uint32_t program[] = {8, 0x80000025, 0x10000000, 0x80000011, 0, 0, 0x80000006, 0x8000001f, 0x20000005, 4};
            for (size_t i = 0; i < std::size(program); ++i) CHECK(Get32(script, 0x30 + i * 4) == program[i]);
            CHECK_EQ_STR(S(Bytes(script.begin() + 0x58, script.end())), std::string("bus", 4));
        }
    }
    CHECK_EQ_STR(S(ReadFile(a / "features" / "end" / "end.RSE")), "its own");
    CHECK(!fs::exists(a / "rides" / "carpet" / "carpet.RSE") && !fs::exists(a / "rides" / "carpet" / "carpet.rse"));
    CHECK(!fs::exists(out / "water" / "features" / "bus" / "bus.RSE"));
    const std::string fotobth = S(ReadFile(a / "upgrades" / "fotobth" / "fotobth.sam"));
    CHECK(TpiSamInteger(fotobth, "Info", "Id", id) && id == 5060);
    CHECK(TpiSamInteger(fotobth, "AddOn", "UpgradesId", id) && id == 5010);
    CHECK(TpiSamInteger(S(ReadFile(a / "features" / "ftoilet" / "ftoilet.sam")), "Info", "Id", id) && id == 5070);
    CHECK(TpiSamInteger(S(ReadFile(a / "features" / "litbin" / "litbin.sam")), "Info", "Id", id) && id == 5080);
    CHECK(TpiSamInteger(S(ReadFile(a / "rides" / "loop" / "loop.sam")), "Info", "Id", id) && id == 5020);
    const std::string track = S(ReadFile(a / "rides" / "loop" / "coaster.sam"));
    CHECK(track.find("Track.fScrollRate 0.5") != std::string::npos && track.find("Track.pcTextureFilename") != std::string::npos);
    CHECK(track.find("Bogus") == std::string::npos && track.find("Info.Id") == std::string::npos);
    CHECK(!fs::exists(a / "features" / "Facilities.sam"));
    CHECK(fs::exists(a / "shops" / "Shops.sam"));
    CHECK(!fs::exists(a / "features" / "info"));
    CHECK(!fs::exists(a / "features" / "noid"));
    CHECK(!fs::exists(a / "obstacles"));
    CHECK(!fs::exists(a / "facilities"));
    CHECK(!fs::exists(a / "rides" / "Online_Rides.sam"));
    CHECK(!fs::exists(a / "Online_Standard.sam"));
    CHECK(!fs::exists(a / "cem_Hoardings.sam"));
    const std::string ridesSam = S(ReadFile(a / "rides" / "Rides.sam"));
    CHECK(ridesSam.find("WhichUIType") != std::string::npos && ridesSam.find("ExpectedPrice") == std::string::npos);
    CHECK(fs::exists(a / "shops" / "Shops.sam"));
    // Textures: fsh to 32-bit tga, the old tga beside it gone, the plain tga kept, the lists rewritten.
    const Bytes flag = ReadFile(a / "rides" / "carpet" / "flag.tga");
    CHECK(flag.size() == 18 + 16 && flag[16] == 32);
    CHECK_EQ_STR(S(ReadFile(a / "rides" / "carpet" / "wood.tga")), "plain tga");
    CHECK_EQ_STR(S(ReadFile(a / "rides" / "carpet" / "qickload.txt")), "flag.tga\nwood.tga\n");
    CHECK(fs::exists(a / "rides" / "carpet" / "textures" / "carpett.tga"));
    CHECK(!fs::exists(a / "rides" / "carpet" / "textures" / "carpett.fsh"));
    // A name the folder does not hold (TPI keeps sign1.tga in the world's sharetex) is left out of the list.
    // An animated texture list becomes a copy of its most used frame, named in the list; a mesh there is left out.
    CHECK_EQ_STR(S(ReadFile(a / "rides" / "carpet" / "textures" / "qickload.txt")), "carpett.tga\nblink.tga\n");
    CHECK(ReadFile(a / "rides" / "carpet" / "textures" / "blink.tga") == ReadFile(a / "rides" / "carpet" / "textures" / "carpett.tga"));
    CHECK(!fs::exists(a / "rides" / "carpet" / "textures" / "blink.ifl"));
    CHECK(!fs::exists(a / "rides" / "carpet" / "textures" / "carpetm.MD2"));
    CHECK(fs::exists(a / "rides" / "carpet" / "stexture" / "carpett.tga"));
    // An 8-bit grey TGA becomes a 24-bit one (it is opaque): B = G = R, the top-left origin kept, the id field gone.
    const Bytes grey = ReadFile(a / "rides" / "carpet" / "stexture" / "grey.tga");
    const Bytes greyWant = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 24, 0x20, 10, 10, 10, 200, 200, 200};
    CHECK(grey == greyWant);
    CHECK(fs::exists(a / "sideshow" / "hoopla" / "textures" / "h.tga"));
    // World wads: rebuilt, textures converted, the terrain's .tct named after the world.
    std::vector<TpiWadMember> members;
    CHECK(ReadTpiWad(ReadFile(a / "terrain.wad"), members, err));
    std::string names;
    for (const auto& m : members) names += m.name + ";";
    CHECK_EQ_STR(names, "Arabian.tct;base.map;qickload.txt;pathtex/p.tga;pathtex/q.tga;");
    CHECK_EQ_STR(S(members[2].data), "pathtex\\p.tga\n");
    // A texture with see-through texels stays 32-bit; an opaque one becomes 24-bit, the same texels without their alpha
    // (the game's loader takes every 4-byte texel texture as one with alpha, TextureLoadManager.cpp UncompressedConvert).
    if (members.size() == 5) {
        CHECK(members[3].data.size() == 18 + 16 && members[3].data[16] == 32 && members[3].data[17] == 8);
        Bytes opaque;
        CHECK(TpiFshToTga(kOpaqueFsh, opaque, err) && opaque.size() == 18 + 16 && opaque[16] == 32);
        Bytes want(opaque.begin(), opaque.begin() + 18);
        want[16] = 24;
        want[17] = 0;
        for (size_t i = 18; i + 3 < opaque.size(); i += 4) {
            CHECK(opaque[i + 3] == 255);
            want.insert(want.end(), {opaque[i], opaque[i + 1], opaque[i + 2]});
        }
        CHECK(members[4].data == want);
        CHECK(want.size() == 18 + 12 && want[18] == 100 && want[21] == 10);
    }
    CHECK(ReadTpiWad(ReadFile(out / "water" / "terrain.wad"), members, err));
    CHECK(!members.empty() && members[0].name == "water.tct");
    // The water world's lake obstacle is in its terrain: the mesh's second node, its quad hidden and drawn again under it
    // with the shown one beside it (the last node), its texture added.
    names.clear();
    for (const auto& m : members) names += m.name + ";";
    CHECK_EQ_STR(names, "water.tct;base.map;qickload.txt;base.MD2;pathtex/p.tga;pathtex/q.tga;textures/water.tga;");
    {
        namespace md2 = tpw::assets::tpi_island;
        md2::Md2Model merged;
        CHECK(members.size() == 7 && md2::ReadMd2(members[3].data, merged, err, true));
        CHECK(merged.nodes.size() == 3 && merged.nodes[1].name == "lake" && merged.nodes[2].name == "tpi underground" &&
              merged.nodes[2].faces.size() == 4);
        CHECK(merged.heightField.size() == 0x30 + 6 * 4 + 2 * 4 && merged.heightField[0x30 + 6 * 4] == 0x42 &&
              merged.heightField[0x30 + 6 * 4 + 4] == 0x43);
        CHECK(members.size() == 7 && members[6].data.size() > 18 && members[6].data[2] == 2);
    }
    CHECK(!fs::exists(out / "water" / "obstacles"));
    CHECK(ReadTpiWad(ReadFile(a / "sharetex.wad"), members, err) && members.size() == 2 && members[1].name == "s.tga");
    CHECK(fs::exists(a / "queue.wad") && !fs::exists(a / "Queue.wad.tmp"));
    CHECK(!fs::exists(a / "dynamic.wad"));
    // Copies.
    CHECK_EQ_STR(S(ReadFile(a / "2dmap.tga")), "2d");
    CHECK_EQ_STR(S(ReadFile(a / "sky" / "sky.tga")), "sky");
    CHECK_EQ_STR(S(ReadFile(a / "ssky" / "sky.tga")), "ssky");
    CHECK_EQ_STR(S(ReadFile(a / "Music" / "MusicHD.sdt")), "m");
    CHECK_EQ_STR(S(ReadFile(a / "hoardings.map")), "hm");
    CHECK(ReadFile(out / "science" / "Sound" / "cat_ambientSFX.map") ==
          SfxMap({{{114, {{3, 0, 0x05, 0x05}}}, {224, {{9, 0, 0x06, 0x06}}}}}));
    CHECK(ReadFile(out / "science" / "Sound" / "cat_ridesSFX.map") == ambient);
    CHECK(ReadFile(out / "water" / "Sound" / "cat_ambientSFX.map") == ambient);
    // The world's .sam files.
    const std::string standard = S(ReadFile(a / "Standard.sam"));
    CHECK_EQ_STR(standard, "MapInfo.HeightfieldWidth 96\nFixedItemInfo.TicketBoothAPosX 40\nFixedItemInfo.TicketBoothBPosY 41\n");
    const std::string global = S(ReadFile(a / "global.sam"));
    CHECK(TpiSamInteger(global, "ParkName", "GateObjectId", id) && id == 5050);
    // Key costs in Theme Park Inc's own order (its missions open Land of Invention, then Polar Zone, then Arabian Nights):
    // 1, 3 and 5, each one the lobby can draw (1 to 5, never 0).
    for (const auto& [world, keys] : {std::pair{"arabian", 5}, std::pair{"water", 3}, std::pair{"science", 1}}) {
        int32_t cost = 0;
        CHECK(TpiSamInteger(S(ReadFile(out / world / "global.sam")), "Keys", "CostToEnter", cost) && cost == keys);
        CHECK(cost >= 1 && cost <= 5);
    }
    CHECK(TpiSamInteger(global, "Tickets", "CanEarnTicketsInTheme", id) && id == 1);
    CHECK(global.find("StaffPoolInfo") == std::string::npos && global.find("AvailableFromStart") == std::string::npos);
    CHECK_EQ_STR(S(ReadFile(a / "Hoardings.sam")), "HoardingClicks[0].X 0\nHoardingClicks[0].Y 0\n");
    CHECK_EQ_STR(S(ReadFile(a / "EndHoardings.sam")), "HoardingClicks[0].X 2\n");
    // The record and the stamp; staging gone.
    const std::string record = S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt"));
    CHECK(record.find("world arabian:") != std::string::npos);
    CHECK(record.find("Sound/cat_ambientSFX.map: event 224 volume 100 to 6\n") != std::string::npos);
    CHECK(record.find("left as it is") == std::string::npos);
    CHECK(record.find("left out: obstacles/ (terrain.wad has no base.MD2)") != std::string::npos);
    CHECK(record.find("obstacle lake: before clearing, 1 nodes, 2 faces in the terrain; blocked cells: 1 made hidden, 0 hidden "
                      "already, 0 left open") !=
          std::string::npos);
    CHECK(record.find("left out: facilities/info") != std::string::npos);
    CHECK(record.find("features/noid") != std::string::npos);
    CHECK(record.find("rides/carpet: Info.WorkingLife") != std::string::npos);
    CHECK(record.find("items converted: 33, left out: 6") != std::string::npos);
    CHECK(record.find("features/bus (Info.Id 1600 -> 1600, ") != std::string::npos &&
          record.find(", given a script that plays its build animation once", record.find("features/bus (")) <
              record.find("\n", record.find("features/bus (")));
    CHECK(record.find("given a script", record.find("features/end (")) > record.find("\n", record.find("features/end (")));
    CHECK(record.find("left out: facilities/Facilities.sam") != std::string::npos);
    // The golden ticket ride is listed with its ticket cost; no cash price is invented for it.
    CHECK(record.find("rides/carpet (Info.Id 3112 -> ") != std::string::npos);
    CHECK(record.find(" files, costs 1 golden ticket, no price per use (was 25), named \"Carpet\" (was \"carpet\", in title case))") !=
          std::string::npos);
    CHECK(record.find("names: the items' own working names in title case (") != std::string::npos);
    CHECK(record.find("rides/Rides.sam: no price per use (was 20)") != std::string::npos);
    CHECK(record.find("shops/burger (Info.Id 3300 -> 5030, ") != std::string::npos &&
          record.find("no price per use", record.find("shops/burger (")) > record.find("\n", record.find("shops/burger (")));
    CHECK(record.find("gate features/Gates named \"Arabian Nights\"") != std::string::npos);
    CHECK(record.find("animated texture rides/carpet/textures/blink.ifl written as blink.tga, its most used frame (carpett.tga)") != std::string::npos);
    CHECK(record.find("left out: rides/carpet/textures/carpetm.MD2 (a mesh among the textures)") != std::string::npos);
    CHECK(record.find("qickload name left out: rides/carpet/textures/qickload.txt: sign1.tga (not in this wad)") != std::string::npos);
    CHECK(record.find("priced") == std::string::npos && record.find("$") == std::string::npos);
    CHECK(record.find("rides/mystery.wad: copied from the jungle world of") != std::string::npos);
    CHECK(record.find("features/groundc.wad: copied from the space world of") != std::string::npos);
    CHECK(record.find("advisor.wad: copied from the hallow world of") != std::string::npos);
    CHECK(record.find("    left out: advisor.wad\n") != std::string::npos);
    CHECK(!fs::exists(dataRoot / "assets" / ".staging" / "tpi-0" / "data"));
    CHECK(fs::is_empty(dataRoot / "assets" / ".staging"));
    // Converting again replaces the worlds whole: a stray file in the old output goes.
    WriteFile(out / "arabian" / "stray.txt", B("x"));
    CHECK(ConvertTpi(disc, dataRoot, reason));
    CHECK(!fs::exists(out / "arabian" / "stray.txt"));
    CHECK(fs::exists(a / "rides" / "carpet" / "carpet.sam"));
    CHECK(!fs::exists(dataRoot / "assets" / "tpi" / ".tpi-previous-data"));
    // Only the second edition's game is there: its donor items are found, whatever their case.
    fs::remove_all(dataRoot / "assets" / "eur", ec);
    CHECK(ConvertTpi(disc, dataRoot, reason));
    if (!reason.empty()) std::printf("  convert (us only): %s\n", reason.c_str());
    CHECK(ReadFile(out / "water" / "rides" / "mystery.wad") == DonorWad("hallow", "mystery"));
    CHECK(S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt")).find("Mystery.WAD") != std::string::npos);
    // No Theme Park World data at all: refused with a reason that says what to do, the old output kept.
    fs::remove_all(dataRoot / "assets" / "us", ec);
    CHECK(!ConvertTpi(disc, dataRoot, reason));
    CHECK(reason.find("mystery item") != std::string::npos && reason.find("import Theme Park World first") != std::string::npos);
    CHECK(fs::exists(a / "rides" / "carpet" / "carpet.sam"));
    MakeDonorItems(dataRoot, "eur");
    // A donor world with its mystery item but no ground item: refused too.
    fs::remove(dataRoot / "assets" / "eur" / "Theme Park Data" / "data" / "levels" / "space" / "features" / "groundc.wad", ec);
    CHECK(!ConvertTpi(disc, dataRoot, reason));
    std::printf("  convert (no groundc): %s\n", reason.c_str());
    CHECK(reason.find("space world") != std::string::npos && reason.find("groundc.wad") != std::string::npos);
    MakeDonorItems(dataRoot, "eur");
    // A donor world with no advisor: refused, since nothing of Theme Park Inc's advisor stands in for it.
    fs::remove(dataRoot / "assets" / "eur" / "Theme Park Data" / "data" / "levels" / "jungle" / "Advisor.wad", ec);
    CHECK(!ConvertTpi(disc, dataRoot, reason));
    std::printf("  convert (no advisor): %s\n", reason.c_str());
    CHECK(reason.find("jungle world") != std::string::npos && reason.find("advisor.wad") != std::string::npos);
    CHECK(fs::exists(a / "rides" / "carpet" / "carpet.sam"));
    MakeDonorItems(dataRoot, "eur");
    // A copy with a world missing is refused and leaves the old output in place.
    fs::remove_all(levels / "science", ec);
    auto partial = OpenFolder(tree.string(), err);
    TpiDisc bad = disc;
    bad.source = std::move(partial);
    CHECK(!ConvertTpi(bad, dataRoot, reason));
    CHECK(reason.find("science") != std::string::npos);
    CHECK(fs::exists(a / "rides" / "carpet" / "carpet.sam"));
    CHECK(fs::is_empty(dataRoot / "assets" / ".staging"));
    // Not a Theme Park Inc copy at all.
    TpiDisc none = disc;
    none.source = OpenFolder((scratch / "data-root").string(), err);
    CHECK(!ConvertTpi(none, dataRoot, reason));
    fs::remove_all(tree, ec);
    fs::remove_all(dataRoot, ec);
}


// Theme Park Inc's esprites.wad for the sprite test: Arabian whole (one sprite spelt in lower case, its costume heads
// with no first person view), Polar Zone without costumes, with an entertainer whose first person picture is missing
// and with its costume heads in a lower-case folder, Land of Invention with a costume head whose picture is missing and
// a second entertainer whose name is the first's with a 2 (in lower case); generic and intro groups the converter
// leaves alone.
Bytes TpiSpriteWad() {
    std::vector<TpiWadMember> wad;
    AddSprite(wad, "Arabian\\Costumeheads", "SPR_AK", false);
    AddSprite(wad, "Arabian\\Costumes", "SPR_AK");
    AddSprite(wad, "Arabian\\Entertainers", "SPR_PH");
    AddSprite(wad, "Arabian\\Entertainers", "Spr_ge");
    AddSprite(wad, "Generic\\Kids", "SPR_TK");
    AddSprite(wad, "Science\\Costumes", "SPR_GK");
    wad.push_back({"Science\\Costumeheads\\SPR_GK.ESP", Esp("headless", false)});
    AddSprite(wad, "Science\\Entertainers", "SPR_BU");
    AddSprite(wad, "Science\\Entertainers", "spr_bu2", false);
    AddSprite(wad, "Water\\Entertainers", "SPR_FS");
    wad.push_back({"Water\\Entertainers\\SPR_PG.ESP", Esp("first person, no FPC", true)});
    wad.push_back({"Water\\Entertainers\\SPR_PG.TPC", B("tpc Water\\EntertainersSPR_PG")});
    AddSprite(wad, "Water\\costumeheads", "SPR_ES", false);
    AddSprite(wad, "wIntro\\Entertainers", "SPR_IN");
    return BuildTpiWad(wad);
}

// The files of one converted group, by name.
std::map<std::string, Bytes> GroupFiles(const fs::path& folder) {
    std::map<std::string, Bytes> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(folder, ec)) files[tpw::PathText(e.path().filename())] = ReadFile(e.path());
    return files;
}

bool HasEsp(const std::map<std::string, Bytes>& files) {
    for (const auto& [name, bytes] : files)
        if (name.size() > 4 && (name.substr(name.size() - 4) == ".ESP" || name.substr(name.size() - 4) == ".esp")) return true;
    return false;
}

// The entertainers, costumes and costume heads of the three worlds (the game crashed hiring an entertainer in a world
// with none): Theme Park Inc's own where whole, else the donor world's; the player's esprites.wad untouched and nothing
// else of either wad in the converted tree.
void TestSprites(const fs::path& scratch) {
    const fs::path tree = scratch / "tpi-sprites-copy";
    const fs::path dataRoot = scratch / "sprites-data-root";
    std::error_code ec;
    fs::remove_all(tree, ec);
    fs::remove_all(dataRoot, ec);
    const fs::path levels = tree / "Data" / "levels";
    for (const char* world : {"arabian", "water", "science"}) MakeWorld(levels, world, std::string(world) + ".tct");
    WriteFile(tree / "Data" / "esprites.wad", TpiSpriteWad());
    RegisterTestTables();
    MakeDonorItems(dataRoot, "eur");
    const fs::path tpwWad = dataRoot / "assets" / "eur" / "Theme Park Data" / "data" / "esprites.wad";
    const Bytes tpwBefore = ReadFile(tpwWad);
    std::string err, reason;
    TpiDisc disc;
    disc.edition = "Theme Park Inc";
    disc.path = tpw::PathText(tree);
    disc.source = OpenFolder(tpw::PathText(tree), err);
    CHECK(disc.source != nullptr);
    if (!disc.source) return;
    CHECK(ConvertTpi(disc, dataRoot, reason));
    if (!reason.empty()) std::printf("  convert (sprites): %s\n", reason.c_str());
    const fs::path sprites = dataRoot / "assets" / "tpi" / "data" / "esprites";
    // Every world has all three groups, each with at least one sprite.
    for (const char* world : {"arabian", "water", "science"})
        for (const char* group : {"Entertainers", "Costumes", "Costumeheads"}) {
            const auto files = GroupFiles(sprites / world / group);
            CHECK(HasEsp(files));
            if (!HasEsp(files)) std::printf("  no sprites in %s/%s\n", world, group);
        }
    // Theme Park Inc's groups as its wad holds them (names kept), a first person view's .FPC with them.
    const auto arabian = GroupFiles(sprites / "arabian" / "Entertainers");
    CHECK(arabian.size() == 9 && arabian.count("Spr_ge.ESP") && arabian.count("SPR_PH.FPC"));
    CHECK(arabian.count("SPR_PH.ESP") && arabian.at("SPR_PH.ESP") == Esp("Arabian\\EntertainersSPR_PH", true));
    CHECK(arabian.count("SPR_PH.TPC") && S(arabian.at("SPR_PH.TPC")) == "tpc Arabian\\EntertainersSPR_PH");
    // Three entertainers in every world, as the game takes three (the third of a world with two crashed it): the first
    // by name written again, with its picture files, under the first free name with a number; the other groups as they are.
    CHECK(arabian.count("Spr_ge2.ESP") && arabian.at("Spr_ge2.ESP") == Esp("Arabian\\EntertainersSpr_ge", true));
    CHECK(arabian.count("Spr_ge2.FPC") && S(arabian.at("Spr_ge2.FPC")) == "fpc Arabian\\EntertainersSpr_ge");
    CHECK(arabian.count("Spr_ge2.TPC") && S(arabian.at("Spr_ge2.TPC")) == "tpc Arabian\\EntertainersSpr_ge");
    const auto science = GroupFiles(sprites / "science" / "Entertainers");
    CHECK(science.size() == 8 && science.count("spr_bu2.ESP") && !science.count("SPR_BU2.ESP"));
    CHECK(science.count("SPR_BU3.ESP") && science.at("SPR_BU3.ESP") == Esp("Science\\EntertainersSPR_BU", true));
    CHECK(science.count("SPR_BU3.FPC") && science.count("SPR_BU3.TPC") &&
          S(science.at("SPR_BU3.TPC")) == "tpc Science\\EntertainersSPR_BU");
    CHECK(GroupFiles(sprites / "arabian" / "Costumes").size() == 3);
    const auto heads = GroupFiles(sprites / "arabian" / "Costumeheads");
    CHECK(heads.size() == 2 && heads.count("SPR_AK.TPC") && !heads.count("SPR_AK.FPC"));
    CHECK(GroupFiles(sprites / "water" / "Costumeheads").count("SPR_ES.ESP"));
    // Polar Zone's missing costumes and broken entertainers, and Land of Invention's broken costume heads: the donor
    // worlds' (Halloween's costumes and entertainers, Space's heads).
    const auto waterCostumes = GroupFiles(sprites / "water" / "Costumes");
    CHECK(waterCostumes.count("SPR_CH.ESP") && waterCostumes.at("SPR_CH.ESP") == Esp("Hallow\\CostumesSPR_CH", true));
    const auto waterEntertainers = GroupFiles(sprites / "water" / "Entertainers");
    CHECK(waterEntertainers.size() == 9 && waterEntertainers.count("SPR_EH.ESP") && !waterEntertainers.count("SPR_FS.ESP"));
    CHECK(waterEntertainers.count("SPR_EH2.ESP") && waterEntertainers.count("SPR_EH3.ESP") &&
          waterEntertainers.at("SPR_EH3.ESP") == Esp("Hallow\\EntertainersSPR_EH", true) &&
          waterEntertainers.count("SPR_EH3.FPC") && S(waterEntertainers.at("SPR_EH3.FPC")) == "fpc Hallow\\EntertainersSPR_EH");
    const auto scienceHeads = GroupFiles(sprites / "science" / "Costumeheads");
    CHECK(scienceHeads.size() == 2 && scienceHeads.count("SPR_HS.ESP") && !scienceHeads.count("SPR_GK.ESP"));
    // Nothing of the generic groups, the intro or the stock themes, and no esprites.wad: the stock groups the game
    // loads are the player's esprites.wad's, byte for byte.
    std::vector<std::string> top;
    for (const auto& e : fs::directory_iterator(sprites, ec)) top.push_back(tpw::PathText(e.path().filename()));
    std::sort(top.begin(), top.end());
    CHECK(top == (std::vector<std::string>{"arabian", "science", "water"}));
    CHECK(!fs::exists(dataRoot / "assets" / "tpi" / "data" / "esprites.wad"));
    CHECK(ReadFile(tpwWad) == tpwBefore);
    const std::string record = S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt"));
    CHECK(record.find("    arabian/Entertainers: 3 sprites, 9 files, from Theme Park Inc's esprites.wad; written again: Spr_ge "
                      "as Spr_ge2, as the game takes 3 entertainers\n") != std::string::npos);
    CHECK(record.find("    arabian/Costumes: 1 sprite, 3 files, from Theme Park Inc's esprites.wad\n") != std::string::npos);
    CHECK(record.find("    water/Costumes: 1 sprite, 3 files, from the hallow world's in ") != std::string::npos &&
          record.find("(Theme Park Inc's: no sprite)") != std::string::npos);
    CHECK(record.find("science/Costumeheads: 1 sprite, 2 files, from the space world's in ") != std::string::npos &&
          record.find("SPR_GK.ESP has its picture files missing") != std::string::npos);
    CHECK(record.find("water/Entertainers: 3 sprites, 9 files, from the hallow world's in ") != std::string::npos &&
          record.find("SPR_PG.ESP has its picture files missing); written again: SPR_EH as SPR_EH2, SPR_EH as SPR_EH3, "
                      "as the game takes 3 entertainers\n") != std::string::npos);
    // A copy without esprites.wad: every group is the donor's; Halloween has no costume heads, so Polar Zone has none,
    // as Theme Park World's Halloween world.
    fs::remove(tree / "Data" / "esprites.wad", ec);
    disc.source = OpenFolder(tpw::PathText(tree), err);
    CHECK(ConvertTpi(disc, dataRoot, reason));
    CHECK(GroupFiles(sprites / "arabian" / "Entertainers").count("SPR_EJ.ESP"));
    CHECK(!fs::exists(sprites / "water" / "Costumeheads"));
    CHECK(S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt"))
              .find("water/Costumeheads: none (Theme Park Inc's copy has no Data/esprites.wad; the hallow world has none either") !=
          std::string::npos);
    // Neither has entertainers for a world: refused, the old worlds kept.
    std::vector<TpiWadMember> noJungle;
    CHECK(ReadTpiWad(TpwSpriteWad(), noJungle, err));
    noJungle.erase(std::remove_if(noJungle.begin(), noJungle.end(),
                                  [](const TpiWadMember& m) { return m.name.compare(0, 20, "Jungle/Entertainers/") == 0; }),
                   noJungle.end());
    for (auto& m : noJungle) std::replace(m.name.begin(), m.name.end(), '/', '\\');
    WriteFile(tpwWad, BuildTpiWad(noJungle));
    CHECK(!ConvertTpi(disc, dataRoot, reason));
    std::printf("  convert (no entertainers): %s\n", reason.c_str());
    CHECK(reason.find("no Entertainers sprites for the arabian world") != std::string::npos &&
          reason.find("the jungle world's in ") != std::string::npos);
    CHECK(GroupFiles(sprites / "arabian" / "Entertainers").count("SPR_EJ.ESP"));
    fs::remove_all(tree, ec);
    fs::remove_all(dataRoot, ec);
}

// A user folder with a name outside ASCII ("Zoë"): the copy, the data folder and the staging folder all under it. The
// paths go through the converter as UTF-8 text and back (tpw::PathText), so the worlds land in place and the record
// names the folder as it is spelt. The text is UTF-8 in the UTF-8 code page (the launcher's, from its manifest) and on
// macOS and Linux; this test program has no manifest, so on Windows it is the ANSI code page's spelling of the name
// (0xEB for the e with diaeresis in code page 1252), which is what the checks compare with (zoeText).
void TestNonAsciiPath(const fs::path& scratch) {
    const std::string zoe = "Zo\xc3\xab";
    const std::string zoeText = tpw::PathText(fs::u8path(zoe));
    const fs::path home = scratch / fs::u8path(zoe);
    const fs::path tree = home / "TPI copy";
    const fs::path dataRoot = home / "data";
    std::error_code ec;
    fs::remove_all(home, ec);
    const fs::path levels = tree / "Data" / "levels";
    for (const char* world : {"arabian", "water", "science"}) MakeWorld(levels, world, std::string(world) + ".tct");
    WriteFile(tree / "Data" / "esprites.wad", TpiSpriteWad());
    RegisterTestTables();
    MakeDonorItems(dataRoot, "eur");
    std::string err, reason;
    TpiDisc disc;
    disc.edition = "Theme Park Inc";
    disc.path = tpw::PathText(tree);
    disc.source = OpenFolder(disc.path, err);
    CHECK(disc.source != nullptr);
    if (!disc.source) return;
    CHECK(disc.path.find(zoeText) != std::string::npos);
    CHECK(ConvertTpi(disc, dataRoot, reason));
    if (!reason.empty()) std::printf("  convert (Zo\xc3\xab): %s\n", reason.c_str());
    CHECK(ReadTpiConverterStamp(dataRoot) == kTpiConverterVersion);
    const fs::path out = dataRoot / "assets" / "tpi" / "data";
    CHECK(fs::exists(out / "levels" / "arabian" / "rides" / "carpet" / "carpet.sam"));
    CHECK(ReadFile(out / "levels" / "water" / "rides" / "mystery.wad") == DonorWad("hallow", "mystery"));
    CHECK(fs::exists(out / "esprites" / "science" / "Entertainers" / "SPR_BU.ESP"));
    const std::string record = S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt"));
    CHECK(record.find("from: " + tpw::PathText(tree) + "\n") != std::string::npos);
    CHECK(record.find("copied from the jungle world of " + tpw::PathText(dataRoot / "assets")) != std::string::npos);
    CHECK(record.find("/" + zoeText + "/") != std::string::npos || record.find("\\" + zoeText + "\\") != std::string::npos);
    CHECK(fs::is_empty(dataRoot / "assets" / ".staging"));
    fs::remove_all(home, ec);
}

// ---- the real thing, when asked

bool LoadTable(const fs::path& file, std::vector<SamRow>& rows) {
    const Bytes bytes = ReadFile(file);
    if (bytes.empty() || bytes.size() % sizeof(SamRow) != 0) return false;
    rows.resize(bytes.size() / sizeof(SamRow));
    std::memcpy(rows.data(), bytes.data(), bytes.size());
    return true;
}

// The donor items come from the player's Theme Park World data, so the real conversion's data folder links each edition
// folder of `tpwAssets` (a data folder's assets/, the "tpi" folder and hidden ones such as .staging aside) into its own
// assets/.
void LinkTpwEditions(const fs::path& tpwAssets, const fs::path& dataRoot) {
    fs::create_directories(dataRoot / "assets");
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(tpwAssets, ec)) {
        const std::string name = entry.path().filename().string();
        if (!entry.is_directory() || name == "tpi" || name.front() == '.') continue;
        const fs::path link = dataRoot / "assets" / entry.path().filename();
        if (fs::exists(fs::symlink_status(link))) continue;
        fs::create_directory_symlink(fs::absolute(entry.path()), link, ec);
    }
}

void TestRealData(const std::string& data, const std::string& tables, const std::string& tpwAssets,
                  const fs::path& scratch) {
    for (const char* name : {"rides.sam", "standard.sam", "global.sam", "hoardings.sam", "coaster.sam"}) {
        std::vector<SamRow> rows;
        if (!LoadTable(fs::path(tables) / (std::string(name) + ".bin"), rows)) {
            std::printf("FAIL cannot read table %s\n", name);
            g_fail++;
            return;
        }
        SetSamTable(name, rows.data(), rows.size());
        if (std::string(name) == "hoardings.sam") SetSamTable("endhoardings.sam", rows.data(), rows.size());
        if (std::string(name) == "rides.sam")
            for (const char* other : {"shops.sam", "sideshow.sam", "features.sam", "upgrades.sam"})
                SetSamTable(other, rows.data(), rows.size());
    }
    std::string err;
    auto source = OpenFolder(data, err);
    CHECK(source != nullptr);
    if (!source) return;
    TpiDisc disc;
    disc.edition = "Theme Park Inc";
    disc.path = data;
    disc.source = std::move(source);
    const fs::path dataRoot = scratch / "tpi-real-data-root";
    LinkTpwEditions(tpwAssets, dataRoot);
    // The player's esprites.wad (its generic groups and Theme Park World's four themes), to compare after.
    std::map<std::string, Bytes> tpwSprites;
    for (const char* game : {"eur", "us", "eur-pc", "us-pc"}) {
        const fs::path wad = fs::path(tpwAssets) / game / "Theme Park Data" / "data" / "esprites.wad";
        if (fs::exists(wad)) tpwSprites[game] = ReadFile(wad);
    }
    const auto started = std::chrono::steady_clock::now();
    std::string reason;
    const bool ok = ConvertTpi(disc, dataRoot, reason);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    CHECK(ok);
    std::printf("real conversion: %s in %.1f s -> %s\n", ok ? "ok" : reason.c_str(), seconds, dataRoot.string().c_str());
    if (ok) {
        const std::string record = S(ReadFile(dataRoot / "assets" / "tpi" / "converted.txt"));
        std::printf("%s", record.substr(0, record.find("\n\n") + 1).c_str());
        CHECK(ReadTpiConverterStamp(dataRoot) == kTpiConverterVersion);
        // Every world's entertainers, costumes and costume heads are Theme Park Inc's own, file for file as its
        // esprites.wad holds them unpacked; the player's esprites.wad is the same bytes as before.
        std::vector<TpiWadMember> tpi;
        std::string err;
        CHECK(ReadTpiWad(ReadFile(fs::path(data) / "Data" / "esprites.wad"), tpi, err));
        const fs::path sprites = dataRoot / "assets" / "tpi" / "data" / "esprites";
        int groups = 0, files = 0;
        for (const char* world : {"arabian", "water", "science"})
            for (const char* group : {"Entertainers", "Costumes", "Costumeheads"}) {
                const auto out = GroupFiles(sprites / world / group);
                CHECK(HasEsp(out));
                std::string prefix = std::string(world) + "/" + group + "/";
                for (char& c : prefix) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                size_t want = 0, sprites = 0;
                for (const auto& m : tpi) {
                    std::string lower = m.name;
                    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lower.compare(0, prefix.size(), prefix) != 0) continue;
                    want++;
                    const std::string name = m.name.substr(prefix.size());
                    const auto it = out.find(name);
                    CHECK(it != out.end() && it->second == m.data);
                    // A copy written again (a 2 or 3 after the name) holds the same bytes.
                    for (const char* n : {"2", "3"}) {
                        const auto copy = out.find(name.substr(0, name.rfind('.')) + n + name.substr(name.rfind('.')));
                        if (copy != out.end()) {
                            CHECK(copy->second == m.data);
                            want++;
                        }
                    }
                }
                for (const auto& [name, bytes] : out)
                    sprites += name.size() > 4 && (name.substr(name.size() - 4) == ".ESP" || name.substr(name.size() - 4) == ".esp");
                CHECK(want > 0 && out.size() == want);
                CHECK(sprites >= (std::string(group) == "Entertainers" ? 3u : 1u));
                groups += HasEsp(out);
                files += static_cast<int>(out.size());
                CHECK(record.find(prefix.substr(0, prefix.find('/') + 1) + group + ": ") != std::string::npos);
            }
        CHECK(record.find(" world's in ") == std::string::npos);   // no group from a donor
        std::printf("sprites: %d groups, %d files from Theme Park Inc's esprites.wad\n", groups, files);
        for (const auto& [game, bytes] : tpwSprites)
            CHECK(ReadFile(fs::path(tpwAssets) / game / "Theme Park Data" / "data" / "esprites.wad") == bytes);
        CHECK(!tpwSprites.empty());
    }
}

}  // namespace

int main() {
    TestWadHash();
    TestWadRoundTrip();
    TestFsh();
    TestSamFilter();
    TestSfxPatch();
    std::string scratch = Env("TPW_TEST_SCRATCH");
#ifdef TPW_TEST_BUILD_SCRATCH
    if (scratch.empty()) scratch = TPW_TEST_BUILD_SCRATCH;
#endif
    if (scratch.empty()) {
        Skip("conversion of a Data tree: no scratch folder (TPW_TEST_SCRATCH)");
    } else {
        fs::create_directories(scratch);
        TestConvertTree(scratch);
        TestSprites(scratch);
        TestNonAsciiPath(scratch);
        const std::string data = Env("TPW_TPI_DATA"), tables = Env("TPW_TPI_TABLES"), tpwAssets = Env("TPW_TPW_ASSETS");
        if (data.empty() || tables.empty() || tpwAssets.empty())
            Skip("real conversion: TPW_TPI_DATA, TPW_TPI_TABLES or TPW_TPW_ASSETS is not set");
        else
            TestRealData(data, tables, tpwAssets, scratch);
    }
    std::printf("tpi convert tests: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    return g_fail == 0 ? 0 : 1;
}
