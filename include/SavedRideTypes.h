// SavedRideTypes.h: the ride types a park's saved rides need (the port's fixes heldpark and rideloadskip). A park keeps
// each ride it has as a record in its rides module (LoadRides, src/game/rides/TPRide.cpp): the slot's marker, then the
// record's type number, place and flags (0x26 bytes), its node and embedded counts and words (0x10 bytes), its embedded
// and node words, and its animation time controls (0x2c bytes each). How many time controls a record has is not in the
// file: it is its type's. A record whose type the game has not got (a Theme Park Inc ride the imported worlds lack) has no
// instance to restore, and the rides after it were lost with it, so the track rides restoring their cars from those slots
// faulted. CSaveLoad::Load walks the records before any module is read (MissingSavedRideType) and refuses such a park as
// LoadHeader refuses one whose ride record needs a missing ride. Header only and pure, so the host tests see what the game
// does.
#pragma once

