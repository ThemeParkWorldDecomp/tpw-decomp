// Test library for the chat postcard hand-over, the outbox static initialiser and the JPEG error exit (unit v5579;
// libtpw_unit_ChatPrePostCard.dylib, tests/test_ChatPrePostCard.py), main app:
//   src/game/ui/ui_onlineOutbox.cpp, src/game/ui/ui_onlinePanel.cpp   ChatPrePostCard, ChatReinstateAfterPostcard, LockChatPanelOff, __sinit_ui_onlineOutbox_cpp,
//   my_error_exit; it defines the four globals only these functions use.
//
// The callees outside the unit are fakes that record what the real ones read: UI_SendMessage, InterfaceWindow::CaptureKb, the string
// and render area constructors, __register_global_object, the pointer glue and the long jump. The globals of other files are defined here.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "calllog.h"
#include "platform/arena.h"
#include "FS_UserInfo.h"
#include "InterfaceWindow.h"
#include "TbGraphicFile.h"
#include "TbDynamicStringTemplate.h"
#include "UI_Base.h"
#include "lobby.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "online.h"

using namespace calllog;

// ---- globals of other files the unit reads or writes, defined here
InterfaceWindow* gOnlineWindow = nullptr;
InterfaceWindow* gOnlinePanel = nullptr;
uint8_t gOnlineBlockToggle = 0;
uint8_t gOnlinePanelShown = 0;
alignas(8) uint32_t gOutboxPreview[12] = {};
extern const char16_t gEmailSeparators[10] = {0, u'\n', 0};

namespace {
int64_t Raw(const void* p) { return static_cast<int64_t>(reinterpret_cast<uintptr_t>(p) & 0xffffffffu); }
}  // namespace

// ---- the fakes
int32_t UI_SendMessage(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    ev("UI_SendMessage", {Raw(window), static_cast<uint32_t>(message), static_cast<uint32_t>(a), static_cast<uint32_t>(b)});
    return 0;
}

void InterfaceWindow::CaptureKb(int32_t on) { ev("CaptureKb", {Raw(this), static_cast<uint32_t>(on)}); }

void UIMain_DestroyWString(void*) {}

// The fake constructor makes the vtable needed: its one out-of-line virtual that nothing else here defines.
template <>
void TbDynamicStringTemplate<char16_t>::AllocBuffer(uint32_t) {}

template <>
void TbDynamicStringTemplate<char16_t>::Format(const char16_t*, ...) {}
template <>
void TbDynamicStringTemplate<char16_t>::ResizeTo(uint32_t) {}
template <>
TbDynamicStringTemplate<char16_t>::~TbDynamicStringTemplate() {}
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const TbDynamicStringTemplate&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t*) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const TbStringBase<char16_t>&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t&) { return *this; }
template <>
TbDynamicStringTemplate<char16_t>& TbDynamicStringTemplate<char16_t>::operator+=(const char16_t*) { return *this; }

template <>
TbDynamicStringTemplate<char16_t>::TbDynamicStringTemplate(const char16_t* text) {
    ev("StringCtor", {lab(this), text == reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gEmailSeparators) + 6) ? 1 : 0});
}

TbRenderArea::TbRenderArea(const TbSize& size, const TbPixFormat& format, uint8_t* data, uint32_t stride) {
    ev("RenderArea", {lab(this), static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height), format.mRedMask, format.mGreenMask,
                      format.mBlueMask, format.mAlphaMask, static_cast<uint32_t>(format.mBitsPerPixel),
                      static_cast<uint32_t>(format.mPalettised), Raw(data), stride});
}

extern "C" void __register_global_object(void* object, void* destructor, void* node) {
    ev("RegisterGlobal", {lab(object), destructor == reinterpret_cast<void*>(&UIMain_DestroyWString) ? 1 : 0, lab(node)});
}

namespace outbox_callee {
void ErrorOutput(uint32_t tvector, jpeg_common_struct* cinfo) { ev("ErrorOutput", {tvector, lab(cinfo)}); }
void JpegLongJmp(void* buffer, int32_t value) { ev("LongJmp", {lab(buffer), static_cast<uint32_t>(value)}); }
}  // namespace outbox_callee

extern "C" {

uint64_t tpw_cpc_addr(const char* name) {
    struct Entry {
        const char* name;
        const void* address;
    };
    const Entry entries[] = {
        {"w80", &gOnlineWord80},
        {"w84", &gOnlineSavedWindow84},
        {"window", &gOnlineWindow},
        {"panel", &gOnlinePanel},
        {"toggle", &gOnlineBlockToggle},
        {"shown", &gOnlinePanelShown},
        {"string", gOutboxStringStorage},
        {"node", gOutboxStringNode},
        {"preview", gOutboxPreview},
    };
    for (const Entry& entry : entries)
        if (std::strcmp(entry.name, name) == 0) return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(entry.address));
    return 0;
}

uint64_t tpw_cpc_alloc(uint32_t size) { return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(tpw::ArenaAllocZeroed(size))); }

// The arena offset a Ptr32 field holds for a block the module allocated.
uint32_t tpw_cpc_offset(uint64_t address) { return tpw::ArenaOffset(reinterpret_cast<void*>(static_cast<uintptr_t>(address))); }

int32_t tpw_cpc_layout(int64_t* out) {
    int32_t n = 0;
    out[n++] = static_cast<int64_t>(sizeof(jpeg_common_struct));
    out[n++] = static_cast<int64_t>(offsetof(jpeg_common_struct, err));
    return n;
}

void tpw_cpc_pre() { ChatPrePostCard(); }
void tpw_cpc_reinstate() { ChatReinstateAfterPostcard(); }
void tpw_cpc_lock() { init_callee::LockChatPanelOff(); }
void tpw_cpc_sinit() { __sinit_ui_onlineOutbox_cpp(); }
void tpw_cpc_exit(uint64_t cinfo) { my_error_exit(reinterpret_cast<jpeg_common_struct*>(static_cast<uintptr_t>(cinfo))); }

}  // extern "C"
