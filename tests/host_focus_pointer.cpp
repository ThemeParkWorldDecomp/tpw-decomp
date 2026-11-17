// host_focus_pointer.cpp: the pointer after a trip to another window (the port's fix focuspointer, tpw-port PR #133 by
// samueleagle91): the real UGamework::Run (platform/gamework.cpp) takes the host's pointer and focus events and tells the
// game, which here drops the pointer's moves while it is inactive, as the game's UI does (UI_DevicePointerMove). The pointer
// was left at a window edge, moved away while another window had the focus, and the focus came back: the game had the edge,
// and once unpaused the camera scrolled toward it until the mouse moved (reported by slappyhead). Checked:
//   - with no pointer move seen yet, the focus coming back tells the game nothing;
//   - the pointer moved while inactive: at the focus coming back the game has the pointer's last place;
//   - the pointer not moved while inactive: the game is told the same place again, nothing else changes;
//   - the pointer left the window (SDL's mouse leave, pointerleave) before the focus came back: the game is not told of a
//     place in the window, and the pointer stays out;
//   - the pointer's moves while active go to the game as before, one call each.
// No window (SDL's event queue takes the synthetic events without one): a window coordinate is the game pixel. Run by ctest
// ("focus_pointer") in a build with the port's fixes. Exits nonzero on a failure.
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "platform.h"
#include "platform/samsutils.h"

namespace {

int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

int gLeftCalls = 0;   // tpw::gPortPointerLeft: the game told the pointer is out

// The game: whether it is active, the pointer place it has (moves dropped while inactive), and every call in turn.
struct Game : SamsUtilities::UGamework {
    bool active = true;
    std::string pointer = "none";
    std::vector<std::string> log;
    Game() : UGamework(0) {}
    static std::string At(const SamsUtilities::UEvent& event) {
        const SamsUtilities::UPoint where = event.GetPosition();
        return std::to_string(where.x) + "," + std::to_string(where.y);
    }
    bool Process() override { return true; }
    void HandleMouseMoved(const SamsUtilities::UEvent& event) override {
        log.push_back(std::string("moved ") + At(event) + (active ? "" : " (dropped)"));
        if (active) pointer = At(event);
    }
    void HandleActivateWindow(GrafPort* window, bool on) override {
        (void)window;
        active = on;
        log.push_back(on ? "active" : "inactive");
    }
};

struct Ev {
    enum Kind { kMove, kLost, kGained, kLeave } kind;
    float x = 0, y = 0;
};

// The host's events in this order, all in the first pass of a run of two.
Game RunEvents(const std::vector<Ev>& events) {
    for (const Ev& e : events) {
        SDL_Event sdl{};
        switch (e.kind) {
        case Ev::kMove:
            sdl.type = SDL_EVENT_MOUSE_MOTION;
            sdl.motion.x = e.x;
            sdl.motion.y = e.y;
            break;
        case Ev::kLost: sdl.type = SDL_EVENT_WINDOW_FOCUS_LOST; break;
        case Ev::kGained: sdl.type = SDL_EVENT_WINDOW_FOCUS_GAINED; break;
        case Ev::kLeave: sdl.type = SDL_EVENT_WINDOW_MOUSE_LEAVE; break;
        }
        SDL_PushEvent(&sdl);
    }
    tpw::Config& config = tpw::config();
    config.maxFrames = tpw::CurrentPass() + 2;
    Game game;
    game.Run();
    return game;
}

std::string Log(const Game& game) {
    std::string all;
    for (const std::string& line : game.log) all += (all.empty() ? "" : " | ") + line;
    return all;
}

}  // namespace

int main() {
    if (!SDL_InitSubSystem(SDL_INIT_EVENTS)) {
        std::printf("FAILED: SDL events\n");
        return 1;
    }
    tpw::gPortPointerLeft = [] { ++gLeftCalls; };

    {
        // No pointer move seen yet: the focus going and coming back tells the game of no place.
        const Game game = RunEvents({{Ev::kLost}, {Ev::kGained}});
        Check(Log(game) == "inactive | active" && game.pointer == "none", "no move seen: nothing told at the focus, got " + Log(game));
    }
    {
        // At the right edge, moved away while another window had the focus, the focus back: the game has the last place.
        const Game game = RunEvents({{Ev::kMove, 639, 200}, {Ev::kLost}, {Ev::kMove, 400, 220}, {Ev::kMove, 320, 240}, {Ev::kGained}});
        Check(game.pointer == "320,240", "moved while inactive: the game has the last place at the focus, got " + game.pointer);
        Check(Log(game) == "moved 639,200 | inactive | moved 400,220 (dropped) | moved 320,240 (dropped) | active | moved 320,240",
              "moved while inactive: the calls, got " + Log(game));
    }
    {
        // Not moved while inactive: the same place again.
        const Game game = RunEvents({{Ev::kMove, 100, 50}, {Ev::kLost}, {Ev::kGained}});
        Check(game.pointer == "100,50" && Log(game) == "moved 100,50 | inactive | active | moved 100,50",
              "not moved while inactive: the same place told again, got " + Log(game));
    }
    {
        // The pointer left the window before the focus came back: no place in the window, and the game is told it is out.
        gLeftCalls = 0;
        const Game game = RunEvents({{Ev::kMove, 639, 200}, {Ev::kLost}, {Ev::kMove, 700, 200}, {Ev::kLeave}, {Ev::kGained}});
        Check(Log(game) == "moved 639,200 | inactive | moved 700,200 (dropped) | active" && game.pointer == "639,200",
              "left the window: nothing told at the focus, got " + Log(game));
        Check(gLeftCalls > 0, "left the window: the game is told the pointer is out");
        // Back over the window, active: as before, the move goes to the game once.
        const Game back = RunEvents({{Ev::kMove, 10, 20}});
        Check(Log(back) == "moved 10,20" && back.pointer == "10,20", "back over the window: one move, got " + Log(back));
    }
    {
        // Active moves: one call each, as before.
        const Game game = RunEvents({{Ev::kMove, 1, 2}, {Ev::kMove, 3, 4}});
        Check(Log(game) == "moved 1,2 | moved 3,4", "active moves: one call each, got " + Log(game));
    }

    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    std::printf("%s\n", gFailures == 0 ? "focus_pointer: all passed" : "focus_pointer: FAILED");
    return gFailures == 0 ? 0 : 1;
}
