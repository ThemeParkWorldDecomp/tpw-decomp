// UIInterface.cpp: guessed name (the binary has no __sinit for this file): a contiguous range of UIInterface code, named after it.

#include "Advisor.h"
#include "Debug.h"
#include "email.h"
#include "FillLenderList.h"
#include "FS_Online.h"
#include "FS_OnlineList.h"
#include "FS_System.h"
#include "Game.h"
#include "helpers/MslSupportHelpers.h"
#include "HLSprite.h"
#include "InGame.h"
#include "InterfaceWindow.h"
#include "LbTime.h"
#include "Light.h"
#include "LoadPanelMsg.h"
#include "lobby.h"
#include "MainWindowCallback.h"
#include "MapWho.h"
#include "Mouse.h"
#include "online.h"
#include "platform/arena.h"
#include "platform/msl.h"
#include "platform/samsutils.h"
#include "platform/upixels.h"
#include "Postcard.h"
#include "Quickload.h"
#include "rsse.h"
#include "std_basic_filebuf.h"
#include "std_cdeque.h"
#include "std_deque_errorthingy.h"
#include "tb_ddraw.h"
#include "tb_resource_system.h"
#include "tb_resource_texture.h"
#include "TbGraphicFile.h"
#include "TbITable.h"
#include "Text.h"
#include "ThingReceivers.h"
#include "TPCamera.h"
#include "TPConfig.h"
#include "TPEngine.h"
#include "TPGlobals.h"
#include "TPMain.h"
#include "TPResource.h"
#include "TPSettings.h"
#include "TPSound.h"
#include "TPString.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_DrawObject.h"
#include "ui_ErrorDisplay.h"
#include "ui_main.h"
#include "ui_Postcard.h"
#include "ui_publishParkPanel.h"
#include "UI_pump.h"
#include "ui_textmanager.h"
#include "ui_tooltip.h"
#include "UIInterface.h"
#include "World.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>

// UIInterface's mouse, research button, kid and staff room events (0x101f0d14..0x101f10f8) and the error dialog's queue
// (ErrorDlgCancel 0x101f1cd0, AddUIError 0x101f1df0, the block table's destructor 0x101f2520). The inventory files them under
// ui_ErrorDisplay.cpp.

// 0x101a7830 (16 bytes, untranslated): the main panel's research button. A weak stand-in until a unit translates it.
__attribute__((weak)) void MainPanel_EnableResearchButton(bool) {}

// The interface's message pump (main app): what UIInterface::Process does once a pass. ProcessUI (0x101866c4, ui_main.cpp)
// runs the error display, the timers, the online postcard, the three message queues and the on-frame messages; this file has
// it and everything it reaches that was not translated before: the queues' Post, Peek and GetNumQd and the timed queue's
// Process (UI_Base.cpp's range), the dispatch, onframe and UI_SendMessage, the timers' AddTimer and ServiceAllTimers, the
// window members they use (CallWindowHandler, GetChild, GetNext, GetAttr, AmIGettingDestroyed), UI_GetHighestZ, the postcard
// test and TickUIErrors. The set-up is in UI_init.cpp and InterfaceWindow.cpp; the window handlers, the panel's drawing and
// the dialog are other units' (UI_DialogBox and UI_ActuallyDoPostcard are generated stand-ins in the launcher).

extern int32_t gUIOnFrameCount;

extern int32_t gUIGeneralQueued;

extern int32_t gUIDrawQueued;

extern int32_t gPostcardDelay;

extern int32_t gPostcardWanted;

extern std::deque<errorthingy> gUIErrorQueue;

extern bthread_mutex gUIErrorMutex;

extern InterfaceWindow* gUIErrorDialog;

int32_t UI_DispatchMessages(int32_t limit);

int32_t UI_DispatchDrawMessages(int32_t limit);

int32_t UI_DispatchDeferredMessages();

void onframe(InterfaceWindow* window);

int32_t UI_SendOnFrameMessages();

uint32_t IveGotTicks();

int32_t UI_GetHighestZ(InterfaceWindow* window, CoolRect* rect);

void UI_ServiceAllTimers();

bool UI_PostcardPending();

void TickUIErrors();

void ProcessUI();

