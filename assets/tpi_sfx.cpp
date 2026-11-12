// tpi_sfx.cpp: volume changes to Theme Park Inc's sound event maps (tpi_sfx.h). The map is walked whole, every count
// checked against the bytes left, so a patch is applied only to a map that parses to its last byte.
#include "tpi_sfx.h"

namespace tpw::assets {

namespace {

constexpr size_t kHeader = 20 + 8;   // id and version, then a u32 and the category count
constexpr size_t kCategory = 24;
constexpr size_t kEvent = 20;
constexpr size_t kSound = 42;
constexpr size_t kSample = 16;
constexpr size_t kBranch = 8;
constexpr size_t kVolumeMin = 12;   // in a sound; VolumeMax follows

uint32_t Get32(const std::vector<uint8_t> &b, size_t at) {
    return uint32_t(b[at]) | uint32_t(b[at + 1]) << 8 | uint32_t(b[at + 2]) << 16 | uint32_t(b[at + 3]) << 24;
}

// Reserves `count` records of `size` bytes at `at`: false when they do not fit before the end.
bool Take(const std::vector<uint8_t> &b, size_t &at, uint64_t count, size_t size) {
    const uint64_t bytes = count * size;
    if (count > b.size() || bytes > b.size() - at)
        return false;
    at += size_t(bytes);
    return true;
}

}   // namespace

bool PatchTpiSfxVolume(std::vector<uint8_t> &map, const TpiSfxVolumePatch &patch, std::string &reason) {
    const std::vector<uint8_t> &b = map;
    if (b.size() < kHeader) {
        reason = "the map is shorter than its header";
        return false;
    }
    size_t at = kHeader;
    const uint32_t categories = Get32(b, 24);
    const size_t categoriesAt = at;
    if (!Take(b, at, categories, kCategory)) {
        reason = "the map ends inside its categories";
        return false;
    }
    std::vector<size_t> found;   // offsets of the patched event's sounds
    int matches = 0;
    for (uint32_t c = 0; c < categories; ++c) {
        const uint32_t events = Get32(b, categoriesAt + c * kCategory);
        const size_t eventsAt = at;
        if (!Take(b, at, events, kEvent)) {
            reason = "the map ends inside category " + std::to_string(c) + "'s events";
            return false;
        }
        for (uint32_t e = 0; e < events; ++e) {
            const uint32_t id = Get32(b, eventsAt + e * kEvent);
            const uint32_t sounds = Get32(b, eventsAt + e * kEvent + 4);
            const size_t soundsAt = at;
            if (!Take(b, at, sounds, kSound)) {
                reason = "the map ends inside event " + std::to_string(id) + "'s sounds";
                return false;
            }
            if (id == patch.event) {
                ++matches;
                for (uint32_t s = 0; s < sounds; ++s)
                    found.push_back(soundsAt + s * kSound);
            }
            for (uint32_t s = 0; s < sounds; ++s) {
                const size_t sound = soundsAt + s * kSound;
                if (!Take(b, at, Get32(b, sound) & 0xffff, kSample) || !Take(b, at, Get32(b, sound + 4), kBranch)) {
                    reason = "the map ends inside event " + std::to_string(id) + "'s samples";
                    return false;
                }
            }
        }
    }
    if (at != b.size()) {
        reason = std::to_string(b.size() - at) + " bytes follow the map's last sound";
        return false;
    }
    if (matches != 1 || found.empty()) {
        reason = matches == 0 ? "event " + std::to_string(patch.event) + " is not in the map"
                 : matches > 1 ? "event " + std::to_string(patch.event) + " is in the map " + std::to_string(matches) + " times"
                               : "event " + std::to_string(patch.event) + " has no sounds";
        return false;
    }
    for (size_t sound : found)
        if (b[sound + kVolumeMin] != patch.fromVolume || b[sound + kVolumeMin + 1] != patch.fromVolume) {
            reason = "event " + std::to_string(patch.event) + " has volumes " + std::to_string(b[sound + kVolumeMin]) +
                     ".." + std::to_string(b[sound + kVolumeMin + 1]) + ", not " + std::to_string(patch.fromVolume);
            return false;
        }
    for (size_t sound : found)
        map[sound + kVolumeMin] = map[sound + kVolumeMin + 1] = patch.toVolume;
    return true;
}

}   // namespace tpw::assets
