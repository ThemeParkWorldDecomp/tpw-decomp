// ui_ridepanel.cpp: name proven by __sinit_ui_ridepanel_cpp in the main application.

#include "analyser.h"
#include "Debug.h"
#include "FillLenderList.h"
#include "History.h"
#include "InterfaceScrollBar.h"
#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "LLCellSystem.h"
#include "MainBalance.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "ParkTime.h"
#include "platform/msl.h"
#include "ppc_math.h"
#include "RideDB.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPEngine.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_iterators.h"
#include "ui_main.h"
#include "ui_tooltip.h"
#include "WindowScripts.inc"
#include "World.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

// Unit v5363: the ride list panel's FillRideList (0x101b5c14) and UpdateViewWindow (0x101b5f1c).

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> RideId;

// The Mac's TbColour in two registers: the five bytes and nothing else.
TbColour MakeColour(const uint8_t* bytes) {
    alignas(TbColour) uint8_t raw[sizeof(TbColour)] = {bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]};
    return *reinterpret_cast<const TbColour*>(raw);
}

CControlManager* ControlManager() {
    return reinterpret_cast<CControlManager*>(reinterpret_cast<char*>(gGameWorld) + 0x1d943c);
}

CGoldenTicketControl* GoldenTickets() {
    return reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<char*>(gGameWorld) + 0x2c0);
}

}  // namespace

// 0x101b5c14 (728 bytes): fills the list (window 0x1f8) of the ride list panel with the rides of one kind, one row each: the name (or the
// golden ticket text and the negated price), the upgrade cost (or the price) and the state (1 in the park, 2 researched last, 0 else). The
// kind 3 (features) gets two more rows, ids -1 and -2. `refresh` empties the list first. Remembers the kind in gRideListType.
void FillRideList(UIObjType type, int32_t refresh) {
    CParkAnalyser* analyser = gGameWorld->GetParkAnalyser();
    if (gRideListPanel == nullptr) return;
    InterfaceListControl2* list = static_cast<InterfaceListControl2*>(gRideListPanel->GetWindow(0x1f8));
    if (list == nullptr) return;
    if (refresh != 0) list->RemoveAllItems();
    RideInfo* ride = gRideDatabase.GetFirstRide();
    gRideListType = type;
    while (ride != nullptr) {
        if (ride->mLoadFailed == 0 && ride->mUIType == type &&
            ride->Value(static_cast<RideBalanceSlot>(158)).mInteger == 0) {
            ObjectControl* control = ControlManager()->GetObjectControl(RideId{ride->mId}, 0);
            if (control->mAvailable != 0) {
                char16_t name[0x42];
                LCWord item[3];
                item[0].mRaw = 0;
                item[1].mRaw = 0;
                item[2].mRaw = 0;
                int32_t cost = ride->Value(kRideSlot_GoldenTicketCost).mInteger;
                if (cost > 0 && !GoldenTickets()->HasBeenBought(RideId{ride->mId})) {
                    item[0].mText = UIMain_GetString(0x89);
                    item[1].mInt = -cost;
                } else {
                    gRideDatabase.GetTypeName(ride->mId, name, 0x42);
                    item[0].mText = name;
                    item[1].mInt = control->mUpgradeCost;
                }
                if (analyser->NumberOfObjectInPark(RideId{ride->mId}) > 0) {
                    item[2].mInt = 1;
                } else if (ride->mId == analyser->LastOfObjectTypeResearched(type, 0).mIndex ||
                           ride->mId == analyser->LastOfObjectTypeResearched(type, 1).mIndex ||
                           ride->mId == analyser->LastOfObjectTypeResearched(type, 2).mIndex) {
                    item[2].mInt = 2;
                } else {
                    item[2].mInt = 0;
                }
                list->AddItem(item, ride->mId, -1, 0);
            }
        }
        ride = gRideDatabase.GetNextRide();
    }
    if (type == kUIObjFeature) {
        LCWord item[3];
        item[0].mRaw = 0;
        item[1].mRaw = 0;
        item[2].mRaw = 0;
        item[0].mText = UIMain_GetString(0x86);
        item[1].mInt = gMainBalance.mValues[(0x5cc - 8) / 4].mInteger;
        item[2].mInt = 0;
        list->AddItem(item, -1, -1, 0);
        item[0].mText = UIMain_GetString(0x87);
        item[1].mInt = 0;
        item[2].mInt = 0;
        list->AddItem(item, -2, -1, 0);
    }
    list->UpdateDisplay();
}

