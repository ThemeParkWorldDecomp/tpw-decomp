// Test library for unit v5981 (libtpw_unit_InterfaceWindow_link.dylib, tests/test_InterfaceWindow_link.py), main app:
//   src/InterfaceWindow_draw_InterfaceWindow_link.cpp   InterfaceWindow::link, __sinit_ui_islandLobbyController_cpp,
//   LobbyOnlineLoginPanelDlgMsg, AdressesTextPanelMsg, read_JPEG_file
//
// Built with the port fix (unit v5L48), read_JPEG_file opens tpw::GamePathForReading of its path, a recording fake here.
//
// The callees outside the unit are fakes that record the arguments the real ones read and answer from the test's queues. The windows
// are raw blocks of a pool (no constructor runs); the first word of each is a vtable whose slots are all fakes, so a virtual call
// is recorded with the Mac's vtable offset (SetText is slot 10 of the host's table and offset 0x2c of the Mac's).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "calllog.h"
#include "platform/arena.h"
#include "InterfaceWindow.h"
#include "lobby.h"
#include "FS_Online.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "AdvisorScoring.h"
#include "TbDynamicStringTemplate.h"
#include "std_basic_ifstream.h"
#include "platform/macdoze.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes
InterfaceWindow* gUIKeyboardWindow;
char16_t gPostcardText5da5[0x200];
uint8_t gIfsWFiles[0xfc];

namespace {

alignas(16) uint8_t gWinPool[6][sizeof(InterfaceWindow)];
void* gVtable[40];

template <int N>
int32_t Tramp(void* self, void* arg) {
    ev("virtual", {lab(self), N == 10 ? 0x2c : 0x1000 + N, lab(arg)});
    return 0;
}

template <int... I>
void FillVt(std::integer_sequence<int, I...>) {
    ((gVtable[2 + I] = reinterpret_cast<void*>(&Tramp<I>)), ...);
}

void InitVt() {
    static bool done = false;
    if (done) return;
    done = true;
    FillVt(std::make_integer_sequence<int, 30>());
    for (auto& w : gWinPool) *reinterpret_cast<void***>(w) = &gVtable[2];
}

// the jpeg fakes' state: the pixel buffer of the case
uintptr_t gPixBase = 0;

}  // namespace

// ---- the fakes
void UIMain_DestroyWString(void*) {}
extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("register", {lab(object), lab(destructor), lab(node)});
}
template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    int64_t length = 0, hash = 0;
    for (; length < 64 && text[length] != 0; ++length) hash += static_cast<int64_t>(text[length]) * (length + 1);
    ev("TbDyn", {lab(this), length, hash, text[0], text[1], text[2], text[3]});
}
// the class's other virtuals (its vtable is emitted with the constructor): never called here
using WStr = TbDynamicStringTemplate<char16_t>;
template <> void WStr::AllocBuffer(uint32_t) {}
template <> void WStr::ResizeTo(uint32_t) {}
template <> void WStr::Format(const char16_t*, ...) {}
template <> WStr::~TbDynamicStringTemplate() {}
template <> WStr& WStr::operator=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator=(const WStr&) { return *this; }
template <> WStr& WStr::operator=(const char16_t*) { return *this; }
template <> WStr& WStr::operator=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t&) { return *this; }
template <> WStr& WStr::operator+=(const char16_t*) { return *this; }
void InterfaceWindow::setZ(int16_t z) { ev("setZ", {lab(this), z}); }
void InterfaceWindow::setPivot(int16_t x, int16_t y) { ev("setPivot", {lab(this), x, y}); }
int32_t UI_UpdatePointerIn() {
    ev("upi", {});
    return 0;
}
int32_t UI_PostMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("post", {lab(window), message, a, b});
    return 0;
}
int32_t OnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("online", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("online"));
}
int32_t DefaultDialogCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("dflt", {lab(window), message, a, b});
    return static_cast<int32_t>(ret("dflt"));
}
void InterfaceWindow::SetTextOffset(int32_t x, int32_t y) { ev("SetTextOffset", {lab(this), x, y}); }
int32_t InterfaceWindow::CallDefaultWindowHandler(int32_t message, int32_t a, int32_t b) {
    ev("cdwh", {lab(this), message, a, b});
    return static_cast<int32_t>(ret("cdwh"));
}

void my_error_exit(jpeg_common_struct*) {}

namespace outbox_callee {
int32_t JpegSetJmp(void* buffer) {
    uint8_t* b = static_cast<uint8_t*>(buffer);
    uint32_t err, exitWord;
    std::memcpy(&err, b + 0x11c, 4);
    std::memcpy(&exitWord, b - 0x84, 4);
    ev("setjmp", {err, exitWord == static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_error_exit)) ? 1 : 0});
    return static_cast<int32_t>(ret("setjmp"));
}
}  // namespace outbox_callee

// The host file of a game path, faked: "<host>/" and the path with its ':' made '/' (test_InterfaceWindow_link.host_path).
std::string tpw::GamePathForReading(const char* gamePath) {
    ev("GamePathForReading", std::initializer_list<int64_t>{}, {gamePath});
    std::string host = std::string("<host>/") + gamePath;
    for (char& c : host)
        if (c == ':') c = '/';
    return host;
}

