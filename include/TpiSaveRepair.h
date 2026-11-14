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
// Converter 16 makes the walkways footbridge cells like Lost Kingdom's instead (their quads hidden again, base.map
// attribute 0x80 alone: CMapCell::Reset's mode 0x1e, which guests cross on their way to something and nothing is built
// on), since a path built under the deck is hidden by it and parks were left with gaps no guest could cross (the maintainer's Land
// of Invention park, 0.5.3: no path on the two cells over mid-river), and the shown quads fought the deck. A park keeps
// the attribute byte and mode of every cell it saved, so on loading, each walkway cell (the level's attribute 0x80 alone
// over a quad the level hides) that belongs to nothing and holds land, a path or a hidden quad's mode 2, 5 or 7 is made a
// footbridge as a new park makes it: a path loses its links both ways first (CMapCell::Delete), the quad is the level's
// again and the cell takes the level's attribute and is reset (mode 0x1e), keeping its saved flag 0x40
// (TpiRepairedFlags), with its track cell when that belongs to nothing (mode 7). A cell something is built on stays as
// it is, and a second load changes nothing.
// The same mod's repair of the shops of parks saved before converter 15: a park keeps each object type's control
// (CControlManager, made once from the record when the type is first seen), so its shops keep the RipOffOK converter 14
// gave them (0 where the shops folder named none) and visitors still walk away from their prices; on loading, a shop
// type's control whose RipOffOK is below its record's gets the record's. And a photo booth placed before converter 15
// keeps the object flags its record gave it then (CObject::mFlags, saved): without flag 4 (Info.IsChoosable) no visitor
// chooses it, and the state it was built in (kStateThree, which ModelState skips); a placed shop whose record now says
// Info.IsChoosable gets flag 4, and state 3 becomes the normal state a new booth gets.
// And the same mod's repair of the research of parks saved before converter 14: converter 14 added Land of Invention's
// staff room (features/nstaff) in the id its security camera had (7730), moving the camera to the next one (7740). A
// park keeps each type's control (mAvailable, made from the record when the type is first seen) and the research
// projects its lab made when it started (CResearchLab::SetupProjects, new parks only), so in a park started before it
// the staff room keeps the camera's control (unresearched, mAvailable 0, so no build list shows it, and the camera's
// price, CostOfUpgrade) and the camera's project, while the camera's new id has no project and can never be researched.
// On loading, the control of a type whose record has no research cost, saved unavailable or named by a stale project
// (both mean it was made from another record; a statistics upgrade, upgrade level 1 or more, is not stale), is made again from its record as CheckAllControlsInitialised makes it,
// available as SetupProject makes it in a new park, keeping the park's count and first build turn; a project (not a
// statistics upgrade) whose type's record now has no research cost goes to the type it was made for (of its category,
// with its score as research cost and no project of its own), keeping its progress, and a finished one makes that type
// available; and each type with a research cost, no project and no availability gets a new one.
// Header only and pure, so the host test sees the rule the game uses (src/game/save/SaveLoad.cpp).
#pragma once

