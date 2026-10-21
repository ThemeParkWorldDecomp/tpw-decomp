// ThingHelpers.h: file-local helpers that src/game/Thing.cpp shares with Advisor.cpp, MechanicHQ.cpp, ResearchLab.cpp,
// StaffHQ.cpp, Weather.cpp and vector2d.cpp. Before the regroup, the functions using them shared a source file (2 files
// in all), which had one copy of each; that copy now lives here, static inline, so each file that includes this header
// keeps its own internal copy.
#pragma once

#include "CMessageCentre.h"
#include "ThingReceivers.h"

// The cell tag every derived constructor passes: map cell id 1 (cell x 0, y 0), built in the high half of r4 on the Mac.
static inline tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> FirstCell() { return {1}; }

// The block the Mac repeats before EACH Subscribe call (not once): the message centre (global 0x1037f6c4) is created when it is NULL. A
// NULL answer of operator new is stored (the centre stays NULL, no headers are built) and Subscribe then runs with a NULL centre; the
// next block tries again. The 29 headers of 0x10 bytes are built by __construct_array on the Mac and by the member array here.
static inline CMessageCentre* MessageCentreForSubscribe() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

static inline void Subscribe(CThing& thing, int32_t type) { MessageCentreForSubscribe()->Subscribe(thing, static_cast<MsgType>(type)); }

static inline CMessageCentre* Centre() {
    if (gMessageCentre == nullptr) gMessageCentre = new (std::nothrow) CMessageCentre;
    return gMessageCentre;
}

static inline void SubscribeThing(CThing& thing, int32_t type) {
    Centre()->Subscribe(thing, static_cast<MsgType>(type));
}
