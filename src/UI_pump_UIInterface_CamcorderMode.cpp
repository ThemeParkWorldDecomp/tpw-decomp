// UIInterface's mouse, research button, kid and staff room events (0x101f0d14..0x101f10f8) and the error dialog's queue
// (ErrorDlgCancel 0x101f1cd0, AddUIError 0x101f1df0, the block table's destructor 0x101f2520). The inventory files them under
// ui_ErrorDisplay.cpp.
#include <cstdint>
#include <cstring>
#include <new>

#include "Debug.h"
#include "FS_Online.h"
#include "FillLenderList.h"
#include "InterfaceWindow.h"
#include "MainWindowCallback.h"
#include "Mouse.h"
#include "TbITable.h"
#include "Text.h"
#include "UIInterface.h"
#include "UI_Base.h"
#include "UI_pump.h"
#include "World.h"
#include "platform/msl.h"
#include "std_deque_errorthingy.h"
#include "ui_main.h"

// 0x101a7830 (16 bytes, untranslated): the main panel's research button. A weak stand-in until a unit translates it.
__attribute__((weak)) void MainPanel_EnableResearchButton(bool) {}

// 0x101f0ffc
void UIInterface::CamcorderMode() {
    UIMain_CheckPanel();
    CMMFirstPersonTarget* target = new CMMFirstPersonTarget();   // operator new(8), then the constructor
    if (gMouse == nullptr) {
        CMouse* mouse = new CMouse();                              // operator new(1)
        CMMDefault* mode = new CMMDefault();                       // operator new(8), then the constructor
        mouse->AssignMouseMode(mode);
        gMouseWord1E0 = -1;
        gMouseWord1E4 = -1;
        gMouse = mouse;
    }
    gMouse->AssignMouseMode(target);
}

// 0x101f10f8: the game type object is constructed on first use (its guard byte is signed); an easy game (type 2) keeps the button as it is.
void UIInterface::EnableResearchButton(bool on) {
    if (on) {
        if (gLoansGameTypeGuard == 0) {
            new (gLoansGameTypeStorage) CGameType();
            gLoansGameTypeGuard = 1;
        }
        int32_t type;
        std::memcpy(&type, gLoansGameTypeStorage, sizeof(type));
        if (type == 2) return;
    }
    MainPanel_EnableResearchButton(on);
}

static const char kThingZero[] = "Thing has been allocated in ThingArray index 0 - shouldn't happen\n";   // 0x1024f3a2

// 0x101f0dcc
void UIInterface::StaffRoomEvent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff, int32_t arriving) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    CThing* thing = static_cast<CThing*>(gThingArray[staff.mIndex].mThing);
    if (arriving != 0)
        gStaffRoomStatusPanel.StaffWentIntoStaffRoom(thing);
    else
        gStaffRoomStatusPanel.StaffLeftStaffRoom(thing);
}

// 0x101f0d14
void UIInterface::KidArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> kid) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    AllKidsPanel_KidAdded(static_cast<CThing*>(gThingArray[kid.mIndex].mThing));
}

// 0x101f1cd0: the dialog's button. Under the mutex, when an error waits it puts the oldest one's text into the dialog (the virtual at
// slot 0x2c of the table at +0x118) and takes it off. Answers whether nothing waited.
int32_t ErrorDlgCancel(void*) {
    int32_t empty = 1;
    gUIErrorMutex.lock();
    if (!gUIErrorQueue.empty()) {
        const errorthingy error = gUIErrorQueue.front();
        const unsigned char* text = error.mTable->GetConstString(error.mId);
        char16_t* converted = UIMain_ConvertMulti(text);
        findpanel_callee::WindowSetText(gUIErrorDialog, converted);
        gUIErrorQueue.pop_front();
        empty = 0;
    }
    gUIErrorMutex.unlock();
    return empty;
}

// 0x101f1df0: a NULL table means the text resources' table (gText + 0x558).
void AddUIError(int32_t error, TbITable* table) {
    if (table == nullptr) table = static_cast<TbITable*>(gText.mResourcePointers[3]);
    errorthingy record;
    record.mTable = table;
    record.mId = static_cast<uint32_t>(error);
    gUIErrorMutex.lock();
    gUIErrorQueue.push_back(record);
    gUIErrorMutex.unlock();
}

// 0x101f2520: std::__cdeque<errorthingy*>::~__cdeque with its hidden delete flag (the Mac answers this): frees the table, and the object
// when the flag is above 0.
namespace errdeque_callee {
void QueueCdequeDtor(void* cdeque, int16_t flag) {
    auto* self = static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(cdeque);
    if (self != nullptr) {
        // the table pointer is a 32-bit word: the host's heap is above 4 GB (see gMslMacAddressHigh)
        if (self->mWordC != 0)
            OperatorDelete(reinterpret_cast<void*>(static_cast<uintptr_t>((gMslMacAddressHigh << 32) | self->mWordC)));
        if (flag > 0) OperatorDelete(self);
    }
}
}  // namespace errdeque_callee
