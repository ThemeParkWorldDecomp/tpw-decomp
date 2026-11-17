// host_free_camera.cpp: the launcher's "Free camera" (the port's fix freecam, include/PortFreeCamera.h), its input side. Off,
// no key, wheel, drag or pinch is taken. On: Q and E press the camera table's turn keys and Page Up and Page Down its zoom
// keys, Z and X hold the tilt, by their place on the keyboard, and a key up lets go of what its key down took; a notched
// wheel stays the original's while a trackpad's fractional deltas move, zoom, turn and tilt as the settings say, Control
// with the wheel pinches, and only over the park and out of first person; the middle button and Option with the left start
// a drag that turns and tilts by how far it moves, with the sensitivity and the inverted look, and stops in first person
// or when a panel opens; and the camera's own math: the original's tilt for a zoom, the easing, the ground floor. The
// camera table is a fake that records its calls. Fly mode (Tab): its keys are held bits, the wheel, a pinch and the
// right button move, look and set the speed, the move eases; the low view's hysteresis; what a save holds.
// Run by ctest ("free_camera") in a build with the port's fixes, which alone has the functions. Exits nonzero on a failure.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "PortFreeCamera.h"

uint32_t gFirstPersonFlags;
CKeyTable* gKeyTableCamera;

namespace {

std::vector<std::string> gCalls;
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

bool Near(float a, float b, float within = 1e-4f) { return std::fabs(a - b) <= within; }

// AZERTY: A where a US keyboard has Q, W where it has Z.
char Azerty(uint8_t macKeyCode) {
    switch (macKeyCode) {
        case 0x0c: return 'A';
        case 0x0d: return 'Z';
        case 0x00: return 'Q';
        case 0x06: return 'W';
        default: return 0;
    }
}

void Reset() {
    PortFreeClearPending();
    gPortFree.haveFine = false;
    gPortFree.lastFineMs = 0;
    gPortFree.dragButton = -1;
}

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

    // ---- off
    Check(!PortFreeCameraKey('Q', 0, true) && !PortFreeCameraKey('Z', 0, true) && Calls().empty(), "off: Q and Z are not taken");
    Check(!PortFreeWheel(0.0f, 0.5f, true, 0, 1000, true), "off: a trackpad swipe is the game's");
    Check(!PortFreeDragStart(2, 0, true) && gPortFree.dragButton == -1, "off: the middle button is the game's");
    Check(!PortFreePlace(0x0c), "off: Q is not kept down with other keys");
    PortFreePinch(2.0f, true);
    Check(gPortFree.zoomScale == 1.0f, "off: a pinch does nothing");