// 0x101b5f1c (1420 bytes): see the header.
void UpdateViewWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    InterfaceWindow* view = gRideListPanel->GetWindow(0x1ea);
    if (view == nullptr) return;
    RideInfo* ride = gRideDatabase.GetRideInfo(id);
    InterfaceWindow* picture = view->GetWindow(0x1eb);
    TbRenderArea area;
    CoolRect unused;
    uint8_t colour[5] = {0, 0, 0, 0, 0};
    CoolRect rect;
    int16_t kind = static_cast<int16_t>(id.mIndex);
    view->GetClientRect(&rect);
    if (kind < 0 || (ride->Value(kRideSlot_GoldenTicketCost).mInteger > 0 && !GoldenTickets()->HasBeenBought(RideId{ride->mId}))) {
        int32_t width, height;
        uint8_t* pixels = picture->GetClientObjectSurface(&width, &height);
        if (pixels != nullptr) {
            MakeRenderArea(pixels, &area, width, height);
            LbDraw_SetCurrent(&area);
            colour[0] = colour[1] = colour[2] = colour[3] = 0;
            TbRect black{0, 0, width - 1, height - 1};
            LbDraw_RectangleFilled(&black, MakeColour(colour));
            picture->FinishWithClientObjectSurface();
        }
        int32_t string;
        int32_t previewId;
        if (kind > 0) {
            previewId = 0x64;
            string = 0x89;
        } else if (kind == -1) {
            previewId = 0x65;
            string = 0x86;
        } else {
            previewId = 0x66;
            string = 0x87;
        }
        RideInfo* preview = gRideDatabase.GetRideInfo(RideId{static_cast<uint16_t>(previewId)});
        UIMain_ShowRidePreview(rect, preview->mRideTypeNumber, view->mZ + 10);
        const char16_t* text = UIMain_GetString(string);
        view->GetWindow(0x1ec)->SetText(const_cast<char16_t*>(text));
    } else {
        RideInfo* open = gRideDatabase.GetRideInfo(id);
        UIMain_ShowRidePreview(rect, open->mRideTypeNumber, view->mZ + 10);
        int32_t width, height;
        uint8_t* pixels = picture->GetClientObjectSurface(&width, &height);
        if (pixels != nullptr) {
            MakeRenderArea(pixels, &area, width, height);
            LbDraw_SetCurrent(&area);
            int32_t* profile = LLCC_GetRideProfileInfo(ride);
            colour[1] = colour[2] = colour[3] = 0;
            colour[0] = 0;
            TbRect black{0, 0, width - 1, height - 1};
            LbDraw_RectangleFilled(&black, MakeColour(colour));
            int32_t cellWidth = width / 8;
            int32_t cellHeight = height / 8;
            for (int32_t row = 0; row < profile[256]; ++row) {
                for (int32_t column = 0; column < profile[257]; ++column) {
                    int32_t cell = profile[row * 16 + column];
                    if (cell == 0) continue;
                    if (cell == 3) {
                        colour[0] = 0x80;
                        colour[1] = 0xdc;
                        colour[2] = 0x64;
                        colour[3] = 0x0f;
                    } else if (cell < 3) {
                        if (cell == 1) {
                            colour[0] = 0x80;
                            colour[1] = 0x1e;
                            colour[2] = 0xaa;
                            colour[3] = 0xff;
                        } else if (cell > 0) {
                            colour[0] = 0x80;
                            colour[1] = 0x0f;
                            colour[2] = 0xdc;
                            colour[3] = 0x32;
                        }
                    } else if (cell < 5) {
                        colour[0] = 0x80;
                        colour[1] = 0x1e;
                        colour[2] = 0xaa;
                        colour[3] = 0xff;
                    }
                    TbRect box{row * cellWidth, height - (column + 1) * cellHeight - 1, (row + 1) * cellWidth,
                               height - column * cellHeight - 1};
                    LbDraw_RectangleFilled(&box, MakeColour(colour));
                }
            }
            colour[0] = 0xc8;
            colour[1] = 0x1e;
            colour[2] = 0xaa;
            colour[3] = 0xff;
            picture->FinishWithClientObjectSurface();
        }
        TbITable* table = static_cast<TbITable*>(gText.mResourcePointers[21]);
        const unsigned char* raw = table->GetConstString(static_cast<uint32_t>(open->Value(kRideSlot_RideTypeStringIndex).mInteger));
        char16_t* text = UIMain_ConvertMulti(raw);
        view->GetWindow(0x1ec)->SetText(text);
    }
}

