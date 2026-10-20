// Test library for the options panel's DoScroll and StartScroll (unit v5895; libtpw_unit_DoScroll_3.dylib, tests/test_DoScroll_3.py), main app:
//   src/ui_mainpanel2_window_bumhole_DoScroll_3.cpp
// The callees are recording fakes; the text functions really copy and append, so the buffer is compared.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "InterfaceWindow.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "ui_main.h"
#include "ui_mainmenu.h"

using namespace calllog;

CPlayerSettings gPlayerSettings;
char16_t gUIMainConvertBuffer[0x4000];
InterfaceWindow* gSetOnOffRoot;
int32_t gOptionsPanelF0C;
char16_t gOptionsPanelNumberFormats[16];

void CPlayerSettings::Init() {}
CPlayerSettings::~CPlayerSettings() {}

namespace {
InterfaceWindow* sWin;
char16_t sStrings[64][4];
int sStringNext;
int64_t I(int64_t v) { return v; }
}  // namespace

void CPlayerSettings::SetResolution(eRESOLUTION r) { ev("SetResolution", {lab(this), I(static_cast<int32_t>(r))}); }
void CPlayerSettings::SetGraphicsDetail(eDETAIL d) { ev("SetGraphicsDetail", {lab(this), I(static_cast<int32_t>(d))}); }
void CPlayerSettings::SetSoundQuality(int32_t q) { ev("SetSoundQuality", {lab(this), I(q)}); }

void SOUND_SetSfxVolume(uint32_t v) { ev("SetSfx", {I(v)}); }
void SOUND_SetMusicVolume(uint32_t v) { ev("SetMusic", {I(v)}); }
void SOUND_SetSpeechVolume(uint32_t v) { ev("SetSpeech", {I(v)}); }

InterfaceWindow* InterfaceWindow::GetWindow(int32_t id) {
    ev("GetWindow", {lab(this), I(id)});
    return sWin;
}

const char16_t* UIMain_GetString(int32_t id) {
    ev("GetString", {I(id)});
    gUIMainConvertBuffer[0] = static_cast<char16_t>(0x1000 + id);
    gUIMainConvertBuffer[1] = 0x58;
    gUIMainConvertBuffer[2] = 0;
    char16_t* r = sStrings[sStringNext++ & 63];
    r[0] = static_cast<char16_t>(0x1000 + id);
    r[1] = 0x58;
    r[2] = 0;
    return r;
}

namespace options_callee {
void WindowSetText(InterfaceWindow* w, char16_t* text) {
    int64_t c[6] = {0, 0, 0, 0, 0, 0};
    for (int i = 0; i < 6 && text[i]; ++i) c[i] = text[i];
    ev("SetText", {lab(w), c[0], c[1], c[2], c[3], c[4], c[5]});
}
void WindowSetValue(InterfaceWindow* w, int32_t v) { ev("SetValue", {lab(w), I(v)}); }
}  // namespace options_callee

char16_t* wcsncpy(char16_t* dst, const char16_t* src, uint32_t n) {
    int64_t c[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4 && src[i]; ++i) c[i] = src[i];
    ev("wcsncpy", {lab(dst), I(n), c[0], c[1], c[2], c[3]});
    uint32_t i = 0;
    for (; i < n && src[i]; ++i) dst[i] = src[i];
    for (; i < n; ++i) dst[i] = 0;
    return dst;
}

char16_t* wcscat(char16_t* dst, const char16_t* src) {
    int64_t d[4] = {0, 0, 0, 0}, c[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4 && dst[i]; ++i) d[i] = dst[i];
    for (int i = 0; i < 4 && src[i]; ++i) c[i] = src[i];
    ev("wcscat", {lab(dst), d[0], d[1], d[2], d[3], c[0], c[1], c[2], c[3]});
    char16_t* p = dst;
    while (*p) ++p;
    while ((*p++ = *src++)) {
    }
    return dst;
}

int32_t swprintf(char16_t* buffer, uint32_t size, const char16_t* format, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, format);
    int64_t off = format - gOptionsPanelNumberFormats;
    int64_t arg;
    if (off == 0) {
        const char16_t* s = __builtin_va_arg(ap, const char16_t*);
        arg = s[0];
    } else {
        arg = __builtin_va_arg(ap, int32_t);
    }
    __builtin_va_end(ap);
    ev("swprintf", {lab(buffer), I(size), off, arg});
    buffer[0] = 0x70;
    buffer[1] = static_cast<char16_t>(0x30 + off);
    buffer[2] = static_cast<char16_t>(arg);
    buffer[3] = 0;
    return 3;
}

extern "C" {

uint64_t tpw_ds_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"settings", &gPlayerSettings},
        {"buf", gUIMainConvertBuffer},
        {"root", &gSetOnOffRoot},
        {"f0c", &gOptionsPanelF0C},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_ds_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

void tpw_ds_setwin(uint64_t win) { sWin = reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(win)); }

int32_t tpw_ds_layout(int64_t* out) {
    int32_t n = 0;
    using S = CPlayerSettings::Settings;
    out[n++] = static_cast<int64_t>(sizeof(S));
    out[n++] = static_cast<int64_t>(offsetof(S, mResolution));
    out[n++] = static_cast<int64_t>(offsetof(S, mDetail));
    out[n++] = static_cast<int64_t>(offsetof(S, mSfx));
    out[n++] = static_cast<int64_t>(offsetof(S, mMusic));
    out[n++] = static_cast<int64_t>(offsetof(S, mSpeech));
    out[n++] = static_cast<int64_t>(offsetof(S, mMovie));
    out[n++] = static_cast<int64_t>(offsetof(S, mSoundQuality));
    out[n++] = static_cast<int64_t>(offsetof(CPlayerSettings::Volume, mVolume));
    out[n++] = static_cast<int64_t>(sizeof(CPlayerSettings::Volume));
    return n;
}

void tpw_ds_doscroll(int32_t id, int32_t value) { options_panel::DoScroll(id, value); }
void tpw_ds_startscroll(int32_t id, int32_t value) { options_panel::StartScroll(id, value); }

}  // extern "C"
