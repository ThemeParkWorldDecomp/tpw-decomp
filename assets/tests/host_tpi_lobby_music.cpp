// host_tpi_lobby_music.cpp: the island screen's music for a converted Theme Park Inc world (assets/tpi_lobby_music.h),
// built from a synthetic park music bank and map, no game data: which segments are chosen, the bank's entries, and the
// two maps in the layout of Theme Park World's own lobby maps (one looping event of id 1, one sound, equal weights).
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../tpi_lobby_music.h"
#include "../tpi_sfx.h"

using namespace tpw::assets;
using Bytes = std::vector<uint8_t>;

static int failures = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
            ++failures;                                                  \
        }                                                                \
    } while (0)

static void Put16(Bytes &d, size_t at, uint32_t v) {
    d[at] = uint8_t(v);
    d[at + 1] = uint8_t(v >> 8);
}
static void Put32(Bytes &d, size_t at, uint32_t v) {
    Put16(d, at, v & 0xffff);
    Put16(d, at + 2, v >> 16);
}
static uint32_t Get32(const Bytes &d, size_t at) {
    return uint32_t(d[at]) | uint32_t(d[at + 1]) << 8 | uint32_t(d[at + 2]) << 16 | uint32_t(d[at + 3]) << 24;
}
static uint16_t Get16(const Bytes &d, size_t at) { return uint16_t(d[at] | d[at + 1] << 8); }

// How a bank entry is spoilt (the disc's music banks have entries with garbage headers at their ends).
enum class Spoil { kNone, kHeaderSize, kRate, kMono, kSync, kLayer, kPastEnd };

struct BankSample {
    std::string name;
    uint32_t data;   // the MPEG data's size
    Spoil spoil = Spoil::kNone;
};

// A bank in the .sdt layout: a count, the offsets, then each entry's 0x28-byte header and its data (an MPEG-2 layer II
// frame header, then bytes that tell the entries apart).
static Bytes MakeBank(const std::vector<BankSample> &samples) {
    Bytes d(4 + 4 * samples.size(), 0);
    Put32(d, 0, uint32_t(samples.size()));
    for (size_t i = 0; i < samples.size(); ++i) {
        const BankSample &s = samples[i];
        const size_t at = d.size();
        Put32(d, 4 + 4 * i, uint32_t(at));
        d.resize(at + 0x28 + s.data, 0);
        Put32(d, at, s.spoil == Spoil::kHeaderSize ? 0x29 : 0x28);
        Put32(d, at + 4, s.spoil == Spoil::kPastEnd ? s.data + 1000 : s.data);
        std::memcpy(&d[at + 8], s.name.data(), std::min<size_t>(s.name.size(), 16));
        Put16(d, at + 0x18, s.spoil == Spoil::kRate ? 44100 : 22050);
        d[at + 0x1a] = 16;
        d[at + 0x1b] = s.spoil == Spoil::kMono ? 0x24 : 0x25;
        Put32(d, at + 0x20, s.data * 14);
        d[at + 0x28] = s.spoil == Spoil::kSync ? 0x47 : 0xff;
        d[at + 0x29] = s.spoil == Spoil::kLayer ? 0xf3 : 0xf5;   // MPEG-2; layer II (0xf3 is layer III)
        for (uint32_t k = 2; k < s.data; ++k)
            d[at + 0x28 + k] = uint8_t(i * 31 + k);
    }
    return d;
}

struct MapSample {
    uint32_t sample;   // 1-based in the bank
    uint32_t length;   // ms
};

