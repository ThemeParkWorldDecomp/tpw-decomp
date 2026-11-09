// host_move_throttle.cpp: walking in first person in a park visited online (the port's fix movethrottle,
// src/game/ui/ui_onlinePanel.cpp UpdateAvatarPos). The online window's tick calls UpdateAvatarPos once a frame drawn; on the
// Mac each frame the avatar's place changed sent a Move to the chat server, and the server closes a chat after 600 requests a
// minute, so about 10 s of walking at 60 frames a second disconnected the player. UpdateAvatarPos, ChatGoFirstPerson and
// ChatBackToThirdPerson are the real ones, cut from src/game/ui/ui_onlinePanel.cpp; the control, the windows and the clock
// are stand-ins that record what they are asked. A minute of walking at 60 and at 144 frames a second must send a Move at
// most once each 250 ms and move the own avatar every frame; the place the player stops at must be sent, also when the stop
// comes inside the 250 ms, and only once; boarding a ride, leaving it and leaving first person must still send at once and
// leave no Move owed behind. Run by ctest ("move_throttle"); made only with the port's fixes, the original sends every frame.
// Exits nonzero on a failure. "tpw_host_move_throttle schedule FILE" also writes what the minute at 60 frames a second sent
// (the entry, each Move and the leave, with its time in microseconds and its map place) to FILE, for a replay against a chat
// server.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "InterfaceWindow.h"
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "UI_Base.h"
#include "lobby.h"
#include "online.h"
#include "platform/samsutils.h"
#include "ui_Postcard.h"
#include "ui_main.h"

// ---- globals of other files the cut functions read or write
uint32_t gFirstPersonFlags;
s3DPoint gCameraViewPoint;
float gCameraSavedPosition[3];
float gCameraPosition[3];
s3DPoint gPointerWorldPosn;
int32_t gMouseWord1E0;
int32_t gMouseWord1E4;
CMouseMode* gMouseMode;
CMouse* gMouse;
uint8_t gOnlineByte78;
uint8_t gOnlineBlockToggle;
uint8_t gOnlinePanelShown;
InterfaceWindow* gOnlineWord80;
InterfaceWindow* gOnlineWindow;
InterfaceWindow* gOnlinePanel;
COnlineControl* gOnlineControl;

namespace {

uint64_t gNow = 1000000;   // the clock, microseconds

// What the control was asked: a Move (UpdateFirstPersonPos), the own avatar moved alone, and the other calls.
enum Kind { kMove, kTeleportOnly, kEnter, kLeave, kHide, kUnhide };
struct Call {
    Kind kind;
    uint64_t at;
    int32_t x;
    int32_t z;
};
std::vector<Call> gCalls;

alignas(16) unsigned char gControlBytes[0x1000];
alignas(16) unsigned char gWindowBytes[16];
alignas(16) unsigned char gPanelBytes[16];

int gFailures = 0;

void Check(bool ok, const char* what) {
    std::printf("%s: %s\n", ok ? "ok" : "FAILED", what);
    if (!ok) ++gFailures;
}

int32_t Pos(MapPos p) { return (p.mCell << 8) | p.mOffset; }

void Record(Kind kind, MapPos x, MapPos z) { gCalls.push_back({kind, gNow, Pos(x), Pos(z)}); }

}  // namespace

// ---- the stand-ins
namespace SamsUtilities {
uint64_t UTimer::GetAbsolute() { return gNow; }
}  // namespace SamsUtilities

int32_t UI_SendMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void InterfaceWindow::setCursor(int16_t) {}
int32_t InterfaceWindow::GetPosition(CoolRect*, int16_t) { return 0; }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t) { return nullptr; }
void InterfaceWindow::CaptureKb(int32_t) {}

CoolRegion::~CoolRegion() {}
bool CoolRegion::PointIn(int16_t, int16_t) { return false; }
void CoolRegion::Middle(int16_t*, int16_t*) {}
void CoolRegion::Shift(int16_t, int16_t) {}
void CoolRegion::Adjust(int16_t, int16_t, int16_t, int16_t) {}
int32_t CoolRegion::operator==(CoolRegion&) { return 0; }
CoolRect::CoolRect() {}
CoolRect::~CoolRect() {}
bool CoolRect::PointIn(int16_t, int16_t) { return false; }
void CoolRect::Middle(int16_t*, int16_t*) {}
void CoolRect::Shift(int16_t, int16_t) {}
void CoolRect::Adjust(int16_t, int16_t, int16_t, int16_t) {}

namespace online_callee {
void* OperatorNew(uint32_t) { return gControlBytes; }
}  // namespace online_callee

