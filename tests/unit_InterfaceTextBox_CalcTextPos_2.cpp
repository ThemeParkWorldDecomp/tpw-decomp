// Test library for the interface text box, the string heap and the timer's operator delete (unit v5975;
// libtpw_unit_InterfaceTextBox_CalcTextPos_2.dylib, tests/test_InterfaceTextBox_CalcTextPos_2.py), main app:
//   src/game/ui/InterfaceStringHeap.cpp, src/game/ui/InterfaceTextBox.cpp   CalcTextPos, SetText, GetString, GetAndCopy, OperatorDelete
//
// Fakes: the font's GetWidth / GetHeight (record and answer from the queues), operator new[] / operator delete (only while a wrapper
// has the hook on: new[] answers the test's block), msl_assertion_failed (records). wcslen and wcscpy are plain here and in the
// test's PowerPC stubs (no events). The globals of lobby_start_leaves.cpp and UI_timers are defined here, zero at start.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "calllog.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "InterfaceFont.h"
#include "UI_Base.h"
#include "ui_controls.h"

using namespace calllog;

uint8_t* gStringHeapTableB;
uint8_t* gStringHeapTableA;
int32_t gStringHeapRowsB;
int32_t gStringHeapWidthB;
int32_t gStringHeapRowsA;
int32_t gStringHeapWidthA;
int32_t gUITimerCount;
InterfaceTimer* gUITimers;

namespace {
void* gNewBlock = nullptr;
}  // namespace

// ---- the runtime's operator new[] and delete as SetText calls them
void* textbox_callee::NewArray(uint32_t size) {
    ev("newarr", {static_cast<int64_t>(size)});
    return gNewBlock;
}
void textbox_callee::Delete(void* p) { ev("delete", {lab(p)}); }

uint32_t wcslen(const char16_t* text) {
    uint32_t n = 0;
    while (text[n] != 0) ++n;
    return n;
}
char16_t* wcscpy(char16_t* dst, const char16_t* src) {
    char16_t* d = dst;
    while ((*d++ = *src++) != 0) {}
    return dst;
}

void msl_assertion_failed(const char* condition, const char* file, int32_t line) {
    ev("assert", {line}, {condition, file});
}

int32_t InterfaceFont::GetWidth(const char16_t* text, int16_t first, int16_t last) {
    ev("FontWidth", {lab(this), lab(const_cast<char16_t*>(text)), first, last});
    return static_cast<int32_t>(ret("FontWidth"));
}
int32_t InterfaceFont::GetHeight() {
    ev("FontHeight", {lab(this)});
    return static_cast<int32_t>(ret("FontHeight"));
}

extern "C" {

uint64_t tpw_v5975_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"TableB", &gStringHeapTableB}, {"TableA", &gStringHeapTableA}, {"RowsB", &gStringHeapRowsB},
        {"WidthB", &gStringHeapWidthB}, {"RowsA", &gStringHeapRowsA},   {"WidthA", &gStringHeapWidthA},
        {"TimerCount", &gUITimerCount}, {"Timers", &gUITimers},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_v5975_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// font, text, horizontal, vertical, length, textX, textY, clientLeft, clientTop, clientRight, clientBottom, sizeof(box),
// timer mFree, sizeof(timer)
int32_t tpw_v5975_layout(int64_t* out) {
    alignas(16) static unsigned char box[sizeof(InterfaceTextBox) + 16];
    alignas(16) static unsigned char tim[sizeof(InterfaceTimer) + 16];
    std::memset(box, 0, sizeof box);
    InterfaceTextBox* o = reinterpret_cast<InterfaceTextBox*>(box);
    InterfaceTimer* t = reinterpret_cast<InterfaceTimer*>(tim);
    auto off = [](const void* base, const void* member) {
        return static_cast<int64_t>(static_cast<const char*>(member) - static_cast<const char*>(base));
    };
    int32_t n = 0;
    out[n++] = off(o, &o->mFont);
    out[n++] = off(o, &o->mText);
    out[n++] = off(o, &o->mHorizontal);
    out[n++] = off(o, &o->mVertical);
    out[n++] = off(o, &o->mTextLength);
    out[n++] = off(o, &o->mTextX);
    out[n++] = off(o, &o->mTextY);
    out[n++] = off(o, &o->mClientRect.mLeft);
    out[n++] = off(o, &o->mClientRect.mTop);
    out[n++] = off(o, &o->mClientRect.mRight);
    out[n++] = off(o, &o->mClientRect.mBottom);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceTextBox));
    out[n++] = off(t, &t->mFree);
    out[n++] = static_cast<int64_t>(sizeof(InterfaceTimer));
    return n;
}

void tpw_v5975_calc(uint64_t box) { reinterpret_cast<InterfaceTextBox*>(box)->CalcTextPos(); }

int32_t tpw_v5975_settext(uint64_t box, uint64_t text, uint64_t newblock) {
    gNewBlock = reinterpret_cast<void*>(newblock);
    return reinterpret_cast<InterfaceTextBox*>(box)->InterfaceTextBox::SetText(reinterpret_cast<char16_t*>(text));
}

uint64_t tpw_v5975_getstring(int32_t length) { return reinterpret_cast<uintptr_t>(InterfaceStringHeap::GetString(length)); }
uint64_t tpw_v5975_getandcopy(uint64_t text) {
    return reinterpret_cast<uintptr_t>(InterfaceStringHeap::GetAndCopy(reinterpret_cast<const char16_t*>(text)));
}
void tpw_v5975_timerdelete(uint64_t slot) { InterfaceTimer::OperatorDelete(reinterpret_cast<void*>(slot)); }

}  // extern "C"
