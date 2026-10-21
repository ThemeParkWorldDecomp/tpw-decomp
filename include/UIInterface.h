// UIInterface (main app 0x101ef5a8..0x101ef8a0; the inventory files it under
// ui_ErrorDisplay.cpp): switches the UI between the front end, the lobby and the park.
// PARTIAL: InitLobby (src/game/ui/UIInterface.cpp) is translated.
#pragma once
#include <cstdint>

#include "ParkTime.h"

#include "analyser.h"

class CObject;
struct IDXTYPEUniqueNameThingId;
enum ResearchCat : int32_t;

class UIInterface {
public:
    void Load(void* file);  // 0x101f0284 (60 bytes, stand-in; unit v5206): the answer is not read
    void Process();         // 0x101ef5a8
    void InitGeneral();     // 0x101ef5f8
    void DeInitGeneral();   // 0x101ef64c: DeInitGeneralUI (src/game/ui/UIInterface.cpp)
    void InitGame();        // 0x101ef6a4
    void DeInitGame();      // 0x101ef6fc
    void InitLobby();       // 0x101ef750: InitLobbyUI
    void DeInitLobby();     // 0x101ef7a4
    void InitOnline();      // 0x101ef7f8
    void DeInitOnline();    // 0x101ef84c
    void Pause();           // 0x101ef8a0, src/game/ui/UIInterface.cpp: calls PauseUI
    void UnPause();         // 0x101ef8f0, src/game/ui/UIInterface.cpp: calls UnPauseUI
    void DoPostcard();      // 0x101efb6c, src/game/ui/UIInterface.cpp: calls UI_DoPostcard
    void CheckMouseContext(int context);   // 0x101efb0c, src/game/ui/UIInterface.cpp (unit w131): ContextMouse(context)
    void CamcorderMode();   // 0x101f0ffc (200 bytes), untranslated
    // Unit w129 (both untranslated, called by CParkTime::Update and CStaffDB::Update): 0x101efe60 takes the whole nine word date block by value (seven
    // words in registers r4 to r10, two on the stack); 0x101efe04 tells the staff panel that record `slot` changed.
    void DateChange(ParkDate date);
    void StaffDBChange(int32_t slot);
    void SetGameWindowCursor(int16_t cursor);   // 0x101efa54 (unit w143, untranslated; the staff modes pass 9)
    // Unit w144 (untranslated): 0x101f0068 (36 bytes), the shift key state; the callers hand it on as an unsigned short (clrlwi 16), so the
    // answer's type is not known: int32_t here.
    int32_t GetShiftKeyState();
    void ShowFirstPersonWindow(int32_t on);   // 0x101ef994 (unit w141, untranslated)
    // Unit w194 (untranslated): the bankruptcy and the winning windows; the argument is the gates object or the "End" feature's object (NULL for none).
    void Bankrupt(CObject* gates);   // 0x101f11b0 (40 bytes)
    void Congratulations(CObject* feature);   // 0x101f120c (40 bytes)
    // Unit v5182 (untranslated stand-ins): the selected object.
    CObject* GetSelectedObject();             // 0x101effa4 (36 bytes)
    void SetSelectedObject(CObject* object);  // 0x101f0000 (40 bytes)
    // Unit v5257 (stand-ins):
    void ChangeCoasterButtonsEnabled(uint32_t mask);  // 0x101f0f94 (40 bytes)
    void ChangeCoasterButtonsDown(uint32_t mask);     // 0x101f0f2c (40 bytes)
    // Unit v5341 (stand-ins): the selected person (a thing; the kind byte at +2 is read) and the locator removal (nonzero: it did something).
    class CThing* GetSelectedPeep();   // 0x101eff4c (36 bytes)
    int32_t RemoveLocators();          // 0x101f0340 (64 bytes)
    void Save(void* file);             // 0x101f02ec (40 bytes, stand-in; unit v5628): the answer is not read
    // Unit v5574: PostLevelLoad reads the funny time into gUIPostLoadDate (0x1039641c, nine words) and tells the main panel the date.
    void PostLevelLoad();              // 0x101f0124 (300 bytes)
    void PreLevelLoad();               // 0x101f00c0 (52 bytes, stand-in; unit v51006)
    // Unit v5666 (untranslated stand-ins), called by CUIMsgReceiver::ReceiveMessage:
    void EnableResearchButton(bool on);                                                  // 0x101f10f8 (128 bytes)
    void ResearchCategoryComplete(ResearchCat category);                                 // 0x101efc14 (40 bytes)
    void RideDeleted(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);             // 0x101efc84 (4 bytes)
    void RideBrokenDown(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);          // 0x101efce4 (48 bytes)
    void RideCondemned(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> ride);           // 0x101efd74 (48 bytes)
    void ThingDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> thing);             // 0x101f0c2c (140 bytes)
    void KidArrived(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> kid);               // 0x101f0d14 (92 bytes)
    void StaffRoomEvent(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> staff, int32_t arriving);   // 0x101f0dcc (168 bytes)
    // Unit v5960 (stand-in, 40 bytes): shows a tag (CTagSystem::ReceiveMessage builds the parameters on its stack).
    void MessageTag(struct CTagParams& params);   // 0x101efee8
    // Unit v51037: the locator screens (the staff panel for a staff member, else the kid locator).
    void Screen_StaffLoc();   // 0x101f07b4 (76 bytes)
    void Screen_PeepLoc();    // 0x101f0834 (72 bytes)
    // Unit v51048 (stand-in): 0x101ef940 (36 bytes), called by CGame::TogglePause.
    void TogglePause();
    // Unit v51061 (stand-ins): the screens and the map.
    void ShowMap();             // 0x101efbc0
    void ShowGameMainMenu();    // 0x101f03b4
    void Screen_BuyObject();    // 0x101f0410
    void Screen_BuyStaff();     // 0x101f047c
    void Screen_ParkStatus();   // 0x101f04d8
    void Screen_AllItems();     // 0x101f0534
    void Screen_AllStaff();     // 0x101f0594
    void Screen_AllPeeps();     // 0x101f05f4
    void Screen_Finance();      // 0x101f0650
    void Screen_Loans();        // 0x101f06a8
    void Screen_StaffCosts();   // 0x101f06fc
    // Unit v51076 (src/game/ui/UIInterface.cpp): one-call forwarders.
    void ShowVersionTag();                                  // 0x101f1270
    void EndCoasterEdit();                                  // 0x101f0ed4
    void OnlineUpdateBuddy(char16_t* name, int32_t data);   // 0x101f0bc8
    void OnlineRemoveBuddy(char16_t* name);                 // 0x101f0b68
    void OnlineAddBuddy(char16_t* name, int32_t data);      // 0x101f0b08
    void OnlineUpdateContact(char16_t* name, int32_t data); // 0x101f0aa0
    void OnlineRemoveContact(char16_t* name);               // 0x101f0a40
    void OnlineAddContact(char16_t* name, int32_t data);    // 0x101f09dc
    void OnlineReceiveStr(char16_t* text, int32_t data);    // 0x101f0978
    void ToggleHelpText();                                  // 0x101f0908: UI_ToggleHelpText, then the tooltips flag follows UI_HelpTextEnabled
    void Screen_Research();                                 // 0x101f08b0
    void Screen_EntryPrice();                               // 0x101f0758
    void SwitchGameWindow();                                // 0x101ef9fc
};
extern ParkDate gUIPostLoadDate;       // 0x1039641c (defined in src/game/ui/UIInterface.cpp)
extern UIInterface gUIInterface;   // 0x10396440, original name unknown
// Unit v51038: the main panel's research button (0x101a7830, 16 bytes, untranslated; a weak stand-in in src/game/ui/UIInterface.cpp, src/game/ui/ui_mainpanel2.cpp).
void MainPanel_EnableResearchButton(bool on);