    gPortFreeCamera = true;
    // ---- keys
    Check(PortFreeCameraKey('Q', 0, true) && Calls() == "down 9472", "Q presses the left arrow (turn, with the Isometric snap)");
    Check(PortFreeCameraKey('Q', 0, false) && Calls() == "up 9472", "Q's key up lets go of it");
    Check(PortFreeCameraKey('E', 3, true) && PortFreeCameraKey('E', 0, false) && Calls() == "down 9984, up 9984",
          "E (Shift held) is the right arrow");
    // (the original's key table gives the Mac's Page Up key 0x2200 and its Page Down 0x2100)
    Check(PortFreeCameraKey(0x2200, 0, true) && PortFreeCameraKey(0x2200, 0, false) && Calls() == "down 9728, up 9728",
          "Page Up is the up arrow (zoom in)");
    Check(PortFreeCameraKey(0x2100, 0, true) && PortFreeCameraKey(0x2100, 0, false) && Calls() == "down 10240, up 10240",
          "Page Down is the down arrow (zoom out)");
    Check(PortFreeCameraKey('Z', 0, true) && gPortFree.held == (1u << kPortFreeZ) && Calls().empty(), "Z holds the tilt up");
    Check(PortFreeCameraKey('X', 0x30, true) && gPortFree.held == ((1u << kPortFreeZ) | (1u << kPortFreeX)),
          "X (Option held) holds the tilt down too");
    Check(PortFreeCameraKey('Z', 0, false) && PortFreeCameraKey('X', 0, false) && gPortFree.held == 0, "their key ups let go");
    Check(!PortFreeCameraKey('Q', 0xc, true) && !PortFreeCameraKey('Z', 0xc, true) && Calls().empty(),
          "Command or Control leaves the key to the tables");
    Check(!PortFreeCameraKey('B', 0, true) && !PortFreeCameraKey(0x2600, 0, true) && Calls().empty(), "other keys are not taken");
    Check(PortFreeCameraKey('Q', 0, true) && PortFreeCameraKey('Q', 0, true) && Calls() == "down 9472",
          "a repeat of Q presses nothing more");
    gFirstPersonFlags = 2;
    Check(!PortFreeCameraKey('E', 0, true) && Calls().empty(), "first person: E is the game's");
    Check(PortFreeCameraKey('Q', 0, false) && Calls() == "up 9472", "first person: Q's key up still lets go of what it took");
    gFirstPersonFlags = 0;
    Check(PortFreeCameraKey('E', 0, true) && Calls() == "down 9984", "E pressed");
    PortFreeStrayKeyUp(0x0e);
    Check(Calls() == "up 9984", "a stray key up on E's place lets go of it");
    Check(PortFreeCameraKey('Z', 0, true) && PortFreeCameraKey('Q', 0, true) && Calls() == "down 9472", "Z and Q held");
    PortFreeLetGoAll();
    Check(Calls() == "up 9472" && gPortFree.held == 0, "the focus loss lets go of everything");
    Check(PortFreePlace(0x0c) && PortFreePlace(0x07) && PortFreePlace(0x74) && !PortFreePlace(0x0b),
          "Q, X and Page Up are kept down with other keys, B is not");
    gPortPlainKey = Azerty;
    Check(PortFreeCameraKey('A', 0, true) && PortFreeCameraKey('A', 0, false) && Calls() == "down 9472, up 9472",
          "AZERTY: A, on the place of Q, turns");
    Check(PortFreeCameraKey('W', 0, true) && gPortFree.held == (1u << kPortFreeZ) && PortFreeCameraKey('W', 0, false),
          "AZERTY: W, on the place of Z, tilts");
    Check(!PortFreeCameraKey('Q', 0, true) && Calls().empty(), "AZERTY: Q, elsewhere, is not taken as Q");
    gPortPlainKey = nullptr;

