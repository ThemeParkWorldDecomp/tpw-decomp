// The staff panel's status and location leaves (unit v5076): UpdateStatus 0x101bf5c4, UpdateLocator 0x101bf798, StaffLocationSwitch 0x101bf9f4,
// hackfollowmode 0x101bfb48. The thing is a staff member when its kind byte is 4 to 8 (the original tests 4, 5, 6, 8, 7 in turn).
#include <cstdint>

#include "InterfaceWindow.h"
#include "TPCamera.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_main.h"

int8_t gStaffPanelFrameGuard;   // 0x10395a2c
int32_t gStaffPanelFrame;       // 0x10395a30
int32_t gStaffPanelLocation;    // 0x10395a34

namespace {

bool IsStaffKind(const CThing* thing) {
    uint8_t kind = thing->mKind;
    return kind == 4 || kind == 5 || kind == 6 || kind == 8 || kind == 7;
}

// The words of other kinds of thing that the panel reads (the classes are not declared): +0x30 and +0x1d8, 32-bit words.
int32_t ThingWord(const CThing* thing, int32_t offset) {
    return *reinterpret_cast<const int32_t*>(reinterpret_cast<const char*>(thing) + offset);
}

}  // namespace

// 0x101bf5c4 (432 bytes)
void UpdateStatus() {
    if (gStaffPanelFrameGuard == 0) {
        gStaffPanelFrame = 9;
        gStaffPanelFrameGuard = 1;
    }
    CThing* thing = gStaffPanelThing;
    if (thing == nullptr) return;
    if (IsStaffKind(thing)) {
        int32_t state = reinterpret_cast<CStaff*>(thing)->GetGeneralStaffState();
        int32_t frame = UIMain_GetStaffStateFrame(state);
        if (frame != gStaffPanelFrame) {
            gStaffPanelFrame = frame;
            gStaffPanelC->GetWindow(0x148)->SetFrame(gStaffPanelFrame);
        }
    } else {
        int32_t value = ThingWord(thing, 0x30);
        if (value != 0)
            gStaffPanelC->GetWindow(0x14a)->SetValue(value - 1);
        else
            gStaffPanelC->GetWindow(0x14a)->SetValue(0x29a);
    }
}

// 0x101bf798 (464 bytes)
void UpdateLocator() {
    if (gStaffPanelThing == nullptr) return;
    InterfaceWindow* window = gStaffPanelC->GetWindow(0x147);
    CThing* thing = gStaffPanelThing;
    if (IsStaffKind(thing)) {
        window->SetText(const_cast<char16_t*>(reinterpret_cast<CStaff*>(thing)->GetName()));
    } else {
        int32_t value = ThingWord(thing, 0x1d8);
        const TbITable* table = static_cast<const TbITable*>(gText.mResourcePointers[6]);
        if (value == 0) {
            table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c9);
        } else {
            TbIntegerVTag tag(0x18, value);
            table->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1c8, &tag);
        }
        window->SetText(gUIMainConvertBuffer);
    }
    UpdateStatus();
    UIMain_ZoomToPeep(reinterpret_cast<CPeep*>(gStaffPanelThing));
}

// 0x101bf9f4 (296 bytes)
void StaffLocationSwitch(int32_t which) {
    if (gStaffPanelLocation == 0) {
        gStaffPanelLocation = which;
        GetNextStaffMember();
        if (gStaffPanelThing != nullptr) {
            gMainPanel->link(gStaffPanelC, 1, 1);
            gStaffPanelC->Show(1);
            UpdateLocator();
            gFirstPersonFlags |= 0x1000;
        }
    } else {
        CThing* before = gStaffPanelThing;
        if (gStaffPanelLocation != which) gStaffPanelThing = nullptr;
        gStaffPanelLocation = which;
        GetNextStaffMember();
        if (gStaffPanelThing == nullptr) {
            gStaffPanelC->Show(0);
            gStaffPanelC->unlink();
            gFirstPersonFlags &= 0xffffefffu;
        } else {
            if (before == nullptr) {
                gMainPanel->link(gStaffPanelC, 1, 1);
                gStaffPanelC->Show(1);
            }
            UpdateLocator();
            gFirstPersonFlags |= 0x1000;
        }
    }
}

// 0x101bfb48 (616 bytes): zooms to the staff member and, for a staff kind with a patrol area, draws the area's rectangle (mode 0x136) in four lines.
void hackfollowmode() {
    if (gStaffPanelThing == nullptr) return;
    if ((gFirstPersonFlags & 0x1000) == 0) return;
    UIMain_ZoomToPeep(reinterpret_cast<CPeep*>(gStaffPanelThing));
    CThing* thing = gStaffPanelThing;
    if (!IsStaffKind(thing)) return;
    CStaff* staff = reinterpret_cast<CStaff*>(thing);
    if (staff->HasPatrolArea() == 0) return;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> start;
    tag_IDX_TYPE<IDXTYPEUniqueNameMapId, uint16_t> end;
    staff->GetPatrolArea(&start, &end);
    uint32_t a = static_cast<uint32_t>(start.mIndex) - 1;
    uint32_t b = static_cast<uint32_t>(end.mIndex) - 1;
    int32_t x1 = static_cast<int32_t>(a & 0x7f);
    int32_t x2 = static_cast<int32_t>(b & 0x7f);
    int32_t y1 = static_cast<int32_t>(a >> 7);
    int32_t y2 = static_cast<int32_t>(b >> 7);
    const CELL_MODES mode = static_cast<CELL_MODES>(0x136);
    int32_t ex, ey, sx;

    ex = x2; ey = y1; sx = x1;
    if (x1 != x2) { sx = x1 + 1; ex = ex - 1; }
    LLCC_DrawLine(mode, sx, y1, &ex, &ey);

    ex = x2; ey = y2;
    LLCC_DrawLine(mode, x2, y1, &ex, &ey);

    ex = x2; ey = y2; sx = x1;
    if (x1 != x2) { sx = x1 + 1; ex = ex - 1; }
    LLCC_DrawLine(mode, sx, y2, &ex, &ey);

    ex = x1; ey = y1;
    LLCC_DrawLine(mode, x1, y2, &ex, &ey);
}
