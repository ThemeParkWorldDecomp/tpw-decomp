// host_map_quit.cpp: quitting with the park map open. GAMESYSTEM_Close runs UI_CloseSystem after CGame::ExitLevel has deleted
// the world, and the map's destroy message (CUI2DMap::MapCallback, 0x14) cancels its advisor message through
// UIMain_CancelAdvisorMessage, which asks gGameWorld's advisor. On the Mac the read through the null world did not fault (Mac
// OS 9 has no memory protection); in the port it was a SIGSEGV in CWorld::GetAdvisor at shutdown. The port's fix (fix-input
// gPortNoWorldAdvisorGuard, include/ui_main.h, set by the launcher) returns while the world is null. The function is
// the real one, cut from src/game/ui/ui_main.cpp; the world and the advisor are stand-ins that record what they are asked, so
// a build without the fix fails here instead of faulting. Run by ctest ("map_quit") in a build with the port's fixes; a build
// without them makes the program, which fails. Exits nonzero on a failure.
#include <cstdint>
#include <cstdio>

#include "AdvisorQueue.h"
#include "World.h"
#include "ui_main.h"

CWorld* gGameWorld = nullptr;

namespace {

int gAskedNullWorld = 0;          // GetAdvisor called on a null world (the crash)
int gCancelled = 0;               // CancelMessage calls that reached an advisor
AdvisorMessage gLastMessage = static_cast<AdvisorMessage>(0);
unsigned char gWorldBytes[16];    // a live world's address; the stand-ins never read it
unsigned char gAdvisorBytes[16];  // the live world's advisor

int gFailures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what);
    if (!ok) ++gFailures;
}

}  // namespace

// The stand-ins read their own address through a volatile copy: a compiler may take a comparison of this with null as false.
CAdvisor* CWorld::GetAdvisor() {
    CWorld* volatile self = this;
    if (self == nullptr) {
        ++gAskedNullWorld;
        return nullptr;
    }
    return reinterpret_cast<CAdvisor*>(gAdvisorBytes);
}

int32_t CAdvisor::CancelMessage(AdvisorMessage message) {
    CAdvisor* volatile self = this;
    if (self != nullptr) {
        ++gCancelled;
        gLastMessage = message;
    }
    return 0;
}

int main() {
    // TPW_HOST_MAP_QUIT_GUARD comes from CMake in a build with the port's fixes (the export takes the fixes' own macro out
    // of tests/, and tpw-port builds this file from its tpw-decomp copy).
#ifdef TPW_HOST_MAP_QUIT_GUARD
    gPortNoWorldAdvisorGuard = true;   // as the launcher sets it (launcher/main.cpp)
#endif
    // The quit: the world is gone when the map's destroy message cancels the map's advisor message (0x130).
    gGameWorld = nullptr;
    UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x130));
    Check(gAskedNullWorld == 0, "with the world deleted, the map's cancel does not ask the null world for its advisor");
    Check(gCancelled == 0, "with the world deleted, nothing is cancelled");

    // In a park: the cancel still reaches the world's advisor.
    gGameWorld = reinterpret_cast<CWorld*>(gWorldBytes);
    UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x130));
    Check(gCancelled == 1 && gLastMessage == static_cast<AdvisorMessage>(0x130), "in a park, the cancel reaches the advisor");

    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", gFailures);
        return 1;
    }
    std::printf("map quit: OK\n");
    return 0;
}
