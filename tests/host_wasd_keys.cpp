// host_wasd_keys.cpp: the launcher's "W A S D move the camera" through the run loop (the port's fixes wasd-camera and
// wasd-stuck): the real UGamework::Run (platform/gamework.cpp) takes the keys, as --hold sends them and as the host's key events
// come, and gives them to HandleTyping, which here does what the game's does with the four (TPMain.cpp HandleTyping and
// Engine_GameWinMsg, include/port_wasd.h). The camera table is a fake that keeps which keys are down. A held A (Mac key code
// 0) with another key pressed and let go before it left keypad 4 down for good, the camera scrolling left (reported by the maintainer).
// Run by ctest ("wasd_keys") in a build with the port's fixes, which alone has the setting. Exits nonzero on a failure.
#include <SDL3/SDL.h>

#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "platform.h"
#include "platform/samsutils.h"
#include "port_wasd.h"

// SDL's own keymap calls (src/events/SDL_keymap_c.h and SDL_keyboard_c.h of the static SDL3 the program links), as
// platform/tests/platform_tests.cpp sets a layout with no window.
extern "C" {
typedef struct SDL_Keymap SDL_Keymap;
SDL_Keymap* SDL_CreateKeymap(void);
void SDL_SetKeymapEntry(SDL_Keymap* keymap, SDL_Scancode scancode, SDL_Keymod modstate, SDL_Keycode keycode);
void SDL_SetKeymap(SDL_Keymap* keymap, bool send_event);
bool SDL_InitKeyboard(void);
void SDL_QuitKeyboard(void);
}

uint32_t gFirstPersonFlags;
CKeyTable* gKeyTableCamera;

namespace {

std::set<int> gDown;   // the camera table's keys down
int gFailures = 0;

void Check(bool ok, const std::string& what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what.c_str());
    if (!ok) ++gFailures;
}

std::string Down() {
    std::string all;
    for (const int key : gDown) all += (all.empty() ? "" : " ") + std::to_string(key);
    return all.empty() ? "none" : all;
}

// The game's key handling of the four: the window over the park (Engine_GameWinMsg) at the key down and up, then
// HandleTyping's own let-go at the key up, with the key the key tables get (the plain key, upper-cased).
struct Game : SamsUtilities::UGamework {
    std::vector<std::string> camera;   // per pass, the camera keys down at its last Process
    Game() : UGamework(0) {}
    bool Process() override {
        const size_t pass = static_cast<size_t>(tpw::CurrentPass());
        if (camera.size() <= pass) camera.resize(pass + 1);
        camera[pass] = Down();
        return true;
    }
    void HandleTyping(char character, char keyCode, uint32_t modifiers, bool down) override {
        const char plain = tpw::PlainKeyCharacter(static_cast<uint8_t>(keyCode));
        const int32_t key = plain != 0 ? plain : std::toupper(static_cast<unsigned char>(character));
        const int32_t mods = modifiers != 0 ? 1 : 0;
        PortWasdCameraKey(key, mods, down);
        if (!down) PortWasdLetGo(key);
    }
};

// Runs the scripted keys (scancode, the passes it goes down and up, counted from 1) for that many passes; the camera keys
// down at the end of each pass.
std::vector<std::string> Run(const std::vector<tpw::Config::ScriptedKey>& keys, int32_t passes) {
    tpw::Config& config = tpw::config();
    const int32_t first = tpw::CurrentPass();
    config.keys.clear();
    for (const tpw::Config::ScriptedKey& key : keys) config.keys.push_back({key.scancode, first - 1 + key.down, first - 1 + key.up});
    config.maxFrames = first - 1 + passes;
    Game game;
    game.Run();
    config.keys.clear();
    std::vector<std::string> camera;
    for (int32_t pass = 1; pass <= passes; ++pass) {
        const size_t at = static_cast<size_t>(first - 1 + pass);
        camera.push_back(at < game.camera.size() ? game.camera[at] : "?");
    }
    return camera;
}

// A down on pass 1 and up on 30; the other key down on 10 and up on 15. Down and up again, nothing is left down.
void HeldA(const char* name, int32_t other, bool several) {
    tpw::gPortSeveralKeys = several;
    const std::string mode = several ? " (several keys at once)" : " (one key at a time)";
    const std::vector<std::string> camera = Run({{SDL_SCANCODE_A, 1, 30}, {other, 10, 15}}, 40);
    Check(camera[4] == "100", std::string("A held scrolls left (keypad 4)") + mode);
    Check(camera[34] == "none" && camera[39] == "none" && gDown.empty(),
          std::string("A held, ") + name + " pressed and let go, then A let go: nothing stays down" + mode + ", got " + camera[39]);
    gDown.clear();
    for (int16_t& held : gPortWasdHeld) held = 0;
    tpw::gPortSeveralKeys = false;
}

}  // namespace

int32_t CKeyTable::KeyDown(int16_t key, int16_t) {
    gDown.insert(key);
    return 0;
}
int32_t CKeyTable::KeyUp(int16_t key, int16_t) {
    gDown.erase(key);
    return 0;
}

