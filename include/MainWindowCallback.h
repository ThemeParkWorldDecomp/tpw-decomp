// Unit v5002 (src/MainWindowCallback.cpp): the "all items" panel's message functions and the all-features panel (ui_allStaffPanel.cpp).
#pragma once
#include <cstdint>

#include "InterfaceWindow.h"
#include "analyser.h"
#include "ui_controls.h"
#include "ui_main.h"
#include "ThingParts2.h"

class CThing;

// ---- globals (no other file names them; defined in src/MainWindowCallback.cpp, names invented)
extern InterfaceWindow* gAllItemsPanel;      // 0x103947e0 the open all-items panel (cleared when it is destroyed)
extern int32_t gAllItemsListType;            // 0x103947dc 0 rides, 1 shops, 2 sideshows, 3 features: what the list shows
extern InterfaceListControl2* gAllItemsList; // 0x103947d8 the list the all-features panel built
extern int32_t gAllRidesSort;                // 0x102a8cf8 the sort column of each kind of list (1 at start)
extern int32_t gAllShopsSort;                // 0x102a8cfc
extern int32_t gAllShowsSort;                // 0x102a8d00
extern int32_t gAllFeaturesSort;             // 0x102a8d04
extern int32_t gAllColumnButtonFont;         // 0x102a8cf4 the font number of the column buttons (8 at start)
extern int16_t gAllFeaturesScript[];         // 0x102a8c3c the all-features list's window script (the image's words, generated at configure time)
extern int16_t gAllShowsScript[];            // 0x102a8b42 the all-sideshows list's window script (data: only its address is used here)