void UIInterface::Process() { ProcessUI(); }

// ui_main.cpp (main app of SimTheme Park 2.04b3), the part that starts the interface:
// the meshes of the interface (0x10184abc..0x10185640), InitGeneralUI (0x10185b68), the
// clock tick objects (0x10185b08, 0x10187994) and the file's global strings.
//
// Every .md2 file of the "UI" resource becomes a draw object in the library: the frames
// ("!frame", "!f_plain", "!cbut") and the slider ("!slider") under their file name, any
// other mesh under the name of its root node. The meshes of the language's folder follow.
// The first mesh loaded is what puts the "welcome" picture on the screen (CMesh::Init
// steps the loading picture).

using bfl::FileSystem::CFileInfo;
using bfl::FileSystem::CFileStorage;

extern InterfaceTitleTextObject* gTitleText;

extern CursorTable gCursorTable;

extern TbDynamicStringTemplate<char> gUIFrameName;

extern TbDynamicStringTemplate<char> gUISliderName;

extern TbDynamicStringTemplate<char> gUIPlainFrameName;

extern LbTimeClockTickFunction gUIClockTick;

InterfaceDrawEngineResizableFrame* TPResource_AddFrameMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder);

InterfaceDrawEngineResizableSlider* TPResource_AddSliderMesh(CFileInfo info, CFileStorage&, const TbDynamicStringTemplate<char>& folder);

InterfaceDrawEngineMesh* TPResource_AddMeshDrawObject(CFileInfo info, CFileStorage&, TbDynamicStringTemplate<char> folder);

int32_t LoadAllUIMeshes();

int32_t LoadLocalisedUIMeshes();

void InitGeneralUI();

extern InterfaceWindow* gUIGeneralWindowA;

extern InterfaceWindow* gUIGeneralWindowB;

extern InterfaceWindow* gUIGeneralWindowC;

void DeInitGeneralUI();

// UIInterface::InitGeneral (0x101ef5f8; the class is in the range the inventory files under
// ui_ErrorDisplay.cpp).
void UIInterface::InitGeneral() { InitGeneralUI(); }

// UIInterface::DeInitGeneral (0x101ef64c).
void UIInterface::DeInitGeneral() { DeInitGeneralUI(); }

// ui_main.cpp (main): InitGameUI 0x10185e58 (944 B), DeInitGameUI 0x1018622c (212 B).
// UIInterface wrappers: InitGame 0x101ef6a4 (44 B), DeInitGame 0x101ef6fc (36 B).

extern float gGameWindowCentreX;

extern float gGameWindowCentreY;

extern float gGameWindowHalfWidth;

extern float gGameWindowHalfHeight;

void InitGameUI();

void DeInitGameUI();

void UIInterface::InitGame() {
    InitGameUI();
    UI_ClearSelected();
}
void UIInterface::DeInitGame() { DeInitGameUI(); }

// The interface's colour cursors and the lobby's interface set-up (main app, ui_main.cpp range:
// LoadBFCurs 0x10185670, InitialiseCOLOURCursors 0x1018575c, InitLobbyUI 0x10186324; the
// InterfaceBFCursor constructor and LoadData 0x101fe208, 0x101fe52c, which the inventory files
// under TPEmbed.cpp; UIInterface::InitLobby 0x101ef750).

extern CLight gUILight;

void InitLobbyUI();

void UIInterface::InitLobby() { InitLobbyUI(); }

// The way out of the lobby (main app): the key sparkle when the player enters an island (ui_islandLobbyController.cpp
// 0x101e1460), the interface's lobby tear-down (ui_main.cpp 0x10186434, ui_ErrorDisplay.cpp 0x101ef7a4) and the first-person
// ride test (TPCamera.cpp 0x100329cc).

void DeInitLobbyUI();

void UIInterface::DeInitLobby() { DeInitLobbyUI(); }

// Unit v51126: the tree constructors and insert_one of the nocase set, the wstring set and the buddy map, the allocator deallocates,
// the Argument destroyers, the nocase wide string destructor, the tear_down of the deque of Argument vectors, two make_iterator /
// getloc pieces, and four forwarders of the UI. The structures keep the Mac's 32-bit words (the arena is 32-bit addressable).

