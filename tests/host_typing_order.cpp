// host_typing_order.cpp: letters typed into an edit box keep their order (the port's fix typing-order, tpw-port PR #132 by
// samueleagle91, with the two changes made on top of it): the real UGamework::Run (platform/gamework.cpp) takes the keys, as
// --hold sends them and as the host's key events come, and gives them to HandleTyping, which here keeps the order the keys
// came up in (a letter goes to an edit box at its key-up, TPMain.cpp HandleTyping). Whether keys stay down together is
// PortKeyTogetherFor (include/port_wasd.h), the launcher's PortKeyTogether with the edit box given by the test:
//   - with "Free camera" on, typing in an edit box keeps key-down order: S down, D down, D up, S up types "sd" (the free
//     camera's keys stayed down together, and "ds" came out); the same with "Several keys at once" and with W A S D;
//   - out of an edit box the camera's keys stay down together, as before (S and D are both down until their key-ups);
//   - a key held together with another when an edit box takes the keyboard comes up at its own key-up: S down, D down (in
//     the park), the edit box takes the keyboard, S up releases S and not D, D up releases D.
// Run by ctest ("typing_order") in a build with the port's fixes, which alone has the settings. Exits nonzero on a failure.
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "platform.h"
#include "platform/samsutils.h"
#include "port_wasd.h"

uint32_t gFirstPersonFlags;
CKeyTable* gKeyTableCamera;

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

// What the launcher's settings would be, and whether an edit box has the keyboard (gUIKeyboardWindow in the game).
bool gEditBox = false;
bool gSeveral = false;

// The launcher's PortKeyTogether (launcher/main.cpp) with the test's edit box.
bool KeyTogetherHere(uint8_t macKeyCode) { return PortKeyTogetherFor(macKeyCode, gEditBox, gSeveral); }

// The letters of the keys this test uses, by their Mac key codes (a US keyboard).
char Letter(char keyCode) {
    switch (static_cast<uint8_t>(keyCode)) {
    case 0x01: return 's';
    case 0x02: return 'd';
    case 0x0c: return 'q';
    case 0x0e: return 'e';
    case 0x28: return 'k';
    default: return '?';
    }
}

// The game: each key-down and key-up in turn, and the letters in the order their keys came up (what an edit box types).
struct Game : SamsUtilities::UGamework {
    std::string typed;    // the letters, at their key-ups
    std::string events;   // "s+ d+ d- s-": every key-down (+) and key-up (-)
    std::vector<std::string> eventsAtPass;   // per pass, the events so far at its last Process
    Game() : UGamework(0) {}
    bool Process() override {
        const size_t pass = static_cast<size_t>(tpw::CurrentPass());
        if (eventsAtPass.size() <= pass) eventsAtPass.resize(pass + 1);
        eventsAtPass[pass] = events;
        // The edit box takes the keyboard on the pass the case names (a name box opening while keys are held).
        if (gEditBoxFrom != 0 && static_cast<int32_t>(pass) >= gEditBoxFrom) gEditBox = true;
        return true;
    }
    void HandleTyping(char character, char keyCode, uint32_t modifiers, bool down) override {
        (void)character;
        (void)modifiers;
        const char letter = Letter(keyCode);
        events += std::string(events.empty() ? "" : " ") + letter + (down ? "+" : "-");
        if (!down) typed += letter;
    }
    static int32_t gEditBoxFrom;
};
int32_t Game::gEditBoxFrom = 0;

struct Result {
    std::string typed;
    std::string events;
    std::vector<std::string> eventsAtPass;   // from pass 1
};

// Runs the scripted keys (scancode, the passes it goes down and up, counted from 1) for that many passes.
Result Run(const std::vector<tpw::Config::ScriptedKey>& keys, int32_t passes, int32_t editBoxFrom = 0) {
    tpw::Config& config = tpw::config();
    const int32_t first = tpw::CurrentPass();
    config.keys.clear();
    for (const tpw::Config::ScriptedKey& key : keys) config.keys.push_back({key.scancode, first - 1 + key.down, first - 1 + key.up});
    config.maxFrames = first - 1 + passes;
    Game::gEditBoxFrom = editBoxFrom != 0 ? first - 1 + editBoxFrom : 0;
    if (editBoxFrom != 0) gEditBox = false;   // in the park until that pass
    Game game;
    game.Run();
    config.keys.clear();
    Game::gEditBoxFrom = 0;
    Result result{game.typed, game.events, {}};
    for (int32_t pass = 1; pass <= passes; ++pass) {
        const size_t at = static_cast<size_t>(first - 1 + pass);
        result.eventsAtPass.push_back(at < game.eventsAtPass.size() ? game.eventsAtPass[at] : "?");
    }
    return result;
}

// The host's key events, all in one pass, in this order (true: down).
Result RunEvents(const std::vector<std::pair<SDL_Scancode, bool>>& order) {
    for (const auto& [scancode, down] : order) {
        SDL_Event key{};
        key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        key.key.scancode = scancode;
        key.key.down = down;
        SDL_PushEvent(&key);
    }
    return Run({}, 2);
}

void Settings(bool freeCamera, bool wasd, bool several) {
    gPortFreeCamera = freeCamera;
    gPortWasdCamera = wasd;
    gSeveral = several;
    gEditBox = false;
}

}  // namespace

int32_t CKeyTable::KeyDown(int16_t, int16_t) { return 0; }
int32_t CKeyTable::KeyUp(int16_t, int16_t) { return 0; }

