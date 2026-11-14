// TpiGuests.h: the TPI Worlds and Rides mod's extra visitors (TPW_MOD_HOOK tpi_worlds): Theme Park Inc's kids, parents
// and grandparents walking among Theme Park World's kids in the mod's worlds, a look and nothing else; there, Theme Park
// World's TA and CH are not worn, since Theme Park Inc's BD and GB are the same two kids.
// The game's visitors are the sprites of the kids group (gESPGroups[0], "kids") and, on a ride, of the kids' heads
// (gESPGroups[1]); a visitor's sprite number (CPerson +0x20) is a random pick below the group's count, saved with the
// park. The converter writes Theme Park Inc's generic visitors into each of the mod's worlds as that theme's own kids
// and kidsheads (assets/tpi_convert.cpp, ConvertGuests). In the mod's worlds those are loaded after the group, outside
// its count (ESP_FindTpiExtraGuests), so every pick, the player's number of kid sprites and the saved sprite numbers stay
// what they were. Each sprite of the kids set, the group's and the extras, is a look with a kind (kid, parent,
// grandparent), a sex and whether it is worn, by its name (kTpiLookTags); a visitor wears the look its thing index gives
// (TpiGuestLook: 70 in 100 a kid look, 20 a parent's, 10 a grandparent's, as Theme Park Inc's levels/Standard.sam
// spreads its generations, each worn look of a kind as often as the others), only where its sprite script is made
// (TpiGuestSpriteOf).
// A script's sprite number past the group's count is saved, but every build without this hook brings it back below the
// count when the park is loaded (ESP_FixSpriteIDsFromDetail), so the visitor is a kid there.
// Header only and pure, so the host test sees what the game does.
#pragma once