namespace v51126 {

void ArgumentDestroy(Argument* argument);

void ArgumentDestroyRange2(Argument* first, Argument* last) {
    for (; first < last; first++) ArgumentDestroy(first);
}

void ArgumentVectorDestroy(void* vector) {
    if (vector != nullptr) v51126_callee::ArgumentVectorTearDown(vector);
}

void ArgumentVectorDestroyRange(void* first, void* last) {
    for (uint8_t* p = static_cast<uint8_t*>(first); p < static_cast<uint8_t*>(last); p += 0xc) ArgumentVectorDestroy(p);
}

void* ArgVectorPtrCdequeDestruct(ArgVectorPtrCdeque* self, int16_t deleteFlag) {
    if (self != nullptr) {
        if (self->mWordC != 0) v51126_callee::OperatorDelete(P(self->mWordC));
        if (deleteFlag > 0) v51126_callee::OperatorDelete(self);
    }
    return self;
}

void ArgumentDequeTearDown(MslArgumentDeque* self) {
    if (self->mCount == 0) return;
    MslArgumentDequeIter end;
    argdeque_new::MakeIterator(&end, self, self->mFirst + self->mSize);
    MslArgumentDequeIter begin;
    argdeque_new::MakeIterator(&begin, self, self->mFirst);
    MslArgumentDequeIter last = end;
    ArgumentDequeDestroy(self, begin, last);
    uint32_t count = self->mCount;
    uint32_t sum = self->mStart + count;
    MslArgumentCursor cursor;
    cursor.mCdeque = A(&self->mCapacity);
    cursor.mIndex = count;
    cursor.mSlot = sum < self->mCapacity ? self->mTable + sum * 4 : self->mTable + (sum - self->mCapacity) * 4;
    v51126_callee::CdequeDecrement(&cursor);
    uint32_t remaining = cursor.mIndex;
    uint32_t slot = self->mTable + self->mStart * 4;
    for (uint32_t i = 0; i != remaining;) {
        v51126_callee::OperatorDelete(P(*W(slot)));
        slot += 4;
        i++;
        if (slot == self->mTable + self->mCapacity * 4) slot = self->mTable;
    }
}

}  // namespace v51126

// 0x102229f8 / 0x10223670 are v51126::BuddyNodeDeallocate / WStringNodeDeallocate (distinct names, one body).

// std::basic_streambuf<wchar_t>::getloc(out) const: the two locale words are copied and the reference count is bumped.
namespace filebuf_wclose_callee {
void Getloc(void* out, void* streambuf) {
    uint32_t* o = static_cast<uint32_t*>(out);
    const uint8_t* sb = static_cast<const uint8_t*>(streambuf);
    uint32_t impl, refs;
    std::memcpy(&impl, sb + 0x1c, 4);
    std::memcpy(&refs, sb + 0x20, 4);
    o[0] = impl;
    o[1] = refs;
    uint32_t r = o[1];
    if (r != 0) {
        uint32_t* counter = reinterpret_cast<uint32_t*>(tpw::gArenaBase + r);
        *counter = *counter + 1;
    }
}
}  // namespace filebuf_wclose_callee

// std::deque<errorthingy>::make_iterator(ulong), the second instance.
namespace errdeque_callee {
void MakeIteratorMutable(MslErrorDequeIterator* out, MslErrorDeque* self, uint32_t index) {
    MslErrorDequeCursor cursor;
    auto slotAt = [](uint32_t slot) { return *reinterpret_cast<uint32_t*>(tpw::gArenaBase + slot); };
    cursor.mCdeque = tpw::ArenaOffset(&self->mBlocks);
    cursor.mWord4 = 0;
    cursor.mSlot = self->mBlocks.mWordC + self->mBlocks.mWord8 * 4;
    uint32_t block;
    uint32_t element;
    uint32_t blockSize = self->mBlockSize;
    if (index < blockSize) {
        block = self->mBlocks.mWord4 != 0 ? slotAt(cursor.mSlot) : 0;
        element = block + index * 8;
    } else {
        uint32_t quotient = blockSize != 0 ? index / blockSize : 0;
        v51126_callee::CursorAdd(reinterpret_cast<MslArgumentCursor*>(&cursor), static_cast<int32_t>(quotient));
        block = slotAt(cursor.mSlot);
        element = block + (index - quotient * blockSize) * 8;
    }
    out->mCursor = cursor;
    out->mElement = element;
    out->mBlockEnd = block + blockSize * 8;
    out->mIndex = index;
}
}  // namespace errdeque_callee