COnlineControl::COnlineControl() {}
void COnlineControl::HideAvatar() { gCalls.push_back({kHide, gNow, 0, 0}); }
void COnlineControl::EnteringFirstPerson(MapPos x, MapPos y) { Record(kEnter, x, y); }
void COnlineControl::LeavingFirstPerson(MapPos x, MapPos y) { Record(kLeave, x, y); }
void COnlineControl::UpdateFirstPersonPos(MapPos x, MapPos y) { Record(kMove, x, y); }
void COnlineControl::UnHideAvatar(MapPos x, MapPos y) { Record(kUnhide, x, y); }
void OnlineControlTeleportOwnAvatar(COnlineControl*, MapPos x, MapPos y) { Record(kTeleportOnly, x, y); }

namespace {

// The map place the cut functions make of a position (ui_onlinePanel.cpp ToMapPos): a tenth as cell and 1/256 offset.
int32_t MapOf(float position) {
    const float tenth = position / 10.0f;
    const int32_t cell = static_cast<int32_t>(tenth) & 0xff;
    const int32_t offset = static_cast<int32_t>(256.0f * (tenth - static_cast<float>(cell))) & 0xff;
    return (cell << 8) | offset;
}

void Frame(uint64_t frameTime) {
    gNow += frameTime;
    UpdateAvatarPos();
}

size_t Count(Kind kind, size_t from = 0) {
    size_t n = 0;
    for (size_t i = from; i < gCalls.size(); ++i)
        if (gCalls[i].kind == kind) ++n;
    return n;
}

const Call* Last(Kind kind) {
    for (size_t i = gCalls.size(); i-- > 0;)
        if (gCalls[i].kind == kind) return &gCalls[i];
    return nullptr;
}

void EnterFirstPerson(float x, float z) {
    gFirstPersonFlags = 0x2;
    gPointerWorldPosn.x = x;
    gPointerWorldPosn.z = z;
    gCameraViewPoint.x = x;
    gCameraViewPoint.z = z;
    ChatGoFirstPerson();
}

// A walk of the given seconds at the given frame rate, a step along x each frame (a new map place each frame).
void Walk(double seconds, double rate, float step) {
    const uint64_t frameTime = static_cast<uint64_t>(1000000.0 / rate);
    const int frames = static_cast<int>(seconds * rate);
    for (int i = 0; i < frames; ++i) {
        gCameraViewPoint.x += step;
        Frame(frameTime);
    }
}

void Idle(double seconds, double rate) {
    const uint64_t frameTime = static_cast<uint64_t>(1000000.0 / rate);
    const int frames = static_cast<int>(seconds * rate);
    for (int i = 0; i < frames; ++i) Frame(frameTime);
}

// The most Moves in any one second, and the least time between two Moves, from the call `from` on.
void MoveRates(size_t from, int* mostInASecond, uint64_t* leastGap) {
    std::vector<uint64_t> at;
    for (size_t i = from; i < gCalls.size(); ++i)
        if (gCalls[i].kind == kMove) at.push_back(gCalls[i].at);
    *mostInASecond = 0;
    *leastGap = UINT64_MAX;
    for (size_t i = 0; i < at.size(); ++i) {
        size_t j = i;
        while (j < at.size() && at[j] - at[i] < 1000000) ++j;
        if (static_cast<int>(j - i) > *mostInASecond) *mostInASecond = static_cast<int>(j - i);
        if (i > 0 && at[i] - at[i - 1] < *leastGap) *leastGap = at[i] - at[i - 1];
    }
}

void WalkAt(double rate) {
    char what[160];
    gCalls.clear();
    EnterFirstPerson(500.0f, 700.0f);
    Check(Count(kEnter) == 1, "entering first person sends the avatar's place");
    Idle(1.0, rate);   // the countdown after entering
    const size_t from = gCalls.size();
    const int frames = static_cast<int>(60.0 * rate);
    Walk(60.0, rate, 0.05f);
    const size_t moves = Count(kMove, from);
    const size_t alone = Count(kTeleportOnly, from);
    int most = 0;
    uint64_t least = 0;
    MoveRates(from, &most, &least);
    std::printf("  %.0f frames a second: %d frames, %zu Moves, at most %d in a second, %.1f ms apart at least\n", rate, frames, moves, most,
                static_cast<double>(least) / 1000.0);
    std::snprintf(what, sizeof what, "a minute of walking at %.0f frames a second sends at most four Moves a second", rate);
    Check(most <= 4 && least >= 250000, what);
    std::snprintf(what, sizeof what, "at %.0f frames a second the Moves of a minute and the entry stay under the server's 600 requests", rate);
    Check(moves + 1 < 600 && moves >= 200, what);
    std::snprintf(what, sizeof what, "at %.0f frames a second the own avatar still moves every frame", rate);
    Check(static_cast<int>(moves + alone) == frames, what);

    // The stop: one more step, then standing still.
    const size_t step = gCalls.size();
    gCameraViewPoint.x += 0.05f;
    Frame(static_cast<uint64_t>(1000000.0 / rate));
    Idle(2.0, rate);
    const Call* last = Last(kMove);
    std::snprintf(what, sizeof what, "at %.0f frames a second the place the player stopped at is sent, once", rate);
    Check(Count(kMove, step) == 1 && last != nullptr && last->x == MapOf(gCameraViewPoint.x) && last->z == MapOf(gCameraViewPoint.z), what);
    const size_t afterStop = Count(kMove);
    Idle(2.0, rate);
    std::snprintf(what, sizeof what, "at %.0f frames a second standing still sends nothing more", rate);
    Check(Count(kMove) == afterStop, what);

    ChatBackToThirdPerson();
    Idle(0.5, rate);
}

// Writes the entry, the Moves and the leave of gCalls to path, one a line: the kind, the time in microseconds, x and z.
bool WriteSchedule(const char* path) {
    std::FILE* file = std::fopen(path, "w");
    if (file == nullptr) return false;
    for (const Call& call : gCalls) {
        const char* kind = call.kind == kEnter ? "enter" : call.kind == kMove ? "move" : call.kind == kLeave ? "leave" : nullptr;
        if (kind != nullptr)
            std::fprintf(file, "%s %llu %d %d\n", kind, static_cast<unsigned long long>(call.at), call.x, call.z);
    }
    return std::fclose(file) == 0;
}

}  // namespace

