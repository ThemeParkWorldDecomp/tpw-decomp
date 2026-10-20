// UI_DefaultEditBoxCallback, main 0x101cf1ec (1,604 bytes), unit w37c.
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "InterfaceFont.h"
#include "platform/msl.h"

int32_t UI_DefaultEditBoxCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    auto* box = static_cast<InterfaceEditBox*>(window);
    switch (message) {
    case 0x13:
        if (box->mCursorDrawObject != nullptr) {
            CoolRect rect(static_cast<int16_t>(box->mClientRect.mLeft + box->mF150),
                          static_cast<int16_t>(box->mClientRect.mTop + box->mF154),
                          static_cast<int16_t>(box->mClientRect.mLeft + box->mF150 + 8),
                          static_cast<int16_t>(box->mF156 + box->mClientRect.mTop + box->mF154));
            DOChange change{};
            change.mRect = &rect;
            box->mCursorDrawObject->Change(0x10, &change);
        }
        break;
    case 0x1a:
        box->CaptureKb(1);
        break;
    case 0x20:
        if (box->mTimer != nullptr) box->mTimer->Stop();
        box->mF158 = -1;
        if (box->mCursorDrawObject != nullptr) box->mCursorDrawObject->Show(0);
        if (gUIAutoRedraw != 0) UI_PostMessage(box, 3, 0, 0);
        break;
    case 0x1f:
        if (box->mTimer != nullptr) box->mTimer->Start();
        box->mF158 = 1;
        if (box->mCursorDrawObject != nullptr) box->mCursorDrawObject->Show(1);
        if (gUIAutoRedraw != 0) UI_PostMessage(box, 3, 0, 0);
        if (a == 0) box->SelectAll();
        break;
    case 0x10:
        if (box->mF14C == -1 && box->mF158 != -1) {
            box->mF158 = static_cast<int16_t>(box->mF158 ^ 1);
            if (box->mCursorDrawObject != nullptr) box->mCursorDrawObject->Show(box->mF158);
            if (gUIAutoRedraw != 0) UI_PostMessage(box, 3, 0, 0);
        }
        break;
    case 0x1000a:
        box->Keydown(static_cast<int16_t>(a), b);
        break;
    case 0x1000b: {
        const int16_t key = static_cast<int16_t>(a);
        if (key == 13) {
            if ((box->mAttributes & 0x80) == 0) UI_SendMessage(box->mParent, 0x802, box->mId, b);
        } else if (key == 9) UI_SendMessage(box->mParent, 0x803, box->mId, b);
        else if (key == 27) UI_SendMessage(box->mParent, 0x804, box->mId, b);
        else UI_SendMessage(box->mParent, 0x806, box->mId, key);
        break;
    }
    case 0x1000c:
        box->Edit(static_cast<int16_t>(a));
        break;
    case 0x10006:
        if (box->mF161 == 0) {
            box->CheckClientMouse(message, a, b);
            box->CaptureKb(1);
        }
        break;
    case 0x10007:
        if (box->mF161 == 0) {
            box->CheckClientMouse(message, a, b);
            box->SelectAll();
        }
        break;
    case 0x10005:
    case 0x10003:
        if (box->mF161 != 0) break;
        if (message == 0x10005) {
            if (a != 0 || box->mFont == nullptr) break;
        } else if (box->mRightToLeft != 0 || (box->mButtonsHeld & 1) == 0) break;
        if (box->mBuffer != nullptr && box->mClientDrawObject != nullptr && box->mClientDrawObject->Query(1, 0) != 0) {
            const uint32_t length = wcslen(box->mBuffer);
            const int16_t fontHeight = static_cast<int16_t>(box->mFont->GetHeight());
            const int32_t point = message == 0x10005 ? b : a;
            const int32_t index = NearestCharToPos(box->mClientDrawObject, (point >> 16) - box->mClientRect.mLeft,
                                                 static_cast<int32_t>(static_cast<uint32_t>(point) & 0xffff) -
                                                 box->mClientRect.mTop + (fontHeight >> 1), static_cast<int32_t>(length));
            if (index >= 0) box->MoveCursor(message == 0x10005 ? static_cast<int16_t>(index) : -1, 1,
                                          message == 0x10005 ? -1 : static_cast<int16_t>(index));
        }
        break;
    default:
        break;
    }
    return UI_DefaultWindowCallback(window, message, a, b);
}
