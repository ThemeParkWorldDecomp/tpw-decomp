// host_wasd_camera.cpp: the launcher's "W A S D move the camera" (the port's fix wasd-camera, PortWasdCameraKey in
// include/port_wasd.h). Off, the four keys are not taken; on, the plain four go to the camera table as keypad 8, 4, 2 and 6 over
// the park and as the arrows in first person, by their place on the keyboard (the layout hook gPortPlainKey), and a key up lets
// go of the key its key down pressed even when the view changed in between, whatever modifier is held by then and wherever
// the keyboard is (PortWasdLetGo), and a panel takes the plain four and does nothing (PortWasdNothing). The camera table is a
// fake that records its calls.
// Run by ctest ("wasd_camera") in a build with the port's fixes, which alone has the function. Exits nonzero on a failure.
#include <cstdio>
#include <string>
#include <vector>

#include "port_wasd.h"

uint32_t gFirstPersonFlags;
CKeyTable* gKeyTableCamera;

namespace {

std::vector<std::string> gCalls;   // "down 104", "up 9728", ...
int gFailures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what);
    if (!ok) ++gFailures;
}

std::string Calls() {
    std::string all;
    for (const std::string& call : gCalls) all += (all.empty() ? "" : ", ") + call;
    gCalls.clear();
    return all;
}

// AZERTY: Z where a US keyboard has W, Q where it has A.
char Azerty(uint8_t macKeyCode) {
    switch (macKeyCode) {
        case 0x0d: return 'Z';
        case 0x00: return 'Q';
        case 0x01: return 'S';
        case 0x02: return 'D';
        default: return 0;
    }
}
char AltGrHeld(uint8_t) { return 0; }

}  // namespace

int32_t CKeyTable::KeyDown(int16_t key, int16_t modifiers) {
    gCalls.push_back("down " + std::to_string(key) + (modifiers != 0 ? " mod" : ""));
    return 0;
}
int32_t CKeyTable::KeyUp(int16_t key, int16_t modifiers) {
    gCalls.push_back("up " + std::to_string(key) + (modifiers != 0 ? " mod" : ""));
    return 0;
}

int main() {
    CKeyTable camera{};
    gKeyTableCamera = &camera;

    Check(!PortWasdCameraKey('W', 0, true) && Calls().empty(), "off: W is not taken");

    gPortWasdCamera = true;
    Check(PortWasdCameraKey('W', 0, true) && Calls() == "down 104", "park: W presses keypad 8 (scroll up)");
    Check(PortWasdCameraKey('W', 0, false) && Calls() == "up 104", "park: W's key up lets go of keypad 8");
    Check(PortWasdCameraKey('A', 0, true) && PortWasdCameraKey('A', 0, false) && Calls() == "down 100, up 100", "park: A is keypad 4");
    Check(PortWasdCameraKey('S', 0, true) && PortWasdCameraKey('S', 0, false) && Calls() == "down 98, up 98", "park: S is keypad 2");
    Check(PortWasdCameraKey('D', 0, true) && PortWasdCameraKey('D', 0, false) && Calls() == "down 102, up 102", "park: D is keypad 6");
    Check(!PortWasdCameraKey('S', 12, true) && !PortWasdCameraKey('S', 48, false) && Calls().empty(),
          "a modifier keeps the key's own meaning (Cmd-S, Option-S)");
    Check(!PortWasdCameraKey('Q', 0, true) && !PortWasdCameraKey(0x2600, 0, true) && Calls().empty(), "other keys are not taken");
    Check(PortWasdCameraKey('W', 0, true) && !PortWasdCameraKey('W', 3, false) && Calls() == "down 104, up 104",
          "a key up with Shift held by then lets go of keypad 8, and goes on to the tables as Shift-W");
    Check(PortWasdCameraKey('D', 0, true) && Calls() == "down 102", "D pressed over the park");
    PortWasdLetGo('D');
    Check(Calls() == "up 102", "a key up wherever the keyboard is (HandleTyping) lets go of keypad 6");
    Check(PortWasdCameraKey('D', 0, false) && Calls().empty(), "and the window's own key up then lets go of nothing");
    Check(PortWasdCameraKey('A', 0, true) && PortWasdNothing('A', 0) && Calls() == "down 100, up 100",
          "a panel takes the plain A and lets go of the keypad 4 it held");
    Check(PortWasdNothing('S', 0) && !PortWasdNothing('S', 12) && !PortWasdNothing('B', 0) && Calls().empty(),
          "a panel takes the plain S (no All staff), not Cmd-S nor another key, and presses nothing");

    gFirstPersonFlags = 2;   // walking
    Check(PortWasdCameraKey('W', 0, true) && PortWasdCameraKey('W', 0, false) && Calls() == "down 9728, up 9728",
          "first person: W is the up arrow (forward)");
    Check(PortWasdCameraKey('S', 0, true) && PortWasdCameraKey('S', 0, false) && Calls() == "down 10240, up 10240",
          "first person: S is the down arrow (back)");
    Check(PortWasdCameraKey('A', 0, true) && PortWasdCameraKey('D', 0, true) && Calls() == "down 9472, down 9984",
          "first person: A and D are the left and right arrows (turn)");
    gFirstPersonFlags = 0;   // left first person with A and D held
    Check(PortWasdCameraKey('A', 0, false) && PortWasdCameraKey('D', 0, false) && Calls() == "up 9472, up 9984",
          "a key up lets go of the key its key down pressed, though the view changed");
    Check(PortWasdCameraKey('W', 0, true) && Calls() == "down 104", "park again: W scrolls");
    gFirstPersonFlags = 2;
    Check(PortWasdCameraKey('W', 0, true) && Calls() == "up 104, down 9728", "a repeat after the view changed swaps the key");
    Check(PortWasdCameraKey('W', 0, false) && Calls() == "up 9728", "and its key up lets go of the new one");
    Check(PortWasdCameraKey('W', 0, false) && Calls().empty(), "a second key up lets go of nothing, and is still taken");
    gFirstPersonFlags = 0;

    gPortPlainKey = Azerty;
    Check(PortWasdCameraKey('Z', 0, true) && PortWasdCameraKey('Z', 0, false) && Calls() == "down 104, up 104",
          "AZERTY: Z, on the place of W, scrolls up");
    Check(PortWasdCameraKey('Q', 0, true) && PortWasdCameraKey('Q', 0, false) && Calls() == "down 100, up 100",
          "AZERTY: Q, on the place of A, scrolls left");
    Check(!PortWasdCameraKey('W', 0, true) && !PortWasdCameraKey('A', 0, true) && Calls().empty(),
          "AZERTY: W and A, elsewhere on the keyboard, are not taken");
    gPortPlainKey = AltGrHeld;
    Check(!PortWasdCameraKey('W', 0, true) && !PortWasdCameraKey(0, 0, true) && Calls().empty(),
          "no letter from the layout (AltGr held): nothing is taken");
    gPortPlainKey = nullptr;

    gPortWasdCamera = false;
    Check(!PortWasdCameraKey('D', 0, true) && Calls().empty(), "off again: D is not taken");
    Check(!PortWasdNothing('S', 0) && Calls().empty(), "off again: a panel's S is the shortcuts table's");

    std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