// Unit v5241: the ride list panel's info window set-up (InitInfoWindow 0x101b6500) and refresh (UpdateInfoWindow 0x101b6a5c).

namespace {

const char16_t kIntegerFormat[] = u"%d";            // 0x1024e43c
const char kGradientName[] = "ridestatbar.wct";     // 0x1024e41a

// value * 1024 / 100 as the listing does it (slwi 10, then the multiply-high division by 100).
int32_t Scaled(int32_t value) { return static_cast<int32_t>(static_cast<uint32_t>(value) << 10) / 100; }

// The ride record's balance slots at Mac offsets 0x13c, 0x19c and 0x1b0 (0x0c + 4 * slot).
int32_t SlotAt(RideInfo* ride, int32_t slot) { return ride->Value(static_cast<RideBalanceSlot>(slot)).mInteger; }

void SetUpText(InterfaceWindow* window, int32_t stringId) {
    InterfaceTextObjectSprite* object = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(object);
    window->SetFont(GetFontTableEntry(6));
    window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

void SetUpBar(InterfaceWindow* panel, int32_t id, int32_t value) {
    InterfaceWindow* window = panel->GetWindow(id);
    UIMain_IndentClient(window, 0, 10);
    InterfaceDrawEngineHGradient* object = new InterfaceDrawEngineHGradient(const_cast<char*>(kGradientName));
    window->SetClientDrawObject(object);
    window->SetValue(value);
}

}  // namespace

void InitInfoWindow() {
    InterfaceWindow* panel = gRideListPanel->GetWindow(0x1ed);
    if (panel == nullptr) return;
    SetUpText(panel->GetWindow(0x1f4), 0x7d);
    SetUpText(panel->GetWindow(0x1f3), 0x7e);
    SetUpText(panel->GetWindow(0x1f1), 0x7f);
    SetUpText(panel->GetWindow(0x1f6), 0x80);
    SetUpText(panel->GetWindow(0x1f7), 0x81);
    SetUpText(panel->GetWindow(0x1f5), 0x88);
    SetUpText(panel->GetWindow(0x1f2), 0x88);
    SetUpBar(panel, 0x1ee, 500);
    SetUpBar(panel, 0x1ef, 200);
    SetUpBar(panel, 0x1f0, 100);
}

void UpdateInfoWindow(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    char16_t text[16];     // r1+0x3c
    InterfaceWindow* panel = gRideListPanel->GetWindow(0x1ed);
    if (panel == nullptr) return;
    int16_t key = static_cast<int16_t>(id.mIndex);
    if (key < 0) return;
    RideInfo* ride = gRideDatabase.GetRideInfo(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>{id.mIndex});
    int32_t count = gGameWorld->GetParkAnalyser()->NumberOfObjectInPark(id);
    if (count != 0) {
        swprintf(text, 10, kIntegerFormat, count);
    } else {
        swprintf(text, 10, UIMain_GetString(0x88));
    }
    InterfaceWindow* countWindow = panel->GetWindow(0x1f5);
    if (countWindow != nullptr) countWindow->SetClientDrawText(text);
    switch (gRideListType) {
        case 0: {
            InterfaceWindow* window = panel->GetWindow(0x1ee);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 76)));
            window = panel->GetWindow(0x1f2);
            if (window != nullptr) {
                swprintf(gUIMainConvertBuffer, 0x3fff, kIntegerFormat, SlotAt(ride, 100));
                window->SetText(gUIMainConvertBuffer);
            }
            window = panel->GetWindow(0x1ef);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 105)));
            break;
        }
        case 2: {
            InterfaceWindow* window = panel->GetWindow(0x1ee);
            if (window != nullptr) window->SetValue(Scaled(SlotAt(ride, 76)));
            break;
        }
        default:
            break;
    }
}

