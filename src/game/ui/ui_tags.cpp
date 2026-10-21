// ui_tags.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of free functions, named after what they do.

#include "analyser.h"
#include "Debug.h"
#include "InterfaceWindow.h"
#include "LbFile.h"
#include "LLCellSystem.h"
#include "MainWindowCallback.h"
#include "online.h"
#include "platform/msl.h"
#include "rsse.h"
#include "ThingReceivers.h"
#include "TPSound.h"
#include "UI_Base.h"
#include "ui_DrawObject.h"
#include "ui_main.h"
#include "World.h"
#include <cstdint>
#include <cstring>
#include <new>

// ui_StatusPanel.cpp (main): InitTagPanel 0x10190350 (272 B), DeInitTagPanel 0x10190484 (76 B),
// CreateStaticTags 0x10191b28 (240 B), DestroyStaticTags 0x10191c40 (140 B), InstantRemoveAllTags 0x10192280 (112 B).

InterfaceWindow* gTagPanel;        // 0x1038cd78
InterfaceWindow* gTagPanelSource;  // 0x1038cd80
sStaticTag gStaticTags[10];        // 0x1038cd84, Mac stride 0x208

void InitTagPanel() {
    gTagPanel = gUIRootWindow->Build(gTagPanelScript, TagDlgCallback);
    gTagPanel->unlink();
    gTagPanel->setExtraParam(0, reinterpret_cast<void*>(1));
    gTagPanel->Show(0);
    InterfaceWindow* textWindow = gTagPanel->GetWindow(0x10d32);
    if (textWindow != nullptr) {
        InterfaceTextObjectSprite* sprite = new (std::nothrow) InterfaceTextObjectSprite(nullptr);
        textWindow->SetClientDrawObject(sprite);
        InterfaceFont* font = GetFontTableEntry(6);
        textWindow->SetFont(font);
        textWindow->SetFontWrap(1, 1);
        textWindow->SetFontJust(0, 0);
    }
}

void DeInitTagPanel() {
    if (gTagPanel != nullptr) UI_SendMessage(gTagPanel, 4, 0, 0);
    gTagPanelSource = nullptr;
}

// ui_StatusPanel_SetButton.cpp (main): SetButton 0x101904f8 (1316 B), DoButton 0x10190a60 (588 B).

namespace {
// The Mac's string pool after "mb_locate" (0x1024dd34): the names SetButton looks up are at these offsets.
const char kNames[] =
    "mb_locate\0mb_buy\0b_loan\0mb_resrch\0b_pkinfo\0b_staffcost\0mb_callmech\0b_entry\0mb_accept\0mb_decline";
}  // namespace

void SetButton(InterfaceWindow* window, TagButtonAction action) {
    const char* name;
    int32_t help;   // -1: leaves mHelpText alone
    switch (static_cast<int32_t>(action)) {
        case 3: name = kNames + 0x00; help = 0x204; break;
        case 4: name = kNames + 0x00; help = 0x203; break;
        case 9: name = kNames + 0x0a; help = 0x209; break;
        case 10: name = kNames + 0x0a; help = 0x20a; break;
        case 11: name = kNames + 0x0a; help = 0x20b; break;
        case 12: name = kNames + 0x0a; help = 0x20c; break;
        case 13: name = kNames + 0x0a; help = 0x20d; break;
        case 14: name = kNames + 0x0a; help = 0x20e; break;
        case 15: name = kNames + 0x0a; help = 0x20f; break;
        case 16: name = kNames + 0x0a; help = 0x210; break;
        case 17: name = kNames + 0x0a; help = 0x211; break;
        case 18: name = kNames + 0x11; help = 0x212; break;
        case 19: name = kNames + 0x18; help = 0x213; break;
        case 20: name = kNames + 0x22; help = 0x214; break;
        case 21: name = kNames + 0x2b; help = 0x215; break;
        case 22: return;   // the jump table's entry 19 goes straight to the epilogue
        case 23: name = kNames + 0x37; help = 0x217; break;
        case 24: name = kNames + 0x43; help = 0xec; break;
        case 26: name = kNames + 0x4b; help = -1; break;
        case 27: name = kNames + 0x55; help = -1; break;
        default:
            window->SetDrawObject(nullptr);
            window->Enable(0);
            return;
    }
    window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>(name)));
    window->Enable(1);
    if (help != -1) window->mHelpText = help;
}