void UIInterface::InitOnline() { InitOnlineUI(); }

// Unit v51076: small forwarders of UIInterface and the CodeWarrior deque pieces next to them.

// 0x102b1428, original name unknown: the first word of a new error deque (0x40).
uint32_t gMslErrorDequeMapSize = 0x40;

// ---- std::__cdeque<const void*>
// 0x101f25e8: one element less, the start moves on and wraps at the capacity.
void std::__cdeque<const void*, std::allocator<const void*>>::pop_front() {
    mWord4 = mWord4 - 1;
    mWord8 = mWord8 + 1;
    if (mWord8 >= mWord0) mWord8 = mWord8 - mWord0;
}

// 0x101f492c: the smaller of two constants the Mac stores on its stack, then reads back: 0x3fffffff.
uint32_t std::__cdeque<const void*, std::allocator<const void*>>::max_size() const { return 0x3fffffffu; }

namespace cdeque_callee {
// 0x101f2dd8 iterator::iterator(const iterator&): the three words; the Mac's r3 stays the destination.
MslCdequeIter* IterCopy(MslCdequeIter* dest, const MslCdequeIter* src) {
    dest->mDeque = src->mDeque;
    dest->mIndex = src->mIndex;
    dest->mPtr = src->mPtr;
    return dest;
}
}  // namespace cdeque_callee

// ---- std::__cdeque<errorthingy*>
void ErrorCdequeResize(void* self, uint32_t n, errorthingy* value) {
    static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(self)->resize(n, value);
}

void ErrorCdequeReserve(void* self, uint32_t n) {
    static_cast<std::__cdeque<const void*, std::allocator<const void*>>*>(self)->reserve(n);
}

// ---- std::deque<errorthingy>::deque(const allocator&) 0x101f5158: seven words (0x1c bytes), the first from the global.
namespace errsinit_callee {
void DequeCtor(void* self, void* allocator) {
    (void)allocator;
    uint32_t* w = static_cast<uint32_t*>(self);
    w[0] = gMslErrorDequeMapSize;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
}
}  // namespace errsinit_callee

void UIInterface::DeInitOnline() { DeInitOnlineUI(); }

// UIInterface::Pause (main app, SimTheme Park 2.04b3, ui_ErrorDisplay.cpp's range; unit w81). Its mirror, UnPause, is in src/UI_pump.cpp.

// 0x101ef8a0 (36 bytes): PauseUI(); `this` is not used.
void UIInterface::Pause() { PauseUI(); }

void UIInterface::UnPause() { UnPauseUI(); } // 0x101ef8f0

void UIInterface::TogglePause() { TogglePauseUI(); }
void UIInterface::ShowFirstPersonWindow(int32_t on) { UI_SwitchGameWindow(on == 0); }   // cntlzw, srwi 5
void UIInterface::SwitchGameWindow() { ToggleGameWindow(); }

void UI_MainPanelDateChange(int32_t day, int32_t month, int32_t year);   // 0x101a6234 (stand-in)

ParkDate gUIPostLoadDate;   // 0x1039641c

// 0x101efa54
void UIInterface::SetGameWindowCursor(int16_t cursor) {
    if (gGameWindow != nullptr) {
        UI_SetGameWindowCursor(cursor);
        return;
    }
    if (gOnlineWindow != nullptr) gOnlineWindow->setCursor(cursor);
    if (gOnlinePanel != nullptr) gOnlinePanel->setCursor(cursor);
}

// UIInterface_context.cpp (main, unit w131): UIInterface::CheckMouseContext 0x101efb0c (40 bytes).

// Hands the mouse context to ContextMouse (0x10182548); the answer of the Mac is nothing.
void UIInterface::CheckMouseContext(int context) { ContextMouse(context); }

// The postcard panel (main app, ui_Postcard.cpp, 0x101b0ae4..0x101b2418), the keyboard key and UIInterface forwarding call.