int main() {
    CKeyTable camera{};
    gKeyTableCamera = &camera;
    // As the launcher sets them with the setting on (launcher/main.cpp).
    gPortWasdCamera = true;
    tpw::gPortReleaseKeyA = true;
    tpw::gPortStrayKeyUp = PortWasdStrayKeyUp;
    tpw::gPortKeyTogether = PortWasdPlace;

    {
        const std::vector<std::string> alone = Run({{SDL_SCANCODE_A, 1, 30}}, 40);
        Check(alone[4] == "100" && alone[25] == "100" && alone[34] == "none" && gDown.empty(), "A alone: down while held, up after");
    }
    HeldA("W", SDL_SCANCODE_W, false);
    HeldA("D", SDL_SCANCODE_D, false);
    HeldA("Escape", SDL_SCANCODE_ESCAPE, false);
    HeldA("W", SDL_SCANCODE_W, true);
    HeldA("Escape", SDL_SCANCODE_ESCAPE, true);
    {
        // In first person A turns left (the left arrow).
        gFirstPersonFlags = kFirstPersonAny;
        const std::vector<std::string> camera = Run({{SDL_SCANCODE_A, 1, 30}, {SDL_SCANCODE_W, 10, 15}}, 40);
        Check(camera[4] == std::to_string(0x2500) && camera[39] == "none" && gDown.empty(), "first person: A held, W tapped, A let go: nothing stays down");
        gDown.clear();
        for (int16_t& held : gPortWasdHeld) held = 0;
        gFirstPersonFlags = 0;
    }
    {
        // W held, A tapped (the keys other than A were always released in turn): nothing stays down.
        const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 10, 15}}, 40);
        Check(camera[39] == "none" && gDown.empty(), "W held, A tapped, W let go: nothing stays down");
        gDown.clear();
        for (int16_t& held : gPortWasdHeld) held = 0;
    }
    {
        // The host's key events, one pass: A down, W down, W up, A up.
        Check(SDL_InitSubSystem(SDL_INIT_EVENTS), "SDL events");
        const SDL_Scancode order[4] = {SDL_SCANCODE_A, SDL_SCANCODE_W, SDL_SCANCODE_W, SDL_SCANCODE_A};
        for (int i = 0; i < 4; ++i) {
            SDL_Event key{};
            key.type = i < 2 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            key.key.scancode = order[i];
            key.key.down = i < 2;
            SDL_PushEvent(&key);
        }
        Run({}, 2);
        Check(gDown.empty(), "key events A down, W down, W up, A up: nothing stays down, got " + Down());
        gDown.clear();
        for (int16_t& held : gPortWasdHeld) held = 0;
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }
    {
        // A layout with no Latin letters (Russian): A types no character the Mac has (0) and its key code is 0, so the run
        // loop remembers no key for it, but the tables still get A (its QWERTY key). A alone, down and up.
        Check(SDL_InitKeyboard(), "SDL keyboard");
        SDL_Keymap* keymap = SDL_CreateKeymap();
        // SDL takes a layout for one with no Latin letters when A to D have none (as its video drivers set it).
        const SDL_Keycode russian[5][2] = {{SDL_SCANCODE_A, 0x444}, {SDL_SCANCODE_B, 0x438}, {SDL_SCANCODE_C, 0x441},
                                           {SDL_SCANCODE_D, 0x432}, {SDL_SCANCODE_W, 0x446}};
        for (const SDL_Keycode* key : russian) SDL_SetKeymapEntry(keymap, static_cast<SDL_Scancode>(key[0]), SDL_KMOD_NONE, key[1]);
        SDL_SetKeymap(keymap, false);
        const std::vector<std::string> alone = Run({{SDL_SCANCODE_A, 1, 30}}, 40);
        Check(alone[4] == "100" && alone[39] == "none" && gDown.empty(), "Russian layout: A alone, down while held, up after, got " + alone[39]);
        gDown.clear();
        for (int16_t& held : gPortWasdHeld) held = 0;
        SDL_QuitKeyboard();
    }

    // The diagonals (the port's fix wasd-diagonal, platform/gamework.cpp): two of the four held together are both down,
    // with "Several keys at once" off, each until its own key-up; a key that is not one of the four releases them.
    for (const bool several : {false, true}) {
        tpw::gPortSeveralKeys = several;
        const std::string mode = several ? " (several keys at once)" : " (one key at a time)";
        {
            // W held through, A from 10 to 20: up and left together, then up alone.
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 10, 20}}, 40);
            Check(camera[4] == "104" && camera[14] == "100 104" && camera[24] == "104" && camera[34] == "none" && gDown.empty(),
                  "W held, A held with it: keypad 8 and 4 together, then 8 alone" + mode + ", got " + camera[14] + " / " + camera[24]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
        }
        {
            // The other pairs: S with D, W with D, S with A.
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_S, 1, 30}, {SDL_SCANCODE_D, 10, 20}}, 40);
            Check(camera[14] == "98 102" && camera[24] == "98" && camera[34] == "none" && gDown.empty(),
                  "S held, D held with it: keypad 2 and 6 together" + mode + ", got " + camera[14]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
            const std::vector<std::string> camera2 = Run({{SDL_SCANCODE_D, 1, 30}, {SDL_SCANCODE_W, 10, 20}}, 40);
            Check(camera2[14] == "102 104" && camera2[24] == "102" && camera2[34] == "none" && gDown.empty(),
                  "D held, W held with it: keypad 6 and 8 together" + mode + ", got " + camera2[14]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
            const std::vector<std::string> camera3 = Run({{SDL_SCANCODE_A, 1, 30}, {SDL_SCANCODE_S, 10, 20}}, 40);
            Check(camera3[14] == "98 100" && camera3[24] == "100" && camera3[34] == "none" && gDown.empty(),
                  "A held, S held with it: keypad 4 and 2 together" + mode + ", got " + camera3[14]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
        }
        {
            // The first key let go first: W from 1 to 15, A from 10 to 30: together, then left alone.
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 15}, {SDL_SCANCODE_A, 10, 30}}, 40);
            Check(camera[12] == "100 104" && camera[19] == "100" && camera[34] == "none" && gDown.empty(),
                  "W held then let go while A is held: keypad 4 stays down alone" + mode + ", got " + camera[19]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
        }
        {
            // Three of the four: W and A held, then D on top (A with D: the camera table has both).
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 5, 30}, {SDL_SCANCODE_D, 10, 20}}, 40);
            Check(camera[14] == "100 102 104" && camera[24] == "100 104" && camera[34] == "none" && gDown.empty(),
                  "W, A and D held: all three down, then W and A" + mode + ", got " + camera[14] + " / " + camera[24]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
        }
        {
            // In first person: W with A walks and turns at once (the up and left arrows).
            gFirstPersonFlags = kFirstPersonAny;
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 10, 20}}, 40);
            const std::string both = std::to_string(0x2500) + " " + std::to_string(0x2600);
            Check(camera[14] == both && camera[24] == std::to_string(0x2600) && camera[34] == "none" && gDown.empty(),
                  "first person: W held, A held with it: up and left arrows together" + mode + ", got " + camera[14]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
            gFirstPersonFlags = 0;
        }
        if (!several) {
            // One key at a time: Escape going down while W and A are held releases both (the original's way for any
            // other key), and nothing stays down after.
            const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 5, 30}, {SDL_SCANCODE_ESCAPE, 10, 12}}, 40);
            Check(camera[8] == "100 104" && camera[14] == "none" && camera[34] == "none" && gDown.empty(),
                  "W and A held, Escape tapped: both let go at Escape, nothing stays down" + mode + ", got " + camera[14]);
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
        }
        {
            // The host's key events, one pass: W down, A down, A up, W up: both down together in the pass, nothing after.
            Check(SDL_InitSubSystem(SDL_INIT_EVENTS), "SDL events");
            const SDL_Scancode order[4] = {SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_A, SDL_SCANCODE_W};
            for (int i = 0; i < 4; ++i) {
                SDL_Event key{};
                key.type = i < 2 ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                key.key.scancode = order[i];
                key.key.down = i < 2;
                SDL_PushEvent(&key);
            }
            Run({}, 2);
            Check(gDown.empty(), "key events W down, A down, A up, W up: nothing stays down" + mode + ", got " + Down());
            gDown.clear();
            for (int16_t& held : gPortWasdHeld) held = 0;
            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        }
    }
    tpw::gPortSeveralKeys = false;
    {
        // With the setting off (no key stays down with another), W then A is A alone, as before.
        tpw::gPortKeyTogether = nullptr;
        const std::vector<std::string> camera = Run({{SDL_SCANCODE_W, 1, 30}, {SDL_SCANCODE_A, 10, 20}}, 40);
        Check(camera[14] == "100" && camera[24] == "none" && camera[34] == "none" && gDown.empty(),
              "without the diagonals: A going down lets go of W, got " + camera[14] + " / " + camera[24]);
        gDown.clear();
        for (int16_t& held : gPortWasdHeld) held = 0;
        tpw::gPortKeyTogether = PortWasdPlace;
    }

    // The safety nets (include/port_wasd.h): the focus lost or the park's window gone lets go of all four; a key-up the run
    // loop could not match lets go of what that key's place holds, and of nothing else.
    PortWasdCameraKey('A', 0, true);
    PortWasdCameraKey('W', 0, true);
    PortWasdLetGoAll();
    Check(gDown.empty() && gPortWasdHeld[0] == 0 && gPortWasdHeld[1] == 0, "PortWasdLetGoAll lets go of every key the four hold");
    PortWasdCameraKey('A', 0, true);
    PortWasdCameraKey('D', 0, true);
    PortWasdStrayKeyUp(0x0d);   // W's place, which holds nothing
    PortWasdStrayKeyUp(0x7e);   // the up arrow, not one of the four
    Check(Down() == "100 102", "a stray key-up of W or of another key lets go of nothing");
    PortWasdStrayKeyUp(0x00);
    Check(Down() == "102", "a stray key-up of A lets go of keypad 4 only");
    PortWasdStrayKeyUp(0x02);
    Check(gDown.empty(), "a stray key-up of D lets go of keypad 6");

    std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
