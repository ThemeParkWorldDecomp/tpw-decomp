// Unit v5064: the publish park panel and the online park list panel (src/PublishPanelMsg.cpp; the inventory files them under
// ui_publishParkPanel.cpp and ui_onlineParkListPanel.cpp). The globals below have no known names (invented); the Mac addresses are in
// the comments and the Mac offsets of the members.
#pragma once
#include <cstdint>

#include "FS_OnlineList.h"
#include "InterfaceWindow.h"
#include "TbTList.h"

// 0x102e4864 points to this: the park the player picked (the Mac's words at 0x08 and 0x14; the host's pointer is wider, so the name
// is after the gap).
struct OnlineParkSelection {
    int32_t mF00;                // 0x00
    int32_t mF04;                // 0x04
    int32_t mID;                 // 0x08  the city (list) or the park (upload) number
    int32_t mF0C;                // 0x0c
    int32_t mF10;                // 0x10
    const char16_t* mName;       // 0x14  a pointer on the Mac (the host's is at 0x18)
};
// 0x102e4868: a global whose word at 0x08 is the path of the park file to upload (a pointer on the Mac).
struct OnlineUploadFile {
    int32_t mF00;                // 0x00
    int32_t mF04;                // 0x04
    const char* mPath;           // 0x08
};
extern OnlineParkSelection* gOnlineParkSelection;   // 0x102e4864
extern OnlineUploadFile gOnlineUploadFile;          // 0x102e4868
extern InterfaceWindow* gPublishParkPanel;          // 0x10396278: the panel's window, 0 when none
extern char16_t gPublishParkDescription[0x80];      // 0x1039627c: the description's text (0x7e characters shown)
extern char16_t gPublishParkName[0x10];             // 0x1039637c: the park's name (0xf characters)
extern int16_t gPublishParkPanelScript[2];          // 0x102b11e6: the panel's window script (only its address is used here)
extern int16_t gOnlineParkListScript[2];            // 0x102b1086: the list panel's window script
extern InterfaceWindow* gOnlineParkListPanel;       // 0x10396234
extern void (*gOnlineParkListClose)();              // 0x10396230: the function the list panel's caller gave

// The callbacks the panels register (stand-ins elsewhere; the Mac passes the addresses of their TVectors).
int32_t PublishPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);          // 0x101eca2c
void CreatePublishParkPanel();                                                                    // 0x101eccc8
void CreateOnlineParkListPanel(void (*onClose)());                                                // 0x101ec4e0
void RemovePublishParkPanel();                                                                    // 0x101ecf6c
void PostRequestUploadCallbackFunction(int32_t error, const void* context);                      // 0x101ec9b8
int32_t OnlineParkListPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101ebba8
void OnlineParkListPostGetAllParksInCityCallbackFn(int32_t error, const void* context, TbPtrList<CFSOnlinePark>* parks);   // 0x101ec444

// Unit v5210 (src/PublishPanelMsg_LCFormat.cpp): the list panel's row functions and its fill. LCFormat and LCPostFormat of
// ui_onlineParkListPanel.cpp (the names exist at other addresses; CreateOnlineParkListPanel above still names the wrong ones).
class SwearFilter;
InterfaceWindow* OnlineParkListLCFormat();                 // 0x101ebd58 (LCFormat)
void OnlineParkListLCPostFormat(InterfaceWindow* row);     // 0x101ebef4 (LCPostFormat)
void FillParkList(TbPtrList<CFSOnlinePark>* parks);        // 0x101ec1d4
void RemoveOnlineParkListPanel();                          // 0x101ec840 (stand-in)
extern int32_t gOnlineParkListRowHeight;                   // 0x1039622c  the row height OnlineParkListLCFormat works out once
extern int8_t gOnlineParkListRowInit;                      // 0x10396228  0 until the first call has reset the height
namespace parklist_callee {
void RankCtor(void* mem);                                  // 0x1018f93c InterfaceRankTextObject(NULL) (class undeclared)
// SwearFilter::Filter on the library string the slot holds (a pointer to its representation); defined weakly in the unit's file.
bool FilterMsl(SwearFilter* filter, void* slot, bool replace);
}  // namespace parklist_callee
