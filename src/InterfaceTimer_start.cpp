// InterfaceTimer::Start, main 0x101d8460 (60 bytes), unit w37c.
#include "UI_Base.h"
void InterfaceTimer::Start() {
    mRunning = 1;
    mStart = IveGotTicks();
}
