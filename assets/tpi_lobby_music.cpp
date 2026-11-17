// tpi_lobby_music.cpp: the island screen's music for a converted Theme Park Inc world (tpi_lobby_music.h). The park
// music's map is walked whole as tpi_sfx.cpp walks a map; the lobby's two maps are written in the layout of Theme Park
// World's own four (162 and 60 bytes for three samples), which differ only in the samples' lengths.
#include "tpi_lobby_music.h"

#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

namespace tpw::assets {

namespace {

constexpr size_t kHeader = 20 + 8;   // id and version, then a u32 and the category count
constexpr size_t kCategory = 24;
constexpr size_t kEvent = 20;
constexpr size_t kSound = 42;
constexpr size_t kSample = 16;
constexpr size_t kBranch = 8;
constexpr size_t kEntryHeader = 0x28;   // a bank sample's header

constexpr uint32_t kShortest = 15000;   // the usable lengths, ms
constexpr uint32_t kLongest = 25000;
constexpr size_t kLoops = 3;            // the lobby's samples, as Theme Park World's have

// The two maps' ids (the first 16 bytes): the event map's and the bank map's.
constexpr uint8_t kSfxId[16] = {0x00, 0x2c, 0x61, 0xe9, 0xd0, 0x31, 0xd2, 0x11, 0xb4, 0x09, 0x00, 0xb0, 0xc9, 0x93, 0xf2, 0x03};
constexpr uint8_t kBankId[16] = {0x01, 0x2c, 0x61, 0xe9, 0xd0, 0x31, 0xd2, 0x11, 0xb4, 0x09, 0x00, 0xa0, 0xc9, 0x93, 0xf2, 0x03};
constexpr char kBankName[] = "Sound\\LobbyMusic";

uint32_t Get32(const std::vector<uint8_t> &b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}

void Put16(std::vector<uint8_t> &b, uint32_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}

void Put32(std::vector<uint8_t> &b, uint32_t v) {
    Put16(b, v & 0xffff);
    Put16(b, v >> 16);
}

void PutBytes(std::vector<uint8_t> &b, std::initializer_list<uint8_t> bytes) { b.insert(b.end(), bytes); }

// Reserves `count` records of `size` bytes at `at`: false when they do not fit before the end.
bool Take(const std::vector<uint8_t> &b, size_t &at, uint64_t count, size_t size) {
    const uint64_t bytes = count * size;
    if (count > b.size() || bytes > b.size() - at)
        return false;
    at += size_t(bytes);
    return true;
}

struct MapSample {
    uint32_t sample;   // 1-based in the bank
    uint32_t length;   // ms
};

// The bank's entry for 1-based `sample` when it is a whole one (its offset and size), else false.
bool Entry(const std::vector<uint8_t> &bank, uint32_t sample, size_t &offset, size_t &size) {
    if (bank.size() < 4 || sample == 0 || sample > Get32(bank, 0) || uint64_t(sample) * 4 + 4 > bank.size())
        return false;
    offset = Get32(bank, sample * 4);
    if (offset > bank.size() || bank.size() - offset < kEntryHeader)
        return false;
    const uint32_t data = Get32(bank, offset + 4);
    const uint32_t rate = bank[offset + 0x18] | uint32_t(bank[offset + 0x19]) << 8;
    if (Get32(bank, offset) != kEntryHeader || rate != 22050 || bank[offset + 0x1a] != 16 || (bank[offset + 0x1b] & 1) == 0 ||
        data < 4 || data > bank.size() - offset - kEntryHeader)
        return false;
    // An MPEG frame's sync (11 bits) and layer II.
    const uint8_t *audio = bank.data() + offset + kEntryHeader;
    if (audio[0] != 0xff || (audio[1] & 0xe0) != 0xe0 || ((audio[1] >> 1) & 3) != 2)
        return false;
    size = kEntryHeader + data;
    return true;
}

std::string EntryName(const std::vector<uint8_t> &bank, size_t offset) {
    std::string name;
    for (size_t i = 8; i < 0x18 && bank[offset + i] != 0; ++i)
        name += char(bank[offset + i]);
    return name;
}

}   // namespace

bool BuildTpiLobbyMusic(const std::vector<uint8_t> &musicMap, const std::vector<uint8_t> &musicBank, TpiLobbyMusic &out,
                        std::string &reason) {
    out = {};
    const std::vector<uint8_t> &b = musicMap;
    if (b.size() < kHeader) {
        reason = "the music map is shorter than its header";
        return false;
    }
    // The mood groups (every sound of every event), with their samples.
    std::vector<std::vector<MapSample>> groups;
    size_t at = kHeader;
    const uint32_t categories = Get32(b, 24);
    const size_t categoriesAt = at;
    if (!Take(b, at, categories, kCategory)) {
        reason = "the music map ends inside its categories";
        return false;
    }
    for (uint32_t c = 0; c < categories; ++c) {
        const uint32_t events = Get32(b, categoriesAt + c * kCategory);
        const size_t eventsAt = at;
        if (!Take(b, at, events, kEvent)) {
            reason = "the music map ends inside its events";
            return false;
        }
        for (uint32_t e = 0; e < events; ++e) {
            const uint32_t sounds = Get32(b, eventsAt + e * kEvent + 4);
            const size_t soundsAt = at;
            if (!Take(b, at, sounds, kSound)) {
                reason = "the music map ends inside its sounds";
                return false;
            }
            for (uint32_t s = 0; s < sounds; ++s) {
                const size_t sound = soundsAt + s * kSound;
                const uint32_t samples = Get32(b, sound) & 0xffff;
                const size_t samplesAt = at;
                if (!Take(b, at, samples, kSample) || !Take(b, at, Get32(b, sound + 4), kBranch)) {
                    reason = "the music map ends inside its samples";
                    return false;
                }
                std::vector<MapSample> group;
                for (uint32_t i = 0; i < samples; ++i)
                    group.push_back({Get32(b, samplesAt + i * kSample), Get32(b, samplesAt + i * kSample + 8)});
                groups.push_back(group);
            }
        }
    }
    if (at != b.size()) {
        reason = std::to_string(b.size() - at) + " bytes follow the music map's last sound";
        return false;
    }

    // The group with the largest set of usable samples of one length.
    std::vector<MapSample> best;
    uint32_t bestGroup = 0;
    for (size_t g = 0; g < groups.size(); ++g) {
        std::vector<MapSample> usable;
        for (const MapSample &s : groups[g]) {
            size_t offset = 0, size = 0;
            if (!Entry(musicBank, s.sample, offset, size)) {
                ++out.unusable;
                continue;
            }
            if (s.length >= kShortest && s.length <= kLongest)
                usable.push_back(s);
        }
        std::vector<MapSample> set;
        for (const MapSample &centre : usable) {
            std::vector<MapSample> near;
            for (const MapSample &s : usable)
                if (uint64_t(std::abs(int64_t(s.length) - int64_t(centre.length))) * 100 <= centre.length)
                    near.push_back(s);
            if (near.size() > set.size())
                set = near;
        }
        if (set.size() > best.size()) {
            best = set;
            bestGroup = uint32_t(g + 1);
        }
    }
    if (best.empty()) {
        reason = "no group of the park music has a whole sample of 15 to 25 s";
        out.unusable = 0;
        return false;
    }
    for (const MapSample &s : best) {
        bool again = false;
        for (const TpiLobbyPick &p : out.picks)
            again = again || p.sample == s.sample;
        if (again)
            continue;
        size_t offset = 0, size = 0;
        Entry(musicBank, s.sample, offset, size);
        out.picks.push_back({bestGroup, s.sample, s.length, EntryName(musicBank, offset)});
        if (out.picks.size() == kLoops)
            break;
    }
    const uint32_t count = uint32_t(out.picks.size());

    // The bank: the entries as they are.
    Put32(out.bank, count);
    size_t next = 4 + 4 * size_t(count);
    std::vector<std::pair<size_t, size_t>> entries;
    for (const TpiLobbyPick &p : out.picks) {
        size_t offset = 0, size = 0;
        Entry(musicBank, p.sample, offset, size);
        entries.push_back({offset, size});
        Put32(out.bank, uint32_t(next));
        next += size;
    }
    for (const auto &[offset, size] : entries)
        out.bank.insert(out.bank.end(), musicBank.begin() + std::ptrdiff_t(offset),
                        musicBank.begin() + std::ptrdiff_t(offset + size));

    // The event map: one category, one looping event of one sound whose samples are the bank's, equally likely.
    std::vector<uint8_t> &m = out.sfxMap;
    m.insert(m.end(), std::begin(kSfxId), std::end(kSfxId));
    Put32(m, 0);   // version
    Put32(m, 0);
    Put32(m, 1);   // categories
    Put32(m, 1);   // the category: its events, a pointer, u16 3, u16 flags 0, floats 1, 0, 0
    Put32(m, 0);
    Put16(m, 3);
    Put16(m, 0);
    PutBytes(m, {0x00, 0x00, 0x80, 0x3f});
    Put32(m, 0);
    Put32(m, 0);
    Put32(m, kTpiLobbyMusicEvent);   // the event: id, sounds, a pointer, priority, flags, u16
    Put32(m, 1);
    Put32(m, 0);
    Put32(m, 4000);
    Put16(m, kTpiLobbyMusicFlags);
    Put16(m, 0);
    Put32(m, count);   // the sound: samples, branches, a pointer, volume 100..100, pitch 0..0, timing, weight, distance
    Put32(m, 0);
    Put32(m, 0);
    PutBytes(m, {0x64, 0x64, 0x00, 0x00});
    Put16(m, 0);
    Put16(m, 0);
    PutBytes(m, {0x32, 0x32});
    for (int i = 0; i < 4; ++i)
        Put16(m, 0);
    Put32(m, 0xffff);
    PutBytes(m, {0x00, 0x00, 0xc8, 0x42});   // 100.0f
    Put32(m, 0);                             // the branches' pointer
    for (uint32_t i = 0; i < count; ++i) {   // the samples: bank index, cumulative weight, length, bank, u16
        Put32(m, i + 1);
        Put32(m, uint32_t(0xffffu * (i + 1) / count));
        Put32(m, out.picks[i].length);
        Put16(m, 1);
        Put16(m, 0);
    }

    // The bank map: the one bank, named as Theme Park World's lobby banks are.
    std::vector<uint8_t> &k = out.bankMap;
    k.insert(k.end(), std::begin(kBankId), std::end(kBankId));
    Put32(k, 0);   // version
    Put32(k, 0);   // the banks' pointer
    Put32(k, 1);   // banks
    Put32(k, 0);   // the bank: u32, u8, the name's pointer (the reader sets them), flags 1, u8
    k.push_back(0);
    Put32(k, 0);
    k.push_back(1);
    k.push_back(0);
    Put32(k, uint32_t(sizeof kBankName));
    k.insert(k.end(), kBankName, kBankName + sizeof kBankName);
    return true;
}

}   // namespace tpw::assets
