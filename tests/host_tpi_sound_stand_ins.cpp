// host_tpi_sound_stand_ins.cpp: the TPI Worlds and Rides mod's broken music samples (include/TpiSoundStandIns.h,
// TPW_MOD_HOOK tpi_worlds) on the host, without game data: which .sdt headers are a sample's, and the stand-ins of an
// event laid out as Arabian Nights' park music (seven levels, the sixth and seventh made only of samples the bank does
// not have, the fifth partly), Polar Zone's (the last of five partly), an event with nothing broken, one with nothing
// that plays, and samples of other banks left alone. Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "TpiSoundStandIns.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

// The fields of the map's CSampleData (include/TbMapStreamer.h) that the helper reads and writes.
struct Sample {
    uint32_t mF00;
    uint32_t mF04;
    uint32_t mF08;
    uint16_t mBankIndex;
    uint16_t mF0E;
};

// A sound of samples numbered first..last of bank `bank`, each weighing one more than the one before and lasting
// 1000 + its number milliseconds.
static std::vector<Sample> Sound(uint32_t first, uint32_t last, uint16_t bank) {
    std::vector<Sample> out;
    uint32_t weight = 0;
    for (uint32_t n = first; n <= last; ++n) out.push_back(Sample{n, weight += 100, 1000 + n, bank, 7});
    return out;
}

struct Event {
    std::vector<std::vector<Sample>> sounds;
    uint32_t Run(const std::set<std::pair<uint16_t, uint32_t>>& broken) {
        std::vector<Sample*> pointers;
        std::vector<uint32_t> counts;
        for (auto& sound : sounds) {
            pointers.push_back(sound.data());
            counts.push_back(static_cast<uint32_t>(sound.size()));
        }
        const auto plays = [&broken](const Sample& s) { return broken.count({s.mBankIndex, s.mF00}) == 0; };
        return tpw::TpiStandInSamples(pointers.data(), counts.data(), static_cast<uint32_t>(sounds.size()), plays);
    }
};

static std::set<std::pair<uint16_t, uint32_t>> Broken(uint16_t bank, uint32_t first, uint32_t last) {
    std::set<std::pair<uint16_t, uint32_t>> out;
    for (uint32_t n = first; n <= last; ++n) out.insert({bank, n});
    return out;
}

static bool AllPlay(const Event& event, const std::set<std::pair<uint16_t, uint32_t>>& broken) {
    for (const auto& sound : event.sounds)
        for (const Sample& s : sound)
            if (broken.count({s.mBankIndex, s.mF00}) != 0) return false;
    return true;
}

static void TestHeaders() {
    using tpw::TpiSdtSampleIsSound;
    Check(TpiSdtSampleIsSound(0x28, 1000, 0x100, 0x100 + 0x28 + 1000), "a header whose data ends at the file's end");
    Check(TpiSdtSampleIsSound(0x28, 1000, 0x100, 0x10000), "a header within the file");
    Check(!TpiSdtSampleIsSound(0x28, 1000, 0x100, 0x100 + 0x28 + 999), "data one byte past the end");
    Check(!TpiSdtSampleIsSound(0x28, 0, 0x100, 0x10000), "a sample of no size");
    Check(!TpiSdtSampleIsSound(0xb2f5fffu, 1000, 0x100, 0x10000), "MPEG data where the header should be");
    Check(!TpiSdtSampleIsSound(0x28, 841143874u, 0x2000000, 0x26e6000), "Arabian Nights' 139th: a size past the file");
    Check(!TpiSdtSampleIsSound(0x28, 0xfffffff0u, 0x100, 0x10000), "a negative size taken as unsigned");
    Check(!TpiSdtSampleIsSound(0x28, 0x10, 0xffffffe0u, 0xffffffffull), "an offset near 4 GB does not wrap");
    Check(TpiSdtSampleIsSound(0x28, 0x10, 0xffffffc8u, 0x100000000ull), "a sample ending at 4 GB");
}