int32_t DoButton(TagButtonAction action, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id, int32_t extra) {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> ride;
    ride.mIndex = static_cast<uint16_t>(extra);
    switch (static_cast<int32_t>(action)) {
        case 3:
            UIMain_ZoomToThing(id);
            return 0;
        case 4: {
            tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> map;
            map.mIndex = static_cast<uint16_t>(extra);
            UIMain_ZoomToMapId(map);
            return 0;
        }
        case 9: CreateRideListPanel(static_cast<UIObjType>(0), ride); return 2;
        case 10: CreateRideListPanel(static_cast<UIObjType>(1), ride); return 2;
        case 11: CreateRideListPanel(static_cast<UIObjType>(2), ride); return 2;
        case 12: CreateRideListPanel(static_cast<UIObjType>(3), ride); return 2;
        case 13: CreateHireListPanel(static_cast<StaffType>(0)); return 2;
        case 14: CreateHireListPanel(static_cast<StaffType>(1)); return 2;
        case 15: CreateHireListPanel(static_cast<StaffType>(2)); return 2;
        case 16: CreateHireListPanel(static_cast<StaffType>(3)); return 2;
        case 17: CreateHireListPanel(static_cast<StaffType>(4)); return 2;
        case 18: CreateLoansPanel(); return 2;
        case 19: CreateResearchPanel(); return 2;
        case 20: CreateParkStatusPanel(); return 2;
        case 21: CreateStaffCostsPanel(); return 2;
        case 22: CreateYearEndPanel(); return 2;
        case 23:
            GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
            static_cast<CObject*>(gThingArray[id.mIndex].mThing)->RequestMaintenance(1);
            return 1;
        case 24: CreateEntryPricePanel(); return 2;
        case 26: gGameWorld->GetChallengeHQ()->ChallengeAccepted(); return 2;
        case 27: gGameWorld->GetChallengeHQ()->ChallengeDeclined(); return 2;
        default: return 0;
    }
}

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

void CreateStaticTags() {
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        tag.mActive = 0;
        InterfaceWindow* window = new (std::nothrow) InterfaceWindow(nullptr, 0, i + 900, 0, 0, 70, 65);
        if (window != nullptr) {
            window->SetDrawObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("msgtag")));
            window->SetCallbackFunction(TagCallback);
            window->mHelpText = 0x1db;
        }
        tag.mWindow = window;
        if (window != nullptr) tag.mWindow->setExtraParam(2, tag.mPayload);
    }
}

void DestroyStaticTags() {
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        if (tag.mActive == 0 && tag.mWindow != nullptr) {
            delete tag.mWindow;
            tag.mWindow = nullptr;
        }
    }
}

// FindStaticTag 0x10191cf4 and AddTag 0x101921f4 (unit v5249).

// The index of the first static tag that holds the window, -1 for none. The index is a 16-bit counter.
int32_t FindStaticTag(InterfaceWindow* window) {
    int32_t result = -1;
    int16_t found = 0;
    for (int16_t i = 0; i < 10 && found == 0; ++i) {
        if (gStaticTags[i].mWindow == window) {
            found = 1;
            result = i;
        }
    }
    return result;
}

// ui_StatusPanel_FindStaticTag_OverrideTag.cpp (main): OverrideTag 0x10191d7c (332 B), TagDlgCallback 0x10191628 (1220 B).

