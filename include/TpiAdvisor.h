// TpiAdvisor.h: the TPI Worlds and Rides mod's advisor hook (tpw-mods tpi-worlds; TPW_MOD_HOOK tpi_worlds in ADV_Call).
// A line of the response table (tables/advisor_lines.cpp) is said from the global speech bank (sound set 0) or from the
// current world's own (any other sound set: gAdvisorThemeSoundBank and the lip-sync file under the world's folder). The
// mod's worlds have no speech of their own (Theme Park Inc's zones have none), so that bank is the donor world's
// (platform/tpi_content.h: a world's Speech folder falls back to the donor's), and every line of it names the donor: they
// are the world's welcome (response 1, message _ADVISOR_WELCOME, said on entering a park) and responses 399 to 402 (the
// four _ADVISOR_WELCOME_<world> messages, which no scorer asks for), all of them the world bank's sound 1, "Welcome to
// Halloween World" in Polar Zone. Theme Park Inc's own speech has no welcome for its three zones, so such a line is said
// as the game's welcome that names no world, response 390 ("Welcome to Theme Park World. I'm the advisor round here ...",
// the lobby's first line to a new player, global bank). Header only, so that the host test reads the same rule.
#pragma once

#include <cstdint>

#include "Advisor.h"
#include "TpiWorlds.h"

namespace tpw {

// The response the mod's worlds say in place of a line of the world's own speech: the game's welcome that names no world.
inline constexpr int32_t kTpiAdvisorStandIn = 390;

// The last part of a path (after its last ':', '/' or '\\'), the whole of it with none, "" for nullptr: the world's folder
// name of get_theme_path's answer (the data path, the levels folder and the current theme's name).
inline const char* TpiThemeOfPath(const char* path) {
    if (path == nullptr) return "";
    const char* last = path;
    for (const char* c = path; *c != '\0'; ++c) {
        if (*c == ':' || *c == '/' || *c == '\\') last = c + 1;
    }
    return last;
}

// The line the advisor says for `line` in the world whose level folder is `theme` (any case; nullptr for none), in `out`.
// In one of the mod's worlds (TpiWorldOf), a line of the world's own sound set is said as `standIn` (the row of
// kTpiAdvisorStandIn, nullptr when the table has none): its sound, lip-sync file, sequence, hat and hand from the global
// bank, by `line`'s advisor model (the park's), with `line`'s id and tag. A global line there, and every line in another
// world, is `line` as it is. False, with `out` untouched, when the line is to be left unsaid: a line of the world's own
// speech in one of the mod's worlds with no stand-in, or a stand-in that is not itself a global line.
inline bool TpiAdvisorLine(const AdvisorLine& line, const AdvisorLine* standIn, const char* theme, AdvisorLine& out) {
    if ((line.mWhichAndSoundSet >> 16) == 0 || TpiWorldOf(theme) == nullptr) {
        out = line;
        return true;
    }
    if (standIn == nullptr || (standIn->mWhichAndSoundSet >> 16) != 0) return false;
    out = *standIn;
    out.mId = line.mId;
    out.mWhichAndSoundSet = line.mWhichAndSoundSet & 0xffff;
    out.mTag = line.mTag;
    return true;
}

}  // namespace tpw
