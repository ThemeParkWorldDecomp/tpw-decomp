// ui_ridepanel.cpp range of the main app (SimTheme Park 2.04b3), unit v5L06: RemoveRideListPanel (0x101b8c28, 56 bytes).
#include "UI_Base.h"
#include "ui_main.h"

// Message 4 to the ride list panel (sent to NULL as well when there is none, as the Mac does).
void RemoveRideListPanel() { UI_SendMessage(gRideListPanel, 4, 0, 0); }