    // ---- the wheel
    Reset();
    Check(!PortFreeWheel(0.0f, 1.0f, false, 0, 1000, true), "a notched wheel is the original's (10 a notch)");
    Check(PortFreeWheel(0.25f, 0.5f, true, 0, 2000, true) && Near(gPortFree.dForward, 0.5f * kPortFreeSwipeMove) &&
              Near(gPortFree.dRight, 0.25f * kPortFreeSwipeMove),
          "Moves the camera: a swipe moves forward and sideways");
    Check(PortFreeWheel(0.0f, 1.0f, false, 0, 2200, true), "a whole delta within 300 ms of a fractional one is the trackpad's");
    Check(!PortFreeWheel(0.0f, 1.0f, false, 0, 2600, true), "and one after 300 ms is a wheel's again");
    Reset();
    gPortFreeSwipeZooms = true;
    Check(PortFreeWheel(0.5f, 0.5f, true, 0, 3000, true) && Near(gPortFree.dZoom, -0.5f * kPortFreeSwipeZoom) &&
              Near(gPortFree.dYaw, 0.5f * kPortFreeSwipeTurn) && gPortFree.dForward == 0.0f,
          "Zooms and rotates: up and down zooms, sideways turns");
    gPortFreeSwipeZooms = false;
    Reset();
    Check(PortFreeWheel(0.5f, 0.5f, true, 0x30, 4000, true) && Near(gPortFree.dYaw, 0.5f * kPortFreeSwipeTurn) &&
              Near(gPortFree.dPitch, -0.5f * kPortFreeSwipeTilt),
          "Option: up and down tilts, sideways turns");
    Reset();
    gPortFreeInvertLook = true;
    gPortFreeLookSensitivity = 2.0f;
    Check(PortFreeWheel(0.0f, 0.5f, true, 0x30, 4100, true) && Near(gPortFree.dPitch, 0.5f * kPortFreeSwipeTilt * 2.0f),
          "Option: inverted and twice as sensitive");
    gPortFreeInvertLook = false;
    gPortFreeLookSensitivity = 1.0f;
    Reset();
    Check(PortFreeWheel(0.0f, 1.0f, false, 0xc, 5000, true) && Near(gPortFree.zoomScale, kPortFreePinchZoom),
          "Control with a notched wheel pinches (Windows): zooms in");
    Check(PortFreeWheel(0.0f, -2.0f, true, 0xc, 5010, true) && Near(gPortFree.zoomScale, 1.0f / kPortFreePinchZoom, 1e-3f),
          "and out");
    Reset();
    Check(!PortFreeWheel(0.0f, 0.5f, true, 0, 6000, false) && gPortFree.dForward == 0.0f, "not over the park: the game's");
    gFirstPersonFlags = 4;
    Check(!PortFreeWheel(0.0f, 0.5f, true, 0, 6100, true) && gPortFree.dForward == 0.0f, "first person: the game's");
    gFirstPersonFlags = 0;

    // ---- drags and the pinch
    Reset();
    Check(!PortFreeDragStart(0, 0, true) && !PortFreeDragStart(1, 0, true), "a plain left or right button is the game's");
    Check(!PortFreeDragStart(2, 0, false), "the middle button off the park is the game's");
    Check(PortFreeDragStart(2, 0, true) && gPortFree.dragButton == 2, "the middle button starts a drag");
    PortFreeDragMove(10.0f, -4.0f);
    Check(Near(gPortFree.dYaw, 10.0f * kPortFreeDragTurn) && Near(gPortFree.dPitch, -4.0f * kPortFreeDragTurn),
          "a drag turns and tilts by how far it moves");
    Check(PortFreeDragGoing(false) && !PortFreeDragGoing(true), "a panel opening stops the drag");
    gFirstPersonFlags = 0x10;
    Check(!PortFreeDragGoing(false), "first person stops the drag");
    gFirstPersonFlags = 0;
    PortFreeDragEnd();
    Reset();
    PortFreeDragMove(10.0f, 10.0f);
    Check(gPortFree.dYaw == 0.0f, "no drag: a move does nothing");
    gPortFreeInvertLook = true;
    gPortFreeLookSensitivity = 0.5f;
    Check(PortFreeDragStart(0, 0x30, true) && gPortFree.dragButton == 0, "Option with the left button starts a drag");
    PortFreeDragMove(0.0f, 8.0f);
    Check(Near(gPortFree.dPitch, -8.0f * kPortFreeDragTurn * 0.5f), "inverted, at half the sensitivity");
    PortFreeDragEnd();
    gPortFreeInvertLook = false;
    gPortFreeLookSensitivity = 1.0f;
    Reset();
    PortFreePinch(1.25f, true);
    Check(Near(gPortFree.zoomScale, 0.8f), "a pinch spreading zooms in");
    PortFreePinch(1.25f, false);
    Check(Near(gPortFree.zoomScale, 0.8f), "a pinch off the park does nothing");

