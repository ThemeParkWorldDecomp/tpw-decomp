# Unit w37b: thumb dragging, list box selection/drawing, list control teardown and the sprite font selector.
set(TPW_UNIT_UIListBox src/game/ui/InterfaceScrollBar.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/InterfaceDrawEngine.cpp src/game/ui/InterfaceListBox.cpp src/game/ui/InterfaceListControl2.cpp
    src/game/ui/intrface.cpp src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIListBox_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z13recurseDeleteP10tagLBEntry   # recurseDelete(tagLBEntry*)
  __Z14CalcThumbtrackP8CoolRectS_iii   # CalcThumbtrack(CoolRect*, CoolRect, int, int, int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z25UI_DefaultListBoxCallbackP15InterfaceWindowiii   # UI_DefaultListBoxCallback(InterfaceWindow*, int, int, int)
  __Z28UI_DefaultThumbtrackCallbackP15InterfaceWindowiii   # UI_DefaultThumbtrackCallback(InterfaceWindow*, int, int, int)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN15InterfaceWindow19GetPreviousPositionEP8CoolRect   # InterfaceWindow::GetPreviousPosition(CoolRect*)
  __ZN15InterfaceWindow22UpdatePreviousPositionEv   # InterfaceWindow::UpdatePreviousPosition()
  __ZN16InterfaceListBox10ClientDrawEv   # InterfaceListBox::ClientDraw()
  __ZN16InterfaceListBox12DrawFunctionEv   # InterfaceListBox::DrawFunction()
  __ZN16InterfaceListBox14CalcClientAreaEv   # InterfaceListBox::CalcClientArea()
  __ZN16InterfaceListBox14RemoveAllItemsEv   # InterfaceListBox::RemoveAllItems()
  __ZN16InterfaceListBox15GetIndexedEntryEi   # InterfaceListBox::GetIndexedEntry(int)
  __ZN16InterfaceListBoxD1Ev   # InterfaceListBox::~InterfaceListBox()
  __ZN21InterfaceListControl210ClientDrawEv   # InterfaceListControl2::ClientDraw()
  __ZN21InterfaceListControl214CalcClientAreaEv   # InterfaceListControl2::CalcClientArea()
  __ZN21InterfaceListControl2D1Ev   # InterfaceListControl2::~InterfaceListControl2()
  __ZN5CFont3SetENS_6FontIdENS_10RenderTypeE   # CFont::Set(CFont::FontId, CFont::RenderType)
  __ZN8CoolRect14RectIntersectsEPS_   # CoolRect::RectIntersects(CoolRect*)
  __ZN8CoolRect3GetEPsS0_S0_S0_   # CoolRect::Get(short*, short*, short*, short*)
  __ZN8CoolRect3SetEssss   # CoolRect::Set(short, short, short, short)
  __ZN8CoolRect5ShiftEss   # CoolRect::Shift(short, short)
  __ZN8CoolRect6AdjustEssss   # CoolRect::Adjust(short, short, short, short)
  __ZN8CoolRect6MiddleEPsS0_   # CoolRect::Middle(short*, short*)
  __ZN8CoolRect7GetDimsEPsS0_   # CoolRect::GetDims(short*, short*)
  __ZN8CoolRect7PointInEss   # CoolRect::PointIn(short, short)
  __ZN8CoolRectC1Essss   # CoolRect::CoolRect(short, short, short, short)
  __ZN8CoolRectC1Ev   # CoolRect::CoolRect()
  __ZN8CoolRectD1Ev   # CoolRect::~CoolRect()
  _gCFontTable
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
