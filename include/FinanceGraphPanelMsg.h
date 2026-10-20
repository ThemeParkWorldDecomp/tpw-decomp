// Unit v5073: FinanceGraphPanelMsg (src/FinanceGraphPanelMsg.cpp)
#pragma once

#include <cstdint>

#include "History.h"
#include "InterfaceWindow.h"
#include "TbColour.h"
#include "UI_Base.h"
#include "ui_controls.h"

// The finance panel's grapher; only the two range calls are used here.
class CUIGrapher {
public:
    void SetXRange(int32_t a, int32_t b, int32_t c);   // 0x101fd798
    void SetYRange(int32_t a, int32_t b, int32_t c);   // 0x101fd7d8
    // Unit v5202 (stand-ins, bodies not translated here)
    void SetDrawArea(int32_t a, int32_t b, int32_t c, int32_t d);   // 0x101fd818
    void SetXAxisStyle(TbColour colour);                            // 0x101fd880
    void SetYAxisStyle(TbColour colour);                            // 0x101fd8d4
    void SetGridStyle(TbColour colour);                             // 0x101fd928
    void SetBGStyle(TbColour colour);                               // 0x101fd97c
    void Clear();                                                   // 0x101fd9d0
    void DrawAxes();                                                // 0x101fda4c
    void DrawGrid();                                                // 0x101fdb1c
    void DrawGraph(int32_t* data, int32_t count, int32_t limit, TbColour colour);   // 0x101fddbc
    // Unit v51015: the fields DrawAxes reads, at the Mac's offsets (the words not named here are not read by it).
    int32_t mF00;      // 0x00
    int32_t mF04;      // 0x04
    int32_t mF08;      // 0x08
    int32_t mF0C;      // 0x0c
    int32_t mF10;      // 0x10
    int32_t mF14;      // 0x14
    int32_t mF18;      // 0x18
    int32_t mF1C;      // 0x1c
    int32_t mF20;      // 0x20
    int32_t mF24;      // 0x24
    int32_t mF28;      // 0x28
    int32_t mF2C;      // 0x2c
    uint8_t mXAxis[5];   // 0x30 (a TbColour: alpha, red, green, blue, index)
    uint8_t mYAxis[5];   // 0x35 (a TbColour)
    // Units v51056 and v51047: the two styles the setters write (0x44 bytes in all).
    uint8_t mGrid[5];    // 0x3a (a TbColour)
    uint8_t mBG[5];      // 0x3f (a TbColour: Clear fills the draw area with it)
};

int32_t GetMaxYValue();                                                                 // 0x1019ef1c
int32_t GetMinYValue();                                                                 // 0x1019f0bc
void SetupGraph(CUIGrapher& grapher, CoolRect& rect, int32_t a, int32_t b, int32_t c, int32_t d, int32_t e,
                int32_t f);                                                             // 0x101f6480
void DrawMeGraph(InterfaceWindow* window, CUIGrapher& grapher, void (*callback)(CUIGrapher&));   // 0x101f66c0
void RemoveFinancePanelPanel();                                                         // 0x101a0c8c

extern uint8_t gFinanceKissMyLines[16];     // 0x10267ba0: TVector of KissMyLines (its address is passed to DrawMeGraph)
extern uint32_t gFinanceGraphFlags;         // 0x102a9ee4: selected-button bits, 1 at start
extern CUIGrapher gFinanceGrapher;          // 0x10394894
extern int32_t gFinanceGraphMode;           // 0x10394900
extern InterfaceWindow* gFinanceGraphPanel; // 0x10394904

int32_t FinanceGraphPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1019f5e4

// Unit v5142: the park status panel (src/ui_loginpanel_msg_sinit_ui_parkStatus_cpp.cpp)
void CUIGrapher_Construct(CUIGrapher* self);   // 0x101fd6e8: CUIGrapher::CUIGrapher(), a stand-in
void __sinit_ui_parkStatus_cpp();               // 0x101adbe4
extern CUIGrapher gParkStatusGrapher;           // 0x10394a28
extern uint8_t gParkStatusColours[5][5];        // 0x10394a6c, 71, 76, 7b, 80: alpha, red, green, blue, palette index
extern int32_t gParkStatusRange;                // 0x10394a88
extern InterfaceWindow* gParkStatusPanel;       // 0x10394a8c
extern uint32_t gParkStatusFlags;               // 0x102abbac: bits 0..4 select the histories, 8 at start
extern uint8_t gParkInfoLines[16];              // 0x10267ca8: TVector of ParkInfoLines (its address is passed to DrawMeGraph)

// Unit v5202: GraphLine (src/MinAnalyserValue_SetupGraph.cpp)
extern int32_t gGraphLineBuffer[160];   // 0x10396480: the values handed to DrawGraph
void GraphLine(CUIGrapher& grapher, CHistory<float, 144>* history, TbColour& colour, int32_t step, int32_t mode);   // 0x101f62cc
void GraphLine(CUIGrapher& grapher, CHistory<int32_t, 144>* history, TbColour& colour, int32_t step, int32_t mode);   // 0x101f6124

// Unit v5232: the park status panel's message function (src/ParkInfoLines.cpp)
extern int32_t gParkStatusLastTotal;     // 0x10394a18: the visitors since the beginning the panel shows
extern int32_t gParkStatusLastInPark;    // 0x10394a1c: the visitors in the park the panel shows
void RemoveParkStatusPanel();            // 0x101ada00 (stand-in, 56 bytes)
void DoDominantThoughts();               // 0x101aba14
void ParkInfoLines(CUIGrapher& grapher);   // 0x101abb90

// Unit v5377: __sinit_ui_FinancePanel_cpp (src/sinit_ui_FinancePanel_cpp.cpp)
void __sinit_ui_FinancePanel_cpp();            // 0x101a0dbc
extern uint8_t gFinancePanelColours[8][5];     // 0x103948d8, dd, e2, e7, ec, f1, f6, fb: alpha, red, green, blue, palette index
int32_t ParkStatusPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x101abce4

// Unit v5904
void KissMyLines(CUIGrapher& grapher);   // 0x1019f25c (424 bytes)

// Unit v5160: CUIGrapher::DrawGrid / DrawGraph (src/ui_cursor_classes_CUIGrapher_DrawGraph.cpp)
// Object offsets (Mac): 0x00 x span, 0x04 y span, 0x08 left, 0x0c top, 0x10 right, 0x14 bottom, 0x1c y min, 0x24 y max,
// 0x28 columns, 0x2c rows, 0x30 grid label TbColour, 0x3a line TbColour (5 bytes each, read by byte).
extern char16_t gGrapherNumberFormat[];   // 0x1024f70c: u"%d"
extern int32_t gGrapherFont;              // 0x102b1558: 10

// Unit v51004 (src/MainWindowCallback_CreateAllItemsPanel_AllStaffPanelMsg.cpp)
int32_t FinancePanelPanelMsg(InterfaceWindow* window, int32_t message, int32_t a, int32_t b);   // 0x1019f434 (368 bytes)
