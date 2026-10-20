// LbSCBack.cpp of the bullfrog library: window name, screen saver, message pump, screen mode (unit v5284).
#include <cstring>

#include "LbSCBack.h"

// The string table at 0x1006c689: "Bullfrog" followed by "DISPLAY" (at +9).
static const char kBullfrogStrings[] = "Bullfrog\0DISPLAY";

char gLbWindowName[64] = "Bullfrog";   // 0x10072600
LbSCBackGlobals gLbSCBack;              // 0x10073e0c

// Empty host adapters for __construct_array: the list bodies are TbPtrList_LbSEFunction_Ctor and _DtorBody (units v51127, v5L21),
// but the host's gLbSCBack keeps the Mac's 0x10-byte stride, which a host TbPtrList does not fit.
void LbSEFunctionList_Construct(void*) {}
void LbSEFunctionList_Destruct(void*) {}
void LbSystemEventCallback_Destruct(void*) {}

// 0x10014018: drains up to 2001 pending messages.
void LbApp_ProcessWindowMessages() {
    NS_MacDoze::tagMSG msg;
    int32_t budget = 2000;
    for (;;) {
        if (NS_MacDoze::PeekMessage(&msg, nullptr, 0, 0, 1) == 0) break;
        bool last = (budget == 0);
        budget--;
        if (last) break;
        NS_MacDoze::TranslateMessage(&msg);
        NS_MacDoze::DispatchMessage(&msg);
    }
}

// 0x100141a8: the result is 0 when the call succeeds, -1 otherwise.
LbAppResult LbApp_EnableWindowsScreenSaver(int32_t enable) {
    LbAppResult result;
    if (NS_MacDoze::SystemParametersInfoA(0x11, static_cast<uint32_t>(enable), nullptr, 0) != 0)
        result.mCode = 0;
    else
        result.mCode = -1;
    return result;
}

// 0x10014278: a NULL name gives "Bullfrog"; the copy is cut at 64 bytes (no terminator then).
LbAppResult LbApp_SetWindowName(const char* name) {
    if (name != nullptr)
        strncpy(gLbWindowName, name, 0x40);
    else
        strncpy(gLbWindowName, kBullfrogStrings, 0x40);
    LbAppResult result;
    result.mCode = 0;
    return result;
}

// 0x10014374
void LbApp_GetWindowsScreenMode(TbScreenMode& mode) {
    NS_MacDoze::InternalHandle* dc = NS_MacDoze::CreateDC(kBullfrogStrings + 9, nullptr, nullptr, nullptr);
    if (dc != nullptr) {
        mode.mDepth = NS_MacDoze::GetDeviceCaps(dc, 12);
        mode.mWidth = NS_MacDoze::GetDeviceCaps(dc, 8);
        mode.mHeight = NS_MacDoze::GetDeviceCaps(dc, 10);
        NS_MacDoze::DeleteDC(dc);
    }
}

// 0x10014b4c
void __sinit_LbSCBack_cpp() {
    LbSCBackGlobals* object = &gLbSCBack;
    __construct_array(object, reinterpret_cast<void*>(&LbSEFunctionList_Construct),
                      reinterpret_cast<void*>(&LbSEFunctionList_Destruct), 0x10, 6);
    object = &gLbSCBack;
    object->mCriticalSection = static_cast<void**>(::operator new(4));
    NS_MacDoze::InitializeCriticalSection(object->mCriticalSection);
    __register_global_object(object, reinterpret_cast<void*>(&LbSystemEventCallback_Destruct), object->mNode);
}