    // ---- modifiers and the camera's math
    Check(PortFreeGameModifiers(0x200) == 3 && PortFreeGameModifiers(0x100) == 0xc && PortFreeGameModifiers(0x1000) == 0xc &&
              PortFreeGameModifiers(0x800) == 0x30 && PortFreeGameModifiers(0) == 0,
          "the Mac's modifiers as the game's byte");
    Check(PortFreeSpeedFactor(3) == 3.0f && Near(PortFreeSpeedFactor(0x30), 1.0f / 3.0f) && PortFreeSpeedFactor(0) == 1.0f,
          "Shift is three times as fast, Option a third");
    Check(Near(PortFreeFormulaPitch(70.0f), 0.7853982f) && Near(PortFreeFormulaPitch(130.0f), 1.134464f) &&
              Near(PortFreeFormulaPitch(100.0f), 0.5f * (0.7853982f + 1.134464f)),
          "the original's tilt for a zoom");
    Check(PortFreeFormulaPitch(-200.0f) == kPortFreePitchLow && PortFreeFormulaPitch(1000.0f) == 1.4707963f,
          "within the limits");
    Check(PortFreeClampPitch(-1.0f) == kPortFreePitchLow && PortFreeClampPitch(2.0f) == kPortFreePitchHigh, "the tilt's limits");
    Check(Near(PortFreeEase(0.0f, 1.0f, 70.0f), 0.5f) && PortFreeEase(0.0f, 1.0f, 140.0f) == 1.0f &&
              PortFreeEase(0.0f, 1.0f, 0.0f) == 0.0f,
          "the easing: half the way in 70 ms, all of it in 140, none paused");
    gPortFreeSmoothing = false;
    Check(PortFreeEase(0.0f, 1.0f, 0.0f) == 1.0f, "Smoothing off: the target at once");
    gPortFreeSmoothing = true;
    auto flat = [](float, float) { return 0.0f; };
    const float low = PortFreeFloorPitch(0.01f, 0.3f, 100.0f, 50.0f, 0.0f, 50.0f, flat);
    Check(Near(100.0f * std::sin(low), kPortFreeGroundMargin, 1e-3f), "flat ground: the eye is raised to the margin");
    Check(PortFreeFloorPitch(0.6f, 0.3f, 100.0f, 50.0f, 0.0f, 50.0f, flat) == 0.6f, "a tilt above the floor is kept");
    // A hill of height 40 behind the target (the eye's side), flat in front.
    auto hill = [](float, float z) { return z < 20.0f ? 40.0f : 0.0f; };
    const float raised = PortFreeFloorPitch(0.05f, 0.0f, 100.0f, 0.0f, 0.0f, 50.0f, hill);
    const float eyeZ = 50.0f - 100.0f * std::cos(raised);
    Check(100.0f * std::sin(raised) >= hill(0.0f, eyeZ) + kPortFreeGroundMargin - 1e-3f, "a hill under the eye raises it above the hill");
    Check(PortFreeFloorPitch(0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, hill) == 0.05f, "no zoom: nothing to raise");

