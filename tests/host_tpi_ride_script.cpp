// host_tpi_ride_script.cpp: the TPI Worlds and Rides mod's start of a ride script (include/TpiRideScript.h, TPW_MOD_HOOK
// tpi_worlds in CObject::CreateRideScript) on the host, without game data: which scripts start with SPECIAL set, for
// made-up O group flags and variable counts, and that the numbers match the game's (the O group is the twelfth letter of
// CDILSMEUWBRO, SPECIAL the eleventh variable). Exits nonzero on the first failed check's count.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "TpiRideScript.h"

static int gFailures = 0;

static void Check(bool ok, const std::string& what) {
    if (!ok) {
        ++gFailures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

int main() {
    using namespace tpw;
    // The numbers: the O group's index in the letters of the ride animation kinds, SPECIAL after the ten names before it.
    const char kinds[] = "CDILSMEUWBRO";
    Check(std::strchr(kinds, 'O') - kinds == kRideAnimKindO, "O is kind 11");
    const char* names[] = {"LETMEON", "LETMEOFF", "CAPACITY", "DURATION", "BREAKSTAT", "ONRIDE", "RIDECLOSED", "BROKEN", "WORN", "RUNNING", "SPECIAL"};
    Check(std::strcmp(names[kRideScriptVarSpecial], "SPECIAL") == 0, "SPECIAL is variable 10");

    // A ride with an O animation and a full variable table starts with SPECIAL set, whichever of the two group bits is on.
    for (uint32_t flags : {1u, 2u, 3u, 0x7u, 0xffffffffu}) {
        Check(TpiRideScriptStartsSpecial(flags, 19), "O group flags " + std::to_string(flags) + ": set");
    }
    // No O animation (a litter bin, any Theme Park World ride): left as the file gives it, whatever the other bits.
    for (uint32_t flags : {0u, 4u, 0x100u, 0xfffffffcu}) {
        Check(!TpiRideScriptStartsSpecial(flags, 19), "O group flags " + std::to_string(flags) + ": left");
    }
    // A script too short to have SPECIAL is never written past its end.
    for (int32_t count : {-1, 0, 1, 10}) {
        Check(!TpiRideScriptStartsSpecial(3u, count), "variables " + std::to_string(count) + ": left");
    }
    Check(TpiRideScriptStartsSpecial(3u, 11), "variables 11: set");

    if (gFailures == 0) std::printf("tpi_ride_script: all checks passed\n");
    return gFailures;
}
