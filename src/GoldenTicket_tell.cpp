// CGoldenTicketControl::TellTheWorld (main app; unit w183): the three overloads (local 0x100fcbb0, global 0x100fcf1c, secret 0x100fd4f4) that
// announce a won golden ticket. Each traces what was won, then (by what EarnedTicket answered) posts one CMsgAdvisor to the message centre, and
// always ends with TellIfFirst. Kept out of src/GoldenTicket.cpp, whose test fakes the three.
#include <new>

#include "AdvisorQueue.h"
#include "AdvisorScoring.h"
#include "CMessageCentre.h"
#include "Debug.h"
#include "MsgAdvisor.h"
#include "ObjectParts.h"
#include "analyser.h"

namespace {

// The message centre, made on the first use at each post (0x1d0 bytes, the array of 0x1d subscription headers), as src/GoldenTicket.cpp does.
CMessageCentre* TicketMessageCentre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

// One announcement: a request (thing id 0, secondary 0, the response number, not override-only-once, score -1) wrapped in a CMsgAdvisor and
// posted; the Mac's inlined destructor is the one store of the vtable. `id` is the AdvisorMessage number (names unknown).
void Announce(int32_t id, int32_t responseNo) {
    const CAdvisorRequest request(static_cast<AdvisorMessage>(id), tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>{0}, 0, responseNo, false, -1);
    CMsgAdvisor message(request);
    TicketMessageCentre()->PostMessage(message);
}

}  // namespace

// 0x100fcbb0 (660 bytes): `type` is the ticket's number within its group (the response number), `won` what EarnedTicket answered (1 ticket, 2 and
// key, 3 and park). Any other won (0, negative, 4 and above) is traced as unrecognised and announces nothing.
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeLocal type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Local golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Local ticket only\n");
        Announce(0xd7, t);
    } else if (w == 2) {
        CTRACE(3, "Local ticket and key\n");
        Announce(0xd8, t);
    } else if (w == 3) {
        CTRACE(3, "Local ticket, key and park\n");
        Announce(0xd9, t);
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Local) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}

// 0x100fcf1c (1392 bytes): won 1 to 3 as the local one (ids 0xdc to 0xde); won 4 is the global award, "moved here", announced by `type` (0 coaster
// height 0xea, 1 gokart excitement 0xeb, 2 water length 0xec, 3 big park 0xed; response number -1; any other type nothing).
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeGlobal type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Global golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Global ticket only\n");
        Announce(0xdc, t);
    } else if (w == 2) {
        CTRACE(3, "Global ticket and key\n");
        Announce(0xdd, t);
    } else if (w == 3) {
        CTRACE(3, "Global ticket, key and park\n");
        Announce(0xde, t);
    } else if (w == 4) {
        CTRACE(3, "Global ticket award moved here\n");
        if (t == 0) {
            CTRACE(3, "Global ticket award coaster height\n");
            Announce(0xea, -1);
        } else if (t == 1) {
            CTRACE(3, "Global ticket award gokart excitement\n");
            Announce(0xeb, -1);
        } else if (t == 2) {
            CTRACE(3, "Global ticket award water length\n");
            Announce(0xec, -1);
        } else if (t == 3) {
            CTRACE(3, "Global ticket award big park\n");
            Announce(0xed, -1);
        }
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Global) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}

// 0x100fd4f4 (660 bytes): as the local one, ids 0xe1 to 0xe3.
void CGoldenTicketControl::TellTheWorld(GoldenTicketTypeSecret type, GoldenTicketTypeWon won) {
    const int32_t t = static_cast<int32_t>(type);
    const int32_t w = static_cast<int32_t>(won);
    CTRACE(3, "Won a Secret golden ticket (type - %d, num - %d)\n", w, t);
    if (w == 1) {
        CTRACE(3, "Secret ticket only\n");
        Announce(0xe1, t);
    } else if (w == 2) {
        CTRACE(3, "Secret ticket and key\n");
        Announce(0xe2, t);
    } else if (w == 3) {
        CTRACE(3, "Secret ticket, key and park\n");
        Announce(0xe3, t);
    } else {
        CTRACE(3, "CGoldenTicketControl::TellTheWorld (Secret) is being passed an unrecognised type value (%d)\n", w);
    }
    TellIfFirst();
}
