// missing_rides.h: what the player is told when a park needs rides the game has not got (the port's fix heldpark, in
// src/game/save/heldpark.cpp). The game refuses such a park (CSaveLoad::LoadHeader, kErrorMissingRides) and, with the fix,
// keeps the rides it lacked in gPortMissingRides; this names them: the Gold Edition's (the launcher's Gold rides) by count,
// and a mod's content rides (platform/content_overlay.h) by the titles their .sam files give, with the mod's name. A park
// whose ride records need a ride type the game has not got (CSaveLoad::Load, gPortMissingRideType) is told by its number.
// Defined in platform/content_overlay.cpp.
#pragma once

#include <cstdint>
#include <string>

namespace tpw {

// Where the park was being opened, which the message's last sentence follows.
enum class MissingRidesPlace {
    kOwnPark,      // the player's most recent park, as a game starts: it is not opened, and the game goes back to the menu
    kOnlinePark,   // a park downloaded to visit online: likewise
    kLoadedPark    // a park loaded from inside another (the load panel, the quick load key): the park the player is in goes on
};

// The message for a park that needs the rides whose bits `missing` has (a park file's ride record: 0x20 bytes, ride
// number n is bit (n - 1) & 7 of byte (n - 1) >> 3), or, when `missingType` is not 0, whose ride records need that ride
// type, which no loaded ride carries (CSaveLoad::Load, gPortMissingRideType): Theme Park Inc's (5000 to 7999, the
// converter's world blocks; most likely the SimCoaster Bonus Disk's, which an import may lack) or another. Plain text,
// English, under 500 characters (the error dialog's limit), one line.
std::string MissingRidesText(const uint8_t* missing, MissingRidesPlace place, uint32_t missingType = 0);

}  // namespace tpw
