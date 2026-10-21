// Test library for the lobby's small leaves part 8 (unit w88; libtpw_unit_LobbyLeaves8.dylib, tests/test_LobbyLeaves8.py):
//   src/game/rides/RideDB.cpp         CRideDatabase::DeleteCustomEffects 0x10156e34
//   src/game/ui/ui_main.cpp          PauseUI 0x1018679c, TogglePauseUI 0x10186854
//   src/game/ui/ui_mainpanel2.cpp   ShowPauseWindow 0x101a86d0, TogglePauseWindow 0x101a87c4, PauseWindowMsg 0x101a8504
//   src/game/ui/ui_mainpanel2.cpp          the pause window's script (0x102aacfc) and animation (0x102aadbc)
//   src/game/ui/ui_mainpanel2.cpp    gPauseWindow 0x103949cc and HidePauseWindow (only the global is used)
//
// Real: the sources above and the rectangle (src/game/ui/CoolRect.cpp, src/game/ui/CoolRegion.cpp, src/game/ui/UI_Base.cpp). Fakes that record the same event on both sides: PAR_KillLibEmitter,
// SOUND_FreeCatagory, UI_PauseTimerSystem, UI_UnPauseTimerSystem, UI_SendMessage, UIMain_DecideKbCapture, Dialog (which also logs the
// words of the script it was given), DefaultDialogCallback, InterfaceWindow::GetPosition (it writes the rectangle from the case's queue),
// setPivot and startAnim (which also logs the animation's frames), CKeyTable::Search and CGame::TogglePause. Each operation has one entry
// point: a description of numbers in, a dump of numbers out, the events in the call log. The Mac-layout objects of a case are built in
// emulator memory by the test module; here the windows and the rides are plain blocks (the fakes never look inside the windows).
#include <cstdint>
#include <cstring>

#include "calllog.h"

#include "Game.h"
#include "InterfaceWindow.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "keyboard.h"
#include "rsse.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"

using namespace calllog;

// ---- globals of other files that the unit owns here
// The game is only an address here (its string members need the string library): raw storage under the symbol's name.
#ifdef __APPLE__
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("_gGame");
#else
extern unsigned char gGameStorage[sizeof(CGame)] __asm__("gGame");
#endif
alignas(16) unsigned char gGameStorage[sizeof(CGame)] = {};
InterfaceWindow* gUIRootWindow;
CKeyTable* gKeyTable;

namespace {

enum Label : int32_t { kWindowA = 1, kWindowB = 2, kWindowC = 3, kRoot = 4, kGame = 5, kTable = 6, kDatabase = 7 };

// Windows are blocks of bytes: the fakes only use their addresses.
alignas(16) unsigned char gWindowBytes[4][0x140];
InterfaceWindow* Window(int32_t k) { return reinterpret_cast<InterfaceWindow*>(gWindowBytes[k]); }

// The words of an array as the events carry them, ten to an event: (first index, ten words).
void LogWords(const char* name, const int16_t* words, int32_t count) {
    for (int32_t i = 0; i < count; i += 10) {
        ev(name, {i, i < count ? words[i] : 0, i + 1 < count ? words[i + 1] : 0, i + 2 < count ? words[i + 2] : 0,
                  i + 3 < count ? words[i + 3] : 0, i + 4 < count ? words[i + 4] : 0, i + 5 < count ? words[i + 5] : 0,
                  i + 6 < count ? words[i + 6] : 0, i + 7 < count ? words[i + 7] : 0, i + 8 < count ? words[i + 8] : 0,
                  i + 9 < count ? words[i + 9] : 0});
    }
}

// What a fake does to the state behind the function's back, from the case's queue "Hook": 1 puts the C window in gPauseWindow, 2 clears it,
// 3 sets the ride count to "HookCount", 4 sets the walk's position to "HookPos".
CRideDatabase* gDatabase;
void Hook() {
    const int64_t action = ret("Hook", 0);
    if (action == 1) gPauseWindow = Window(2);
    if (action == 2) gPauseWindow = nullptr;
    if (action == 3 && gDatabase != nullptr) gDatabase->mRideCount = static_cast<int32_t>(ret("HookCount", 0));
    if (action == 4 && gDatabase != nullptr) gDatabase->mF72C = static_cast<int32_t>(ret("HookPos", 0));
}

}  // namespace

