// LayoutReplayCoasterSpeed 0x10059120 (80 B): sets the coaster's speed, then marks its object (if any) with flag 0x8000.
#include <cstdint>

#include "TPCoaster.h"
#include "rsse.h"

void LayoutReplayCoasterSpeed(uint32_t instance, uint32_t speed);

void LayoutReplayCoasterSpeed(uint32_t instance, uint32_t speed) {
    CoasterSetSpeed(instance, speed);
    uint32_t* object = static_cast<uint32_t*>(gObjectTable[instance]);   // no bounds check, like the original
    if (object != nullptr) *object |= 0x8000;
}
