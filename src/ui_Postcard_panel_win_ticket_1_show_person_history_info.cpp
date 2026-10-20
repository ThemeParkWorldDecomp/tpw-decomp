// show_person_history_info 0x1014f65c: when the pointed thing is a staff member, guest or such (kind byte at +2 is
// 1, 4 to 8 or 0x12), report all its events. Always answers 1.
#include <cstdint>

#include "intrface.h"
#include "ThingReceivers.h"
#include "keytable_actions.h"

int32_t show_person_history_info() {   // 0x1014f65c
    CThing* thing = gIntrface.mPointedThing;
    if (thing != nullptr) {
        uint8_t kind = reinterpret_cast<uint8_t*>(thing)[2];
        bool show = kind == 1 || kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7 || kind == 0x12;
        if (show) reinterpret_cast<CPeep*>(thing)->ReportAllEvents();
    }
    return 1;
}
