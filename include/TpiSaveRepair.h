// TpiSaveRepair.h: the TPI Worlds and Rides mod's repairs of parks saved before a converter showed more of a world's ground
// (TPW_MOD_HOOK tpi_worlds). A park file keeps every map cell's mode (CCellBase::Serialise), and a cell whose height field
// quad was hidden when the park started was given the hidden quad's mode by CMapCell::Reset (2, 0x1e or 7, from the
// attribute byte): no land to buy and no path. Converter 14 shows the quads of the bridges' walkways and ramps as land
// (the walkable bridges) and those of Land of Invention's gate, so a new park gets them as ordinary land, but a park saved
// with converter 12 still holds the hidden quad's mode on them. Loading such a park draws each cell
// again from its saved mode, and a mode 7 cell gets the plain tile, which hides its quad once more (SetHeightfieldTexture),
// so the bridges stay hidden ground that no path can cross. On loading a park in one of the mod's worlds, each map cell
// that still has the hidden quad's mode of its attribute, belongs to nothing and is not a path edge, while the level's own
// quad (gHeightfieldCopy, kept as the level was read) is shown, gets that quad back and is reset as a new park resets it
// (CMapCell::Reset), keeping whether it was inside the park (flag 0x40); its track cell is reset too when it has the hidden
// quad's mode 7. A cell the park has built on never matches, and a second load changes nothing.
// The same mod's repair of the shops of parks saved before converter 15: a park keeps each object type's control
// (CControlManager, made once from the record when the type is first seen), so its shops keep the RipOffOK converter 14
// gave them (0 where the shops folder named none) and visitors still walk away from their prices; on loading, a shop
// type's control whose RipOffOK is below its record's gets the record's. And a photo booth placed before converter 15
// keeps the object flags its record gave it then (CObject::mFlags, saved): without flag 4 (Info.IsChoosable) no visitor
// chooses it, and the state it was built in (kStateThree, which ModelState skips); a placed shop whose record now says
// Info.IsChoosable gets flag 4, and state 3 becomes the normal state a new booth gets.
// Header only and pure, so the host test sees the rule the game uses (src/game/save/SaveLoad.cpp).
#pragma once