// Unit v5102: the ride list panel's info window (ChangeInfoWindow 0x101b7108) and help line (UpdateHelpText 0x101b6d18).

int32_t gHelpTextState = -1;   // 0x102acee0 (name invented)

namespace {

// GetWindow of the info window, then Show when it is there.
void ShowChild(InterfaceWindow* info, int32_t id, int16_t on) {
    InterfaceWindow* child = info->GetWindow(id);
    if (child != nullptr) child->Show(on);
}

}  // namespace

void UpdateHelpText(tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t>& id) {
    char16_t text[0x80];     // r1+0xdc; left unset for a negative id other than -1 and -2, as on the Mac
    char16_t name[0x40];     // r1+0x5c
    InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
    CoolRect rect;
    window->GetClientRect(&rect);
    int16_t mouseX, mouseY;
    UI_GetMouseXY(mouseX, mouseY);
    if (!rect.PointIn(mouseX, mouseY)) {
        gHelpTextState = -1;
        return;
    }
    int16_t key = static_cast<int16_t>(id.mIndex);
    if (key < 0) {
        if (static_cast<uint16_t>(gRideListF88) == id.mIndex) return;
        gHelpTextState = 4;
        gRideListF88 = static_cast<int16_t>(id.mIndex);
        if (key == -1) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x21a);
        } else if (key == -2) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x21b);
        }
    } else {
        ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
        int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
        RideInfo* ride = gRideDatabase.GetRideInfo(id);
        int32_t state;
        if (ride->Value(kRideSlot_GoldenTicketCost).mInteger == 0 ||
            gGameWorld->mParkTime.GoldenTickets()->HasBeenBought(id)) {
            state = (control->mUpgradeCost > balance) ? 2 : 3;
        } else {
            state = gGameWorld->mParkTime.GoldenTickets()->CanIAfford(id) ? 0 : 1;
        }
        if (state == gHelpTextState && static_cast<uint16_t>(gRideListF88) == id.mIndex) return;
        gHelpTextState = state;
        gRideListF88 = static_cast<int16_t>(id.mIndex);
        if (state == 2 || state == 3) {
            ride = gRideDatabase.GetRideInfo(id);
            static_cast<TbITable*>(gText.mResourcePointers[21])
                ->FormatString(name, 0x3f, static_cast<uint32_t>(ride->Value(kRideSlot_RideTypeStringIndex).mInteger));
            TbUniStringVTag tag(0, name);
            static_cast<TbITable*>(gText.mResourcePointers[7])
                ->FormatString(text, 0x80, state == 2 ? 0x96 : 0x94, &tag);
        } else if (state == 0) {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x97);
        } else {
            static_cast<TbITable*>(gText.mResourcePointers[7])->FormatString(text, 0x80, 0x98);
        }
    }
    SetHelpTextString(text);
}
void ChangeInfoWindow(UIObjType type) {
    InterfaceWindow* info = gRideListPanel->GetWindow(0x1ed);
    if (info == nullptr) return;
    int32_t kind = static_cast<int32_t>(type);
    if (kind == 2) {
        ShowChild(info, 0x1f3, 1);
        ShowChild(info, 0x1f1, 0);
        ShowChild(info, 0x1f6, 0);
        ShowChild(info, 0x1f6, 0);   // asked twice
        ShowChild(info, 0x1f7, 0);
        ShowChild(info, 0x1ee, 1);
        ShowChild(info, 0x1f2, 0);
        ShowChild(info, 0x1ef, 0);
        ShowChild(info, 0x1f0, 0);
    } else if (kind == 0 || kind == 1 || kind == 3) {
        int16_t on = (kind == 0) ? 1 : 0;
        ShowChild(info, 0x1f3, on);
        ShowChild(info, 0x1f1, on);
        ShowChild(info, 0x1f6, on);
        ShowChild(info, 0x1f7, on);
        ShowChild(info, 0x1ee, on);
        ShowChild(info, 0x1f2, on);
        ShowChild(info, 0x1ef, on);
        ShowChild(info, 0x1f0, on);
    }
}

// RidePanelMsg 0x101b7538 (2084 bytes): the message handler of the ride list panel (unit v51005).