int main() {
    // As the launcher sets them (launcher/main.cpp): A released first (keya), PortKeyTogether, no stray key-ups here.
    tpw::gPortReleaseKeyA = true;
    tpw::gPortStrayKeyUp = nullptr;
    tpw::gPortKeyTogether = KeyTogetherHere;
    tpw::gPortSeveralKeys = false;

    struct Mode {
        const char* name;
        bool freeCamera, wasd, several;
    };
    const Mode modes[] = {{"Free camera", true, false, false},
                          {"Free camera with W A S D", true, true, false},
                          {"W A S D", false, true, false},
                          {"Several keys at once", false, false, true},
                          {"none", false, false, false}};

    for (const Mode& mode : modes) {
        const std::string with = std::string(" (") + mode.name + ")";
        // Typing in an edit box, the second key pressed before the first was let go and let go first: key-down order.
        Settings(mode.freeCamera, mode.wasd, mode.several);
        gEditBox = true;
        {
            const Result r = Run({{SDL_SCANCODE_S, 1, 7}, {SDL_SCANCODE_D, 3, 5}}, 10);
            Check(r.typed == "sd", "edit box: S down, D down, D up, S up types \"sd\"" + with + ", got \"" + r.typed + "\"");
        }
        {
            const Result r = Run({{SDL_SCANCODE_Q, 1, 7}, {SDL_SCANCODE_E, 3, 5}}, 10);
            Check(r.typed == "qe", "edit box: Q down, E down, E up, Q up types \"qe\"" + with + ", got \"" + r.typed + "\"");
        }
        {
            const Result r = Run({{SDL_SCANCODE_D, 1, 7}, {SDL_SCANCODE_K, 3, 5}}, 10);
            Check(r.typed == "dk", "edit box: D down, K down, K up, D up types \"dk\"" + with + ", got \"" + r.typed + "\"");
        }
        {
            Check(SDL_InitSubSystem(SDL_INIT_EVENTS), "SDL events");
            const Result r = RunEvents({{SDL_SCANCODE_S, true}, {SDL_SCANCODE_D, true}, {SDL_SCANCODE_D, false}, {SDL_SCANCODE_S, false}});
            Check(r.typed == "sd", "edit box, key events in one pass: S down, D down, D up, S up types \"sd\"" + with + ", got \"" + r.typed + "\"");
            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        }
        gEditBox = false;
    }

    // Out of an edit box the camera's keys stay down together, each until its own key-up (wasd-diagonal, freecam): S and D
    // both down from pass 3, D up at 5, S up at 7.
    for (const Mode& mode : modes) {
        if (!mode.freeCamera && !mode.wasd && !mode.several) continue;
        Settings(mode.freeCamera, mode.wasd, mode.several);
        const std::string with = std::string(" (") + mode.name + ")";
        const Result r = Run({{SDL_SCANCODE_S, 1, 7}, {SDL_SCANCODE_D, 3, 5}}, 10);
        Check(r.eventsAtPass[3] == "s+ d+" && r.events == "s+ d+ d- s-",
              "in the park: S and D down together, each up at its own key-up" + with + ", got \"" + r.events + "\"");
    }

    // A key held together with another when an edit box takes the keyboard (from pass 4, between D's key-down and the
    // key-ups) comes up at its own key-up, and the other key stays down until its own.
    for (const Mode& mode : modes) {
        if (!mode.freeCamera && !mode.wasd && !mode.several) continue;
        Settings(mode.freeCamera, mode.wasd, mode.several);
        const std::string with = std::string(" (") + mode.name + ")";
        {
            // S, the held key, let go first: S comes up, not D.
            const Result r = Run({{SDL_SCANCODE_S, 1, 6}, {SDL_SCANCODE_D, 3, 8}}, 10, 4);
            Check(r.eventsAtPass[6] == "s+ d+ s-", "edit box from pass 4: S's key-up releases S and not D" + with + ", got \"" + r.eventsAtPass[6] + "\"");
            Check(r.events == "s+ d+ s- d-", "edit box from pass 4: D comes up at its own key-up" + with + ", got \"" + r.events + "\"");
        }
        {
            // D, the remembered key, let go first: D comes up, then S at its own.
            const Result r = Run({{SDL_SCANCODE_S, 1, 8}, {SDL_SCANCODE_D, 3, 6}}, 10, 4);
            Check(r.eventsAtPass[6] == "s+ d+ d-", "edit box from pass 4: D's key-up releases D and not S" + with + ", got \"" + r.eventsAtPass[6] + "\"");
            Check(r.events == "s+ d+ d- s-", "edit box from pass 4: S comes up at its own key-up" + with + ", got \"" + r.events + "\"");
        }
        {
            // Then a new key in the edit box: one at a time again, the held key let go first (releaseHeldKeys).
            const Result r = Run({{SDL_SCANCODE_S, 1, 9}, {SDL_SCANCODE_D, 3, 9}, {SDL_SCANCODE_K, 5, 7}}, 12, 4);
            Check(r.events == "s+ d+ s- d- k+ k-", "edit box from pass 4: K going down releases S and D first" + with + ", got \"" + r.events + "\"");
        }
        gEditBox = false;
    }

    std::printf("%s\n", gFailures == 0 ? "typing_order: all passed" : "typing_order: FAILED");
    return gFailures == 0 ? 0 : 1;
}
