// Unit v5981: InterfaceWindow::link (0x101ddf20), the island lobby controller's static initialiser (0x101e1e3c), the online login panel's
// message function (0x101e26fc), the outbox address panel's message function (0x101e4b00) and the JPEG loader (0x101e538c).
#include "InterfaceWindow.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include "ui_DrawObject.h"
#include "lobby.h"
#include "FS_Online.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "AdvisorScoring.h"
#include "TbDynamicStringTemplate.h"
#include "std_basic_ifstream.h"

// 0x101ddf20: makes `child` the LAST child of this window. Nothing happens (0) when `child` is NULL or has a parent or siblings already.
// The flags a and b (the Mac's chars, tested as signed bytes) give the child the parent's depth plus 2 and the parent's pivot. Then the
// pointer is updated, and a window that held the keyboard, when it is this one, loses it (message 0x20) and the pointer is updated again.
void InterfaceWindow::link(InterfaceWindow* child, char a, char b) { linkChecked(child, a, b); }
int32_t InterfaceWindow::linkChecked(InterfaceWindow* child, char a, char b) {
    if (child == nullptr || child->mParent != nullptr || child->mNext != nullptr || child->mPrevious != nullptr) return 0;
    child->mParent = this;
    InterfaceWindow* last = nullptr;
    if (mChild == nullptr) {
        mChild = child;
    } else {
        InterfaceWindow* walk = mChild;
        do {
            last = walk;
            walk = walk->mNext;
        } while (walk != nullptr);
        last->mNext = child;
    }
    child->mPrevious = last;
    if (a != 0) child->setZ(static_cast<int16_t>(mZ + 2));
    if (b != 0) child->setPivot(mPivotX, mPivotY);
    UI_UpdatePointerIn();
    if (gUIKeyboardWindow == this) {
        UI_PostMessage(gUIKeyboardWindow, 0x20, 0, 0);
        gUIKeyboardWindow = nullptr;
        UI_UpdatePointerIn();
    }
    return 1;
}

// The two wide texts of the string area at 0x1024ee1e (+0x22 and +0x30).
static const char16_t kIslandLobbyText1[] = u"upload";
static const char16_t kIslandLobbyText2[] = u"gms.dat";

alignas(16) uint8_t gIslandLobbyText1Storage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395ce0 (0x14 bytes on the Mac)
uint8_t gIslandLobbyText1Node[24];                                                         // 0x10395cf4
alignas(16) uint8_t gIslandLobbyText2Storage[sizeof(TbDynamicStringTemplate<char16_t>)];   // 0x10395cc0
uint8_t gIslandLobbyText2Node[24];                                                         // 0x10395cd4

// 0x101e1e3c: makes the two global wide strings and registers their destructors.
void __sinit_ui_islandLobbyController_cpp() {
    TbDynamicStringTemplate<char16_t>* first = new (gIslandLobbyText1Storage) TbDynamicStringTemplate<char16_t>(kIslandLobbyText1);
    __register_global_object(first, reinterpret_cast<void*>(&UIMain_DestroyWString), gIslandLobbyText1Node);
    TbDynamicStringTemplate<char16_t>* second = new (gIslandLobbyText2Storage) TbDynamicStringTemplate<char16_t>(kIslandLobbyText2);
    __register_global_object(second, reinterpret_cast<void*>(&UIMain_DestroyWString), gIslandLobbyText2Node);
}

// 0x101e26fc: the online login panel's handler first; the default dialog handler when it did not take the message.
int32_t LobbyOnlineLoginPanelDlgMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (OnlineLoginPanelDlgMsg(window, message, a, b) != 0) return 0;
    return DefaultDialogCallback(window, message, a, b);
}

// 0x101e4b00: message 0x800 (scroll) moves the text up by b and has the window's virtual at vtable offset 0x2c (SetText) show the
// shared postcard text again; every message then goes to the default handler.
int32_t AdressesTextPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x800) {
        window->SetTextOffset(0, 0 - b);
        window->SetText(gPostcardText5da5);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

namespace outbox_callee {
// Stand-in for the import (setjmp): the launcher's build has no way to call it, so it answers "first return".
__attribute__((weak)) int32_t JpegSetJmp(void*) { return 0; }
}  // namespace outbox_callee

// 0x101e538c: decodes the JPEG file `path` into `pixels`, a row of output_width * output_components bytes per scanline. The Mac keeps
// the error manager (0x84 bytes), the jump buffer (0x11c) and the decompressor (0x1d0) together on the stack.
void read_JPEG_file(char* path, uint8_t* pixels) { read_JPEG_fileChecked(path, pixels); }
int32_t read_JPEG_fileChecked(char* path, uint8_t* pixels) {
    uint8_t* rows = pixels;
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        fprintf(reinterpret_cast<FILE*>(gIfsWFiles + 0xa8), "can't open %s\n", path);
        return 0;
    }
    struct {
        jpeg_error_mgr mgr;
        uint8_t jumpBuffer[0x11c];
        jpeg_decompress_struct cinfo;
    } frame;
    frame.cinfo.err = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(jpeg_std_error(&frame.mgr)));
    *reinterpret_cast<uint32_t*>(frame.mgr.mBytes) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&my_error_exit));
    if (outbox_callee::JpegSetJmp(frame.jumpBuffer) != 0) {
        jpeg_destroy_decompress(&frame.cinfo);
        fclose(file);
        return 0;
    }
    jpeg_CreateDecompress(&frame.cinfo, 0x3e, 0x1d0);
    jpeg_stdio_src(&frame.cinfo, file);
    jpeg_read_header(&frame.cinfo, 1);
    jpeg_start_decompress(&frame.cinfo);
    uint32_t components;
    std::memcpy(&components, reinterpret_cast<uint8_t*>(&frame.cinfo) + 0x7c, 4);   // output_components, inside f78
    uint32_t stride = frame.cinfo.output_width * components;
    while (frame.cinfo.output_scanline < frame.cinfo.output_height) {
        jpeg_read_scanlines(&frame.cinfo, &rows, 1);
        rows += stride;
    }
    jpeg_finish_decompress(&frame.cinfo);
    jpeg_destroy_decompress(&frame.cinfo);
    fclose(file);
    return 1;
}