namespace {

typedef tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> RideTag;

// The mouse is made on first use (the same code is inlined everywhere in the original): the mode is assigned to it afterwards.
void SetMouseMode(CMouseMode* mode) {
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse();
        CMMDefault* normal = new CMMDefault();
        mouse->AssignMouseMode(normal);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(mode);
}

// The tail the two ways of placing a ride share (the ride is chosen and affordable).
int32_t PlaceRide(CMMPathDraw* draw, RideTag id) {
    SetMouseMode(draw);
    RemoveRideListPanel();
    RideInfo* ride = gRideDatabase.GetRideInfo(id);
    if (ride->mBalance.mValues[kRideSlot_WhichUIType].mInteger == 0) {
        if (UI_IterateThingNext(kUIIterRides, nullptr) == nullptr)
            UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xc9));
        else
            UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xce));
    } else {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xce));
    }
    return 0;
}

}  // namespace

int32_t RidePanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x1e) {
        RideTag& view = reinterpret_cast<RideTag&>(gRideListF8C);
        if (static_cast<uint16_t>(gRideListF8C) != gRideListPending) {
            if (static_cast<int32_t>(IveGotTicks()) - gRideListF90 > 500) {
                UpdateViewWindow(view);
                gRideListF8C = 0;
            }
        }
        UIMain_TickRidePreview();
        if (static_cast<uint16_t>(gRideListF8A) != gRideListHelpShown)
            UpdateHelpText(reinterpret_cast<RideTag&>(gRideListF8A));
    } else if (message < 0x1e) {
        if (message == 0x10) {
            if (a == 0x80080) {
                InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0x1f8));
                list->UpdateDisplay();
                InterfaceWindow* title = window->GetWindow(0x200);
                int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                TbITable* strings = static_cast<TbITable*>(gText.mResourcePointers[6]);
                if (balance < 0) {
                    ShowStatusTag tag(0xd, static_cast<int32_t>(labs(balance)));
                    strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, &tag);
                } else {
                    ShowStatusTag tag(0xd, balance);
                    strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, &tag);
                }
                title->SetText(gUIMainConvertBuffer);
            }
        } else if (message < 0x10) {
            if (message == 5) {
                RemoveRideListPanel();
                return 1;
            }
        } else if (message == 0x15) {
            InitInfoWindow();
            ChangeInfoWindow(static_cast<UIObjType>(gRideListType));
            if (window->setAnimTimer(0x80080, 1000) != 0) window->startAnimTimer(0x80080);
        } else if (message < 0x15 && message > 0x13) {
            gRideListPanel = nullptr;
            UIMain_DestroyRidePreview();
            UIMain_CancelAdvisorMessage(static_cast<AdvisorMessage>(0xbc));
        }
    } else if (message == 0x401) {
        InterfaceWindow* found = window->GetWindow(a);
        if (found != nullptr && b != -1) {
            int32_t data;
            static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
            RideTag id{static_cast<uint16_t>(data)};
            if (id.mIndex != static_cast<uint16_t>(gRideListF8C)) {
                gRideListF8C = static_cast<int16_t>(id.mIndex);
                gRideListF90 = static_cast<int32_t>(IveGotTicks());
            }
            gRideListF8A = static_cast<int16_t>(id.mIndex);
            UpdateInfoWindow(id);
        }
    } else if (message < 0x401) {
        if (message == 0x100) {
            if (a == 0x1ff) {
                CreateHireListPanel(static_cast<StaffType>(-1));
                return 0;
            }
            if (a < 0x1ff && a == -2) {
                RemoveRideListPanel();
                return 0;
            }
        } else if (message > 0xff && message > 0x3ff) {
            InterfaceWindow* found = window->GetWindow(a);
            if (found != nullptr && b != -1) {
                int32_t balance = gGameWorld->GetBankAccount()->GetBalance();
                int32_t data;
                static_cast<InterfaceListControl2*>(found)->GetData(b, &data);
                if (data < 1) {
                    if (data == -1) {
                        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x39));
                        SetMouseMode(draw);
                        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x15a));
                    } else if (data < -1 && data > -3) {
                        CMMPathDraw* draw = new CMMPathDraw(static_cast<CELL_MODES>(0x3a));
                        SetMouseMode(draw);
                        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0x159));
                    }
                    RemoveRideListPanel();
                    return 0;
                }
                RideTag id{static_cast<uint16_t>(data)};
                ObjectControl* control = gGameWorld->GetControlManager()->GetObjectControl(id, 0);
                RideInfo* ride = gRideDatabase.GetRideInfo(id);
                CGoldenTicketControl* tickets =
                    reinterpret_cast<CGoldenTicketControl*>(reinterpret_cast<uintptr_t>(gGameWorld) + 0x2c0);
                if (ride->mBalance.mValues[kRideSlot_GoldenTicketCost].mInteger == 0 || tickets->HasBeenBought(id)) {
                    if (control->mUpgradeCost <= balance)
                        return PlaceRide(new CMMPathDraw(static_cast<CELL_MODES>(4), id), id);
                    UIMain_NotEnoughDough();
                } else if (tickets->CanIAfford(id)) {
                    return PlaceRide(new CMMPathDraw(static_cast<CELL_MODES>(4), id), id);
                } else {
                    UIMain_NotEnoughDough();
                }
            }
        }
    } else if (message == 0x406) {
        gRideListSortColumn = b;
    }
    return UIMain_DefaultPanelMsg(window, message, a, b);
}

