// Unit v5279: the online panel's contacts and messages lists. Mac file ui_onlineParkFindPanel.cpp.
//   0x101e7648 ContactsLCEntryHandler, 0x101e776c MessagesLCEntryHandler, 0x101e7a58 ContactsLCFormat, 0x101e7b34 ContactsLCPostFormat,
//   0x101e8068 _OnlineAddContact, 0x101e812c _OnlineRemoveContact, 0x101e81f4 _OnlineUpdateContact
#include <cstdint>

#include "FillLenderList.h"
#include "InterfaceFont.h"
#include "InterfaceWindow.h"
#include "UI_Base.h"
#include "ui_controls.h"
#include "ui_main.h"

uint32_t gContactsFontIndex = 7;                                                  // 0x102b0c4c
uint8_t gMessagesLCColours[32] = {0x00, 0xe2, 0x06, 0x00};                        // 0x102b0c58
uint32_t gContactsLCColours[4] = {0xffffff00u, 0, 0, 0};                          // 0x102b0c78
int32_t gOnlineContactsBusy = 0;                                                  // 0x10395e7c

// 0x101e7648: message 0x403 sets the font colour from the table by the low bits of b (bit 0 clear: word 0; else bit 1 clear: word 3;
// else bit 2 clear: word 1; else word 2); the bytes are the word's, most significant first. Every message then goes to the default handler.
int32_t ContactsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        uint32_t colour;
        if ((b & 1) == 0)
            colour = gContactsLCColours[0];
        else if ((b & 2) == 0)
            colour = gContactsLCColours[3];
        else if ((b & 4) == 0)
            colour = gContactsLCColours[1];
        else
            colour = gContactsLCColours[2];
        window->SetFontColour(colour >> 24, (colour >> 16) & 0xff, (colour >> 8) & 0xff, 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e776c: message 0x403 sets the font colour from the entry b & 7 of the table.
int32_t MessagesLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b) {
    if (message == 0x403) {
        const uint8_t* entry = gMessagesLCColours + ((static_cast<uint32_t>(b) << 2) & 0x1c);
        window->SetFontColour(entry[0], entry[1], entry[2], 0xff);
    }
    return window->CallDefaultWindowHandler(message, a, b);
}

// 0x101e7a58: the row window, 100 wide, as high as the font (scaled by 0x600 / the real screen height) plus 4.
InterfaceWindow* ContactsLCFormat() {
    InterfaceFont* font = GetFontTableEntry(gContactsFontIndex);
    int16_t height = static_cast<int16_t>(font->GetHeight());
    int16_t bottom = static_cast<int16_t>(height * 0x600 / gUIRealScreenHeight + 4);
    void* memory = lender_callee::OperatorNew(0x138);
    if (memory != nullptr) lender_callee::WindowCtor(memory, nullptr, 0, 0, 0, 0, 100, bottom);
    InterfaceWindow* window = static_cast<InterfaceWindow*>(memory);
    window->SetCallbackFunction(ContactsLCEntryHandler);
    return window;
}

// 0x101e7b34
void ContactsLCPostFormat(InterfaceWindow* window) {
    void* memory = lender_callee::OperatorNew(0x19c);
    if (memory != nullptr) lender_callee::SpriteCtor(memory);
    window->SetClientDrawObject(static_cast<InterfaceTextObjectSprite*>(memory));
    window->SetFont(GetFontTableEntry(gContactsFontIndex));
    window->SetFontColour(0xff, 0xff, 0xff, 0xff);
}

// 0x101e8068
void _OnlineAddContact(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        UI_LCAdd item;
        item.mRaw = 0;
        item.mText = name;
        list->AddItem(&item, data, -1, 1);
    }
}

// 0x101e812c
void _OnlineRemoveContact(char16_t* name) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) list->RemoveItem(position);
    }
}

// 0x101e81f4
void _OnlineUpdateContact(char16_t* name, int32_t data) {
    if (gOnlinePanel != nullptr && gOnlineContactsBusy == 0) {
        InterfaceWindow* window = gOnlinePanel->GetWindow(0xc3123);
        InterfaceListControl2* list = static_cast<InterfaceListControl2*>(window->GetWindow(0xc3124));
        int32_t position = list->FindString(name, 1);
        if (position > -1) {
            list->SetData(position, data);
            list->UpdateDisplay();
        }
    }
}
