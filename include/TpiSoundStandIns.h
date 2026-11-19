// TpiSoundStandIns.h: the TPI Worlds and Rides mod's broken music samples (TPW_MOD_HOOK tpi_worlds).
// Two of Theme Park Inc's retail music banks end in samples that are not there: Arabian Nights' Music/MusicHD.sdt has 143
// offsets but only the first 103 point at a sample's 0x28-byte header (the file stops at 0x26e6000, and from the 104th on
// the offsets land in MPEG data of another encoding, so the "headers" are noise: sizes of up to 4 GB, rates such as 27216,
// 87 bits, no MPEG flag), and Polar Zone's (water) has 116 of which the last 15 (from the 102nd, the file stops at exactly
// 30 MB and repeats its own earlier clusters) are the same. Every Theme Park Inc copy has these files byte for byte (a
// Theme Park Inc install and the SimCoaster USA disc). The park music's event (2, a branching sentence)
// lists them: Arabian Nights' sounds 6 (how busy 75 to 90) and 7 (91 to 100) are made of them only and its sound 5 (59 to
// 74) has 14 of them among its 31; Polar Zone's sound 5 (77 to 90) has 15 of 38. The game started such a sample as
// 8-bit PCM read from the MPEG data, a burst of noise that ended at once, and the sentence started it again: one sample
// over and over, 100 times a second ("fast repeats of the same sound", the maintainer, 2026-11-17).
// In the mod's worlds a streaming bank's sample whose header is not one is kept with no size (so it never starts, as the
// codec factory refuses a sample of no size), and each sample of a category's map that names such a sample stands in for
// another of the same event that plays: one of its own sound's, in their order, else one of the nearest sound before it
// that has any (the level of music below), else after it. Only the sample's number, length and bank change; its place,
// weight and flags stay, so the sound is chosen and its samples drawn as before. Nothing is saved.
// Header only and pure, so the host test sees what the game does.
#pragma once

