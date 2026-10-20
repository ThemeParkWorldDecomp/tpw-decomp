// CThing::GetThingId 0x1012e1f8 and CThing::ReceiveMessage 0x1012e470 (main app, unit w107; the inventory files them under Thing.cpp).
#include "AdvisorQueue.h"
#include "ThingReceivers.h"
#include "analyser.h"

// The Mac returns the two-byte tag through a hidden pointer and copies the half-word at +0 into it.
tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> CThing::GetThingId() const { return {mThingId}; }

// A switch on the kind byte through a jump table at 0x102a42d0; each case hands the thing and the message, unchanged, to the class's
// ReceiveMessage (a direct call). Kinds 0 and 2 and everything above 0x13 do nothing.
void CThing::ReceiveMessage(const CMsg& message) {
    switch (mKind) {
    case 1: reinterpret_cast<CPerson*>(this)->ReceiveMessage(message); break;
    case 3: reinterpret_cast<CObject*>(this)->ReceiveMessage(message); break;
    case 4: reinterpret_cast<CMechanic*>(this)->ReceiveMessage(message); break;
    case 5: reinterpret_cast<CHandyman*>(this)->ReceiveMessage(message); break;
    case 6: reinterpret_cast<CEntertainer*>(this)->ReceiveMessage(message); break;
    case 7: reinterpret_cast<CGuard*>(this)->ReceiveMessage(message); break;
    case 8: reinterpret_cast<CResearcher*>(this)->ReceiveMessage(message); break;
    case 9: reinterpret_cast<CStaffHQ*>(this)->ReceiveMessage(message); break;
    case 10: reinterpret_cast<CMechanicHQ*>(this)->ReceiveMessage(message); break;
    case 11: reinterpret_cast<CAdvisor*>(this)->ReceiveMessage(message); break;
    case 12: reinterpret_cast<CTagSystem*>(this)->ReceiveMessage(message); break;
    case 13: reinterpret_cast<CParkAnalyser*>(this)->ReceiveMessage(message); break;
    case 14: reinterpret_cast<CResearchLab*>(this)->ReceiveMessage(message); break;
    case 15: reinterpret_cast<CWeather*>(this)->ReceiveMessage(message); break;
    case 16: reinterpret_cast<CBankAccount*>(this)->ReceiveMessage(message); break;
    case 17: reinterpret_cast<CUIMsgReceiver*>(this)->ReceiveMessage(message); break;
    case 18: reinterpret_cast<COnlinePerson*>(this)->ReceiveMessage(message); break;
    case 19: reinterpret_cast<CChallengeHQ*>(this)->ReceiveMessage(message); break;
    default: break;
    }
}