// ---- the unit's functions
int32_t MainWindowCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x10197c2c
int32_t ListTemplateCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);     // 0x10197dac
void CreateAllFeaturesPanel(CThing* thing);                                                        // 0x101979c0
int32_t ActualListCallback(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x10198064

// ---- untranslated callees
void RemoveAllItemsPanel();                              // 0x1019862c
void CreateParkStatusPanel();                            // 0x101aca78
void CreateAllKidsPanel(CThing* thing);                  // 0x101952bc
void CreateAllRidesPanel(CThing* thing);                 // 0x1019762c
void CreateAllShopsPanel(CThing* thing);                 // 0x101972ac
void CreateAllShowsPanel(CThing* thing);                 // 0x10196ed0
void CreateAllStaffPanel(StaffType type, CThing* thing); // 0x10199630
void UIMain_ZoomToThing(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, uint16_t> id);   // 0x10181b00
void UpdateRideListEntries(InterfaceListControl2* list);       // 0x1019600c
void UpdateSideshowListEntries(InterfaceListControl2* list);   // 0x10196208
void MAKECOLUMNBUTTON(InterfaceWindow* window, char16_t* text, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e);   // 0x101818e4
int32_t FillAllFeaturesListCallback(CThing* thing);      // 0x10195f24 (UI_IterateThingList's function)
InterfaceWindow* LCFormat();   // 0x10194df0: the all-kids row maker (src/ui_allKidsPanel_AllKidsPanelMsg.cpp); the all-items one is AllItemsListLCFormat (v5L14)

// Unit v5013 (src/ui_main_kbcapture_UIMain_ShowRidePreview.cpp): the ride and sprite previews.
#include "UI_Base.h"
struct sRidePreviewInfo;
class CPeep;
extern sRidePreviewInfo* gRidePreview;   // 0x10380c44 the ride preview being shown (0 for none)
extern CPeep* gSpritePreviewPeep;        // 0x10380c3c the peep whose sprite is shown (0 for none)
extern int32_t gSpritePreviewHandle;     // 0x10380c40 the spawned sprite copy
extern uint16_t gZoomSkipThing;          // 0x102a7866 the thing id UIMain_ZoomToThing ignores (0 at start)
void UIMain_ShowRidePreview(CoolRect& rect, uint32_t id, int32_t z);              // 0x10181400
void UIMain_ShowSpritePreview(CoolRect& rect, CPeep* peep, int32_t z, float scale);   // 0x10181648
float CalcZTrans(int32_t z);                                                      // 0x101879d0 (src/TailSeven.cpp, unit v5L16)
sRidePreviewInfo* CreateRidePreview(float x, float y, float z, float w, float h, uint32_t id);   // 0x1006c3bc
void DestroyRidePreview(sRidePreviewInfo* info);                                  // 0x1006ccd0
void UIMain_DestroySpritePreview();                                               // 0x10181864
void LCPostFormat(InterfaceWindow* window);   // 0x101c346c: the staff room attach function (src/LCPostFormat.cpp); the all-items one is AllItemsListLCPostFormat (v5L14)

// Unit v5019 (src/CreateAllStaffPanel.cpp): the all-staff panel.
extern int32_t gStaffColumnFont;         // 0x102a9084 the font number of the column buttons (8 at start)
extern int32_t gStaffSort;               // 0x102a9088 the sort column of the staff list (1 at start)
extern InterfaceWindow* gStaffPanel;     // 0x103947f0 the open all-staff panel
extern int32_t gStaffType;               // 0x103947ec the kind of staff the panel lists
extern int16_t gAllStaffScript[];        // 0x102a8d0e the panel's window script (the image's words, generated at configure time)
int32_t AllStaffPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x10198ac4 (untranslated)
int32_t AllStaffPanelLCHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10198d54 (untranslated)
void FillAllStaffList(StaffType type, int32_t flag);   // 0x101987fc (untranslated)
void RemoveAllStaffPanel();   // 0x10199e20 (56 bytes, stand-in)

// Unit v5066 (src/MainWindowCallback_CreateAllItemsPanel.cpp): CreateAllItemsPanel (ui_main.h), the entry handler and the staff list.
class CObject;
extern int16_t gAllItemsScript[];        // 0x102a8884 the all-items panel's window script (the image's words, generated at configure time)
extern uint16_t gAllItemsNoParent;       // 0x102a8d0c the upgrade-parent id meaning "none" (0 at start)
extern InterfaceListControl2* gStaffListControl;   // 0x103947e8 the staff list being filled
int32_t AllItemsLCEntryHandler(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10198264
int32_t FillAllStaffListCallback(CThing* thing);   // 0x10198690 (UI_IterateThingList's function)
void UpdateListEntries();                          // 0x10198944
int32_t UIMain_GetObjectState(CObject* object, int32_t flag);   // 0x10180b84 (untranslated)
// Unit v5694 (src/ui_main_panel_UIMain_GetObjectState.cpp): halfword globals (all 0 at start) UIMain_GetObjectState compares thing ids with.
extern uint16_t gUIMainId7858;   // 0x102a7858 the mechanic-list head meaning "none" for a ride that wants an upgrade
extern uint16_t gUIMainId785A;   // 0x102a785a the staff id meaning "none" (upgrade case)
extern uint16_t gUIMainId785C;   // 0x102a785c handyman head meaning "none" (flagged, relief)
extern uint16_t gUIMainId785E;   // 0x102a785e staff id meaning "none" (flagged, relief)
extern uint16_t gUIMainId7860;   // 0x102a7860 mechanic head meaning "none" (flagged)
extern uint16_t gUIMainId7862;   // 0x102a7862 staff id meaning "none" (flagged)
extern uint16_t gUIMainId7864;   // 0x102a7864 the object id meaning "none" for UpgradesWhat
int32_t IsTrackValid(void* coaster);   // 0x100528d8 (stand-in)
uint32_t UIMain_GetObjectStateTextColour(int32_t state);       // 0x10180fc0 (untranslated; a four-byte answer through a hidden pointer on the Mac)

// Unit v5213 (src/CreateRidePreview.cpp): the ride preview's record and its scale.
#include "m3d2.h"
struct sRidePreviewInfo {            // 0x88 bytes on the Mac, one static block at 0x102d1f20
    uint32_t mActive;                // 0x00
    uint32_t mRide;                  // 0x04  the instance InstanceRide made
    float mF08[16];                  // 0x08  the root node's matrix words (0x10..0x4f)
    float mAngle;                    // 0x48  wrapped by fmod at 2 pi
    float mX;                        // 0x4c
    float mY;                        // 0x50
    float mW;                        // 0x54
    float mH;                        // 0x58
    float mF5C;                      // 0x5c
    float mF60;                      // 0x60
    float mF64;                      // 0x64
    float mF68;                      // 0x68
    float mF6C;                      // 0x6c
    uint32_t mF70;                   // 0x70  not touched
    Ptr32<sTexture> mTextures[4];    // 0x74  sign1 and sign2 of the preview's mesh, then the ride type's
    uint32_t mRideType;              // 0x84
};
static_assert(sizeof(sRidePreviewInfo) == 0x88);
extern sRidePreviewInfo gRidePreviewBlock;   // 0x102d1f20
void DetermineRidePreviewScale(uint32_t ride, float w, float h, float* scale, float* unused, float* offsetX, float* offsetZ,
                               float* rangeX, float* rangeZ, float* ratio, float* distance, float* distanceMid);   // 0x1006c198
void SetRideMobile(uint32_t& ride);   // 0x10068280 (stand-in elsewhere)

// Unit v5219 (src/AddItemToAllRidesList.cpp): the all-kids panel's creation and rows, the rides and sideshows list rows.
extern int16_t gAllKidsScript[];            // 0x102a868c the all-kids panel's window script (the image's words, generated at configure time)
extern int32_t gAllKidsColumnFont;          // 0x102a887c the font number of the column buttons (8 at start)
extern int32_t gAllKidsCounter;             // 0x103947c0 cleared before the list is filled
extern uint16_t gAllRidesNoParent;          // 0x102a8d0a the upgrade-parent id meaning "no other ride" (0 at start)
extern InterfaceListControl2* gAllKidsListControl;   // 0x103947c4 (src/ui_allKidsPanel_AllKidsPanelMsg.cpp)
extern InterfaceWindow* gAllKidsMsgWindow;           // 0x103947c8 (same file)
extern int32_t gAllKidsSelectedIndex;                // 0x102a8880 (same file)
int32_t FillAllKidsListCallback(CThing* thing);      // 0x10194880 (UI_IterateThingList's function)
int32_t AllKidsPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x10194b4c
void AddItemToAllRidesList(const CThing* thing, InterfaceListControl2* list, CObject* object);   // 0x10195978
void AllKidsPanel_KidAdded(CThing* thing);           // 0x101957b8
int32_t FillAllSideshowsListCallback(CThing* thing); // 0x10195dc8 (UI_IterateThingList's function)

// Unit v5153 (src/CreateAllStaffPanel_LCFormat.cpp): the all-staff list's row maker and attach function (LCFormat and LCPostFormat of
// ui_allStaffPanel.cpp; named AllStaff... here because the same names exist at other addresses) and AllStaffPanelLCHandler (above).
extern int32_t gStaffListFormat;         // 0x103947e4 the row height AllStaffLCFormat keeps (0 until the first row)
extern uint32_t gStaffListFont;          // 0x102a9080 the font table index of the list's text (7 at start)
InterfaceWindow* AllStaffLCFormat();                     // 0x101991e8
void AllStaffLCPostFormat(InterfaceWindow* window);      // 0x10199370

// Unit v5335 (src/AddItemToAllRidesList_UpdateSideshowListEntries.cpp): the all-items list's row window (the file's LCFormat, 0x10196388).
extern int32_t gAllItemsRowHeight;      // 0x103947d4 the row window's height, worked out once (0 until then)
extern uint32_t gAllItemsFontIndex;     // 0x102a8cf0 the font table index the height is measured with (7 at start)
InterfaceWindow* AllItemsListLCFormat();   // 0x10196388 (named so because LCFormat exists at other addresses)
namespace allitems_callee {
// The virtual call the row height is worked out with: slot 0xc of the object at +0x10 of the font entry (the entry is the argument);
// answers a 16-bit height. Defined in src/AddItemToAllRidesList_UpdateSideshowListEntries_callees.cpp, faked by the test library.
int16_t FontVirtual(InterfaceFont* font);
}  // namespace allitems_callee

// Unit v5560 (src/ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6.cpp): the all-items list's attach function (the file's LCPostFormat, 0x101966ec).
void AllItemsListLCPostFormat(InterfaceWindow* window);   // 0x101966ec (named so because LCPostFormat exists at other addresses)
namespace allitems_post_callee {
// Callees the host cannot call as the Mac does; defined in src/ui_allKidsPanel_AllKidsPanelMsg_LCPostFormat_6_callees.cpp, faked by the test library.
void* OperatorNew(uint32_t size);                       // 0x102384b8
void SpriteCtor(void* mem);                             // 0x1018d11c InterfaceTextObjectSprite(NULL)
void NumberCtor(void* mem);                             // 0x1018d930 InterfaceNumberTextObject(NULL)
void CashCtor(void* mem);                               // 0x1018de60 InterfaceCashTextObject(NULL)
void GradientCtor(void* mem, const char* mesh);         // 0x101898c8 InterfaceDrawEngineHGradient(mesh)
void WindowSetValue(InterfaceWindow* window, int32_t value);   // the virtual SetValue (slot 0x24 of the table at +0x118)
}  // namespace allitems_post_callee

// Unit v5572 (src/CreateParkStatusPanel.cpp): CreateParkStatusPanel (declared above).
int32_t ThoughtMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);       // 0x101ab968 (untranslated; weak stand-in in the unit's file)
int32_t GraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);    // 0x101ac044 (untranslated; weak stand-in)
extern int16_t gParkStatusScript[];        // 0x102ab734 the park status panel's window script (the image's words, generated at configure time)
extern const char gParkStatusMeshNames[];  // 0x1024e180 "sad_undec.wct", then at +0xe "hap.wct" (the gradients' mesh names)
// Callees the host cannot call as the Mac does (the allocation, constructors of classes whose vtables live elsewhere, the virtual calls
// at vtable +0x24 / +0x2c of the table at window +0x118): weak definitions in the unit's file, faked by the test library.
namespace parkstatus_callee {
void* OperatorNew(uint32_t size);                       // 0x102384b8
void SpriteCtor(void* mem);                             // 0x1018d11c InterfaceTextObjectSprite(NULL)
void DrawSpriteCtor(void* mem);                         // 0x1018c978 InterfaceDrawObjectSprite(NULL)
void NumberCtor(void* mem);                             // 0x1018d930 InterfaceNumberTextObject(NULL)
void GradientCtor(void* mem, const char* meshName);     // 0x101898c8 InterfaceDrawEngineHGradient(meshName)
void WindowSetValue(InterfaceWindow* window, int32_t value);        // virtual slot 0x24
void WindowSetText(InterfaceWindow* window, const char16_t* text);  // virtual slot 0x2c
}  // namespace parkstatus_callee

