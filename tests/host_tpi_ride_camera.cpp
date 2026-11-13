// host_tpi_ride_camera.cpp: the TPI Worlds and Rides mod's ride camera node (include/TpiRideCamera.h, TPW_MOD_HOOK tpi_worlds
// in SetupRideRideMatrixAndSuch) on the host, without game data: the ice skates' camera mesh (a head and an accessory) gives
// its head, the head with id 1 wins over an earlier head, any head will do when none has id 1, and a mesh without a head
// gives none. Exits nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "TpiRideCamera.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

struct Node {
    uint32_t mId;
    uint32_t mFlags;
};

static uint32_t Fallback(const std::vector<Node>& nodes) {
    return tpw::TpiRideCameraFallback(static_cast<uint32_t>(nodes.size()), [&nodes](uint32_t i, uint32_t* id, uint32_t* flags) {
        *id = nodes[i].mId;
        *flags = nodes[i].mFlags;
    });
}

int main() {
    using namespace tpw;
    // Theme Park Inc's ice skates (Polar zone ride 6030): a rider's head, then an accessory.
    Check(Fallback({{1, 0xb1}, {1, 0x411}}) == 0, "ice skates: the head");
    // The accessory first: the head is still found.
    Check(Fallback({{1, 0x411}, {1, 0xb1}}) == 1, "accessory first: the head");
    // Two heads: id 1 is the first rider's, wherever it is.
    Check(Fallback({{2, 0x80}, {1, 0x80}}) == 1, "the head with id 1 wins");
    Check(Fallback({{1, 0x80}, {2, 0x80}}) == 0, "the head with id 1 first");
    // Heads, none with id 1: the first of them.
    Check(Fallback({{1, 0x400}, {3, 0x80}, {2, 0x80}}) == 1, "no id 1: the first head");
    // No head: nothing will do, as with the Mac.
    Check(Fallback({{1, 0x411}, {2, 0x131 & ~kRideNodeHead}}) == kRideNodeNone, "no head: none");
    Check(Fallback({}) == kRideNodeNone, "no entries: none");
    // The flag is the head flag and not the camera's.
    Check((kRideNodeHead & kRideNodeCamera) == 0, "head and camera flags differ");

    if (gFailures == 0) std::printf("tpi_ride_camera: all checks passed\n");
    return gFailures;
}
