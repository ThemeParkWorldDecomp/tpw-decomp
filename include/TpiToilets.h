// TpiToilets.h: the TPI Worlds and Rides mod's men's and women's toilets (TPW_MOD_HOOK tpi_worlds).
// Theme Park Inc has two toilets, mtoilet ("for boys", Info.GenderSpecific 1) and ftoilet ("for girls", 2); the converter
// writes both as Theme Park World features (features/mtoilet and features/ftoilet, "Toilet - Male" and "Toilet - Female"),
// toilets every visitor uses. In the mod's worlds, while the park has at least one of each (built, open or not), a man or
// boy uses only the men's and a woman or girl only the women's; with one kind only, everyone uses it, as before. A
// visitor's sex is its look's (include/TpiGuests.h, TpiGuestFemale). Theme Park World's own toilet stays everyone's.
// Applied where a visitor picks a ride (CPerson::MakeMajorDecision, CPerson::LookForMinorDest) and where it reaches the
// back of the queue (CPerson::ProcessStateWalksToMajor), so a visitor never joins the other kind's queue. Nothing is
// saved: the kind is the item's folder, the counts are the park's objects, the sex the look.
// Header only and pure, so the host test sees what the game does.
#pragma once