// Unit v5952 (src/AddItemToAllRidesList_FillAllRidesListCallback_Upgrades.cpp): the upgrades row of the rides list, the shops row, the
// kid-gone removal and the kids list's attach function (the LCPostFormat at 0x10194fa8, named so because LCPostFormat exists at other addresses).
// FillAllFeaturesListCallback (0x10195f24) is declared above.
extern uint16_t gAllRidesNoUpgradeParent;   // 0x102a8d08 (halfword, 0 at start): the upgrade-parent id meaning "no parent"
extern int32_t gAllKidsFontIndex;           // 0x102a8878 (7 at start; defined in src/ui_allKidsPanel_AllKidsPanelMsg.cpp): the kids list's font table index
int32_t FillAllRidesListCallback_Upgrades(CThing* thing);   // 0x10195ba8 (UI_IterateThingList's function)
int32_t FillAllShopsListCallback(CThing* thing);            // 0x10195cb0 (UI_IterateThingList's function)
void AllKidsPanel_KidGone(CThing* thing);                   // 0x10195708
void AllKidsListLCPostFormat(InterfaceWindow* window);      // 0x10194fa8
namespace kidlist_callee {
// Callees the host cannot call as the Mac does (constructors of classes not declared, a stack CoolRect); weak stand-ins in the src file, faked by the test library.
void VisitorNumCtor(void* mem);   // 0x1018eb38 InterfaceVisitorNumTextObject(NULL) (stand-in: the class is not declared)
void RectCtor(void* rect);        // 0x101c7448 CoolRect::CoolRect()
void RectDtor(void* rect);        // 0x101c74e0 CoolRect::~CoolRect() (flag -1)
}  // namespace kidlist_callee

// Unit v5964 (src/MainWindowCallback_CreateAllRidesPanel.cpp)
extern int16_t gAllRidesScript[];                   // 0x102a8a5a the rides and shops lists' window script (the image's words, generated at configure time)
int32_t FillAllRidesListCallback(CThing* thing);    // 0x10195b40 (UI_IterateThingList's function, untranslated)

// Unit v5977 (src/Debug_overlays_DestroyRidePreview.cpp)
void TickRidePreview(sRidePreviewInfo* info);       // 0x1006c970 (808 bytes)