// A park music map in the layout tpi_sfx.h describes (version 1, as Theme Park Inc's): one category, one event (id 2)
// whose sounds are `groups`, each with its samples and two branches.
static Bytes MakeMusicMap(const std::vector<std::vector<MapSample>> &groups) {
    Bytes d(28 + 24 + 20, 0);
    for (int i = 0; i < 16; ++i)
        d[i] = uint8_t(0xa0 + i);
    Put32(d, 16, 1);
    Put32(d, 24, 1);
    Put32(d, 28, 1);
    Put32(d, 52, 2);
    Put32(d, 56, uint32_t(groups.size()));
    Put32(d, 64, 10000);
    Put16(d, 68, 1542);
    for (const auto &g : groups) {
        const size_t at = d.size();
        d.resize(at + 42, 0);
        Put32(d, at, 0x10000 | uint32_t(g.size()));   // the high bits are not part of the count
        Put32(d, at + 4, 2);
        d[at + 12] = d[at + 13] = 0x64;
    }
    for (const auto &g : groups) {
        for (size_t i = 0; i < g.size(); ++i) {
            const size_t at = d.size();
            d.resize(at + 16, 0);
            Put32(d, at, g[i].sample);
            Put32(d, at + 4, uint32_t(0xffff * (i + 1) / g.size()));
            Put32(d, at + 8, g[i].length);
            Put16(d, at + 12, 1);
        }
        for (uint32_t b = 1; b <= 2; ++b) {
            const size_t at = d.size();
            d.resize(at + 8, 0);
            Put32(d, at, b);
        }
    }
    return d;
}

// The entry of 1-based `sample` in `bank`, header and data.
static Bytes EntryOf(const Bytes &bank, uint32_t sample) {
    const uint32_t at = Get32(bank, 4 * sample);
    return Bytes(bank.begin() + at, bank.begin() + at + 0x28 + Get32(bank, at + 4));
}