void UI_DoPostcard();

void UIInterface::DoPostcard() { UI_DoPostcard(); }

void UIInterface::ShowMap() { gUI2DMap.CreateMapPanel(); }

void UIInterface::ResearchCategoryComplete(ResearchCat category) { ResearchPanelCategoryComplete(category); }

void UIInterface::RideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t>) {}

void UIInterface::RideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) { gRideStatusPanel.BreakDown(ride); }

void UIInterface::RideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride) { gRideStatusPanel.Condemned(ride); }

void UIInterface::StaffDBChange(int32_t slot) { UI_HirePanelStateChange(slot); }

// Unit v51037, main app: the buttons menu underlay's message function and its hide function (Mac file ui_onlineParkFindPanel.cpp), the static
// initialisers of the online park info and list panels, RemoveOnlineParkListPanel, and four small UIInterface functions.

// Callees no header declares (untranslated stand-ins, or defined in a file with a local declaration).
void LocatorByeByeKid(CThing* thing);                                       // 0x101bf98c (56 bytes, src/gameplay_leaves.cpp)
// 0x101efe60 (80 bytes): the day, the month plus one and the year of the date block (the other words are not read).
void UIInterface::DateChange(ParkDate date) {
    UI_MainPanelDateChange(date.mDay, date.mMonth + 1, date.mYear);
}

void UIInterface::MessageTag(CTagParams& params) { AddTag(params); }

CThing* UIInterface::GetSelectedPeep() { return UI_GetSelectedPeep(); }

CObject* UIInterface::GetSelectedObject() { return static_cast<CObject*>(UI_GetSelectedObject()); }

void UIInterface::SetSelectedObject(CObject* object) { UI_SetSelectedObject(object); }

int32_t UIInterface::GetShiftKeyState() { return static_cast<int32_t>(UIMain_GetShiftKeyState()); }

// Unit v51061: UIInterface stand-ins, each a thin call into the UI.

void UIInterface::PreLevelLoad() {
    if (GameWindowToggle_Hidden()) ToggleGameWindow();
}

// 0x101f0124
void UIInterface::PostLevelLoad() {
    ParkDate* d = &gUIPostLoadDate;
    const uint8_t* world = reinterpret_cast<const uint8_t*>(gGameWorld);
    UI_ClearSelected();
    if (*reinterpret_cast<const int32_t*>(world + 0x1da738) == 4) UI_BankruptAfterLoad();
    UIMain_EnableAllKeyTables();
    reinterpret_cast<const CParkTime*>(world + 0x2a0)
        ->GetFunnyTime(&d->mYear, &d->mMonth, &d->mDay, &d->mDayOfWeek, &d->mHour, &d->mMinute, &d->mSecond, nullptr);
    d->mYear = d->mYear - 0x76c;
    d->mDay = d->mDay - 1;
    d->mDay = d->mDay + 1;
    d->mMonth = d->mMonth - 1;
    ParkDate copy = *d;
    UI_MainPanelDateChange(copy.mDay, copy.mMonth + 1, copy.mYear);
}

void UIInterface::Load(void* file) {
    InstantRemoveAllTags();
    LoadTags(file);
}

void UIInterface::Save(void* file) { SaveTags(file); }

int32_t UIInterface::RemoveLocators() {
    if (StaffPanelUp()) {
        RemoveStaffPanel();
        return 1;
    }
    return 0;
}

void UIInterface::ShowGameMainMenu() { CreateMainMenu(0); }

void UIInterface::Screen_BuyObject() {
    tag_IDX_TYPE<IDXTYPEUniqueNameObjectId, uint16_t> none;
    none.mIndex = 0;
    CreateRideListPanel(static_cast<UIObjType>(-1), none);
}

void UIInterface::Screen_BuyStaff() { CreateHireListPanel(static_cast<StaffType>(-1)); }

void UIInterface::Screen_ParkStatus() { CreateParkStatusPanel(); }

void UIInterface::Screen_AllItems() { CreateAllItemsPanel(static_cast<UIObjType>(-1), nullptr); }

void UIInterface::Screen_AllStaff() { CreateAllStaffPanel(static_cast<StaffType>(-1), nullptr); }

