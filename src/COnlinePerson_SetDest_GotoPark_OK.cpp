// online.cpp (main app): the park-goto callbacks of the online client.
#include <new>

#include "GameSys.h"
#include "online.h"
#include "platform/samsutils.h"

namespace {
// Strings of the data section at 0x10250e7f (the TOC slot -0x395c).
const char kGotoFailed[] = "goto ftp download failed\n";

COnlineControl* Control() {
    if (gOnlineControl == nullptr) {
        void* block = online_callee::OperatorNew(0xe8c);
        if (block != nullptr) new (block) COnlineControl();
        gOnlineControl = static_cast<COnlineControl*>(block);
    }
    return gOnlineControl;
}
}  // namespace

// 0x10213728
int32_t GotoPark_OK(void*) {
    Control()->GotoStoredPerson();
    return 1;
}

// 0x102137bc
void GotoCallbackFn(int32_t status, const void*) {
    if (status == 0) {
        GS_SetCurrentGameStatus(GS_LEAVE_PARK);
    } else {
        Control()->GotoFailed();
        SamsUtilities::UDebug(kGotoFailed);
    }
}
