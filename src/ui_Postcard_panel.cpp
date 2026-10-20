// The postcard panel (main app, ui_Postcard.cpp, 0x101b0ae4..0x101b2418), the keyboard key and UIInterface forwarding call.
#include "ui_Postcard.h"
#include <cstring>
#include <new>
#include "Postcard.h"
#include "Game.h"
#include "FS_Online.h"
#include "Text.h"
#include "TbITable.h"
#include "TbGraphicFile.h"
#include "UI_Base.h"
#include "UIInterface.h"
#include "UI_pump.h"
#include "ui_main.h"
#include "ui_controls.h"
#include "ui_ErrorDisplay.h"
#include "HLSprite.h"
#include "Advisor.h"
#include "tb_ddraw.h"
#include "tb_resource_texture.h"

Postcard* gPostcard;
InterfaceWindow* gPostcardPanel;
int32_t gPostcardSent;
int32_t gPreviewDragging;
int32_t gPreviewLastX;
uint32_t gPreviewLastY;
int32_t gPreviewOffsetX;
int32_t gPreviewOffsetY;
uint8_t gAddressEdited;
uint8_t gMessageEdited;
uint8_t gTempPathFetched;
char gTempPath[0x200];
char16_t gPostcardMessage[0x100];
char16_t gPostcardAddress[0x60];
char16_t gPostcardText5da5[0x200];

namespace {
void FillPreview(uint8_t* surface, int32_t width, int32_t height) {
    const int32_t format = gei_TextureSettings.mFormat;
    if (format == 2 || format == 4)
        gPostcard->generate_preview_into_buffer(surface, width, height, 2);
    else if (format == 3 || format == 5)
        gPostcard->generate_preview_into_buffer(surface, width, height, 1);
}

void StyleEditBox(InterfaceEditBox* box, InterfaceCursorPosTextObjectSprite* sprite, int32_t wrap, int32_t stringId) {
    box->SetClientDrawObject(sprite);
    box->SetFont(GetFontTableEntry(0xb));
    box->SetFontColour(0xff, 0xff, 0xff, 0xff);
    box->SetFontWrap(wrap, 0);
    box->SetFontJust(0, 0);
    char cursorName[] = "hilight";
    box->SetCursorDrawObject(DrawObjectLibrary::FindDrawObject(cursorName));
    box->SetText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}
}

void RefreshPreviewWindow(InterfaceWindow* window) {
    TbRenderArea areaA;
    TbRenderArea areaB;
    TbRenderArea areaC;
    CoolRect rect;
    int32_t width;
    int32_t height;
    uint8_t* surface = window->GetClientObjectSurface(&width, &height);
    if (surface != nullptr && gPostcard != nullptr) {
        gPostcard->generate();
        UI_DrawPostcardText(gPostcardMessage, gPostcard->mCard.mPixels, gPostcard->mCard.mWidth,
                             gPostcard->mCard.mHeight, {0x142, 0x5e, 0x27c, 0xd7}, 0xff0000cc, 1);
        FillPreview(surface, width, height);
        window->FinishWithClientObjectSurface();
    }
}

int32_t PostcardPreviewMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x10004) {
        gPreviewDragging = 0;
        window->UnCaptureDevice();
    } else if (message == 0x10003 && gPreviewDragging != 0) {
        const int32_t dy = static_cast<int32_t>(static_cast<uint32_t>(gUIRealScreenWidth) *
                                               (static_cast<uint32_t>(gPreviewLastX) - static_cast<uint32_t>(a >> 16)));
        gPreviewOffsetY += dy / 2048;
        const int32_t dx = static_cast<int32_t>(static_cast<uint32_t>(gUIRealScreenHeight) *
                                               (gPreviewLastY - (static_cast<uint32_t>(a) & 0xffff)));
        gPreviewOffsetX += dx / 1536;
        if (gPreviewOffsetY < 0) gPreviewOffsetY = 0;
        if (gPreviewOffsetX < 0) gPreviewOffsetX = 0;
        gPreviewLastX = a >> 16;
        gPreviewLastY = static_cast<uint32_t>(a) & 0xffff;
    } else if (message == 0x10005) {
        gPreviewDragging = 1;
        gPreviewLastY = static_cast<uint32_t>(b) & 0xffff;
        gPreviewLastX = b >> 16;
        window->CaptureDevice();
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

int32_t PostcardPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x15) {
        UIMain_DisableAllKeyTables();
    } else if (message == 5) {
        RemovePostcardPanel();
        return 1;
    } else if (message == 0x14) {
        gPostcardPanel = nullptr;
        delete gPostcard;
        gPostcard = nullptr;
        if (TheGameType().mType == kGameTypeOnline) ChatReinstateAfterPostcard();
        else UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x133));
        UIMain_DecideKbCapture();
        UIMain_EnableAllKeyTables();
    } else if (message == 0x801) {
        if (a == 0x5da3) gMessageEdited = 1;
        else if (a == 0x5da4) gAddressEdited = 1;
    } else if (message == 0x100) {
        if (a == 0x5da0) {
            CreateAddressBook();
        } else if (a == -1) {
            if (!gMessageEdited) static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gPostcardMessage, 0x100, 0xd9);
            if (!gAddressEdited) static_cast<TbITable*>(gText.mResourcePointers[6])->FormatString(gPostcardAddress, 0x60, 0xd8);
            RefreshPreviewWindow(window->GetWindow(0x5d9f));
            if (!AddressesValid()) {
                dlgboxparams params;
                params.mText = UIMain_GetString(0xd6);
                params.mRed = 0xff;
                params.mGreen = 0xff;
                params.mBlue = 0xff;
                params.mValue8 = 4;
                UI_DialogBox(params, static_cast<dlgbutton>(1), nullptr, static_cast<dlgbutton>(0), nullptr,
                             static_cast<dlgbutton>(0), nullptr, false);
            } else {
                SendPostcard();
                RemovePostcardPanel();
            }
            gPostcardSent = 1;
            return 0;
        } else if (a == -2) {
            RemovePostcardPanel();
            return 0;
        } else if (a == 0x5da2) {
            RefreshPreviewWindow(window->GetWindow(0x5d9f));
        } else if (a == 0x5da1) {
            char16_t empty[] = u"";
            window->GetWindow(0x5da3)->SetText(empty);
            window->GetWindow(0x5da4)->SetText(empty);
            window->GetWindow(0x5da5)->SetText(empty);
        }
    }
    if (TheGameType().mType == kGameTypeOnline) return window->CallDefaultWindowHandler(message, a, b);
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

