// Test library for the version tag and the "delme" dialog handler (unit v5289; libtpw_unit_ShowVersionNumberTag.dylib, tests/test_ShowVersionNumberTag.py), main app:
//   src/game/ui/ui_StatusPanel.cpp, src/game/ui/ui_tags.cpp   ShowVersionNumberTag, delme, gVersionNumberFormat
//
// The callees outside the unit are recording fakes: swprintf, AddATag, ShowTagPanel, UI_SetSelectedObject, LLCM_SetCurrentObject,
// LLCC_SystemSetMode, LLCM_LeftMouseButtonUp.
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "LLCellSystem.h"
#include "online.h"
#include "platform/msl.h"
#include "ui_main.h"

using namespace calllog;

// ---- the fakes
int swprintf(char16_t* buffer, unsigned int size, const char16_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    int32_t a = va_arg(ap, int32_t);
    int32_t b = va_arg(ap, int32_t);
    va_end(ap);
    ev("swprintf", {static_cast<int64_t>(size), a, b, format[0], format[1], format[2], format[3], format[4], format[5]});
    buffer[0] = static_cast<char16_t>(ret("sw0"));
    buffer[1] = static_cast<char16_t>(ret("sw1"));
    buffer[2] = 0;
    return 0;
}

InterfaceWindow* AddATag(CTagParams& p) {
    ev("AddATag", {static_cast<int64_t>(p.mId), p.mF4, p.mTime, p.mFrame, p.mF14, p.mF18, p.mF1C, p.mF20, p.mShow,
                   p.mText ? p.mText[0] : -1, p.mText ? p.mText[1] : -1, p.mText != nullptr});
    return reinterpret_cast<InterfaceWindow*>(static_cast<uintptr_t>(ret("AddATag")));
}

void ShowTagPanel(InterfaceWindow* window) { ev("ShowTagPanel", {lab(window)}); }

void UI_SetSelectedObject(void* object) { ev("UI_SetSelectedObject", {lab(object)}); }

void LLCM_SetCurrentObject(bool flag) { ev("LLCM_SetCurrentObject", {flag}); }

void LLCC_SystemSetMode(int32_t mode, bool flag) { ev("LLCC_SystemSetMode", {mode, flag}); }

void LLCM_LeftMouseButtonUp(int32_t x, int32_t y, uint16_t shift, int32_t flag) { ev("LLCM_LeftMouseButtonUp", {x, y, shift, flag}); }

extern "C" {

uint64_t tpw_vnt_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"fmt", &gVersionNumberFormat},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_vnt_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_vnt_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(gVersionNumberFormat));
    return n;
}

void tpw_vnt_show() { ShowVersionNumberTag(); }

int32_t tpw_vnt_delme(uint64_t thing) { return delme(reinterpret_cast<void*>(static_cast<uintptr_t>(thing))); }

}  // extern "C"