static void TestArabian() {
    // Seven levels of how busy the park is; the bank (13) has samples 1 to 103, 104 to 143 are not there.
    Event event;
    event.sounds = {Sound(1, 12, 13), Sound(13, 30, 13), Sound(31, 50, 13), Sound(51, 86, 13),
                    Sound(87, 117, 13), Sound(118, 140, 13), Sound(141, 143, 13)};
    const Event before = event;
    const auto broken = Broken(13, 104, 143);
    const uint32_t changed = event.Run(broken);
    Check(changed == 40, "Arabian Nights: the 40 broken samples change, got " + std::to_string(changed));
    Check(AllPlay(event, broken), "Arabian Nights: every sample plays");
    for (size_t s = 0; s < 4; ++s)
        for (size_t k = 0; k < event.sounds[s].size(); ++k)
            Check(event.sounds[s][k].mF00 == before.sounds[s][k].mF00, "Arabian Nights: levels 1 to 4 are untouched");
    // Level 5: 87 to 103 play and stay, 104 to 117 take 87, 88, ... in order.
    for (size_t k = 0; k < event.sounds[4].size(); ++k) {
        const uint32_t was = before.sounds[4][k].mF00;
        const uint32_t want = was <= 103 ? was : 87 + static_cast<uint32_t>(was - 104) % 17;
        Check(event.sounds[4][k].mF00 == want, "Arabian Nights level 5, sample " + std::to_string(was));
        Check(event.sounds[4][k].mF08 == 1000 + want, "Arabian Nights level 5: the length follows the sample");
    }
    // Levels 6 and 7 have none that play: the nearest below with any is level 5, its 17 that play from the first.
    for (size_t s = 5; s < 7; ++s)
        for (size_t k = 0; k < event.sounds[s].size(); ++k)
            Check(event.sounds[s][k].mF00 == 87 + static_cast<uint32_t>(k) % 17,
                  "Arabian Nights level " + std::to_string(s + 1) + ", place " + std::to_string(k));
    // The weights, the flags and the counts stay, so the sounds are chosen and drawn as before.
    for (size_t s = 0; s < 7; ++s) {
        Check(event.sounds[s].size() == before.sounds[s].size(), "the counts stay");
        for (size_t k = 0; k < event.sounds[s].size(); ++k) {
            Check(event.sounds[s][k].mF04 == before.sounds[s][k].mF04, "the weights stay");
            Check(event.sounds[s][k].mF0E == before.sounds[s][k].mF0E, "the flags stay");
            Check(event.sounds[s][k].mBankIndex == 13, "the bank stays the music bank");
        }
    }
}

static void TestPolarZone() {
    // Five levels; the bank (9) has 1 to 101, 102 to 116 are not there and all sit in the fifth.
    Event event;
    event.sounds = {Sound(1, 20, 9), Sound(21, 40, 9), Sound(41, 60, 9), Sound(61, 78, 9), Sound(79, 116, 9)};
    const auto broken = Broken(9, 102, 116);
    Check(event.Run(broken) == 15, "Polar Zone: the 15 broken samples change");
    Check(AllPlay(event, broken), "Polar Zone: every sample plays");
    for (size_t k = 0; k < event.sounds[4].size(); ++k) {
        const uint32_t was = 79 + static_cast<uint32_t>(k);
        const uint32_t want = was <= 101 ? was : 79 + (was - 102) % 23;
        Check(event.sounds[4][k].mF00 == want, "Polar Zone level 5, sample " + std::to_string(was));
    }
}

static void TestEdges() {
    // Nothing broken: nothing changes.
    Event science;
    science.sounds = {Sound(1, 10, 4), Sound(11, 20, 4)};
    const Event before = science;
    Check(science.Run({}) == 0, "Science: nothing changes");
    for (size_t s = 0; s < 2; ++s)
        for (size_t k = 0; k < 10; ++k) Check(science.sounds[s][k].mF00 == before.sounds[s][k].mF00, "Science stays");
    // Nothing plays: left as it is.
    Event dead;
    dead.sounds = {Sound(1, 3, 2), Sound(4, 6, 2)};
    Check(dead.Run(Broken(2, 1, 6)) == 0, "an event of nothing that plays is left");
    Check(dead.sounds[1][2].mF00 == 6, "an event of nothing that plays keeps its samples");
    // The first sound broken: the nearest above stands in.
    Event first;
    first.sounds = {Sound(1, 4, 3), Sound(5, 6, 3), Sound(7, 9, 3)};
    Check(first.Run(Broken(3, 1, 4)) == 4, "the first sound broken");
    Check(first.sounds[0][0].mF00 == 5 && first.sounds[0][1].mF00 == 6 && first.sounds[0][2].mF00 == 5,
          "the first sound takes the second's");
    // A middle sound broken with sounds that play on both sides: the one below wins.
    Event middle;
    middle.sounds = {Sound(1, 2, 3), Sound(3, 4, 3), Sound(5, 6, 3)};
    middle.Run(Broken(3, 3, 4));
    Check(middle.sounds[1][0].mF00 == 1 && middle.sounds[1][1].mF00 == 2, "the sound below stands in first");
    // A sample of another bank that plays stays, and stands in with its own bank.
    Event mixed;
    mixed.sounds = {{Sample{5, 100, 1005, 2, 0}, Sample{104, 200, 1104, 13, 0}}};
    mixed.Run(Broken(13, 104, 104));
    Check(mixed.sounds[0][1].mF00 == 5 && mixed.sounds[0][1].mBankIndex == 2 && mixed.sounds[0][1].mF08 == 1005,
          "a stand-in from another bank brings its bank and length");
    Check(mixed.sounds[0][1].mF04 == 200, "the stand-in keeps the place's weight");
    // No sounds, null arrays.
    Check(tpw::TpiStandInSamples<Sample>(nullptr, nullptr, 0, [](const Sample&) { return true; }) == 0, "no sounds");
    Sample* none[1] = {nullptr};
    const uint32_t zero[1] = {0};
    Check(tpw::TpiStandInSamples<Sample>(none, zero, 1, [](const Sample&) { return false; }) == 0, "an empty sound");
}

int main() {
    TestHeaders();
    TestArabian();
    TestPolarZone();
    TestEdges();
    if (gFailures == 0) std::printf("host_tpi_sound_stand_ins: all checks passed\n");
    return gFailures;
}
