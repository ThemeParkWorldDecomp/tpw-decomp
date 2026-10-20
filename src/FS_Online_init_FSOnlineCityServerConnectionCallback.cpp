// FSOnlineCityServerConnectionCallback 0x101733b0 (124): makes the online system when there is none (operator new(0x404) and the
// constructor, the pointer stored), then hands the four arguments to its CityServerConnectionCallback.
#include "FS_Online.h"

void FSOnlineCityServerConnectionCallback(int32_t a, int32_t b, void* c, void* d) {
    if (gOnlineSystem == nullptr) {
        gOnlineSystem = new CFSOnlineSystem();
    }
    gOnlineSystem->CityServerConnectionCallback(a, b, c, d);
}
