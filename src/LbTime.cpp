#include "LbTime.h"
#include "platform/samsutils.h"

uint32_t LbTime_GetClock() {
    return static_cast<uint32_t>(SamsUtilities::UTimer::GetAbsolute() / 1000);
}