void CreatePostcardPanel() {
    if (gPostcardPanel != nullptr) return;
    if (gOnlineSystem == nullptr) gOnlineSystem = new (std::nothrow) CFSOnlineSystem;
    if (!gOnlineSystem->CanWeSendEmail()) {
        AddUIError(0x4e, nullptr);
        return;
    }
    InterfaceWindow* parent;
    if (TheGameType().mType == kGameTypeOnline) parent = gOnlineWindow;
    else {
        UIMain_CheckPanel();
        parent = gGameWindow;
    }
    InterfaceWindow* panel = parent->Build(gPostcardPanelDescription, PostcardPanelMsg);
    if (panel != nullptr) {
        gPostcardPanel = panel;
        if (TheGameType().mType != kGameTypeOnline) UIMain_SetPanel(gPostcardPanel);
        UIMain_SetTitleText(gPostcardPanel, 0x5d9e, const_cast<char16_t*>(UIMain_GetString(0xd0)));
        auto* box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da5));
        if (box != nullptr) {
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            std::memset(gPostcardText5da5, 0, sizeof(gPostcardText5da5));
            box->SetStaticTextArea(gPostcardText5da5, 0x1ff);
            StyleEditBox(box, sprite, 1, 0xd3);
        }
        box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da4));
        if (box != nullptr) {
            std::memset(gPostcardAddress, 0, sizeof(gPostcardAddress));
            box->SetStaticTextArea(gPostcardAddress, 0x5f);
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            StyleEditBox(box, sprite, 0, 0xd1);
        }
        box = static_cast<InterfaceEditBox*>(gPostcardPanel->GetWindow(0x5da3));
        if (box != nullptr) {
            std::memset(gPostcardMessage, 0, sizeof(gPostcardMessage));
            box->SetStaticTextArea(gPostcardMessage, 0xff);
            auto* sprite = new (std::nothrow) InterfaceCursorPosTextObjectSprite(nullptr);
            StyleEditBox(box, sprite, 1, 0xd2);
        }
        InterfaceWindow* preview = gPostcardPanel->GetWindow(0x5d9f);
        if (preview != nullptr) {
            preview->SetCallbackFunction(PostcardPreviewMsg);
            preview->SetClientDrawObject(new (std::nothrow) InterfaceDrawObjectSprite(nullptr));
            TbRenderArea areaA;
            TbRenderArea areaB;
            CoolRect rect;
            int32_t width;
            int32_t height;
            uint8_t* surface = preview->GetClientObjectSurface(&width, &height);
            if (surface != nullptr && gPostcard != nullptr) {
                gPostcard->generate();
                FillPreview(surface, width, height);
                preview->FinishWithClientObjectSurface();
            }
        }
    }
    gMessageEdited = 0;
    gAddressEdited = 0;
    if (TheGameType().mType != kGameTypeOnline) UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x133));
}

void RemovePostcardPanel() { UI_SendMessage(gPostcardPanel, 4, 0, 0); }

void UI_ActuallyDoPostcard() {
    gPostcardSent = 0;
    if (TheGameType().mType != kGameTypeOnline) {
        gGame.mF3C = 1;
        gGame.Unpause();
    }
    delete gPostcard;
    gPostcard = new (std::nothrow) Postcard;
    gPostcard->load_sources();
    DDSURFACEDESC2 desc = gei_DDraw->LockBackbuffer();
    gPostcard->grab(reinterpret_cast<_DDSURFACEDESC2&>(desc));
    gei_DDraw->UnlockBackbuffer();
    gPostcard->generate();
    gUIRootWindow->Show(1);
    UI_ShowCursor();
    UIMain_GetFullyOutOfFirstPerson();
    if (TheGameType().mType == kGameTypeOnline) ChatGoFullScreen();
    else if (GameWindowToggle_Hidden()) ToggleGameWindow();
    if (!gTempPathFetched) {
        NS_MacDoze::GetTempPath(0x200, gTempPath);
        gTempPathFetched = 1;
    }
    CreatePostcardPanel();
    gPostcardWanted = 0;
}

void UI_DoPostcard() {
    UIMain_UISound(0x95);
    if (TheGameType().mType == kGameTypeOnline) ChatPrePostCard();
    else {
        gGame.Pause(static_cast<CGame::ePauseFlag>(0), false);
        gGame.mF3C = 0;
    }
    gUIRootWindow->Show(0);
    UI_HideCursor();
    BTN_HighLight_Kill();
    ADV_Kill(1);
    gPostcardDelay = 3;
    gPostcardWanted = 1;
}

void UIInterface::DoPostcard() { UI_DoPostcard(); }
int32_t TakePostCard() {
    gUIInterface.DoPostcard();
    return 1;
}