// 0x1038cd7c, in bss: set elsewhere (the window to slide in); the Mac's word is zero at start.
InterfaceWindow* gTagPanelPending;
// 0x102a83b2: the frames TagDlgCallback slides the panel with (the image's data): it grows three steps, shrinks three, then again.
UI_RectDelta gTagSlideAnim[7] = {
    {-2, -2, 2, 2}, {-2, -2, 2, 2}, {-2, -2, 2, 2},
    {2, 2, -2, -2}, {2, 2, -2, -2}, {2, 2, -2, -2},
    {kAnimGoto, 0, 0, 0},
};
// 0x102a83ea: the frames TagCallback (src/ui_StatusPanel_SetButton_ShowTagPanel.cpp) shifts the other tags with (the image's data).
UI_RectDelta gTagShiftAnim[15] = {
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {-2, 2, 2, 0}, {-2, 2, 2, 0},
    {4, -4, -4, 0}, {4, -4, -4, 0}, {-2, 2, 2, 0},
    {-2, 2, 2, 0}, {2, -2, -2, 0}, {2, -2, -2, 0},
    {2, -2, -2, 0}, {2, -2, -2, 0}, {kAnimEnd, 0, 0, 0},
};

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

// ui_StatusPanel_SaveTags.cpp (main): AddATag 0x10191ef8 (720 B), SaveTags 0x1019231c (520 B), LoadTags 0x10192704
// (440 B), the status panel's tags, translated apart from ui_StatusPanel.cpp so that the unit libraries compiling that
// file need none of these callees.

// Unit v5001: AddATag 0x10191ef8 (720 B), SaveTags 0x1019231c (520 B), LoadTags 0x10192704 (440 B).

int32_t gMaxTags = 10;                 // 0x102a8464
int32_t gTagCount;                     // 0x1038cd74
InterfaceWindow* gTagFirstWindow;      // 0x103949d0

namespace {
// The file's record of one tag, 0x21c bytes (the file is little-endian: the Mac swaps every field with SamsUtilities::USwap*,
// which are no-ops on this host).
struct TagFileRecord {
    uint32_t mId;          // 0x000
    char16_t mText[256];   // 0x004
    int32_t mF204;         // 0x204
    int16_t mF208;         // 0x208
    int32_t mF20C;         // 0x20c
    int32_t mF210;         // 0x210
    uint16_t mF214;        // 0x214
    int32_t mF218;         // 0x218
};
static_assert(sizeof(TagFileRecord) == 0x21c, "tag record");
int32_t Low32(void* p) { return static_cast<int32_t>(reinterpret_cast<intptr_t>(p)); }
void* Extend(int32_t v) { return reinterpret_cast<void*>(static_cast<intptr_t>(v)); }
}  // namespace

InterfaceWindow* AddATag(CTagParams& params) {
    InterfaceWindow* result = nullptr;
    if (gGameWindow == nullptr) return nullptr;
    if (!OverrideTag(params) || gTagCount >= gMaxTags || gAerialWindow == nullptr) return result;
    int16_t width;   // never read back
    gAerialWindow->GetDims(&width, nullptr, 1);
    InterfaceWindow* window = nullptr;
    int16_t frame = params.mFrame;
    bool found = false;
    for (int32_t i = 0; i < 10 && !found; ++i) {
        if (gStaticTags[i].mActive == 0) {
            found = true;
            gStaticTags[i].mActive = 1;
            window = gStaticTags[i].mWindow;
            window->SetFrame(frame);
        }
    }
    if (window == nullptr) return result;
    if (gTagFirstWindow == nullptr) gTagFirstWindow = window;
    char16_t* dest = static_cast<char16_t*>(window->getExtraParam(2));
    if (params.mText != nullptr && wcslen(params.mText) != 0 && dest != nullptr) {
        wcsncpy(dest, params.mText, 0xff);
        dest[0xff] = 0;
    }
    if (params.mTime > 0) {
        window->setAnimTimer(0x80080, params.mTime);
        window->startAnimTimer(0x80080);
        window->mF134 = 1;
        window->setAnimTimer(0x80081, params.mTime - 5000);
        window->startAnimTimer(0x80081);
    }
    window->setExtraParam(1, Extend(static_cast<int32_t>(params.mId)));
    window->setExtraParam(3, Extend(params.mF1C));
    window->setExtraParam(5, Extend(params.mF14));
    window->setExtraParam(6, Extend(params.mF18));
    window->setExtraParam(4, Extend(params.mFrame));
    window->setExtraParam(8, Extend(params.mF20));
    window->setExtraParam(7, Extend(params.mTime));
    window->setExtraParam(9, Extend(-1));
    window->setExtraParam(10, nullptr);
    UI_PostMessage(gAerialWindow, 0x100001, Low32(window), 0);
    ++gTagCount;
    SOUND_SendEvent(0, gSoundBanks[3], 0x1e, 0, 0, 0);
    return window;
}

