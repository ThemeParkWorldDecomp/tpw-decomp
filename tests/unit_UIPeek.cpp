// Unit w55: every external boundary records, including the IME struct return and the rectangle result.
#include <cstddef>
#include <cstdarg>
#include <cstring>
#include <new>
#include "calllog.h"
#include "ui_main.h"
#include "ui_ime.h"
#include "ui_controls.h"
#include "effect_manager.h"
#include "HLSprite.h"
#include "TPMain.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "TbITable.h"
#include "fake_arena_on_heap.h"
using namespace calllog;

EMEffect gEffects[3];
int32_t gSoundBanks[8];
CPlayerSettings gPlayerSettings;
void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}
GrafPort* gMainWindow;
InterfaceWindow* gUIRootWindow;
UIKeyCapture gUIKeyCapture;
InterfaceMessageQueue* gUIMessageQueue;
InterfaceMessageQueue* gUIDrawQueue;
InterfaceTimedMessages* gUITimedMessages;
int32_t (*gUIPeekCallBack)(InterfaceWindow*, int32_t, int32_t, int32_t);
clockTickFunction* gUIClock;
InterfaceTimer* gUITimers;
int32_t gUITimerCount;
InterfaceTimer* gUITimerList;
int32_t gUITimersPaused;
int32_t gUIActive;

