// host_gamepad.cpp: the launcher's Free camera from a gamepad (the port's fix gamepad, include/PortFreeCamera.h), fed
// SDL's standard layout's axis levels and buttons as the run loop gives them. Off (the Free camera, or its Gamepad row),
// nothing is kept or pressed. On: the sticks do nothing inside the dead zone, then rise on a squared curve; each frame's
// deltas grow with the frame's ms (two frames of 10 ms are one of 20), with the Look sensitivity, Invert vertical look and
// Shift; the right stick turns and tilts, the left moves the orbit, the triggers zoom; in fly mode the left stick and the
// triggers are fly shares; the shoulders and Back press the camera table's turn keys and Home and let go of them, Y starts
// and ends fly mode as Tab does, none of them in the lobby or in first person; a pad gone lets go of all of it. The camera
// table is a fake that records its calls.
// Run by ctest ("gamepad") in a build with the port's fixes, which alone has the functions. Exits nonzero on a failure.
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

bool Near(float a, float b, float within = 1e-6f) { return std::fabs(a - b) <= within * std::fmax(1.0f, std::fabs(b)); }

// Nothing pending, the pad at rest.
void Reset() {
    PortFreeClearPending();
    PortFreePadGone();
    Calls();
}

bool NothingPending() {
    return gPortFree.dYaw == 0.0f && gPortFree.dPitch == 0.0f && gPortFree.dForward == 0.0f && gPortFree.dRight == 0.0f &&
           gPortFree.zoomScale == 1.0f;
}

