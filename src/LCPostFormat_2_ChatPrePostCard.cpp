// ui_onlineOutbox.cpp / ui_onlineParkFindPanel.cpp leftovers (unit v5579): the chat panel's postcard hand-over, the outbox
// file's static initialiser and the JPEG error handler.
#include <cstdint>
#include <new>
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "TbGraphicFile.h"
#include "TbDynamicStringTemplate.h"
#include "TbPixFmt.h"
#include "UI_Base.h"
#include "email.h"
#include "lobby.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "online.h"

extern "C" void __register_global_object(void* object, void* destructor, void* node);
void UIMain_DestroyWString(void* object);   // 0x103bcbb8, the wide string destructor glue (src/OnlineParkInfoPanelMsg.cpp)

// Globals no other file names (original names unknown).
InterfaceWindow* gOnlineWord80 = nullptr;       // 0x10395e80: a window the chat saved before a postcard (not 0 wins in ChatPrePostCard)
InterfaceWindow* gOnlineSavedWindow84 = nullptr; // 0x10395e84: the window ChatPrePostCard chose, put back by ChatReinstateAfterPostcard
alignas(16) uint8_t gOutboxStringStorage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395e40 (0x14 bytes on the Mac)
alignas(8) uint8_t gOutboxStringNode[24];       // 0x10395e54: the global object's registration node

// 0x101e568c
void ChatPrePostCard() {
    InterfaceWindow* saved = gOnlineWord80;
    if (saved != nullptr) {
        gOnlineSavedWindow84 = saved;
        return;
    }
    if (gOnlinePanelShown != 0) {
        gOnlineSavedWindow84 = gOnlinePanel;
        return;
    }
    gOnlineSavedWindow84 = gOnlineWindow;
}

// 0x101e5700
void ChatReinstateAfterPostcard() {
    InterfaceWindow* panel = gOnlinePanel;
    bool same = (panel == gOnlineSavedWindow84);
    UI_SendMessage(panel, 6, same ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, same ? 0 : 1, 0);
    gOnlineSavedWindow84 = nullptr;
}

// 0x101e55ec
namespace init_callee {
void LockChatPanelOff() {
    UI_SendMessage(gOnlinePanel, 6, 0, 0);
    UI_SendMessage(gOnlineWindow, 6, 1, 0);
    gOnlineBlockToggle = 1;
    gOnlineWindow->CaptureKb(0);
}
}  // namespace init_callee

// 0x101e551c
void __sinit_ui_onlineOutbox_cpp() {
    const char16_t* text = reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(gEmailSeparators) + 6);
    TbDynamicStringTemplate<char16_t>* string = new (gOutboxStringStorage) TbDynamicStringTemplate<char16_t>(text);
    __register_global_object(string, reinterpret_cast<void*>(&UIMain_DestroyWString), gOutboxStringNode);
    TbSize size{0x280, 0xf0};
    TbPixFormat format(0xff0000, 0xff00, 0xff, 0xff000000u, 0x20, 0);
    new (gOutboxPreview) TbRenderArea(size, format, nullptr, 0);
}

// 0x101e5310: the error handler of the JPEG loader: prints through the manager's output_message (+8), then jumps out (jump buffer at +0x84).
void my_error_exit(jpeg_common_struct* cinfo) {
    uint8_t* err = cinfo->err.get();
    outbox_callee::ErrorOutput(*reinterpret_cast<uint32_t*>(err + 8), cinfo);
    outbox_callee::JpegLongJmp(err + 0x84, 1);
}
