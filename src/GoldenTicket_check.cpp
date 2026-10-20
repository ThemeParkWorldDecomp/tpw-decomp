// CGoldenTicketControl::CheckForTickets (main app, 0x100fbd2c, 144 bytes; unit w129).
#include "FS_Online.h"
#include "ObjectParts.h"

// The game type object is a function-local static of an inline accessor (TheGameType, include/FS_Online.h): the guard byte at 0x102b3aa4 is tested as a
// signed byte and CGameType's constructor (0x1016f5d8) runs on first use. Only the normal game (type 0, compared after the construction) checks the local,
// global and secret tickets, in that order.
void CGoldenTicketControl::CheckForTickets() {
    if (TheGameType().mType == 0) {
        CheckForLocal();
        CheckForGlobal();
        CheckForSecret();
    }
}
