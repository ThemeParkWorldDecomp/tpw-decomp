// TpiUmbrellas.h: the TPI Worlds and Rides mod's umbrellas (TPW_MOD_HOOK tpi_worlds): in the mod's worlds a visitor who
// buys from an Umbrella Shop walks about holding Theme Park Inc's umbrella, as a visitor holds a balloon.
// Theme Park Inc's umbrella shops say UsageInfo.AppearanceEffect 3 ("Give the person an umbrella"); Theme Park World's
// ride database takes only 0 (nothing), 1 (a balloon) and 2 (a costume), so the converted shop's 3 is dropped and the
// shop gives nothing. The converter writes the ids of the shops that said 3, with Theme Park Inc's umbrella lines of its
// levels/Standard.sam (PeepInfo.UmbrellaModifier and DiscardBalloonPctModifier, GenericUmbrellaModifiers.*,
// PersonSpriteUmbrellaModifiers[i].Y and .FP*), into each of the mod's worlds as TpiUmbrellas.sam (assets/tpi_convert.cpp,
// TpiUmbrellasSam), and Theme Park Inc's Generic\Umbrellas\Spr_um sprite as the world's own umbrellas group;
// CWorld::SetupNewLevel reads the file (TpiParseUmbrellas) into gTpiUmbrellas and ESP_LoadSprites loads the sprite after
// the balloons, outside their count (gTpiUmbrellaSprites), so a random balloon is never an umbrella.
// The visitor's balloon fields carry the umbrella: its script (CPerson +0x210) is the umbrella's, made from the balloon's
// script block, and its balloon life (+0x214) is kTpiUmbrellaLife, one more than any balloon's life (25 to 255), which
// never runs down. Both are saved as the balloon's are, so a park keeps its umbrellas with nothing added to the save.
// A build without this hook (the mod off, an older build) reads the umbrella as a balloon (ESP_FixSpriteIDsFromDetail
// brings its sprite number back below the balloons' count) that pops after 256 turns, as a balloon of that life does.
// Header only and pure, so the host test sees what the game does.
#pragma once

