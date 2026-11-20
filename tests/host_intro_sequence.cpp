// Genuine front-end movie states with an empty movie folder. No game window is
// created. The genuine UI pump has empty queues and no root window; file lookup,
// resource management and movie completion are genuine too. Stops before lobby setup.
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

#include "FS_System.h"
#include "Game.h"
#include "GameSys.h"
#include "TPMovie.h"
#include "TPResource.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "platform/macdoze.h"

// Including platform.h here would collide with the game's pause() declaration.
// Keep config access in a separate source with no game headers.
void IntroSequenceConfig(const std::filesystem::path& game, const std::filesystem::path& user, const char* edition);
bool IntroSequenceVideoStarted();

namespace {
unsigned checks = 0, failures = 0, standIns = 0;
void Check(bool value, const std::string& message) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
}

void tpw_stub_called(const char* name, bool, bool first) {
    ++standIns;
    if (first) std::printf("Unexpected stand-in: %s\n", name);
}

int main() {
    namespace fs = std::filesystem;
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto scratch = fs::temp_directory_path() / ("tpw-intro-sequence-" + std::to_string(nonce));
    InterfaceMessageQueue messages, draw;
    Check(messages.Initialise(16) == 1 && draw.Initialise(16) == 1, "empty real UI queues initialized");
    gUIMessageQueue = &messages;
    gUIDrawQueue = &draw;
    Check(gUIRootWindow == nullptr && gUICursor == nullptr, "no UI window or cursor exists");
    Check(UI_GetButtonFlags() == 0 && NS_MacDoze::GetAsyncKeyState(0x1b) == 0 &&
          NS_MacDoze::GetAsyncKeyState(0x20) == 0, "no input can skip the movies");
    Check(!IntroSequenceVideoStarted(), "no SDL video subsystem has started");
    for (const char* edition : {"stp", "tpw"}) {
        const auto game = scratch / edition;
        const auto user = scratch / (std::string(edition) + "-user");
        fs::create_directories(game / "data/Movies");
        fs::create_directories(user);
        IntroSequenceConfig(game, user, edition);

        auto* previous = gGame.mResourceManager;
        CTPResourceManager manager;
        gGame.mResourceManager = &manager;
        auto* movies = new CTPFMVResource(TbDynamicStringTemplate<char>("Movies"),
                                         TbDynamicStringTemplate<char>("Data:Movies:"));
        movies->AddDiscStorage(TbDynamicStringTemplate<char>("Data:Movies:"));
        Check(manager.RegisterResource(movies) == 1, std::string(edition) + " movie resource registered");
        Check(movies->mStorages->GetCount() == 1, std::string(edition) + " real empty movie folder opened");
        for (const char* stem : {"bf", "bub", "buc", "grav", "jug", "mir", "plan", "roc", "roll"}) {
            TbDynamicStringTemplate<char> resolved;
            const std::string path = std::string("Data:Movies:") + stem + ".mov";
            Check(FS_FindFilePath(TbDynamicStringTemplate<char>(path.c_str()), resolved) == 0,
                  std::string(edition) + " missing " + stem + " through genuine file lookup");
        }

        MOVIE_Stop();
        gGameSystemInitialised = 1;
        gGameSystemState = 4;
        // These are the actual GAMESYSTEM_Tick states, including both calls to
        // MOVIE_Tick. State 1 starts the lobby on its next tick; do not enter it.
        for (const int expected : {5, 6, 7, 8, 1}) {
            Check(GAMESYSTEM_Tick() == 0, std::string(edition) + " intro tick does not request exit");
            Check(gGameSystemState == expected, std::string(edition) + " reaches state " + std::to_string(expected));
            Check(gMovie == nullptr, std::string(edition) + " missing film never creates a movie");
        }
        Check(gUIRootWindow == nullptr && gUICursor == nullptr, std::string(edition) + " sequence created no UI window");
        Check(!IntroSequenceVideoStarted(), std::string(edition) + " sequence never started SDL video");
        Check(MOVIE_Tick() == 1, std::string(edition) + " absent movie reports completion");
        gGameSystemInitialised = 0;
        gGame.mResourceManager = previous;
    }
    gUIMessageQueue = nullptr;
    gUIDrawQueue = nullptr;
    Check(standIns == 0, "no generated stand-in reached");
    fs::remove_all(scratch);
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
