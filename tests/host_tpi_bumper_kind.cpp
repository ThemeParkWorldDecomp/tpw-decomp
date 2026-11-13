// host_tpi_bumper_kind.cpp: the TPI Worlds and Rides mod's bumper ride kind (include/TpiBumperKind.h, TPW_MOD_HOOK
// tpi_worlds in LLCR_AddRideToDB) on the host, without game data: every Theme Park World kind and 0 are kept, every other
// kind becomes the stand-in, and the stand-in is a kind whose template row exists. Exits nonzero on the failed checks' count.
#include <cstdint>
#include <cstdio>
#include <string>

#include "TpiBumperKind.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    using namespace tpw;
    // The template rows: BMP_CreateRide reads row -kind - 1 of 14.
    for (int32_t kind = kBumperKindFirst; kind >= kBumperKindLast; --kind) {
        Check(-kind - 1 >= 0 && -kind - 1 < 14, "kind " + std::to_string(kind) + " has a row");
        Check(TpiBumperKind(kind) == kind, "kind " + std::to_string(kind) + " kept");
    }
    Check(-kBumperKindStandIn - 1 >= 0 && -kBumperKindStandIn - 1 < 14, "the stand-in has a row");
    Check(TpiBumperKind(0) == 0, "0 (not a bumper ride) kept");

    // Kinds past either end: The Scorpions' -15 and made-up ones.
    for (int32_t kind : {-15, -16, -100, INT32_MIN, 1, 14, INT32_MAX}) {
        Check(TpiBumperKind(kind) == kBumperKindStandIn, "kind " + std::to_string(kind) + " built as the stand-in");
    }

    if (gFailures == 0) std::printf("tpi_bumper_kind: all checks passed\n");
    return gFailures;
}