extern "C" FILE* fopen(const char* path, const char* mode) {
    ev("fopen", {}, {path, mode});
    return reinterpret_cast<FILE*>(static_cast<uintptr_t>(ret("fopen")));
}
extern "C" int fclose(FILE* file) {
    ev("fclose", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))});
    return 0;
}
extern "C" int fprintf(FILE* file, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const char* path = va_arg(args, const char*);
    va_end(args);
    ev("fprintf_file", {reinterpret_cast<uint8_t*>(file) == gIfsWFiles + 0xa8 ? 1 : 0});
    ev("fprintf", {}, {format, path});
    return 0;
}
jpeg_error_mgr* jpeg_std_error(jpeg_error_mgr*) {
    ev("std_error", {});
    return reinterpret_cast<jpeg_error_mgr*>(static_cast<uintptr_t>(ret("std_error")));
}
void jpeg_destroy_decompress(jpeg_decompress_struct*) { ev("destroy", {}); }
void jpeg_CreateDecompress(jpeg_decompress_struct* cinfo, int32_t version, uint32_t size) {
    ev("create", {cinfo->err, version, size});
}
void jpeg_stdio_src(jpeg_decompress_struct*, void* file) { ev("stdio_src", {static_cast<int64_t>(reinterpret_cast<uintptr_t>(file))}); }
int32_t jpeg_read_header(jpeg_decompress_struct*, int32_t requireImage) {
    ev("read_header", {requireImage});
    return 1;
}
int32_t jpeg_start_decompress(jpeg_decompress_struct* cinfo) {
    ev("start", {});
    cinfo->output_width = static_cast<uint32_t>(ret("jw"));
    cinfo->output_height = static_cast<uint32_t>(ret("jh"));
    uint32_t components = static_cast<uint32_t>(ret("jc"));
    std::memcpy(reinterpret_cast<uint8_t*>(cinfo) + 0x7c, &components, 4);
    cinfo->output_scanline = static_cast<uint32_t>(ret("js"));
    return 1;
}
uint32_t jpeg_read_scanlines(jpeg_decompress_struct* cinfo, uint8_t** rows, uint32_t max) {
    uintptr_t row = reinterpret_cast<uintptr_t>(rows[0]);
    ev("scanlines", {static_cast<int64_t>(static_cast<uint32_t>(row - gPixBase)), max, cinfo->output_scanline});
    cinfo->output_scanline += 1;
    return 1;
}
int32_t jpeg_finish_decompress(jpeg_decompress_struct*) {
    ev("finish", {});
    return 1;
}

extern "C" {

// The host address of a global by the module's name for it (ADDR's keys).
uint64_t tpw_ilk_addr(const char* name) {
    InitVt();
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w1", gWinPool[1]}, {"w2", gWinPool[2]}, {"w3", gWinPool[3]}, {"w4", gWinPool[4]}, {"w5", gWinPool[5]},
        {"kbd", &gUIKeyboardWindow}, {"text", gPostcardText5da5}, {"files", gIfsWFiles + 0xa8},
        {"str1", gIslandLobbyText1Storage}, {"node1", gIslandLobbyText1Node},
        {"str2", gIslandLobbyText2Storage}, {"node2", gIslandLobbyText2Node},
        {"dtor", reinterpret_cast<const void*>(&UIMain_DestroyWString)},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

// A zeroed block (32-bit addressable).
uint64_t tpw_ilk_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The offsets and sizes the module relies on: the host window's pointer and short fields and its size.
int32_t tpw_ilk_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mNext));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mPrevious));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mChild));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mParent));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mZ));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mPivotX));
    out[n++] = static_cast<int64_t>(offsetof(InterfaceWindow, mPivotY));
    out[n++] = static_cast<int64_t>(sizeof(InterfaceWindow));
    return n;
}

// ---- the functions
int32_t tpw_ilk_link(uint64_t self, uint64_t child, int32_t a, int32_t b) {
    return reinterpret_cast<InterfaceWindow*>(self)->linkChecked(reinterpret_cast<InterfaceWindow*>(child), static_cast<char>(a),
                                                                 static_cast<char>(b));
}
void tpw_ilk_sinit() { __sinit_ui_islandLobbyController_cpp(); }
int32_t tpw_ilk_login(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return LobbyOnlineLoginPanelDlgMsg(reinterpret_cast<InterfaceWindow*>(window), m, a, b);
}
int32_t tpw_ilk_adr(uint64_t window, int32_t m, int32_t a, int32_t b) {
    return AdressesTextPanelMsg(reinterpret_cast<InterfaceWindow*>(window), m, a, b);
}
// 1 when the library is built with the port fix (read_JPEG_file opens the host file of its path), else 0.
int32_t tpw_ilk_portfix() {
    return 0;
}
int32_t tpw_ilk_jpeg(uint64_t path, uint64_t pixels) {
    gPixBase = static_cast<uintptr_t>(pixels);
    return read_JPEG_fileChecked(reinterpret_cast<char*>(path), reinterpret_cast<uint8_t*>(pixels));
}

}  // extern "C"
