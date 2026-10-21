// ui_Postcard.cpp's panel and keyboard.cpp's postcard key (main app, unit w102).
#pragma once
#include <cstdint>
#include "InterfaceWindow.h"
#include "platform/samsutils.h"

class Postcard;
void UI_DoPostcard();             // 0x101b232c
void UI_ActuallyDoPostcard();     // 0x101b20b4
void CreatePostcardPanel();       // 0x101b19fc
int32_t PostcardPanelMsg(InterfaceWindow*, int32_t, int32_t, int32_t); // 0x101b1660
void RemovePostcardPanel();       // 0x101b2050
void RefreshPreviewWindow(InterfaceWindow*); // 0x101b0ae4
// The original leaves the default handler's answer in r3 at its return.
int32_t PostcardPreviewMsg(InterfaceWindow*, int32_t, int32_t, int32_t); // 0x101b1494
int32_t TakePostCard();           // 0x1014e678

// Original names unknown, named by their uses and edit box ids.
extern Postcard* gPostcard;                    // 0x103956f4, original name unknown
extern InterfaceWindow* gPostcardPanel;        // 0x103956fc, original name unknown
extern int32_t gPostcardSent;                  // 0x10394b14, original name unknown
extern int32_t gPreviewDragging;               // 0x10394b10, original name unknown
extern int32_t gPreviewLastX;                  // 0x10394b0c, original name unknown
extern uint32_t gPreviewLastY;                 // 0x10394b08, original name unknown
extern int32_t gPreviewOffsetX;                // 0x10394b1c, original name unknown
extern int32_t gPreviewOffsetY;                // 0x10394b20, original name unknown
extern uint8_t gAddressEdited;                 // 0x10394ba4, original name unknown, box 0x5da4
extern uint8_t gMessageEdited;                 // 0x10394ba5, original name unknown, box 0x5da3
extern uint8_t gTempPathFetched;               // 0x10395026, original name unknown
extern char gTempPath[0x200];                  // 0x10394e26, original name unknown
extern char16_t gPostcardMessage[0x100];        // 0x10395028, original name unknown
extern char16_t gPostcardAddress[0x60];         // 0x1039522a, original name unknown
extern char16_t gPostcardText5da5[0x200];        // 0x103952ec, original name unknown
extern int16_t gPostcardPanelDescription[238]; // 0x102ac00a, original name unknown

#include "ui_main.h"   // the UIMain_* helpers (unit w101) used to be redeclared here

// Untranslated boundaries of this unit.
void CreateAddressBook();          // 0x101b07a4
int32_t AddressesValid();          // 0x101b0d88
void SendPostcard();               // 0x101b0f9c
void ChatPrePostCard();            // 0x101e568c
void ChatGoFullScreen();           // 0x101e57bc
void ChatReinstateAfterPostcard();  // 0x101e5700
int32_t GameWindowToggle_Hidden();  // 0x101a8c1c
void ToggleGameWindow();           // 0x101a8ac0
void UI_DrawPostcardText(char16_t*, void*, int32_t, int32_t,
                         SamsUtilities::URectangle::LongRect, uint32_t, int32_t); // 0x1018bbc4
namespace NS_MacDoze {
uint32_t GetTempPath(uint32_t size, char* path); // platform 0x1000515c
}

// Unit v5012 (src/ui_Postcard_panel_CreateAddressBook.cpp): the address book panel.
void RemoveAddressBookEntry();     // 0x101b0420
void NewEntryInAddressBook();      // 0x101b0168
extern InterfaceWindow* gAddressBookPanel;     // 0x103956f8, original name unknown
extern char16_t gAddressEditText[0x40];        // 0x10394ba6, original name unknown: the edit box's text
extern char16_t gAddressRemoveName[0x40];      // 0x10394b24, original name unknown: the removal dialog's address
extern int16_t gAddressBookDescription[];      // 0x102ac1e6, original name unknown: the panel's script, 184 words (generated)
// Untranslated neighbours the unit points at (Mac TVectors 0x10267d20, 0x10267d18, 0x10267d10, 0x10267d08, 0x10267d00).
int32_t AddressBookMsg(InterfaceWindow*, int32_t, int32_t, int32_t);   // 0x101afb34
int32_t NameEditMsg(InterfaceWindow*, int32_t, int32_t, int32_t);      // 0x101afea4
int32_t DeleteEntryOK(void*);                                          // 0x101b0368
InterfaceWindow* PostcardLCFormat();                                   // 0x101b0620 (LCFormat of this file)
void PostcardLCPostFormat(InterfaceWindow*);                           // 0x101b06d4 (LCPostFormat of this file)
void FillAddressList(int32_t);                                         // 0x101aff60
void AddToAddressField(char16_t*);                                     // 0x101b0e98
void RemoveAddressBook();                                              // 0x101b0a84 (unit v5055 declares it)

// Unit v5884 (src/ui_Postcard_panel_SendPostcard.cpp): the e-mail completion callback and the send half of the panel.
enum EmailCompletionCode : int32_t {};
enum EmailErrorCode : int32_t {};
void IllCallYouBack(EmailCompletionCode code, EmailErrorCode error);   // 0x101b0ca4
extern char gPostcardJpgPath[0x200];   // 0x10394c26, original name unknown: <temp path>postcard.jpg
extern int32_t gPostcardCounter;       // 0x10394b18, original name unknown: numbers the copies sent
extern uint8_t kIllCallYouBackVector[8];            // TVector 0x10267cf8
extern uint8_t kCSubscriptionHeaderCtorVector[8];   // TVector 0x102657e8
extern uint8_t kCSubscriptionHeaderDtorVector[8];   // TVector 0x10267288
extern "C" void __construct_array(void* array, void* ctor, void* dtor, int32_t size, int32_t count);
char16_t* wcschr(const char16_t* text, char16_t c);   // the MSL import (glue 0x10238c68)
// Callees outside the unit (untranslated or imports; fakes in the test library).
namespace sendpostcard_callee {
void* New(uint32_t size);                                   // operator new (glue 0x102384b8)
int32_t Sprintf(char* dst, const char* format, ...);        // sprintf (glue 0x10238578): %s, %ls and %08d are used
void PostcardSave(Postcard* card, char* path);              // Postcard::save 0x100c0c20
void EmailCtor(void* email);                                // Email::Email 0x10202244
void StringAssign(void* str, const char* text, uint32_t length);   // std::string::assign 0x101b2440 (the field's address)
void StringCtor(void* str, const char* text);               // std::string::string 0x10002338
void ListInsert(void* list, void* where, const void* str);  // std::list<std::string>::insert 0x101b25e0
void StringDtor(void* str, int16_t flag);                   // std::string::~string 0x1000221c
void OnlineSystemCtor(void* system);                        // CFSOnlineSystem::CFSOnlineSystem 0x1016f7d0
void UserInfoCtor(void* info);                              // CFSUserInfo::CFSUserInfo 0x1017de54
void WindowSetText(InterfaceWindow* window, char16_t* text);   // the virtual at slot 0x2c of the table at +0x118
}
// The login getters and WStringAssign are outbox_callee's (include/email.h).