// ---- the fakes
void PAR_KillLibEmitter(uint32_t slot) {
    ev("PAR_KillLibEmitter", {slot});
    Hook();
}
void SOUND_FreeCatagory(int32_t category) {
    ev("SOUND_FreeCatagory", {category});
    Hook();
}
void UI_PauseTimerSystem() { ev("UI_PauseTimerSystem"); }
void UI_UnPauseTimerSystem() { ev("UI_UnPauseTimerSystem"); }
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {lab(window), message, a, b});
    Hook();
    return 0;
}
void UIMain_DecideKbCapture() { ev("UIMain_DecideKbCapture"); }
// The answer is a window from the case's queue: 0 NULL, 1 the B window.
InterfaceWindow* Dialog(int16_t* script, InterfaceWindowCallback callback, int32_t modal) {
    ev("Dialog", {script == gPauseDialogDescription, callback == PauseWindowMsg, modal});
    if (script == gPauseDialogDescription) LogWords("Dialog.script", script, 21);
    Hook();
    return ret("Dialog", 0) != 0 ? Window(1) : nullptr;
}
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("DefaultDialogCallback", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("DefaultAnswer", 0));
}
// Writes the rectangle from the case's queue "Rect" (left, top, right, bottom).
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    ev("GetPosition", {lab(this), subtract});
    rect->mLeft = static_cast<int16_t>(ret("Rect", 0));
    rect->mTop = static_cast<int16_t>(ret("Rect", 0));
    rect->mRight = static_cast<int16_t>(ret("Rect", 0));
    rect->mBottom = static_cast<int16_t>(ret("Rect", 0));
    return 1;
}
void InterfaceWindow::setPivot(int16_t x, int16_t y) { ev("setPivot", {lab(this), x, y}); }
int32_t InterfaceWindow::startAnim(UI_RectDelta* anim, int32_t ticks) {
    ev("startAnim", {lab(this), anim == gPauseAnim, ticks});
    int16_t words[20] = {};
    if (anim == gPauseAnim) std::memcpy(words, anim, sizeof words);
    LogWords("startAnim.frames", words, 20);
    return anim != nullptr;
}
// The answer comes from the queue "Search" (32 bits: the caller takes the low 16).
int32_t CKeyTable::Search(int16_t key, int16_t modifiers) {
    ev("Search", {lab(this), key, modifiers});
    return static_cast<int32_t>(ret("Search", 0));
}
void CGame::TogglePause(bool allSounds) { ev("TogglePause", {lab(this), allSounds}); }

