// Test library for LLCC_DrawTrackLine and __sinit_lobby_cpp (unit v5946; libtpw_unit_LLCC_DrawTrackLine.dylib,
// tests/test_LLCC_DrawTrackLine.py), main app:
//   src/game/online/lobby.cpp, src/game/park/TrackCell.cpp
//
// Callees outside the unit (CMap::IsXYValid, the track cell's Modify body, the string constructors, __register_global_object) are
// recording fakes. The globals of other files the unit touches are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "CellBase.h"
#include "TrackCell.h"
#include "TbDynamicStringTemplate.h"
#include "ui_publishParkPanel.h"
#include "World.h"

using namespace calllog;

int32_t gTrackNbrOn485C;
int32_t gLLCC4858;
CWorld* gGameWorld;
// gOnlineUploadFile (0x102e4868) holds the string the sinit builds in it; this definition is padded further under the same symbol.
struct PaddedUpload {
    OnlineUploadFile file;
    uint8_t pad[240];
};
PaddedUpload gPaddedUpload __asm__("_gOnlineUploadFile");

int32_t LLCC_DrawTrackLine(CELL_MODES mode, int32_t x, int32_t y, int32_t* endX, int32_t* endY);
void __sinit_lobby_cpp();
extern uint8_t gLobbyWString48AC[64];
extern uint8_t gLobbyWString488C[64];
extern uint8_t gLobbyNode48C0[16];
extern uint8_t gLobbyNode48A0[16];
extern uint8_t gLobbyNode487C[16];

void UIMain_DestroyWString(void*) {}
void UIMain_DestroyString(void*) {}

namespace {
template <class C>
uint32_t Hash(const C* text, int64_t* n) {
    uint32_t h = 0;
    *n = 0;
    for (; text[*n] != 0; ++*n) h = h * 31u + static_cast<uint32_t>(static_cast<uint16_t>(text[*n]));
    return h;
}
}  // namespace

int32_t CMap::IsXYValid(int32_t x, int32_t y) {
    ev("IsXYValid", {x, y});
    return static_cast<int32_t>(ret("IsXYValid"));
}

int32_t CTrackCell::ModifyBody(CELL_MODES mode, int32_t a, int32_t b) {
    const int64_t off = reinterpret_cast<const char*>(this) - reinterpret_cast<const char*>(gGameWorld);
    ev("Modify", {static_cast<int32_t>(off), static_cast<int32_t>(mode), a, b});
    return static_cast<int32_t>(ret("Modify"));
}

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    int64_t n;
    const uint32_t h = Hash(text, &n);
    ev("ctorW", {lab(this), n, static_cast<int64_t>(h)});
}

template <>
TbDynamicStringTemplate<char>::TbDynamicStringTemplate(const char* text) {
    int64_t n;
    const uint32_t h = Hash(text, &n);
    ev("ctorC", {lab(this), n, static_cast<int64_t>(h)});
}

// The vtable key functions (the constructors store the vtable); never called.
#define TPW_FAKE_VT(CH)                                                                                              \
    template <>                                                                                                      \
    void TbDynamicStringTemplate<CH>::AllocBuffer(uint32_t) {}                                                       \
    template <>                                                                                                      \
    void TbDynamicStringTemplate<CH>::Format(const CH*, ...) {}                                                      \
    template <>                                                                                                      \
    void TbDynamicStringTemplate<CH>::ResizeTo(uint32_t) {}                                                          \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>::~TbDynamicStringTemplate() {}                                                       \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbStringBase<CH>&) { return *this; }   \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const TbDynamicStringTemplate&) { return *this; } \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH*) { return *this; }                 \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator=(const CH&) { return *this; }                 \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const TbStringBase<CH>&) { return *this; }  \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH&) { return *this; }                \
    template <>                                                                                                      \
    TbDynamicStringTemplate<CH>& TbDynamicStringTemplate<CH>::operator+=(const CH*) { return *this; }
TPW_FAKE_VT(char16_t)
TPW_FAKE_VT(char)

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}

extern "C" {

uint64_t tpw_dtl_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"world", &gGameWorld},
        {"g485C", &gTrackNbrOn485C},
        {"g4858", &gLLCC4858},
        {"lw1", gLobbyWString48AC},
        {"lw2", gLobbyWString488C},
        {"up", &gPaddedUpload},
        {"n1", gLobbyNode48C0},
        {"n2", gLobbyNode48A0},
        {"n3", gLobbyNode487C},
        {"dtw", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
        {"dts", reinterpret_cast<const void*>(&UIMain_DestroyString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_dtl_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

int32_t tpw_dtl_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(CWorld, mTrackCells));
    out[n++] = static_cast<int64_t>(sizeof(CTrackCell));
    return n;
}

uint32_t tpw_dtl_draw(int32_t mode, int32_t x, int32_t y, uint64_t endX, uint64_t endY) {
    return static_cast<uint32_t>(LLCC_DrawTrackLine(static_cast<CELL_MODES>(mode), x, y, reinterpret_cast<int32_t*>(endX),
                                                    reinterpret_cast<int32_t*>(endY)));
}

void tpw_dtl_sinit() { __sinit_lobby_cpp(); }

}  // extern "C"