// Unused pump boundaries furnish linkage for UIInterface::UnPause in the same source.
void bthread_mutex::lock() {}
void bthread_mutex::unlock() {}
int32_t highestZ(InterfaceWindow*, CoolRect*) { return 0; }
const unsigned char* TbITable::GetConstString(uint32_t) const { return nullptr; }
char16_t* UIMain_ConvertMulti(const unsigned char*) { return nullptr; }
int32_t ErrorDlgCancel(void*) { return 0; }
int32_t ErrorMessageCallback(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
InterfaceWindow* UI_DialogBox(dlgboxparams&, dlgbutton, dlgcallback, dlgbutton, dlgcallback, dlgbutton, dlgcallback, bool) { return nullptr; }
void UI_ActuallyDoPostcard() {}
void msl_assertion_failed(const char*, const char*, int32_t) {}
int32_t UI_PostMessage(InterfaceWindow*, int32_t, int32_t, int32_t) { return 0; }
void InterfaceWindow::SetCallbackFunction(InterfaceWindowCallback) {}
int32_t InterfaceWindow::CallWindowHandler(int32_t, int32_t, int32_t) { return 0; }

// The fixture uses actual native classes and their vtables; setup populates the observed fields.
InterfaceWindow::InterfaceWindow(InterfaceWindow*, int32_t, int32_t, int16_t, int16_t, int16_t, int16_t) {}
InterfaceWindow::~InterfaceWindow() {}
void InterfaceWindow::DrawFunction() {}
void InterfaceWindow::CalcClientArea() {}
int32_t InterfaceWindow::WhatAmI() { ev("WhatAmI", {lab(this)}); return 1; }
void InterfaceWindow::ClientDraw() {}
int16_t InterfaceWindow::Enable(int16_t) { return 0; }
int32_t InterfaceWindow::operator-(InterfaceWindow&) { return 0; }
int32_t InterfaceWindow::SetValue(int32_t) { return 0; }
int32_t InterfaceWindow::GetValue() { return 0; }
int32_t InterfaceWindow::SetText(char16_t*) { return 0; }
char16_t* InterfaceWindow::GetText() { return nullptr; }
int32_t InterfaceWindow::GetHelpText() { return 0; }
InterfaceButton::InterfaceButton(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceButton::~InterfaceButton() {}
int32_t InterfaceButton::WhatAmI() { ev("WhatAmI", {lab(this)}); return 2; }
int16_t InterfaceButton::Enable(int16_t) { return 0; }
int32_t InterfaceButton::SetText(char16_t*) { return 0; }
char16_t* InterfaceButton::GetText() { return nullptr; }
int32_t InterfaceButton::GetHelpText() { return 0; }
int8_t InterfaceButton::GetState() { ev("GetState", {lab(this)}); return mState; }
InterfaceEditBox::InterfaceEditBox(InterfaceWindow* p, int32_t a, int32_t id, int16_t l, int16_t t, int16_t r, int16_t b)
    : InterfaceWindow(p, a, id, l, t, r, b) {}
InterfaceEditBox::~InterfaceEditBox() {}
void InterfaceEditBox::DrawFunction() {}
void InterfaceEditBox::CalcClientArea() {}
int32_t InterfaceEditBox::WhatAmI() { ev("WhatAmI", {lab(this)}); return 5; }
void InterfaceEditBox::ClientDraw() {}
int32_t InterfaceEditBox::SetText(char16_t*) { return 0; }
int32_t InterfaceEditBox::GetCurrentLen() {
    int32_t n = 0;
    if (mBuffer != nullptr) while (mBuffer[n]) ++n;
    ev("GetCurrentLen", {lab(this)});
    return static_cast<int16_t>(n);
}
void InterfaceEditBox::LockOutMouse(bool on) { ev("LockOutMouse", {lab(this), on}); mF161 = on; }
InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), id});
    for (auto* child = mChild; child != nullptr; child = child->mNext) if (child->mId == id) return child;
    return nullptr;
}
int32_t InterfaceWindow::GetPosition(CoolRect* rect, int16_t subtract) {
    rect->mLeft = static_cast<int16_t>(mRect.mLeft - (subtract ? mAnimOffset[0] : 0));
    rect->mTop = static_cast<int16_t>(mRect.mTop - (subtract ? mAnimOffset[1] : 0));
    rect->mRight = static_cast<int16_t>(mRect.mRight - (subtract ? mAnimOffset[2] : 0));
    rect->mBottom = static_cast<int16_t>(mRect.mBottom - (subtract ? mAnimOffset[3] : 0));
    ev("GetPosition", {lab(this), lab(rect), subtract, rect->mLeft, rect->mTop, rect->mRight, rect->mBottom});
    return 1;
}
int32_t SOUND_SendEvent(int32_t e, int32_t bank, int32_t event, int32_t a, int32_t b, int32_t c) {
    ev("Sound", {e, bank, event, a, b, c});
    return 0;
}
void CTRACE(uint32_t level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    const double area = va_arg(ap, double);
    const int32_t count = va_arg(ap, int);
    va_end(ap);
    uint64_t bits;
    std::memcpy(&bits, &area, 8);
    ev("Trace", {level, static_cast<int64_t>(bits), count}, {format});
}
int32_t EM_SetEmitersOn(int32_t effect, int32_t count, int32_t emitter) {
    ev("Emitters", {effect, count, emitter});
    // A deterministic boundary model, so later Kill observes the count changed by On/Off.
    gEffects[effect].mCount = count;
    for (int i = 0; i < 5; ++i) gEffects[effect].mEmitters[i] = i < count ? 100 + i : 0;
    return 0;
}
void PAR_SetEmitterMask(int32_t handle, int32_t mask) { ev("Mask", {handle, mask}); }
uint32_t RealTimePauseAdjusted() {
    const uint32_t tick = static_cast<uint32_t>(ret("Clock"));
    ev("Clock", {tick});
    return tick;
}
// Unit w92 made gUIIME's classes real (src/game/ui/ui_graph.cpp, src/game/ui/ui_iterators.cpp, src/game/ui/IME.cpp); this shim fakes their lifetime with empty bodies,
// since the test fills the object with a pattern and compares it.
IME::MslWideString::MslWideString() {}
IME::MslWideString::~MslWideString() {}
IME::CIME::CIME() {}
IME::CIME::~CIME() {}
IME::list_cand_data_t::~list_cand_data_t() {}
CUIIME::~CUIIME() {}
IME::ActivationResult IME::CIME::Activate(GrafPort* window) {
    ev("IME.Activate", {lab(this), lab(window)});
    return {{static_cast<int32_t>(ret("Activate")), 0x123, -7, 42}};
}
void IME::CIME::UnActivate() { ev("IME.UnActivate", {lab(this)}); }
void IME::CIME::SetMaxCompositionLength(int32_t length) { ev("IME.MaxLength", {lab(this), length}); }
int32_t UI_SendMessage(InterfaceWindow* w, int32_t m, int32_t a, int32_t b) { ev("Send", {lab(w), m, a, b}); return 0; }
void UI_UnPauseTimerSystem() { ev("UnPauseTimers"); }
void HidePauseWindow() { ev("HidePause"); }