void UIInterface::Screen_AllPeeps() { CreateAllKidsPanel(nullptr); }

void UIInterface::Screen_Finance() { CreateFinancePanel(); }

void UIInterface::Screen_Loans() { CreateLoansPanel(); }

void UIInterface::Screen_StaffCosts() { CreateStaffCostsPanel(); }

void UIInterface::Screen_EntryPrice() { CreateEntryPricePanel(); }

// 0x101f07b4 (76 bytes): the panel's word at +0x0c (its window) tells whether a staff member is shown.
void UIInterface::Screen_StaffLoc() {
    if (reinterpret_cast<UIStatusPanelFields*>(&gStaffStatusPanel)->mWindow.mOffset != 0) {
        ShowStaffPanel(gStaffRoomPanelThing);
    } else {
        ShowStaffPanel(nullptr);
    }
}

// 0x101f0834 (72 bytes)
void UIInterface::Screen_PeepLoc() {
    if (reinterpret_cast<UIStatusPanelFields*>(&gVisitorStatusPanel)->mWindow.mOffset != 0) {
        ShowStaffPanel(gStaffRoomPanelThing);
    } else {
        ShowKidLocatorPanel();
    }
}

void UIInterface::Screen_Research() { CreateResearchPanel(); }
void UIInterface::ToggleHelpText() {
    UI_ToggleHelpText();
    gPlayerSettings.mCurrent.mTooltipsOn = UI_HelpTextEnabled();   // stb at +0x36
}

void UIInterface::OnlineReceiveStr(char16_t* text, int32_t data) { OnlineReceiveString(text, data); }

void UIInterface::OnlineAddContact(char16_t* name, int32_t data) { _OnlineAddContact(name, data); }
void UIInterface::OnlineRemoveContact(char16_t* name) { _OnlineRemoveContact(name); }
void UIInterface::OnlineUpdateContact(char16_t* name, int32_t data) { _OnlineUpdateContact(name, data); }
void UIInterface::OnlineAddBuddy(char16_t* name, int32_t data) { _OnlineAddBuddy(name, data); }
void UIInterface::OnlineRemoveBuddy(char16_t* name) { _OnlineRemoveBuddy(name); }
void UIInterface::OnlineUpdateBuddy(char16_t* name, int32_t data) { _OnlineUpdateBuddy(name, data); }

// 0x101f0c2c (140 bytes): a kid that dies leaves the panels.
void UIInterface::ThingDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr ? 1 : 0, "Thing has been allocated in ThingArray index 0 - shouldn't happen\n");
    CThing* t = static_cast<CThing*>(gThingArray[thing.mIndex].mThing);
    if (t->mKind == 1) {
        gVisitorStatusPanel.KidGone(t);
        AllKidsPanel_KidGone(t);
        LocatorByeByeKid(t);
    }
}

void UIInterface::EndCoasterEdit() { EndCoasterEditMode(); }
void UIInterface::ChangeCoasterButtonsDown(uint32_t mask) { CoasterButtonsDown(mask); }
void UIInterface::ChangeCoasterButtonsEnabled(uint32_t mask) { CoasterButtonsEnabled(mask); }

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

// 0x101f0d14
void UIInterface::KidArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> kid) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    AllKidsPanel_KidAdded(static_cast<CThing*>(gThingArray[kid.mIndex].mThing));
}

// 0x101f0dcc
void UIInterface::StaffRoomEvent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff, int32_t arriving) {
    GAME_ASSERTMSG(gThingArray[0].mThing == nullptr, kThingZero);
    CThing* thing = static_cast<CThing*>(gThingArray[staff.mIndex].mThing);
    if (arriving != 0)
        gStaffRoomStatusPanel.StaffWentIntoStaffRoom(thing);
    else
        gStaffRoomStatusPanel.StaffLeftStaffRoom(thing);
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

void UIInterface::Bankrupt(CObject* gates) { UI_Bankrupt(gates); }
void UIInterface::Congratulations(CObject* feature) { UI_Congratulations(feature); }
// ---- UIInterface
void UIInterface::ShowVersionTag() { ShowVersionNumberTag(); }

// ui_ErrorDisplay.cpp's static initialiser (0x101f499c, unit v5134).
