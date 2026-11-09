// RideBadges.h: the port's ride badges, a small corner tag on the Buy Ride panel's picture of a ride (UpdateViewWindow,
// src/game/ui/ui_ridepanel.cpp): "GOLD" on the Gold Edition's 18 rides the base game does not have, and "PS1" on the
// PlayStation rides mod's. Display only: no ride, park or saved game changes. The launcher fills the lists before the
// game starts (platform/ride_badges.h), from the Gold rides and the content rides it lays over the game folder, and sets
// the switch from its Settings box's "Show Ride Badges" (launcher.ini's ride_badges, on unless "0"). Never filled, as in
// the differential tests, every ride has none. Only with the port's fixes.
#pragma once