namespace {
InterfaceWindow plain(nullptr, 0, 0, 0, 0, 0, 0);
InterfaceButton button(nullptr, 0, 0, 0, 0, 0, 0);
InterfaceEditBox edit(nullptr, 0, 0, 0, 0, 0, 0);
char16_t text[65537];
InterfaceWindow* windows[] = {&plain, &button, &edit};
constexpr std::size_t windowSizes[] = {sizeof plain, sizeof button, sizeof edit};
unsigned char originalWindows[3][sizeof edit];
// The bytes of gUIIME from the IME through the unused bytes before the stored length, as the test filled them.
const unsigned char* ImeState() { return reinterpret_cast<const unsigned char*>(&gUIIME.mIME); }
std::size_t ImeStateSize() { return reinterpret_cast<const unsigned char*>(&gUIIME.mStoredLength) - ImeState(); }
unsigned char originalImeState[sizeof(CUIIME)];
char16_t originalText[65537];
InterfaceWindow* Win(int64_t index) { return index < 0 ? nullptr : windows[index]; }
}
extern "C" {
// Inputs: effects' raw words, static flags/values/limits, windows' coordinates and animation offsets, IME fixtures.
void tpw_peek_reset(const int64_t* a, const uint32_t* effects) {
    std::memcpy(gEffects, effects, sizeof gEffects);
    gHLBaseAreaSet = a[0];
    gHLAreaScaleSet = a[1];
    uint32_t bits = a[2];
    std::memcpy(&gHLBaseArea, &bits, 4);
    bits = a[3];
    std::memcpy(&gHLAreaScale, &bits, 4);
    gHLMinEmitters = a[4];
    gHLMaxEmitters = a[5];
    for (int i = 0; i < 3; ++i) {
        InterfaceWindow* w = windows[i];
        tpw_cl_label(10 + i, w);
        w->mAttributes = a[6];
        w->mId = 70 + i;
        w->mChild = i == 0 ? &edit : nullptr;
        w->mNext = nullptr;
        w->mRect.Set(a[7], a[8], a[9], a[10]);
        for (int j = 0; j < 4; ++j) w->mAnimOffset[j] = a[11 + j];
    }
    button.mState = a[15];
    button.mHilight = a[16];
    edit.mMaxLength = a[17];
    std::memset(text, 0, sizeof text);
    for (int i = 0; i < a[18]; ++i) text[i] = u'x';
    edit.mBuffer = a[19] ? text : nullptr;
    edit.mF161 = a[20];
    std::memset(static_cast<void*>(&gUIIME), 0xa5, sizeof gUIIME);
    gUIIME.mOwner = Win(a[21]);
    gUIIME.mEditBox = static_cast<InterfaceEditBox*>(Win(a[22]));
    gUIIME.mActive = a[23];
    gUIIME.mStoredLength = a[24];
    gUIIME.mStoredInvalid = a[25];
    gPlayerSettings.mCurrent.mTooltipsOn = a[26];
    gSoundBanks[3] = a[27];
    gMainWindow = a[28] ? reinterpret_cast<GrafPort*>(&plain) : nullptr;
    gUIRootWindow = Win(a[29]);
    tpw_cl_label(30, &gUIIME.mIME);
    std::memcpy(originalImeState, ImeState(), ImeStateSize());
    for (int i = 0; i < 3; ++i)
        std::memcpy(originalWindows[i], static_cast<const void*>(windows[i]), windowSizes[i]);
    std::memcpy(originalText, text, sizeof text);
}
int32_t tpw_peek_op(int32_t op, int32_t slot, int32_t message, int32_t a, int32_t b, const float* f) {
    switch (op) {
    case 0: return EffectPeekMsg(Win(slot), message, a, b);
    case 1: BTN_HighLight_On(f[0], f[1], f[2], f[3]); break;
    case 2: BTN_HighLight_Move(f[0], f[1]); break;
    case 3: BTN_HighLight_Kill(); break;
    case 4: BTN_HighLight_Off(); break;
    case 5: gUIIME.Activate(Win(slot)); break;
    case 6: gUIIME.DeActivate(); break;
    case 7: gUIIME.InvalidateStoredString(Win(slot)); break;
    case 8: UnPauseUI(); break;
    case 9: { UIInterface ui; ui.UnPause(); break; }
    }
    return 0;
}
int32_t tpw_peek_dump(int64_t* out) {
    int n = 0;
    uint32_t words[51];
    std::memcpy(words, gEffects, sizeof words);
    for (uint32_t word : words) out[n++] = word;
    out[n++] = static_cast<uint8_t>(gHLBaseAreaSet);
    out[n++] = static_cast<uint8_t>(gHLAreaScaleSet);
    out[n++] = fbits(gHLBaseArea);
    out[n++] = fbits(gHLAreaScale);
    out[n++] = gHLMinEmitters;
    out[n++] = gHLMaxEmitters;
    out[n++] = lab(gUIIME.mOwner);
    out[n++] = lab(gUIIME.mEditBox);
    out[n++] = gUIIME.mActive;
    out[n++] = gUIIME.mStoredLength;
    out[n++] = gUIIME.mStoredInvalid;
    // Include every opaque byte so an accidental wider store is visible.
    for (uint8_t byte : gUIIME.mF08) out[n++] = byte;
    for (uint8_t byte : gUIIME.mF0D) out[n++] = byte;
    // The Mac's 0x160 bytes at +0x1c: all still the fill pattern, or none (the host object is laid out differently).
    const bool imeUntouched = std::memcmp(originalImeState, ImeState(), ImeStateSize()) == 0;
    for (int i = 0; i < 0x160; ++i) out[n++] = imeUntouched ? 0xa5 : 0;
    for (InterfaceWindow* w : windows) {
        out[n++] = w->mAttributes;
        out[n++] = w->mRect.mLeft;
        out[n++] = w->mRect.mTop;
        out[n++] = w->mRect.mRight;
        out[n++] = w->mRect.mBottom;
        for (int16_t offset : w->mAnimOffset) out[n++] = offset;
    }
    out[n++] = button.mState;
    out[n++] = button.mHilight;
    out[n++] = edit.mMaxLength;
    out[n++] = edit.mF161;
    out[n++] = gPlayerSettings.mCurrent.mTooltipsOn;
    out[n++] = gSoundBanks[3];
    out[n++] = lab(gMainWindow);
    out[n++] = lab(gUIRootWindow);
    // Check all window bytes too, except the edit box's separately dumped lock byte.
    for (int i = 0; i < 3; ++i) {
        unsigned char current[sizeof edit];
        std::memcpy(current, static_cast<const void*>(windows[i]), windowSizes[i]);
        if (i == 2) {
            const std::size_t lockOffset = reinterpret_cast<unsigned char*>(&edit.mF161) - reinterpret_cast<unsigned char*>(&edit);
            current[lockOffset] = originalWindows[i][lockOffset];
        }
        out[n++] = std::memcmp(current, originalWindows[i], windowSizes[i]) == 0;
    }
    out[n++] = std::memcmp(text, originalText, sizeof text) == 0;
    return n;
}
}
