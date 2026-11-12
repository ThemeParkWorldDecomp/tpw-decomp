// tpi_sfx.h: Theme Park Inc's sound event maps (Sound/cat_*SFX.map) for Theme Park World's sound code. The two games
// share the format (src/sound: the map's reader), so the converter copies the maps as they are, except where a sound
// was tuned for the PC's 3D sound and is wrong without it: the port has no distance fade (platform/sound_module.cpp),
// so a loop that Theme Park Inc made loud and let DirectSound3D fade with distance plays at its full volume wherever
// the camera is. The converter (tpi_convert.h) lowers those sounds' volumes here.
//
// The layout, little-endian: a 20-byte header (a 16-byte id, then the version: 0 in Theme Park World, 1 in Theme Park
// Inc), a u32, then the category count; the categories (24 bytes: event count, a pointer, u16, u16 flags, three
// floats); then, category by category, their events (20 bytes: id, sound count, a pointer, priority, u16 flags, u16);
// then, event by event, their sounds (42 bytes: sample count in the low 16 bits of a u32, branch count, a pointer, then
// the bytes VolumeMin, VolumeMax, PitchMin, PitchMax at +12..+15, the rest timing, weight and distance); then, sound by
// sound, its samples (16 bytes each) and its branches (8 bytes each). Nothing follows the last sound's branches.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

struct TpiSfxVolumePatch {
    uint32_t event = 0;       // the event's id
    uint8_t fromVolume = 0;   // the VolumeMin and VolumeMax every one of its sounds must have now
    uint8_t toVolume = 0;     // the volume they get (both bytes)
};

// Applies `patch` to the map in `map`: true when the event was found and every one of its sounds had fromVolume as
// both its VolumeMin and VolumeMax, and those bytes now hold toVolume. Otherwise false with a reason, and `map` is
// left as it was: a map that does not parse whole, an event that is not there or is there more than once, or a sound
// whose volumes are not the expected ones (a copy that differs from the one the patch was made for).
bool PatchTpiSfxVolume(std::vector<uint8_t> &map, const TpiSfxVolumePatch &patch, std::string &reason);

}   // namespace tpw::assets