    // ---- fly mode (Tab)
    Reset();
    gPortFree.held = 0;
    gPortFree.fly = gPortFree.flyWanted = false;
    Check(PortFreeCameraKey('Q', 0, true) && Calls() == "down 9472", "orbit: Q pressed before Tab");
    Check(PortFreeCameraKey(9, 0, true) && gPortFree.flyWanted && PortFreeFlying() && Calls() == "up 9472",
          "Tab asks for fly mode and lets go of the orbit's keys");
    Check(PortFreeCameraKey(9, 0, true) && gPortFree.flyWanted, "Tab's key repeat does not toggle it back");
    Check(PortFreeCameraKey(9, 0, false) && (gPortFree.held & (1u << kPortFreeTab)) == 0, "Tab's key up");
    Check(!PortFreeHearsFromEye(), "asked for, not yet flying: the ear stays the orbit's");
    gPortFree.fly = true;
    Check(PortFreeHearsFromEye(), "flying: the ear is at the eye");
    gPortFree.held = 0;
    Check(PortFreeCameraKey('W', 0, true) && PortFreeCameraKey('A', 0, true) && PortFreeCameraKey('S', 0, true) &&
              PortFreeCameraKey('D', 0, true) && PortFreeCameraKey('Q', 0, true) && PortFreeCameraKey('E', 0, true) &&
              PortFreeCameraKey(0x2200, 0, true) && PortFreeCameraKey(0x2100, 0, true) && Calls().empty(),
          "fly: W A S D, Q, E, Page Up and Page Down are taken and press no table key");
    Check(gPortFree.held == ((1u << kPortFreeW) | (1u << kPortFreeA) | (1u << kPortFreeS) | (1u << kPortFreeD) | (1u << kPortFreeQ) |
                             (1u << kPortFreeE) | (1u << kPortFreePgUp) | (1u << kPortFreePgDn)),
          "fly: each is a held bit");
    Check(PortFreeCameraKey('W', 0, false) && (gPortFree.held & (1u << kPortFreeW)) == 0, "fly: W's key up lets go of its bit");
    gPortFree.held = 0;
    // a key with modifiers a table has stays the table's: the game table's quick save, Option-S (keyboard.cpp gKeyEntriesGame)
    gPortFreeTableKey = [](int32_t key, int32_t modifiers) { return key == 'S' && modifiers == 0x30; };
    Check(!PortFreeCameraKey('S', 0x30, true) && gPortFree.held == 0, "fly: Option-S is the tables' (the quick save)");
    Check(PortFreeCameraKey('S', 0, true) && PortFreeCameraKey('S', 0, false), "fly: S alone is still fly mode's");
    Check(PortFreeCameraKey('W', 0x30, true) && PortFreeCameraKey('W', 0x30, false) && gPortFree.held == 0,
          "fly: Option-W, which no table has here, is fly mode's (slower)");
    gPortFreeTableKey = nullptr;
    Check(PortFreeCameraKey('S', 0x30, true) && PortFreeCameraKey('S', 0x30, false), "fly: with no tables Option-S is fly mode's");
    gFirstPersonFlags = kFirstPersonAny;
    Check(!PortFreeCameraKey(9, 0, true) && gPortFree.flyWanted, "first person: Tab is the game's");
    gFirstPersonFlags = 0;
    // the wheel, the drags and the pinch in fly mode
    gPortFree.flyAhead = 0.0f;
    Check(PortFreeWheel(0.0f, 2.0f, false, 0, 9000, true) && Near(gPortFree.flyAhead, 2.0f * kPortFreeFlyWheelStep),
          "fly: a notched wheel moves along the view");
    gPortFree.flyAhead = 0.0f;
    Check(PortFreeWheel(0.0f, -1.0f, true, 0xc, 9010, true) && Near(gPortFree.flyAhead, -kPortFreeFlyWheelStep),
          "fly: Control with the wheel (a Windows pinch) moves back");
    gPortFree.flyAhead = 0.0f;
    Check(PortFreeWheel(0.5f, 0.25f, true, 0, 9020, true) && Near(gPortFree.dYaw, 0.5f * kPortFreeSwipeTurn) &&
              Near(gPortFree.dPitch, -0.25f * kPortFreeSwipeTilt) && gPortFree.flyAhead == 0.0f,
          "fly: a swipe looks");
    Reset();
    Check(PortFreeDragStart(1, 0, true) == 2 && gPortFree.dragButton == 1, "fly: the right button starts a look drag (2: a click passes)");
    gPortFree.flySpeedLive = 1.0f;
    Check(PortFreeWheel(0.0f, 1.0f, false, 0, 9500, true) && Near(gPortFree.flySpeedLive, kPortFreeFlySpeedStep),
          "fly: right button + the wheel speeds up");
    for (int i = 0; i < 20; ++i) PortFreeWheel(0.0f, 1.0f, false, 0, 9600, true);
    Check(gPortFree.flySpeedLive == 4.0f, "fly: the live speed stops at 4 times");
    for (int i = 0; i < 40; ++i) PortFreeWheel(0.0f, -1.0f, false, 0, 9700, true);
    Check(gPortFree.flySpeedLive == 0.25f, "fly: and at a quarter");
    PortFreeDragEnd();
    Check(PortFreeDragStart(2, 0, true) == 1, "fly: the middle button is a look drag as in the orbit");
    PortFreeDragEnd();
    gPortFree.flyAhead = 0.0f;
    PortFreePinch(std::exp(1.0f), true);
    Check(Near(gPortFree.flyAhead, kPortFreeFlyPinchMove, 1e-3f) && gPortFree.zoomScale == 1.0f, "fly: a pinch moves along the view");
    // the move: easing toward the wish, flyAhead taken by the same easing
    {
        const float forward[3] = {0.0f, 0.0f, 1.0f};
        const float right[3] = {1.0f, 0.0f, 0.0f};
        float move[3];
        for (float& v : gPortFree.flyVelocity) v = 0.0f;
        gPortFree.flyAhead = 0.0f;
        PortFreeFlyMove(forward, right, 1.0f, 0.0f, 0.0f, 0.06f, 70.0f, move);
        Check(Near(gPortFree.flyVelocity[2], 0.03f) && Near(move[2], 0.03f * 70.0f) && move[0] == 0.0f && move[1] == 0.0f,
              "fly move: half the speed after 70 ms");
        PortFreeFlyMove(forward, right, 0.0f, -1.0f, 1.0f, 0.06f, 140.0f, move);
        Check(Near(gPortFree.flyVelocity[0], -0.06f) && Near(gPortFree.flyVelocity[1], 0.06f) && gPortFree.flyVelocity[2] == 0.0f,
              "fly move: left and up at full speed after 140 ms, forward stopped");
        for (float& v : gPortFree.flyVelocity) v = 0.0f;
        gPortFree.flyAhead = 20.0f;
        PortFreeFlyMove(forward, right, 0.0f, 0.0f, 0.0f, 0.06f, 70.0f, move);
        Check(Near(move[2], 10.0f) && Near(gPortFree.flyAhead, 10.0f), "fly move: a wheel push goes half its way in 70 ms");
    }
    gPortFree.fly = gPortFree.flyWanted = false;
    gPortFree.held = 0;
    Check(PortFreeDragStart(1, 0, true) == 0, "orbit: the right button is the game's");
    // ---- the low view's hysteresis
    Check(PortFreeLowViewAt(0.40f, false) && !PortFreeLowViewAt(0.45f, false) && PortFreeLowViewAt(0.45f, true) &&
              PortFreeLowViewAt(0.52f, true) && !PortFreeLowViewAt(0.53f, true),
          "low view: in below 25 degrees, out above 30");
    // ---- what a save holds
    Check(PortFreeSaveZoom(25.0f) == 70.0f && PortFreeSaveZoom(300.0f) == 130.0f && PortFreeSaveZoom(100.0f) == 100.0f,
          "a save holds the zoom within 70 to 130");
    Check(Near(PortFreeIsoYaw(0.3f), 0.0f) && Near(PortFreeIsoYaw(0.9f), 1.5707964f) && Near(PortFreeIsoYaw(-0.3f), 0.0f) &&
              Near(PortFreeIsoYaw(-1.0f), -1.5707964f) && Near(PortFreeIsoYaw(3.0f), 3.1415927f) &&
              Near(PortFreeIsoYaw(1.5707964f), 1.5707964f),
          "a save with Isometric on holds the yaw at its nearest quarter turn");
    gPortFreeCamera = false;
    Check(!PortFreeFlying() && !PortFreeHearsFromEye() && !PortFreeCameraKey(9, 0, true), "off: no fly mode, Tab is the game's");
    gPortFreeCamera = true;

    std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
