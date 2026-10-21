# Unit v5L06: the build panel's small UI callees (UIMain_TickRidePreview, UIMain_NotEnoughDough, RemoveRideListPanel,
# InterfaceWindow::FinishWithClientObjectSurface, InterfaceTimer::SetSlice; unit v5L16 adds InterfaceWindow::GetClientObjectSurface).
set(TPW_UNIT_PathInputUI src/game/ui/ui_main.cpp src/game/ui/ui_ridepanel.cpp src/game/ui/InterfaceTimer.cpp
    src/game/ui/InterfaceWindow.cpp platform/arena.cpp)
set(TPW_UNIT_PathInputUI_KEEP
  __Z19RemoveRideListPanelv   # RemoveRideListPanel()
  __Z21UIMain_NotEnoughDoughv   # UIMain_NotEnoughDough()
  __Z22UIMain_TickRidePreviewv   # UIMain_TickRidePreview()
  __ZN14InterfaceTimer8SetSliceEi   # InterfaceTimer::SetSlice(int)
  __ZN15InterfaceWindow22GetClientObjectSurfaceEPiS0_   # InterfaceWindow::GetClientObjectSurface(int*, int*)
  __ZN15InterfaceWindow29FinishWithClientObjectSurfaceEv   # InterfaceWindow::FinishWithClientObjectSurface()
)
