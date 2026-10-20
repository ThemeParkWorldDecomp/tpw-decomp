#include "Debug.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "ThingReceivers.h"
#include "World.h"
#include "analyser.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "UI_Base.h"
#include "rsse.h"

// 0x10190d0c (452 bytes): makes `window` the tag shown in the slide-out panel (gTagPanel).
void ShowTagPanel(InterfaceWindow* window) {
    if (gTagPanelSource == window) return;
    if (static_cast<int8_t>(window->mF134) != 0) {
        int32_t kind = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(6)));
        if (kind != 0x1b) {
            window->stopAnimTimer(0x80080);
            window->stopAnimTimer(0x80081);
            window->mF134 = 0;
        }
        int32_t emitter = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9)));
        if (emitter > -1 && kind != 0x1b) {
            if (PAR_GetEmitterStatus(static_cast<uint32_t>(emitter)) != 0) PAR_KillEmitter(emitter);
        }
    }
    gTagPanel->setExtraParam(1, window->getExtraParam(2));
    gTagPanel->setExtraParam(2, window->getExtraParam(3));
    gTagPanel->setExtraParam(3, window->getExtraParam(5));
    gTagPanel->setExtraParam(4, window->getExtraParam(6));
    gTagPanel->setExtraParam(5, window->getExtraParam(8));
    LinkSlideoutPanel(gTagPanel, 1);
    gTagPanelPending = window;
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x11c));
}

namespace {

// The emitter position the listing computes from a window rectangle: x = (centre x * 100000) / 2048 and
// y = (centre y * 75000) / 1536 (the second one through the multiply-high magic number 0x2aaaaaab).
void EmitterSpot(CoolRect& rect, int32_t& x, int32_t& y) {
    int32_t cx = rect.mLeft + ((rect.mRight - rect.mLeft) >> 1);
    int32_t v = static_cast<int32_t>(static_cast<uint32_t>(cx) * 100000u);
    x = (v >> 11) + ((v < 0 && (v & 0x7ff) != 0) ? 1 : 0);
    int32_t cy = rect.mTop + ((rect.mBottom - rect.mTop) >> 1);
    int32_t w = static_cast<int32_t>(static_cast<uint32_t>(cy) * 75000u);
    int32_t q = static_cast<int32_t>((static_cast<int64_t>(0x2aaaaaab) * w) >> 32) >> 8;
    q += static_cast<int32_t>(static_cast<uint32_t>(q) >> 31);
    y = q;
}

}  // namespace

