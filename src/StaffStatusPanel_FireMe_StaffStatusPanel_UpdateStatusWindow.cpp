// StaffStatusPanel::InitStatusWindow (0x101c4304), UpdateStatusWindow (0x101c4990) and UpdateWindow (0x101c4d2c).
// The object has the UIStatusPanelFields layout (status window at 0x10, window at 0x0c, name window at 0x1c, table at 0x20).
#include <cstdint>

#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "ui_DrawObject.h"
#include "ui_controls.h"
#include "ui_main.h"

namespace {

// The two mesh names the Mac keeps one after the other (0x1024e774 and +0xe = 0x1024e782).
char kStatusBarNames[] = "happygrad.wct\0ridestatbar.wct";

constexpr int32_t kStaffTextTable = 6;   // gText + 0x564: mResourcePointers[6]

// Gives the window `window` a text sprite as client draw object, the font number 6 and, when given, the text.
void SpriteWindow(InterfaceWindow* window, int32_t stringId) {
    InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(6));
    if (stringId != 0) window->SetClientDrawText(const_cast<char16_t*>(UIMain_GetString(stringId)));
}

}  // namespace

void StaffStatusPanel::InitStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    if (fields->mF10.get() == nullptr) return;

    SpriteWindow(fields->mF10->GetWindow(0x51a), 0x40);
    SpriteWindow(fields->mF10->GetWindow(0x51d), 0x41);
    SpriteWindow(fields->mF10->GetWindow(0x519), 0x43);
    SpriteWindow(fields->mF10->GetWindow(0x518), 0x45);
    SpriteWindow(fields->mF10->GetWindow(0x51c), 0x44);
    SpriteWindow(fields->mF10->GetWindow(0x516), 0x42);

    InterfaceWindow* window = fields->mF10->GetWindow(0x51b);
    {
        InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
        window->SetClientDrawObject(sprite);
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
    }

    window = fields->mF10->GetWindow(0x513);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames));
    window->SetValue(100);

    window = fields->mF10->GetWindow(0x514);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames + 0xe));
    window->SetValue(100);

    window = fields->mF10->GetWindow(0x51e);
    {
        InterfaceCashTextObject* cash = new InterfaceCashTextObject(nullptr);
        window->SetClientDrawObject(cash);
        window->SetFont(GetFontTableEntry(6));
        window->SetValue(0);
    }

    window = fields->mF10->GetWindow(0x515);
    UIMain_IndentClient(window, 0, 10);
    window->SetClientDrawObject(new InterfaceDrawEngineHGradient(kStatusBarNames + 0xe));
    window->SetValue(500);

    window = fields->mF10->GetWindow(0x517);
    {
        InterfaceTextObjectSprite* sprite = new InterfaceTextObjectSprite(nullptr);
        window->SetClientDrawObject(sprite);
        window->SetFont(GetFontTableEntry(6));
        window->SetClientDrawText(nullptr);
    }
}

void StaffStatusPanel::UpdateStatusWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
    if (fields->mF10.get() == nullptr) return;

    int32_t state = staff->GetGeneralStaffState();
    int32_t duration = staff->GetEmploymentDuration();
    TbIntegerVTag tag(8, duration);
    static_cast<TbITable*>(gText.mResourcePointers[kStaffTextTable])->FormatString(gUIMainConvertBuffer, 0x3fff, 0x1bb, &tag);

    InterfaceWindow* window = fields->mF10->GetWindow(0x51b);
    window->SetText(gUIMainConvertBuffer);

    window = fields->mF10->GetWindow(0x51e);
    window->SetValue(static_cast<int32_t>(staff->GetSalary()));

    window = fields->mF10->GetWindow(0x514);
    // (skill << 10) / 100 on the unsigned word (mulhwu by 0x51eb851f, then srwi 5).
    uint32_t skill = static_cast<uint32_t>(staff->GetSkill()) << 10;
    window->SetValue(static_cast<int32_t>(skill / 100));

    window = fields->mF10->GetWindow(0x51f);
    window->SetFrame(UIMain_GetStaffStateFrame(state));

    window = fields->mF10->GetWindow(0x517);
    if (state == 1 && staff->HasPatrolArea() == 0) state = 0;
    UIMain_SetStaffStateText(window, state);

    // The two bars: the float truncated (fctiwz), its low byte shifted by 10, divided by 100 (signed, but never negative).
    int32_t happiness = static_cast<int32_t>(staff->mHappiness);
    window = fields->mF10->GetWindow(0x513);
    window->SetValue(static_cast<int32_t>(((static_cast<uint32_t>(happiness) & 0xff) << 10)) / 100);

    int32_t energy = static_cast<int32_t>(staff->mEnergy);
    window = fields->mF10->GetWindow(0x515);
    window->SetValue(static_cast<int32_t>(((static_cast<uint32_t>(energy) & 0xff) << 10)) / 100);
}

void StaffStatusPanel::UpdateWindow() {
    UIStatusPanelFields* fields = reinterpret_cast<UIStatusPanelFields*>(this);
    CStaff* staff = reinterpret_cast<CStaff*>(gStaffRoomPanelThing);
    if (staff == nullptr) return;

    Ptr32<InterfaceWindow> nameWindow;
    nameWindow.mOffset = fields->mF1C;
    nameWindow->SetText(const_cast<char16_t*>(staff->GetName()));

    // The two virtual calls through the table at 0x20: entries 0x20 and 0x1c (each an arena offset of a block holding the function).
    Ptr32<void (*)(StaffStatusPanel*)> entry;
    entry.mOffset = fields->mVtable.get()[0x20 / 4];
    (*entry.get())(this);
    entry.mOffset = fields->mVtable.get()[0x1c / 4];
    (*entry.get())(this);

    InterfaceButton* button = static_cast<InterfaceButton*>(fields->mWindow->GetWindow(0x510));
    if (staff->HasPatrolArea() != 0) {
        UIMain_AdvisorMessage(static_cast<AdvisorMessage>(0xd4));
        button->SetState(1, 1);
    } else {
        button->SetState(0, 1);
    }

    InterfaceWindow* window = fields->mWindow->GetWindow(0x50e);
    if (staff->CanBePickedUp() != 0) {
        window->Enable(1);
    } else {
        window->Enable(0);
    }
}