namespace {

// ---------------------------------------------------------------------------------------------------- DeleteCustomEffects

constexpr int kSlots = 8;
alignas(16) unsigned char gDatabaseStorage[sizeof(CRideDatabase)];
alignas(16) unsigned char gRideStorage[kSlots][sizeof(RideInfo)];

// description: the ride count, the fill byte, the slots filled (at most 8), then per slot: kind (0 NULL, 1 a ride), four emitter words, the
//   sound category
// dump: the walk's position, the count, per slot the ride's five words (0 for a NULL slot), whether the words between are unchanged
int32_t RunEffects(const int64_t* d, int64_t* out) {
    const int32_t count = static_cast<int32_t>(d[0]);
    const int32_t slots = static_cast<int32_t>(d[2]);
    tpw_cl_on = 0;
    gDatabase = reinterpret_cast<CRideDatabase*>(gDatabaseStorage);
    std::memset(gDatabaseStorage, static_cast<int>(d[1]), sizeof gDatabaseStorage);
    tpw_cl_label(kDatabase, gDatabase);
    for (int32_t i = 0; i < 0x14a; ++i) gDatabase->mRides[i] = nullptr;
    for (int32_t i = 0; i < slots; ++i) {
        const int64_t* c = d + 3 + 6 * i;
        if (c[0] == 0) continue;
        RideInfo* ride = reinterpret_cast<RideInfo*>(gRideStorage[i]);
        std::memset(gRideStorage[i], 0x5a, sizeof gRideStorage[i]);
        for (int32_t k = 0; k < 4; ++k) ride->mEmitters[k] = static_cast<uint32_t>(c[1 + k]);
        ride->mSoundCategory = static_cast<int32_t>(c[5]);
        gDatabase->mRides[i] = ride;
    }
    gDatabase->mRideCount = count;
    gDatabase->mF72C = 0x12345;
    unsigned char before[sizeof(CRideNameDB) + sizeof gDatabase->mF6AC];
    std::memcpy(before, &gDatabase->mNames, sizeof(CRideNameDB));
    std::memcpy(before + sizeof(CRideNameDB), gDatabase->mF6AC, sizeof gDatabase->mF6AC);

    tpw_cl_on = 1;
    gDatabase->DeleteCustomEffects();
    tpw_cl_on = 0;

    int32_t n = 0;
    out[n++] = gDatabase->mF72C;
    out[n++] = gDatabase->mRideCount;
    for (int32_t i = 0; i < slots; ++i) {
        const RideInfo* ride = gDatabase->mRides[i];
        for (int32_t k = 0; k < 4; ++k) out[n++] = ride != nullptr ? ride->mEmitters[k] : 0;
        out[n++] = ride != nullptr ? ride->mSoundCategory : 0;
    }
    out[n++] = std::memcmp(before, &gDatabase->mNames, sizeof(CRideNameDB)) == 0 &&
               std::memcmp(before + sizeof(CRideNameDB), gDatabase->mF6AC, sizeof gDatabase->mF6AC) == 0;
    gDatabase = nullptr;
    return n;
}

// ---------------------------------------------------------------------------------------------------- the pause end of the UI

// description: the operation (0 PauseUI, 1 TogglePauseUI, 2 ShowPauseWindow, 3 TogglePauseWindow), the root window (0 none, 1 one),
//   gPauseWindow (0 none, 1 the A window)
// dump: the root window and gPauseWindow as labels
int32_t RunPause(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    gDatabase = nullptr;
    tpw_cl_label(kWindowA, Window(0));
    tpw_cl_label(kWindowB, Window(1));
    tpw_cl_label(kWindowC, Window(2));
    tpw_cl_label(kRoot, Window(3));
    gUIRootWindow = d[1] != 0 ? Window(3) : nullptr;
    gPauseWindow = d[2] != 0 ? Window(0) : nullptr;

    tpw_cl_on = 1;
    switch (d[0]) {
    case 0: PauseUI(); break;
    case 1: TogglePauseUI(); break;
    case 2: ShowPauseWindow(); break;
    case 3: TogglePauseWindow(); break;
    }
    tpw_cl_on = 0;

    out[0] = lab(gUIRootWindow);
    out[1] = lab(gPauseWindow);
    return 2;
}

// ---------------------------------------------------------------------------------------------------- PauseWindowMsg

alignas(16) unsigned char gKeyTableBytes[sizeof(CKeyTable)];

// description: the message, a, b, the key table's enabled word, gPauseWindow (0 none, 1 the A window), the table's other fields' fill byte
// dump: the answer, gPauseWindow as a label, the enabled word, whether the table's other bytes are unchanged
int32_t RunMessage(const int64_t* d, int64_t* out) {
    tpw_cl_on = 0;
    gDatabase = nullptr;
    tpw_cl_label(kWindowA, Window(0));
    tpw_cl_label(kWindowB, Window(1));
    tpw_cl_label(kWindowC, Window(2));
    tpw_cl_label(kRoot, Window(3));
    tpw_cl_label(kGame, gGameStorage);
    CKeyTable* table = reinterpret_cast<CKeyTable*>(gKeyTableBytes);
    std::memset(gKeyTableBytes, static_cast<int>(d[5]), sizeof gKeyTableBytes);
    table->mEnabled = static_cast<int16_t>(d[3]);
    tpw_cl_label(kTable, table);
    gKeyTable = table;
    gPauseWindow = d[4] != 0 ? Window(0) : nullptr;
    unsigned char before[sizeof gKeyTableBytes];
    std::memcpy(before, gKeyTableBytes, sizeof before);

    tpw_cl_on = 1;
    const int32_t answer = PauseWindowMsg(Window(3), static_cast<int32_t>(d[0]), static_cast<int32_t>(d[1]), static_cast<int32_t>(d[2]));
    tpw_cl_on = 0;

    out[0] = answer;
    out[1] = lab(gPauseWindow);
    out[2] = table->mEnabled;
    out[3] = std::memcmp(before, gKeyTableBytes, sizeof before) == 0;
    return 4;
}

}  // namespace

extern "C" {

int32_t tpw_l8_effects(const int64_t* d, int64_t* out) { return RunEffects(d, out); }
int32_t tpw_l8_pause(const int64_t* d, int64_t* out) { return RunPause(d, out); }
int32_t tpw_l8_message(const int64_t* d, int64_t* out) { return RunMessage(d, out); }

}  // extern "C"
