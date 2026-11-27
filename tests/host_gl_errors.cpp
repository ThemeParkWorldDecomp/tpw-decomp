// host_gl_errors.cpp: the port's fix glerrors (include/PortGlErrors.h), OpenGL errors left by earlier calls against
// a texture's bind (tpw-port #125). A made-up OpenGL error queue stands in for the driver, and COpenGLTexture::Select's
// bind and check (src/engine/OpenGLTexture.cpp) are modelled on it: with an error an earlier call left, the Mac's
// Select turns texturing off; with the switch on, the pending errors are taken first (at most kMostPerBind for one
// bind, the first kLogged logged), so only the bind's own error does. No OpenGL.
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>
#include "PortGlErrors.h"

namespace {
int checks = 0, failures = 0;
void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what); }
}

// The driver's error flags: glGetError answers and clears the oldest, 0 with none.
std::deque<uint32_t> gPending;
uint32_t GetError() {
    if (gPending.empty()) return 0;
    const uint32_t error = gPending.front();
    gPending.pop_front();
    return error;
}
std::vector<uint32_t> gLogged;
std::vector<int32_t> gLoggedCount;
void Logged(uint32_t error, int32_t taken) {
    gLogged.push_back(error);
    gLoggedCount.push_back(taken);
}

// Select's bind and check: answers whether texturing stays on. `bindFails` is the bind's own error.
bool Select(bool bindFails) {
    if (tpw::glerrors::drain) tpw::glerrors::Drain(GetError);
    if (bindFails) gPending.push_back(0x0502);   // GL_INVALID_OPERATION
    return GetError() == 0;
}

void Reset() {
    gPending.clear();
    gLogged.clear();
    gLoggedCount.clear();
    tpw::glerrors::taken = 0;
}

void Off() {
    Reset();
    tpw::glerrors::drain = false;
    Check(Select(false), "off: a bind with no error keeps texturing");
    gPending.push_back(0x0500);   // GL_INVALID_ENUM from an earlier call
    Check(!Select(false), "off: an error an earlier call left turns texturing off (the Mac's)");
    Check(!Select(true), "off: the bind's own error turns it off");
    Check(gLogged.empty() && tpw::glerrors::taken == 0, "off: nothing is taken or logged");
}

void On() {
    Reset();
    tpw::glerrors::drain = true;
    tpw::glerrors::log = Logged;
    gPending.push_back(0x0500);
    gPending.push_back(0x0501);
    Check(Select(false), "on: the errors earlier calls left are taken first, so texturing stays");
    Check(gPending.empty() && tpw::glerrors::taken == 2, "on: both are taken");
    Check(gLogged.size() == 2 && gLogged[0] == 0x0500 && gLogged[1] == 0x0501 && gLoggedCount[1] == 2,
          "on: each is logged with how many so far");
    Check(!Select(true), "on: the bind's own error still turns texturing off");
    Check(tpw::glerrors::taken == 2, "on: the bind's own error is the check's, not taken");
    for (int i = 0; i < 10; ++i) gPending.push_back(0x0505);
    Check(Select(false), "on: ten more are taken");
    Check(tpw::glerrors::taken == 12 && static_cast<int32_t>(gLogged.size()) == tpw::glerrors::kLogged,
          "on: only the first kLogged are logged");
    // A lost context answers an error for ever: one bind takes at most kMostPerBind.
    for (int i = 0; i < 100; ++i) gPending.push_back(0x0507);
    Check(tpw::glerrors::Drain(GetError) == tpw::glerrors::kMostPerBind, "on: at most kMostPerBind for one bind");
    Check(gPending.size() == 100 - tpw::glerrors::kMostPerBind, "on: the rest are left");
    tpw::glerrors::log = nullptr;
    Reset();
    gPending.push_back(0x0500);
    Check(Select(false) && gLogged.empty(), "on: with no log hook, the errors are still taken");
    tpw::glerrors::drain = false;
}
}  // namespace

int main() {
    Check(!tpw::glerrors::drain && tpw::glerrors::log == nullptr, "the switch starts off, with no log");
    Off();
    On();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