// CreateRideListPanel 0x101b8434 (1936 bytes, unit v5010): the ride list panel. When the panel is up it is only refilled and the list
// and the type buttons are put right; otherwise it is built from the script, its title, picture, text and list are set up, and the
// advisor is told (message 0xbc).

// The globals no other file names (BSS, and the data the code reads).
int32_t gRideListType = 0;              // 0x10395994
InterfaceWindow* gRideListPanel = nullptr;   // 0x10395998
uint16_t gRideListNoneA = 0;            // 0x102aceec
uint16_t gRideListNoneB = 0;            // 0x102aceee
int32_t gRideListButtonFont = 8;        // 0x102acedc
int32_t gRideListSortColumn = 1;        // 0x102acee4
int16_t gRideListScript[] = {TPW_WINDOW_SCRIPT_102ACA9C};   // 0x102aca9c: the image's words (generated, devtools/gen_game_tables.py)
int32_t gRideListF90 = 0;               // 0x10395990
int16_t gRideListF8C = 0;               // 0x1039598c
int16_t gRideListF8A = 0;               // 0x1039598a
int16_t gRideListF88 = 0;               // 0x10395988

namespace {

// The type buttons (window 0x1f9): the check button of the type is made current (type 2 0x1fa, 0 0x1fb, 1 0x1fd, 3 0x1fc, others none).
void SetTypeButton(InterfaceWindow* buttons, int32_t type) {
    if (buttons == nullptr) return;
    InterfaceCheckButtons* check = static_cast<InterfaceCheckButtons*>(buttons);
    if (type == 2) {
        check->SetCurrent(0x1fa);
    } else if (type < 2) {
        if (type == 0) check->SetCurrent(0x1fb);
        else if (type >= 0) check->SetCurrent(0x1fd);
    } else if (type < 4) {
        check->SetCurrent(0x1fc);
    }
}

// The list shows the entry with the selected id (a 16-bit value, compared as unsigned) unless it is the "none" id.
void SelectEntry(InterfaceListControl2* list, uint16_t selected, uint16_t none) {
    if (selected == none) return;
    int32_t position = list->FindData(static_cast<int32_t>(selected));
    if (position >= 0) list->SetCurrent(position);
}

}  // namespace

