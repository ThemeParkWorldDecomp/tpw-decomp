// TpiRideScript.h: the TPI Worlds and Rides mod's start of a ride script (TPW_MOD_HOOK tpi_worlds, CObject::CreateRideScript).
// Theme Park Inc's Golden Ticket rides (Spin Djinn, The Scorpions and others in all three zones) have a ride script that
// loops the ride's O animation and waits for the script variable SPECIAL (number 10) before it plays the C animation and
// starts taking riders. Theme Park World's engine never sets that variable (no Theme Park World ride script waits on it),
// so placed, such a ride stays in its O pose for ever: the parts the O animation hides stay hidden, the first visitor let
// on waits for ever and the queue only grows. Theme Park World's own Golden Ticket rides work as soon as they are placed,
// so in the mod's worlds a ride whose animation set has an O group starts with SPECIAL set. Theme Park Inc's litter bins
// read SPECIAL for something else and have no O group, so they are left as they are. Header only and pure, so the host
// test sees what the game does.
#pragma once

