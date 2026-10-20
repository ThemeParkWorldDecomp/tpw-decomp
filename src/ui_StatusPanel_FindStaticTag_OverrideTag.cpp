// ui_StatusPanel_FindStaticTag_OverrideTag.cpp (main): OverrideTag 0x10191d7c (332 B), TagDlgCallback 0x10191628 (1220 B).
#include "InterfaceWindow.h"
#include "ui_main.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "platform/msl.h"

// 0x1038cd7c, in bss: set elsewhere (the window to slide in); the Mac's word is zero at start.
InterfaceWindow* gTagPanelPending;
// 0x102a83b2: the Mac's bytes are not known beyond the first word (0xfffefffe).
UI_RectDelta gTagSlideAnim[1] = {{-2, -2, 0, 0}};

// Looks over the sibling windows from gTagFirstWindow on for the tag windows that hold these parameters (extras 1, 3, 8). When
// the id equals the second field the text is copied into the window (extra 2) and shown when it is the panel source, and the
// answer is false; otherwise the aerial window is told (0x100002). True when no window took the text.
bool OverrideTag(CTagParams& params) {
    int32_t result = 1;
    InterfaceWindow* window = gTagFirstWindow;
    while (window != nullptr) {
        int32_t one = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(1)));
        if (params.mF4 == one) {
            uint16_t three = static_cast<uint16_t>(reinterpret_cast<intptr_t>(window->getExtraParam(3)));
            if (params.mF1C == three) {
                int32_t eight = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(8)));
                if (params.mF20 == eight) {
                    if (static_cast<int32_t>(params.mId) == params.mF4) {
                        char16_t* dest = static_cast<char16_t*>(window->getExtraParam(2));
                        wcsncpy(dest, params.mText, 0xff);
                        dest[0xff] = 0;
                        if (window == gTagPanelSource) {
                            InterfaceWindow* text = gTagPanel->GetWindow(0x10d32);
                            text->SetText(dest);
                        }
                        result = 0;
                    } else {
                        UI_SendMessage(gAerialWindow, 0x100002, static_cast<int32_t>(reinterpret_cast<intptr_t>(window)), 1);
                    }
                }
            }
        }
        window = window->GetNext();
    }
    return result != 0;
}

// One of the two buttons (0x10d2f, 0x10d30): DoButton with the action of extra `actionIndex`; 2 tells the aerial window and
// closes the slide-out panel, 1 closes it.
static void TagButton(InterfaceWindow* self, int32_t actionIndex) {
    InterfaceWindow* source = gTagPanelSource;
    uint16_t idValue = static_cast<uint16_t>(static_cast<int16_t>(reinterpret_cast<intptr_t>(self->getExtraParam(2))));
    int32_t extra = static_cast<int32_t>(reinterpret_cast<intptr_t>(self->getExtraParam(5)));
    TagButtonAction action = static_cast<TagButtonAction>(reinterpret_cast<intptr_t>(self->getExtraParam(actionIndex)));
    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
    id.mIndex = idValue;
    int32_t answer = DoButton(action, id, extra);
    if (answer == 2) {
        UI_SendMessage(gAerialWindow, 0x100002, static_cast<int32_t>(reinterpret_cast<intptr_t>(source)), 0);
        if (gTagPanelSource != nullptr) RemoveSlideoutPanel(0);
    } else if (answer < 2 && answer >= 1) {
        if (gTagPanelSource != nullptr) RemoveSlideoutPanel(0);
    }
}

// The message function of the tag dialog panel. 0x1001d9 fills the panel from the extras of gTagPanel and slides it in,
// 0x1001d8 stops the slide and cancels the advisor message 0x11c, 0x100 handles the three buttons, 0x1d (1) clears gTagPanelPending.
int32_t TagDlgCallback(InterfaceWindow* self, int32_t message, int32_t a, int32_t b) {
    if (message == 0x100) {
        if (a == 0x10d2f) {
            TagButton(self, 4);
        } else if (a < 0x10d2f) {
            if (a >= 0x10d2e) {
                InterfaceWindow* source = gTagPanelSource;
                if (source != nullptr) {
                    source->Enable(0);
                    UI_PostMessage(gAerialWindow, 0x100002, static_cast<int32_t>(reinterpret_cast<intptr_t>(gTagPanelSource)), 0);
                    if (gTagPanelSource != nullptr) RemoveSlideoutPanel(0);
                }
            }
        } else if (a < 0x10d31) {
            TagButton(self, 3);
        }
    } else if (message < 0x100) {
        if (message == 0x1d && a == 1) gTagPanelPending = nullptr;
    } else if (message == 0x1001d9) {
        InterfaceWindow* text = gTagPanel->GetWindow(0x10d32);
        if (text != nullptr) {
            char16_t* string = static_cast<char16_t*>(self->getExtraParam(1));
            text->SetClientDrawText(string);
            InterfaceWindow* button = gTagPanel->GetWindow(0x10d30);
            TagButtonAction action = static_cast<TagButtonAction>(reinterpret_cast<intptr_t>(gTagPanel->getExtraParam(3)));
            SetButton(button, action);
            button = gTagPanel->GetWindow(0x10d2f);
            action = static_cast<TagButtonAction>(reinterpret_cast<intptr_t>(gTagPanel->getExtraParam(4)));
            SetButton(button, action);
            gTagPanel->GetWindow(0x10d2e)->SetDrawObject(nullptr);
            int32_t kind = static_cast<int32_t>(reinterpret_cast<intptr_t>(gTagPanel->getExtraParam(4)));
            if (kind == 0x1b) {
                gTagPanel->GetWindow(0x10d2e)->Enable(0);
            } else {
                InterfaceDrawObject* object = DrawObjectLibrary::FindDrawObject(const_cast<char*>("mb_erase"));
                gTagPanel->GetWindow(0x10d2e)->SetDrawObject(object);
                gTagPanel->GetWindow(0x10d2e)->Enable(1);
            }
        }
        InterfaceWindow* window = gTagPanel->GetWindow(0x10d30);
        window->setZ(static_cast<int16_t>(window->mZ + 1));
        window = gTagPanel->GetWindow(0x10d2f);
        window->setZ(static_cast<int16_t>(window->mZ + 1));
        window = gTagPanel->GetWindow(0x10d2e);
        window->setZ(static_cast<int16_t>(window->mZ + 1));
        gTagPanelSource = gTagPanelPending;
        gTagPanelSource->startAnim(gTagSlideAnim, 100);
    } else if (message < 0x1001d9 && message >= 0x1001d8) {
        if (gTagPanelSource != nullptr) gTagPanelSource->stopAnim();
        UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0x11c));
        gTagPanelSource = nullptr;
    }
    return self->CallDefaultWindowHandler(message, a, b);
}
