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
#include "Mouse.h"
#include "TPCamera.h"
#include "TPEngine.h"
#include "ThingReceivers.h"
#include "ppc_math.h"

// 0x10395e6c .. 0x10395e77: the chat panel's first-person bookkeeping (declared in online.h; zero at start like the Mac's bss).
int32_t gOnlineFirstPersonCountdown = 0;   // 0x10395e6c
uint32_t gOnlineSavedFlags = 0;            // 0x10395e70
uint8_t gOnlineMapZ[2] = {0, 0};           // 0x10395e74
uint8_t gOnlineMapX[2] = {0, 0};           // 0x10395e76

namespace {

// The control is made on first use: operator new(0xe8c), then the constructor.
COnlineControl* OnlineControlOrNew() {
    if (gOnlineControl == nullptr) {
        COnlineControl* made = static_cast<COnlineControl*>(online_callee::OperatorNew(0xe8c));
        if (made != nullptr) new (made) COnlineControl();
        gOnlineControl = made;
    }
    return gOnlineControl;
}

// A position as a map cell and a 1/256 offset: the cell is the truncated tenth, the offset the truncated 256 times the rest.
void ToMapPos(float position, uint8_t out[2]) {
    float tenth = position / 10.0f;
    out[0] = static_cast<uint8_t>(ppc_fctiwz(tenth));
    float rest = tenth - static_cast<float>(out[0]);
    out[1] = static_cast<uint8_t>(ppc_fctiwz(256.0f * rest));
}

float FromMapPos(const uint8_t in[2]) {
    float cell = static_cast<float>(in[0]);
    float offset = static_cast<float>(in[1]);
    return 10.0f * ppc_fmadds(offset, 0.00390625f, cell);
}

MapPos MakeMapPos(const uint8_t in[2]) {
    MapPos p;
    p.mCell = in[0];
    p.mOffset = in[1];
    return p;
}

}  // namespace

// 0x101e5ba0
void ChatBackToThirdPerson() {
    if ((gFirstPersonFlags & 0x400) != 0) return;
    bool same = (gOnlinePanel == gOnlineWord80);
    UI_SendMessage(gOnlinePanel, 6, same ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, same ? 0 : 1, 0);
    gOnlineWord80 = nullptr;
    if (gOnlinePanel != nullptr) gOnlinePanel->setCursor(0);
    if (gOnlineWindow != nullptr) gOnlineWindow->setCursor(0);
    ToMapPos(gCameraViewPoint.x, gOnlineMapX);
    ToMapPos(gCameraViewPoint.z, gOnlineMapZ);
    OnlineControlOrNew()->LeavingFirstPerson(MakeMapPos(gOnlineMapX), MakeMapPos(gOnlineMapZ));
    gOnlineByte78 = 0;
    gCameraSavedPosition[0] = FromMapPos(gOnlineMapX);
    gCameraSavedPosition[2] = FromMapPos(gOnlineMapZ);
}

// 0x101e5968
void ChatGoFirstPerson() {
    if (gOnlinePanelShown != 0) gOnlineWord80 = gOnlinePanel;
    else gOnlineWord80 = gOnlineWindow;
    CoolRect rect;
    gOnlineWindow->GetPosition(&rect, 1);
    UI_SendMessage(gOnlinePanel, 6, 0, 0);
    UI_SendMessage(gOnlineWindow, 6, 0, 0);
    ToMapPos(gPointerWorldPosn.x, gOnlineMapX);
    ToMapPos(gPointerWorldPosn.z, gOnlineMapZ);
    OnlineControlOrNew()->EnteringFirstPerson(MakeMapPos(gOnlineMapX), MakeMapPos(gOnlineMapZ));
    if ((gFirstPersonFlags & 4) != 0) OnlineControlOrNew()->HideAvatar();
    gOnlineByte78 = 1;
    gOnlineFirstPersonCountdown = 5;
    gOnlineSavedFlags = gFirstPersonFlags;
}

// 0x101e5e08
void UpdateAvatarPos() {
    bool unhide = false;
    uint32_t flags = gFirstPersonFlags;
    if ((flags & 0x400) != 0) {
        if ((gOnlineSavedFlags & 0x400) == 0) {
            gOnlineByte78 = 0;
            OnlineControlOrNew()->HideAvatar();
        }
    } else if ((flags & 0x16) != 0 && (gOnlineSavedFlags & 0x400) != 0) {
        gOnlineByte78 = 1;
        unhide = true;
        gOnlineFirstPersonCountdown = 0;
    }
    gOnlineSavedFlags = gFirstPersonFlags;
    if (gOnlineFirstPersonCountdown != 0) gOnlineFirstPersonCountdown = gOnlineFirstPersonCountdown - 1;
    if (gOnlineFirstPersonCountdown != 0 || gOnlineByte78 == 0) return;

    uint8_t x[2];
    uint8_t z[2];
    if (unhide) {
        ToMapPos(gCameraPosition[2], z);
        ToMapPos(gCameraPosition[0], x);
    } else {
        ToMapPos(gCameraViewPoint.z, z);
        ToMapPos(gCameraViewPoint.x, x);
    }
    if (x[0] == gOnlineMapX[0] && x[1] == gOnlineMapX[1] && z[0] == gOnlineMapZ[0] && z[1] == gOnlineMapZ[1] && !unhide) return;
    gOnlineMapX[0] = x[0];
    gOnlineMapX[1] = x[1];
    gOnlineMapZ[0] = z[0];
    gOnlineMapZ[1] = z[1];
    if (unhide) OnlineControlOrNew()->UnHideAvatar(MakeMapPos(x), MakeMapPos(z));
    else OnlineControlOrNew()->UpdateFirstPersonPos(MakeMapPos(x), MakeMapPos(z));
}

// 0x101e57bc
void ChatGoFullScreen() {
    uint8_t shown = gOnlinePanelShown;
    if (shown == 0 || gOnlineBlockToggle != 0) return;
    UI_SendMessage(gOnlinePanel, 6, shown == 0 ? 1 : 0, 0);
    UI_SendMessage(gOnlineWindow, 6, shown, 0);
    if (gOnlinePanelShown == 0) {
        gOnlineWindow->CaptureKb(0);
        return;
    }
    InterfaceWindow* w = gOnlinePanel->GetWindow(0xc3123);
    w = w->GetWindow(0xc312b);
    w = w->GetWindow(0xc312c);
    w->CaptureKb(0);
    if (gMouse == nullptr) {
        CMouse* mouse = static_cast<CMouse*>(online_callee::OperatorNew(1));
        if (mouse != nullptr) {
            CMMDefault* mode = static_cast<CMMDefault*>(online_callee::OperatorNew(8));
            if (mode != nullptr) new (mode) CMMDefault();
            mouse->AssignMouseMode(mode);
            gMouseWord1E0 = -1;
            gMouseWord1E4 = -1;
        }
        gMouse = mouse;
    }
    int32_t current = (gMouseMode == nullptr) ? 0 : gMouseMode->GetMode();
    if (current == 9) gOnlinePanel->setCursor(0x13);
}
