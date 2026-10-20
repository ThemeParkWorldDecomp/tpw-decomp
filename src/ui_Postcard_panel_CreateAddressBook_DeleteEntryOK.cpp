#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>
#include <string>
#include "FS_Online.h"
#include "FS_UserInfo.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "TbITable.h"
#include "TbVTag.h"
#include "Text.h"
#include "email.h"
#include "platform/msl.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_Postcard.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "UI_Base.h"
#include "World.h"
#include "ThingReceivers.h"

// The runtime's operator new (0x102384b8, never NULL): a weak seam the test library replaces with a recording fake.
namespace postcard_callee {
__attribute__((weak)) void* OperatorNew(uint32_t size) { return ::operator new(size); }
}  // namespace postcard_callee

// 0x103959a0 (name invented): the research list's row height, worked out by the first ResearchListLCFormat (0 until then).
int32_t gResearchRowHeight = 0;

// The Mac calls a virtual through the font entry's table here (__ptr_glue, r3 = the font); the class is not recovered, so the height
// getter stands in. The test library replaces this function with a recording fake.
__attribute__((weak)) int16_t PostcardFontVirtual(InterfaceFont* font) { return static_cast<int16_t>(font->GetHeight()); }

// 0x101b0368: the "delete this address" dialog's OK button. The argument is the entry position, passed as a pointer.
int32_t DeleteEntryOK(void* position) {
    if (gUserInfo == nullptr) {
        void* memory = postcard_callee::OperatorNew(0xd4);
        CFSUserInfo* info = nullptr;
        if (memory != nullptr) info = new (memory) CFSUserInfo();
        gUserInfo = info;
    }
    gUserInfo->mAddressBook.Remove(static_cast<int32_t>(reinterpret_cast<intptr_t>(position)));
    FillAddressList(1);
    return 1;
}

// 0x101b0620: LCFormat of the postcard file: a window 100 wide, as high as the font 7's height scaled to the screen, plus 6.
InterfaceWindow* PostcardLCFormat() {
    void* memory = postcard_callee::OperatorNew(0x138);
    if (memory != nullptr) {
        int32_t height = PostcardFontVirtual(GetFontTableEntry(7));
        int32_t row = height * 0x600 / gUIRealScreenHeight + 6;   // QUIRK: no check for a zero screen height
        new (memory) InterfaceWindow(nullptr, 0, 0, 0, 0, 100, static_cast<int16_t>(row));
    }
    return static_cast<InterfaceWindow*>(memory);
}

// 0x101b06d4: LCPostFormat of the postcard file.
void PostcardLCPostFormat(InterfaceWindow* window) {
    void* memory = postcard_callee::OperatorNew(0x19c);
    InterfaceTextObjectSprite* sprite = nullptr;
    if (memory != nullptr) sprite = new (memory) InterfaceTextObjectSprite(nullptr);
    window->SetClientDrawObject(sprite);
    window->SetFont(GetFontTableEntry(7));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
}

// 0x101b5b4c
void ResearchPanelCategoryComplete(int32_t category) {
    CResearchLab* lab = gGameWorld->GetResearchLab();
    if (!lab->IsCategoryActive(static_cast<ResearchCat>(category))) lab->NextProject(static_cast<ResearchCat>(category));
    if (gResearchPanel != nullptr) {
        NameWindow(category);
        PercentComplete(category);
    }
}

// 0x101b9ea8: LCFormat of the research list file: the row height is worked out once.
InterfaceWindow* ResearchListLCFormat() {
    if (gResearchRowHeight == 0) {
        int32_t height = PostcardFontVirtual(GetFontTableEntry(gParkListFontIndex));
        gResearchRowHeight = height * 0x600 / gUIRealScreenHeight + 6;   // QUIRK: no check for a zero screen height
    }
    void* memory = postcard_callee::OperatorNew(0x138);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    if (memory != nullptr) new (memory) InterfaceWindow(nullptr, 1, 0, 0, 0, 100, static_cast<int16_t>(gResearchRowHeight));
    if (window != nullptr) window->CreateObject(1, 1, 1, 100, 0, 200, static_cast<int16_t>(gResearchRowHeight));
    return window;
}

// 0x101c82ac: stores the message in the first free slot (time 0); answers 1 when it did. A time of 0 or of 60000 and more is refused.
int16_t InterfaceTimedMessages::Add(InterfaceWindow* window, int32_t message, int32_t a, int32_t b, uint32_t time) {
    int16_t added = 0;
    if (mEntries != nullptr && time != 0 && time < 60000) {
        for (int32_t i = 0; i < mCount && added == 0; i++) {
            if (mEntries[i].mTime == 0) {
                mEntries[i].mTime = time + IveGotTicks();
                added = 1;
                mEntries[i].mWindow = window;
                mEntries[i].mMessage = message;
                mEntries[i].mA = a;
                mEntries[i].mB = b;
            }
        }
    }
    return added;
}
