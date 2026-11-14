// TpiGuests.h: the TPI Worlds and Rides mod's extra visitors (TPW_MOD_HOOK tpi_worlds): Theme Park Inc's grandparents
// walking among Theme Park World's kids in the mod's worlds, a look and nothing else.
// The game's visitors are the sprites of the kids group (gESPGroups[0], "kids") and, on a ride, of the kids' heads
// (gESPGroups[1]); a visitor's sprite number (CPerson +0x20) is a random pick below the group's count, saved with the
// park. The converter writes Theme Park Inc's grandparents into each of the mod's worlds as that theme's own kids and
// kidsheads (assets/tpi_convert.cpp, ConvertGuests). In the mod's worlds those are loaded after the group, outside its
// count (ESP_FindTpiExtraGuests), so every pick, the player's number of kid sprites and the saved sprite numbers stay
// what they were; a visitor wears an extra one by its thing id (TpiGuestPick) only where its sprite script is made
// (TpiGuestSpriteOf). A script's sprite number past the group's count is saved, but every build without this hook
// brings it back below the count when the park is loaded (ESP_FixSpriteIDsFromDetail), so the visitor is a kid there.
// Header only and pure, so the host test sees what the game does.
#pragma once