void CreateRideListPanel(UIObjType type, tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> selectedTag) {
    int32_t kind = static_cast<int32_t>(type);
    const uint16_t selected = selectedTag.mIndex;
    SetLastPanel(1, 1);
    if (gRideListPanel != nullptr) {
        if (kind == -1) kind = gRideListType;
        FillRideList(static_cast<UIObjType>(kind), 1);
        InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
        if (window != nullptr) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
            SetTypeButton(list->GetWindow(0x1f9), kind);
            SelectEntry(list, selected, gRideListNoneA);
        }
        return;
    }

    RemoveSlideoutPanel(1);
    UIMain_CheckPanel();
    gRideListF90 = 0;
    gRideListF8C = 0;
    gRideListF8A = 0;
    gRideListF88 = 0;
    gRideListPanel = gGameWindow->Build(gRideListScript, RidePanelMsg);
    if (gRideListPanel != nullptr) {
        UIMain_SetPanel(gRideListPanel);
        InterfaceWindow* title = gRideListPanel->GetWindow(0x200);
        int32_t balance = static_cast<CWorld*>(gGameWorld)->GetBankAccount()->GetBalance();
        TbITable* strings = static_cast<TbITable*>(gText.mResourcePointers[6]);
        if (balance < 0) {
            TbIntegerVTag tag(0xd, static_cast<int32_t>(0u - static_cast<uint32_t>(balance)));   // labs
            strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1cb, &tag);
        } else {
            TbIntegerVTag tag(0xd, balance);
            strings->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1ca, &tag);
        }
        InterfaceTitleTextObject* titleObject = new InterfaceTitleTextObject(nullptr);
        title->SetDrawObject(titleObject);
        title->SetFontJust(2, 1);
        title->SetFont(GetFontTableEntry(5));
        title->SetFontColour(0xff, 0xff, 0xff, 0xff);
        title->SetText(gUIMainConvertBuffer);

        InterfaceWindow* picture = gRideListPanel->GetWindow(0x1ea)->GetWindow(0x1eb);
        InterfaceDrawObjectSprite* sprite = new InterfaceDrawObjectSprite(nullptr);
        picture->SetClientDrawObject(sprite);
        picture->setZ(static_cast<int16_t>(picture->mZ + 0xc));

        InterfaceWindow* label = gRideListPanel->GetWindow(0x1ea)->GetWindow(0x1ec);
        InterfaceTextObjectSprite* textObject = new InterfaceTextObjectSprite(nullptr);
        label->SetClientDrawObject(textObject);
        label->SetFontJust(1, 1);
        label->SetFontWrap(1, 1);
        label->SetFont(GetFontTableEntry(6));
        label->SetFontColour(0xff, 0xff, 0xff, 0xff);
        label->SetClientDrawText(nullptr);
        label->setZ(static_cast<int16_t>(label->mZ + 0xc));

        InterfaceWindow* window = gRideListPanel->GetWindow(0x1f8);
        if (window != nullptr) {
            InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window);
            list->SetCallbackFunction(RideListLCHandler);
            list->SetColumnFormatsAll(RideListLCFormat, RideListLCPostFormat);
            list->SetColumnType(0, kLCText);
            list->SetColumnType(1, kLCNumber);
            list->SetColumnType(2, kLCNumber);
            list->SetSelectionObject(DrawObjectLibrary::FindDrawObject(const_cast<char*>("hilight")));
            list->SetSort(gRideListSortColumn, false);
            InterfaceWindow* button = list->GetWindow(0x10);
            char16_t* text = const_cast<char16_t*>(UIMain_GetString(0x7b));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x90);
            button = list->GetWindow(0x11);
            text = const_cast<char16_t*>(UIMain_GetString(0x7c));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x91);
            button = list->GetWindow(0x12);
            text = const_cast<char16_t*>(UIMain_GetString(0x8a));
            MAKECOLUMNBUTTON(button, text, gRideListButtonFont, 0xff, 0xef, 0, 0x92);
            if (kind == -1) kind = gRideListType;
            SetTypeButton(list->GetWindow(0x1f9), kind);
            FillRideList(static_cast<UIObjType>(kind), 0);
            int32_t titleString = 0;
            if (kind == 2) titleString = 0x79;
            else if (kind < 2) {
                if (kind == 0) titleString = 0x77;
                else if (kind >= 0) titleString = 0x78;
            } else if (kind < 4) {
                titleString = 0x7a;
            }
            if (titleString != 0)
                UIMain_SetTitleText(gRideListPanel, 0x1fe, const_cast<char16_t*>(UIMain_GetString(titleString)));
            SelectEntry(list, selected, gRideListNoneB);
        }
    }
    UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xbc));
}

// ui_ridepanel.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: RemoveRideListPanel (0x101b8c28, 56 bytes).

// Message 4 to the ride list panel (sent to NULL as well when there is none, as the Mac does).
void RemoveRideListPanel() { UI_SendMessage(gRideListPanel, 4, 0, 0); }

// Unit v51121: the small functions of ui_ridepanel.cpp (RideStatusPanel, ShopStatusPanel), ui_StaffCostsPanel.cpp, the research panel and
// the tank panel message function.

void __sinit_ui_ridepanel_cpp() { gRideListF88 = 0; }                 // 0x101b8c8c
