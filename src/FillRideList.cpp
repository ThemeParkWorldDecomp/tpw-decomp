// Unit v5363: the ride list panel's FillRideList (0x101b5c14) and UpdateViewWindow (0x101b5f1c).
#include <cstddef>
#include <cstdint>

#include "InterfaceWindow.h"
#include "LbDraw.h"
#include "LbDrawPrimitives.h"
#include "MainBalance.h"
#include "MainWindowCallback.h"
#include "ObjectControl.h"
#include "ObjectParts.h"
#include "RideDB.h"
#include "TPEngine.h"
#include "TbColour.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "Text.h"
#include "UI_Base.h"
#include "World.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"

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