int main(int argc, char** argv) {
    const char* schedule = argc == 3 && std::strcmp(argv[1], "schedule") == 0 ? argv[2] : nullptr;
    gOnlineWindow = reinterpret_cast<InterfaceWindow*>(gWindowBytes);
    gOnlinePanel = reinterpret_cast<InterfaceWindow*>(gPanelBytes);
    gOnlineControl = nullptr;

    WalkAt(60.0);
    if (schedule != nullptr) Check(WriteSchedule(schedule), "the schedule is written");
    WalkAt(144.0);

    // The stop right after a Move: the frame after the Move takes one more step and stops; the stop goes once the 250 ms are over.
    gCalls.clear();
    EnterFirstPerson(500.0f, 700.0f);
    Idle(1.0, 60.0);
    gCameraViewPoint.x += 0.05f;
    Frame(16667);   // a Move (the first since entering)
    const uint64_t movedAt = gNow;
    gCameraViewPoint.x += 0.05f;
    Frame(16667);   // owed
    Check(gCalls.back().kind == kTeleportOnly, "a step inside the 250 ms moves the own avatar without a Move");
    Idle(0.5, 60.0);
    const Call* stop = Last(kMove);
    Check(stop != nullptr && stop->x == MapOf(gCameraViewPoint.x) && stop->at >= movedAt + 250000 && stop->at < movedAt + 250000 + 17000,
          "the stop inside the 250 ms is sent on the first frame after them");

    // Boarding a ride with a place owed: the hide goes at once and nothing is owed after it.
    gCameraViewPoint.x += 0.05f;
    Frame(16667);   // a Move
    gCameraViewPoint.x += 0.05f;
    Frame(16667);   // owed
    gFirstPersonFlags = 0x400;
    Frame(16667);
    const size_t afterHide = gCalls.size();
    Check(gCalls.back().kind == kHide, "boarding a ride hides the avatar at once");
    Idle(1.0, 60.0);
    Check(gCalls.size() == afterHide, "on the ride nothing more is sent");
    // Off the ride: the avatar shows again at once, within 250 ms of the last Move or not.
    gFirstPersonFlags = 0x2;
    gCameraPosition[0] = 620.0f;
    gCameraPosition[2] = 640.0f;
    gCameraViewPoint.x = 620.0f;
    gCameraViewPoint.z = 640.0f;
    Frame(16667);
    Check(gCalls.back().kind == kUnhide && gCalls.back().x == MapOf(620.0f), "leaving the ride shows the avatar at once");
    gCameraViewPoint.x += 0.05f;
    Frame(16667);
    gCameraViewPoint.x += 0.05f;
    Frame(16667);
    // Leaving first person with a place owed: the leave sends the camera's place, and no Move comes after it.
    const size_t beforeLeave = gCalls.size();
    gFirstPersonFlags = 0;
    ChatBackToThirdPerson();
    Idle(1.0, 60.0);
    Check(Count(kLeave, beforeLeave) == 1 && Count(kMove, beforeLeave) == 0, "leaving first person sends the leave and no Move after it");
    // Entering again later: nothing owed from before is sent; the first step is sent at once.
    const size_t beforeEnter = gCalls.size();
    EnterFirstPerson(800.0f, 800.0f);
    Idle(1.0, 60.0);
    Check(Count(kMove, beforeEnter) == 0, "entering first person again sends no Move left over");
    gCameraViewPoint.x += 0.05f;
    Frame(16667);
    Check(gCalls.back().kind == kMove, "the first step after entering is sent at once");

    if (gFailures != 0) {
        std::fprintf(stderr, "FAILED: %d checks\n", gFailures);
        return 1;
    }
    std::printf("move throttle: OK\n");
    return 0;
}