// 0x10190f04 (1772 bytes): the message function of a tag window.
int32_t TagCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x12) return window->CallDefaultWindowHandler(message, a, b);

    if (message < 0x12) {
        if (message == 6) {
            int16_t index = static_cast<int16_t>(FindStaticTag(window));
            if (index < 0) return 0;
            if (gStaticTags[index].mActive == 0) return 0;
        } else if (message < 6) {
            if (message >= 5) {
                if (static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(6))) == 0x1b) {
                    tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id;
                    id.mIndex = 0;
                    DoButton(static_cast<TagButtonAction>(0x1b), id, 0);
                }
                int16_t index = static_cast<int16_t>(FindStaticTag(window));
                window->Show(0);
                int32_t emitter = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9)));
                if (emitter > -1 && PAR_GetEmitterStatus(static_cast<uint32_t>(emitter)) != 0) PAR_KillEmitter(emitter);
                window->stopAnimTimer(0x80080);
                window->stopAnimTimer(0x80081);
                sStaticTag* tag = &gStaticTags[index];
                if (tag->mActive == 0) return 0;
                tag->mActive = 0;
                gTagCount = gTagCount - 1;
                int16_t height;
                window->GetDims(nullptr, &height, 1);
                height = static_cast<int16_t>(height + 10);
                gAerialWindow->AdjustPosition(0, height, 0, 0);
                InterfaceWindow* walk = window;
                if (window == gTagFirstWindow) gTagFirstWindow = window->GetNext();
                if (a != 0) {
                    while ((walk = walk->GetNext()) != nullptr) {
                        walk->ShiftPosition(0, height);
                        if (static_cast<int8_t>(walk->mAnimating) == 0) walk->startAnim(gTagShiftAnim, 0x14);
                    }
                    InterfaceWindow* other = gAerialWindow->GetWindow(0x2e);
                    if (other != nullptr) other->startAnim(gTagShiftAnim, 0x32);
                }
                window->unlink();
                InterfaceWindow* source = gTagPanelSource;
                if (source != nullptr && source == window && source != nullptr) RemoveSlideoutPanel(0);
                return 1;
            }
        } else if (message == 0x10) {
            if (a == 0x80080 && static_cast<int8_t>(window->mF134) != 0) {
                window->Enable(0);
                window->mF134 = 0;
                UI_PostMessage(gAerialWindow, 0x100002, static_cast<int32_t>(reinterpret_cast<intptr_t>(window)), 0);
            } else if (a == 0x80081) {
                CoolRect rect;
                if (static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9))) == -1) {
                    window->GetPosition(&rect, 1);
                    int32_t x, y;
                    EmitterSpot(rect, x, y);
                    uint32_t emitter = PAR_InitEmitter(0x5b, x, 0, y);
                    window->setExtraParam(9, reinterpret_cast<void*>(static_cast<uintptr_t>(emitter)));
                    window->stopAnimTimer(0x80081);
                    if (window->GetVisibility() == 0) PAR_SetEmitterMask(static_cast<int32_t>(emitter), 1);
                }
            }
        } else if (message > 0x10) {
            if (a != 0) {
                if (reinterpret_cast<uintptr_t>(window->getExtraParam(10)) == 0) {
                    UIMain_MidWindowParticles(0x5a, window);
                    window->setExtraParam(10, reinterpret_cast<void*>(static_cast<uintptr_t>(1)));
                }
                int32_t emitter = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9)));
                if (emitter > -1 && PAR_GetEmitterStatus(static_cast<uint32_t>(emitter)) != 0) PAR_SetEmitterMask(emitter, 0);
            } else {
                int32_t emitter = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9)));
                if (emitter > -1 && PAR_GetEmitterStatus(static_cast<uint32_t>(emitter)) != 0) PAR_SetEmitterMask(emitter, 1);
            }
        }
    } else if (message == 0x10006) {
        if (a == 0) {
            UIMain_UISound(0xc0);
            InterfaceWindow* source = gTagPanelSource;
            if (source == window) {
                if (source != nullptr) RemoveSlideoutPanel(0);
            } else {
                ShowTagPanel(window);
            }
        } else if (a == 1) {
            if (window != gTagPanelPending) {
                window->Enable(0);
                window->mF134 = 0;
                InterfaceWindow* source = gTagPanelSource;
                if (source == window && source != nullptr) RemoveSlideoutPanel(0);
                InterfaceWindow* parent = window->GetParent();
                UI_PostMessage(parent, 0x100002, static_cast<int32_t>(reinterpret_cast<intptr_t>(window)), 0);
            }
        }
    } else if (message < 0x10006) {
        if (message == 0x14) {
            int16_t index = static_cast<int16_t>(FindStaticTag(window));
            if (index != -1) {
                gStaticTags[index].mActive = 0;
                gStaticTags[index].mWindow = nullptr;
            }
        } else if (message < 0x14) {
            int32_t emitter = static_cast<int32_t>(reinterpret_cast<intptr_t>(window->getExtraParam(9)));
            CoolRect rect;
            if (emitter > -1 && PAR_GetEmitterStatus(static_cast<uint32_t>(emitter)) != 0) {
                window->GetPosition(&rect, 1);
                int32_t x, y;
                EmitterSpot(rect, x, y);
                PAR_SetEmitterPos(emitter, x, 0, y);
            }
        }
    }
    return window->CallDefaultWindowHandler(message, a, b);
}