// The curve's share for a stick held at level along one axis (the radial dead zone of 8000 of 32767).
float Share(float level) {
    const float way = (level - 8000.0f) / (32767.0f - 8000.0f);
    return way * way;
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
    gPortFreeModifiers = [] { return 0; };

    // ---- off: the Free camera off, then on with its Gamepad row off
    gPortFreePadOn = true;
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadFrame(16.0f);
    Check(gPortFreePad.axis[kPortFreePadRightX] == 0 && NothingPending(), "Free camera off: a stick is not kept, nothing moves");
    Check(!PortFreePadButton(kPortFreePadLeftShoulder, true, true) && !PortFreePadButton(kPortFreePadNorth, true, true) &&
              Calls().empty() && !gPortFree.flyWanted,
          "Free camera off: no button is taken");
    gPortFreeCamera = true;
    gPortFreePadOn = false;
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadFrame(16.0f);
    Check(gPortFreePad.axis[kPortFreePadRightX] == 0 && NothingPending(), "Gamepad off: a stick is not kept, nothing moves");
    Check(!PortFreePadButton(kPortFreePadBack, true, true) && Calls().empty(), "Gamepad off: no button is taken");
    gPortFreePadOn = true;

    // ---- no pad: a frame adds nothing
    PortFreePadFrame(16.0f);
    Check(NothingPending(), "no pad: a frame adds nothing");

    // ---- the dead zone and the curve
    PortFreePadAxis(kPortFreePadRightX, 7000);
    PortFreePadAxis(kPortFreePadRightY, -3000);   // 7616 from the middle
    PortFreePadAxis(kPortFreePadLeftX, 5000);
    PortFreePadAxis(kPortFreePadLeftY, -5000);   // 7071 from the middle: inside the circle
    PortFreePadAxis(kPortFreePadRightTrigger, 3855);
    PortFreePadFrame(16.0f);
    Check(NothingPending(), "inside the dead zone (8000) and the trigger's (3855) nothing moves");
    Reset();
    PortFreePadAxis(kPortFreePadRightX, 20000);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, Share(20000.0f) * kPortFreePadTurn * 10.0f) && gPortFree.dPitch == 0.0f,
          "past the dead zone the right stick turns on the squared curve");
    Reset();
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, kPortFreePadTurn * 10.0f), "at full tilt the right stick turns kPortFreePadTurn a ms");
    Reset();
    PortFreePadAxis(kPortFreePadRightX, -32768);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, -kPortFreePadTurn * 10.0f), "full left turns left, no further than full right");
    Reset();
    PortFreePadAxis(kPortFreePadRightX, 23170);   // a diagonal at full tilt: each axis 0.707 of the way
    PortFreePadAxis(kPortFreePadRightY, 23170);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, 0.70710678f * kPortFreePadTurn * 10.0f, 1e-3f) &&
              Near(gPortFree.dPitch, 0.70710678f * kPortFreePadTilt * 10.0f, 1e-3f),
          "a full diagonal keeps its direction (the dead zone is a circle)");

    // ---- dt: speed does not depend on the frame rate
    Reset();
    PortFreePadAxis(kPortFreePadRightX, 25000);
    PortFreePadAxis(kPortFreePadRightY, 15000);
    PortFreePadAxis(kPortFreePadLeftX, -20000);
    PortFreePadAxis(kPortFreePadLeftY, -30000);
    PortFreePadAxis(kPortFreePadRightTrigger, 30000);
    PortFreePadFrame(10.0f);
    PortFreePadFrame(10.0f);
    const float yaw10 = gPortFree.dYaw, pitch10 = gPortFree.dPitch, forward10 = gPortFree.dForward,
                right10 = gPortFree.dRight, zoom10 = gPortFree.zoomScale;
    PortFreeClearPending();
    PortFreePadFrame(20.0f);
    Check(Near(gPortFree.dYaw, yaw10, 1e-5f) && Near(gPortFree.dPitch, pitch10, 1e-5f) &&
              Near(gPortFree.dForward, forward10, 1e-5f) && Near(gPortFree.dRight, right10, 1e-5f) &&
              Near(gPortFree.zoomScale, zoom10, 1e-5f),
          "two frames of 10 ms move as one of 20 ms");
    PortFreeClearPending();
    PortFreePadFrame(0.0f);
    Check(NothingPending(), "a frame of 0 ms (paused, or the first) moves nothing");

    // ---- which way each part goes
    Reset();
    PortFreePadAxis(kPortFreePadLeftY, -32768);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dForward, kPortFreePadMove * 10.0f) && gPortFree.dRight == 0.0f,
          "the left stick up moves the orbit forward");
    Reset();
    PortFreePadAxis(kPortFreePadLeftX, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dRight, kPortFreePadMove * 10.0f) && gPortFree.dForward == 0.0f, "the left stick right moves it right");
    Reset();
    gPortFreeModifiers = [] { return 3; };   // Shift
    PortFreePadAxis(kPortFreePadLeftX, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dRight, kPortFreePadMove * 10.0f * 3.0f), "Shift held moves it three times as fast");
    gPortFreeModifiers = [] { return 0; };
    Reset();
    PortFreePadAxis(kPortFreePadRightTrigger, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.zoomScale, std::exp(-kPortFreePadZoom * 10.0f)) && gPortFree.zoomScale < 1.0f,
          "the right trigger zooms in");
    Reset();
    PortFreePadAxis(kPortFreePadLeftTrigger, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.zoomScale, std::exp(kPortFreePadZoom * 10.0f)), "the left trigger zooms out");
    Reset();
    PortFreePadAxis(kPortFreePadLeftTrigger, 32767);
    PortFreePadAxis(kPortFreePadRightTrigger, 32767);
    PortFreePadFrame(10.0f);
    Check(gPortFree.zoomScale == 1.0f, "both triggers cancel out");
    Reset();
    PortFreePadAxis(kPortFreePadRightY, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dPitch, kPortFreePadTilt * 10.0f), "the right stick down tilts as a mouse drag down does");

    // ---- the settings
    Reset();
    gPortFreeLookSensitivity = 2.5f;
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadAxis(kPortFreePadRightY, 32767);   // full tilt both ways: 0.707 of each
    const float half = 0.70710678f;
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, half * kPortFreePadTurn * 25.0f, 1e-5f) && Near(gPortFree.dPitch, half * kPortFreePadTilt * 25.0f, 1e-5f),
          "Look sensitivity 2.5 turns and tilts 2.5 times as far");
    gPortFreeLookSensitivity = 1.0f;
    PortFreeClearPending();
    gPortFreeInvertLook = true;
    PortFreePadFrame(10.0f);
    Check(Near(gPortFree.dYaw, half * kPortFreePadTurn * 10.0f, 1e-5f) && Near(gPortFree.dPitch, -half * kPortFreePadTilt * 10.0f, 1e-5f),
          "Invert vertical look flips the tilt, not the turn");
    gPortFreeInvertLook = false;

    // ---- first person: nothing
    Reset();
    gFirstPersonFlags = 0x2;
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadAxis(kPortFreePadLeftY, -32767);
    PortFreePadFrame(10.0f);
    Check(NothingPending(), "in first person the sticks move nothing");
    Check(!PortFreePadButton(kPortFreePadLeftShoulder, true, true) && !PortFreePadButton(kPortFreePadNorth, true, true) &&
              Calls().empty() && !gPortFree.flyWanted,
          "in first person the buttons press nothing");
    gFirstPersonFlags = 0;

    // ---- the buttons
    Reset();
    Check(PortFreePadButton(kPortFreePadLeftShoulder, true, true) && Calls() == "down 9472",
          "the left shoulder presses the left arrow (turn, the Isometric snap kept)");
    Check(PortFreePadButton(kPortFreePadLeftShoulder, true, true) && Calls().empty(), "a second down presses nothing more");
    Check(PortFreePadButton(kPortFreePadLeftShoulder, false, true) && Calls() == "up 9472", "its release lets go of it");
    Check(PortFreePadButton(kPortFreePadLeftShoulder, false, true) && Calls().empty(), "a second release lets go of nothing");
    Check(PortFreePadButton(kPortFreePadRightShoulder, true, true) && PortFreePadButton(kPortFreePadRightShoulder, false, true) &&
              Calls() == "down 9984, up 9984",
          "the right shoulder is the right arrow");
    Check(PortFreePadButton(kPortFreePadBack, true, true) && PortFreePadButton(kPortFreePadBack, false, true) &&
              Calls() == "down 9216, up 9216",
          "Back is Home (the reset)");
    Check(!PortFreePadButton(0, true, true) && !PortFreePadButton(1, true, true) && !PortFreePadButton(6, true, true) &&
              !PortFreePadButton(11, true, true) && Calls().empty(),
          "A, B, Start and the D-pad are not taken");
    Check(!PortFreePadButton(kPortFreePadRightShoulder, true, false) && !PortFreePadButton(kPortFreePadBack, true, false) &&
              !PortFreePadButton(kPortFreePadNorth, true, false) && Calls().empty() && !gPortFree.flyWanted,
          "with no park shown (the lobby) no button is taken");
    Check(PortFreePadButton(kPortFreePadRightShoulder, true, true) && Calls() == "down 9984" &&
              PortFreePadButton(kPortFreePadRightShoulder, false, false) && Calls() == "up 9984",
          "a release is taken even when the park has gone");

    // ---- Y: fly mode, as Tab
    Reset();
    Check(PortFreePadButton(kPortFreePadLeftShoulder, true, true) && Calls() == "down 9472", "(the left shoulder held)");
    Check(PortFreePadButton(kPortFreePadNorth, true, true) && gPortFree.flyWanted && Calls() == "up 9472",
          "Y starts fly mode, letting go of the orbit's turn key");
    Check(PortFreePadButton(kPortFreePadNorth, true, true) && gPortFree.flyWanted, "a second down (held) does not toggle back");
    Check(PortFreePadButton(kPortFreePadNorth, false, true) && gPortFree.flyWanted, "its release keeps flying");
    Check(!PortFreePadButton(kPortFreePadLeftShoulder, true, true) && Calls().empty(), "flying, the shoulders do not turn");
    PortFreePadAxis(kPortFreePadLeftY, -32767);
    PortFreePadAxis(kPortFreePadLeftX, 16000);
    PortFreePadAxis(kPortFreePadRightTrigger, 32767);
    PortFreePadAxis(kPortFreePadLeftTrigger, 32767);
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadFrame(10.0f);
    // (16000, -32767) is past the end: full tilt, in its direction
    const float length = std::sqrt(16000.0f * 16000.0f + 32767.0f * 32767.0f);
    Check(Near(gPortFreePad.fly[0], 32767.0f / length, 1e-5f) && Near(gPortFreePad.fly[1], 16000.0f / length, 1e-5f) &&
              gPortFreePad.fly[2] == 0.0f,
          "flying, the left stick is the forward and sideways shares, the triggers cancel out");
    Check(gPortFree.dForward == 0.0f && gPortFree.dRight == 0.0f && gPortFree.zoomScale == 1.0f &&
              Near(gPortFree.dYaw, kPortFreePadTurn * 10.0f),
          "flying, the orbit is not moved but the right stick looks");
    PortFreePadAxis(kPortFreePadLeftTrigger, 0);
    PortFreePadAxis(kPortFreePadLeftY, 0);
    PortFreePadAxis(kPortFreePadLeftX, 0);
    PortFreePadFrame(10.0f);
    Check(gPortFreePad.fly[0] == 0.0f && gPortFreePad.fly[1] == 0.0f && Near(gPortFreePad.fly[2], 1.0f),
          "flying, the right trigger rises");
    PortFreePadAxis(kPortFreePadRightTrigger, 0);
    PortFreePadAxis(kPortFreePadLeftTrigger, 32767);
    PortFreePadFrame(10.0f);
    Check(Near(gPortFreePad.fly[2], -1.0f), "and the left sinks");
    Check(PortFreePadButton(kPortFreePadNorth, true, true) && !gPortFree.flyWanted && PortFreePadButton(kPortFreePadNorth, false, true),
          "Y again ends fly mode");
    PortFreePadFrame(10.0f);
    Check(gPortFreePad.fly[2] == 0.0f, "out of fly mode the shares are 0");
    gPortFree.flyWanted = true;
    Check(PortFreePadButton(kPortFreePadBack, true, true) && Calls() == "down 9216", "flying, Back still resets (Home)");
    gPortFree.flyWanted = false;
    PortFreePadButton(kPortFreePadBack, false, true);
    Calls();

    // ---- gone: unplugged, or the focus lost
    Reset();
    PortFreePadAxis(kPortFreePadRightX, 32767);
    PortFreePadAxis(kPortFreePadLeftY, 32767);
    PortFreePadButton(kPortFreePadLeftShoulder, true, true);
    PortFreePadButton(kPortFreePadBack, true, true);
    Calls();
    PortFreePadGone();
    Check(Calls() == "up 9472, up 9216", "a pad gone lets go of the keys its buttons held");
    PortFreePadFrame(10.0f);
    Check(NothingPending(), "and its sticks move nothing more");
    PortFreePadButton(kPortFreePadRightShoulder, true, true);
    Calls();
    PortFreeLetGoAll();
    PortFreePadLetGoButtons();
    Check(Calls() == "up 9984", "the park's window going lets go of a held shoulder");

    std::printf("%s\n", gFailures == 0 ? "all passed" : "FAILURES");
    return gFailures == 0 ? 0 : 1;
}