int main() {
    // Sixteen samples; 5 and 9 to 13 spoilt, one way each.
    std::vector<BankSample> samples;
    for (uint32_t i = 1; i <= 16; ++i)
        samples.push_back({"Level1-" + std::to_string(i) + ".mp2", 200 + i});
    samples[4].spoil = Spoil::kHeaderSize;
    samples[8].spoil = Spoil::kRate;
    samples[9].spoil = Spoil::kMono;
    samples[10].spoil = Spoil::kSync;
    samples[11].spoil = Spoil::kLayer;
    samples[12].spoil = Spoil::kPastEnd;
    const Bytes bank = MakeBank(samples);
    // Group 1: two segments of 20 s. Group 2: four whole segments within 1% of 19.2 s (one of them named twice), a
    // spoilt one of 19.2 s, one too long and one 2% off. Group 3: five of 16 s, all spoilt. Group 4: three of
    // 19.2 s, as many as group 2 has but later. Group 5: whole segments too short.
    const std::vector<std::vector<MapSample>> groups = {
        {{1, 20000}, {2, 20000}},
        {{3, 30000}, {5, 19200}, {6, 19190}, {4, 19584}, {7, 19210}, {6, 19190}, {8, 19200}},
        {{9, 16000}, {10, 16000}, {11, 16000}, {12, 16000}, {13, 16000}},
        {{14, 19200}, {15, 19200}, {16, 19200}},
        {{1, 9000}, {2, 14999}},
    };
    const Bytes map = MakeMusicMap(groups);

    TpiLobbyMusic lobby;
    std::string reason;
    CHECK(BuildTpiLobbyMusic(map, bank, lobby, reason));
    // Group 2's first three segments of one length, in the map's order: 6, 7 and 8 (5 is spoilt, 3 too long, 4 2% off,
    // 6 not taken twice).
    CHECK(lobby.picks.size() == 3);
    if (lobby.picks.size() == 3) {
        CHECK(lobby.picks[0].group == 2 && lobby.picks[1].group == 2 && lobby.picks[2].group == 2);
        CHECK(lobby.picks[0].sample == 6 && lobby.picks[1].sample == 7 && lobby.picks[2].sample == 8);
        CHECK(lobby.picks[0].length == 19190 && lobby.picks[1].length == 19210 && lobby.picks[2].length == 19200);
        CHECK(lobby.picks[0].name == "Level1-6.mp2" && lobby.picks[2].name == "Level1-8.mp2");
    }
    CHECK(lobby.unusable == 6);   // 5, and 9 to 13

    // The bank: three entries, copied as they are, one after the other.
    CHECK(lobby.bank.size() >= 16 && Get32(lobby.bank, 0) == 3);
    if (lobby.bank.size() >= 16) {
        CHECK(Get32(lobby.bank, 4) == 16);
        CHECK(EntryOf(lobby.bank, 1) == EntryOf(bank, 6));
        CHECK(EntryOf(lobby.bank, 2) == EntryOf(bank, 7));
        CHECK(EntryOf(lobby.bank, 3) == EntryOf(bank, 8));
        CHECK(Get32(lobby.bank, 12) + EntryOf(bank, 8).size() == lobby.bank.size());
    }

    // The event map: Theme Park World's lobby layout, 162 bytes for three samples.
    const Bytes &m = lobby.sfxMap;
    CHECK(m.size() == 162);
    if (m.size() == 162) {
        const uint8_t sfxId[16] = {0x00, 0x2c, 0x61, 0xe9, 0xd0, 0x31, 0xd2, 0x11, 0xb4, 0x09, 0x00, 0xb0, 0xc9, 0x93, 0xf2, 0x03};
        CHECK(std::memcmp(m.data(), sfxId, 16) == 0);
        CHECK(Get32(m, 16) == 0 && Get32(m, 20) == 0 && Get32(m, 24) == 1);   // version 0, one category
        CHECK(Get32(m, 28) == 1 && Get16(m, 36) == 3 && Get16(m, 38) == 0);   // one event
        float f = 0;
        std::memcpy(&f, &m[40], 4);
        CHECK(f == 1.0f);
        // The event: id 1 (lobby_island.cpp sends it to the music category), one sound, priority 4000, looping (4)
        // and not positional (0x200); no 8, so it plays its samples one after another, picking again at each end.
        CHECK(Get32(m, 52) == kTpiLobbyMusicEvent && kTpiLobbyMusicEvent == 1);
        CHECK(Get32(m, 56) == 1 && Get32(m, 64) == 4000);
        CHECK(Get16(m, 68) == 0x0206 && (Get16(m, 68) & 4) != 0 && (Get16(m, 68) & 0x200) != 0 && (Get16(m, 68) & 8) == 0);
        // The sound: three samples, no branches, volume 100, weight 0xffff, distance 100.
        CHECK(Get32(m, 72) == 3 && Get32(m, 76) == 0 && m[84] == 0x64 && m[85] == 0x64 && m[92] == 0x32 && m[93] == 0x32);
        CHECK(Get32(m, 72 + 0x1e) == 0xffff);
        std::memcpy(&f, &m[72 + 0x22], 4);
        CHECK(f == 100.0f);
        // The samples: the bank's 1 to 3, equally likely (cumulative weights), their lengths, bank 1.
        const uint32_t weights[3] = {0x5555, 0xaaaa, 0xffff};
        const uint32_t lengths[3] = {19190, 19210, 19200};
        for (uint32_t i = 0; i < 3; ++i) {
            const size_t at = 114 + 16 * i;
            CHECK(Get32(m, at) == i + 1 && Get32(m, at + 4) == weights[i] && Get32(m, at + 8) == lengths[i]);
            CHECK(Get16(m, at + 12) == 1 && Get16(m, at + 14) == 0);
        }
        // The map parses whole as the converter's other map walker reads maps, and has event 1 at volume 100.
        Bytes copy = m;
        CHECK(PatchTpiSfxVolume(copy, {kTpiLobbyMusicEvent, 0x64, 0x64}, reason) && copy == m);
    }

    // The bank map: one bank, "Sound\LobbyMusic" as Theme Park World's lobby banks are named.
    const Bytes &k = lobby.bankMap;
    CHECK(k.size() == 60);
    if (k.size() == 60) {
        const uint8_t bankId[16] = {0x01, 0x2c, 0x61, 0xe9, 0xd0, 0x31, 0xd2, 0x11, 0xb4, 0x09, 0x00, 0xa0, 0xc9, 0x93, 0xf2, 0x03};
        CHECK(std::memcmp(k.data(), bankId, 16) == 0);
        CHECK(Get32(k, 16) == 0 && Get32(k, 24) == 1 && k[28 + 9] == 1);
        CHECK(Get32(k, 39) == 17 && std::memcmp(&k[43], "Sound\\LobbyMusic", 17) == 0);
    }

    // A tie between two groups goes to the earlier one; a group of fewer than three gives what it has.
    CHECK(BuildTpiLobbyMusic(MakeMusicMap({{{1, 20000}, {2, 20100}}, {{3, 20000}, {4, 20000}}}), bank, lobby, reason));
    CHECK(lobby.picks.size() == 2 && lobby.picks[0].sample == 1 && lobby.picks[1].sample == 2 && Get32(lobby.bank, 0) == 2);
    CHECK(lobby.sfxMap.size() == 146 && Get32(lobby.sfxMap, 72) == 2 && Get32(lobby.sfxMap, 114 + 4) == 0x7fff &&
          Get32(lobby.sfxMap, 130 + 4) == 0xffff);
    // The largest set of one length wins over a longer list of mixed lengths.
    CHECK(BuildTpiLobbyMusic(MakeMusicMap({{{1, 15000}, {2, 18000}, {3, 21000}, {4, 24000}}, {{6, 22000}, {7, 22100}}}), bank,
                             lobby, reason));
    CHECK(lobby.picks.size() == 2 && lobby.picks[0].group == 2 && lobby.picks[0].sample == 6);
    // Two sets of one size in a group: the one whose first sample comes first in the map.
    CHECK(BuildTpiLobbyMusic(MakeMusicMap({{{1, 16000}, {2, 22000}, {3, 16000}, {4, 22000}}}), bank, lobby, reason));
    CHECK(lobby.picks.size() == 2 && lobby.picks[0].sample == 1 && lobby.picks[1].sample == 3);

    // Nothing usable: false, nothing built, and a reason.
    reason.clear();
    CHECK(!BuildTpiLobbyMusic(MakeMusicMap({{{9, 20000}, {5, 20000}}, {{1, 30000}}}), bank, lobby, reason));
    CHECK(lobby.bank.empty() && lobby.sfxMap.empty() && lobby.bankMap.empty() && lobby.picks.empty() && !reason.empty());
    // A map that does not parse whole, at every length short of its end and with a byte after it.
    for (size_t cut = 0; cut < map.size(); ++cut) {
        const Bytes part(map.begin(), map.begin() + long(cut));
        if (BuildTpiLobbyMusic(part, bank, lobby, reason)) {
            std::fprintf(stderr, "FAIL: a map cut to %zu bytes was taken\n", cut);
            ++failures;
            break;
        }
    }
    Bytes longer = map;
    longer.push_back(0);
    CHECK(!BuildTpiLobbyMusic(longer, bank, lobby, reason) && reason.find("1 bytes follow") != std::string::npos);
    // A bank that is not one: too short, or naming fewer samples than the map does.
    CHECK(!BuildTpiLobbyMusic(map, Bytes{1, 0}, lobby, reason));
    const Bytes shortBank = MakeBank({samples[0], samples[1], samples[2]});
    CHECK(BuildTpiLobbyMusic(map, shortBank, lobby, reason) && lobby.picks.size() == 2 && lobby.picks[0].group == 1);

    if (failures) {
        std::fprintf(stderr, "tpi_lobby_music: %d failed\n", failures);
        return 1;
    }
    std::printf("tpi_lobby_music: all passed\n");
    return 0;
}
