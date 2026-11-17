// tpi_lobby_music.h: the island screen's music for a converted Theme Park Inc world. Theme Park Inc has no island
// screen, so it has no lobby music; Theme Park World's lobby plays levels/<world>/Sound/LobbyMusicHD.sdt through the
// category cat_locallobbymusic (src/game/park/ParkSound.cpp SND_RegisterLocalLobbySounds), and each of its four worlds
// made that bank from its own park music: three segments of one mood group of the park music, all of one length
// (Fantasy's are its park music's samples 69, 70 and 71 byte for byte), which the lobby's one looping event picks from
// at random. The converter (tpi_convert.h) makes the same from the world's own Theme Park Inc park music
// (Music/MusicHD.sdt and Music/cat_musicSFX.map, the same formats as Theme Park World's).
//
// The bank (.sdt), little-endian: a u32 count, the u32 offsets of the samples, then at each offset a 0x28-byte header
// (+0 its size 0x28, +4 the data's size, +8 a 16-byte name, +0x18 u16 rate, +0x1a u8 bits, +0x1b u8 flags, bit 0
// stereo, +0x20 the decoded size) followed by the sample's MPEG audio. The maps' layout is tpi_sfx.h's.
//
// Which segments: for each mood group (a sound of the map's events), its usable samples (a whole bank entry: the
// header's size, 22050 Hz, 16 bits, stereo, MPEG-2 Layer II, the data inside the file; and a length of 15 to 25 s, the
// band Theme Park World's lobby loops are in), and among them the largest set within 1% of one sample's length (that of
// the earliest sample in the map on a tie). The group with the largest such set wins (the earlier one on a tie), and
// its first three samples of that set in the map's order are the lobby's. The entries are copied as they are.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tpw::assets {

struct TpiLobbyPick {
    uint32_t group = 0;    // the mood group, 1-based in the map's order
    uint32_t sample = 0;   // the park music bank's sample, 1-based
    uint32_t length = 0;   // its length in ms, as the park music map gives it
    std::string name;      // its name in the bank
};

struct TpiLobbyMusic {
    std::vector<uint8_t> bank;      // LobbyMusicHD.sdt
    std::vector<uint8_t> bankMap;   // cat_locallobbymusicBANK.map
    std::vector<uint8_t> sfxMap;    // cat_locallobbymusicSFX.map
    std::vector<TpiLobbyPick> picks;
    uint32_t unusable = 0;          // the map's samples whose bank entry is not a whole one
};

// The lobby's event id (lobby_island.cpp sends it to the music category) and its flags: looping (4), not positional
// (0x200), and 2, as Theme Park World's four lobby maps have them.
constexpr uint32_t kTpiLobbyMusicEvent = 1;
constexpr uint16_t kTpiLobbyMusicFlags = 0x0206;

// Builds the lobby music from the park music's map `musicMap` and bank `musicBank`. False with a reason, and `out`
// left empty, when the map does not parse whole or no group has a usable sample.
bool BuildTpiLobbyMusic(const std::vector<uint8_t> &musicMap, const std::vector<uint8_t> &musicBank, TpiLobbyMusic &out,
                        std::string &reason);

}   // namespace tpw::assets