bool AddTag(CTagParams& params) {
    if (params.mShow == 1) {
        InterfaceWindow* window = AddATag(params);
        if (window == nullptr) return false;
        ShowTagPanel(window);
        return true;
    }
    return AddATag(params) != nullptr;
}

void InstantRemoveAllTags() {
    for (int32_t i = 0; i < 10; ++i) {
        if (gStaticTags[i].mActive != 0) UI_SendMessage(gStaticTags[i].mWindow, 5, 0, 0);
    }
}

void SaveTags(void* file) {
    int32_t count = 0;
    for (int32_t i = 0; i < 10; ++i) {
        if (gStaticTags[i].mActive != 0) ++count;
    }
    uint32_t transferred = 0;
    LbFile_Write(file, &count, 4, &transferred);
    for (int32_t i = 0; i < 10; ++i) {
        sStaticTag& tag = gStaticTags[i];
        if (tag.mActive == 0) continue;
        InterfaceWindow* window = tag.mWindow;
        TagFileRecord record;
        std::memset(&record, 0, sizeof record);
        record.mId = static_cast<uint32_t>(Low32(window->getExtraParam(1)));
        record.mF204 = Low32(window->getExtraParam(7));
        record.mF208 = static_cast<int16_t>(Low32(window->getExtraParam(4)));
        record.mF20C = Low32(window->getExtraParam(5));
        record.mF210 = Low32(window->getExtraParam(6));
        record.mF214 = static_cast<uint16_t>(Low32(window->getExtraParam(3)));
        record.mF218 = Low32(window->getExtraParam(8));
        wcscpy(record.mText, reinterpret_cast<char16_t*>(tag.mPayload));
        LbFile_Write(file, &record, 0x21c, &transferred);
    }
}

void LoadTags(void* file) {
    CTagParams params;
    params.mId = 0x17f;
    params.mF4 = 0x17f;
    params.mText = nullptr;
    params.mTime = 0;
    params.mFrame = 0;
    params.mF14 = 0;
    params.mF18 = 0;
    params.mF1C = 0;
    params.mF20 = 0;
    params.mShow = 0;
    int32_t count = 0;
    uint32_t transferred = 0;
    LbFile_Read(file, &count, 4, &transferred);
    while (count != 0) {
        TagFileRecord record;
        LbFile_Read(file, &record, 0x21c, &transferred);
        params.mId = record.mId;
        params.mText = record.mText;
        params.mTime = record.mF204;
        params.mFrame = record.mF208;
        params.mF14 = record.mF20C;
        params.mF18 = record.mF210;
        params.mF1C = record.mF214;
        params.mF20 = record.mF218;
        if (params.mShow == 1) {   // never true: the byte is set to 0 above and nothing writes it
            InterfaceWindow* added = AddATag(params);
            if (added != nullptr) ShowTagPanel(added);
        } else {
            AddATag(params);
        }
        --count;
    }
}

// ShowVersionNumberTag 0x101928dc and delme 0x10193084 (ui_StatusPanel.cpp).

// 0x1024dde8: the version tag's format string (wide); starts "Ve" in the original, the rest is not known here.
char16_t gVersionNumberFormat[32] = u"Version %d.%d";

// Makes a tag from the format string with 2 and 0 and shows its panel.
void ShowVersionNumberTag() {
    char16_t text[16];
    CTagParams params = {};
    params.mId = 0x17e;
    params.mF4 = 0x17e;
    swprintf(text, 0x10, gVersionNumberFormat, 2, 0);
    params.mText = text;
    params.mTime = 0;
    params.mShow = 1;
    if (params.mShow == 1) {
        InterfaceWindow* window = AddATag(params);
        if (window != nullptr) ShowTagPanel(window);
    } else {
        AddATag(params);
    }
}
