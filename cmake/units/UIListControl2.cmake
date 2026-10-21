# Unit w93: InterfaceListControl2's item management (the item block, UpdateDisplay, the column formats, AddItem, ChangeItem, the
# current entry) and InterfaceWindow::SetTextOffset and GetDims. The window, the draw object, the message pump and the string
# library are fakes in the unit file; CoolRect is real.
set(TPW_UNIT_UIListControl2 src/game/ui/InterfaceListControl2.cpp src/game/ui/InterfaceWindow.cpp
    src/game/ui/CoolRect.cpp src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp)
set(TPW_UNIT_UIListControl2_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
  __ZN15InterfaceWindow13SetTextOffsetEii   # InterfaceWindow::SetTextOffset(int, int)
  __ZN15InterfaceWindow7GetDimsEPsS0_s   # InterfaceWindow::GetDims(short*, short*, short)
  __ZN21InterfaceListControl210ChangeItemEiP6LCWordiii   # InterfaceListControl2::ChangeItem(int, LCWord*, int, int, int)
  __ZN21InterfaceListControl212AdvanceIndexEiPi   # InterfaceListControl2::AdvanceIndex(int, int*)
  __ZN21InterfaceListControl213AllocNewBlockEi   # InterfaceListControl2::AllocNewBlock(int)
  __ZN21InterfaceListControl213FindFreeEntryERiS0_   # InterfaceListControl2::FindFreeEntry(int&, int&)
  __ZN21InterfaceListControl213GetColumnTypeEs   # InterfaceListControl2::GetColumnType(short)
  __ZN21InterfaceListControl213SetColumnTypeEs10LCDataType   # InterfaceListControl2::SetColumnType(short, LCDataType)
  __ZN21InterfaceListControl213UpdateDisplayEv   # InterfaceListControl2::UpdateDisplay()
  __ZN21InterfaceListControl214ChangeCurEntryEi   # InterfaceListControl2::ChangeCurEntry(int)
  __ZN21InterfaceListControl215GetIndexedEntryEi   # InterfaceListControl2::GetIndexedEntry(int)
  __ZN21InterfaceListControl215SetColumnFormatEiP15InterfaceWindow   # InterfaceListControl2::SetColumnFormat(int, InterfaceWindow*)
  __ZN21InterfaceListControl218SetSelectionObjectEP19InterfaceDrawObject   # InterfaceListControl2::SetSelectionObject(InterfaceDrawObject*)
  __ZN21InterfaceListControl219SetColumnFormatsAllEPFP15InterfaceWindowvEPFvS1_E   # InterfaceListControl2::SetColumnFormatsAll(InterfaceWindow* (*)(), void (*)(InterfaceWindow*))
  __ZN21InterfaceListControl222ShouldIInsertEntryHereEP6LCWordS1_   # InterfaceListControl2::ShouldIInsertEntryHere(LCWord*, LCWord*)
  __ZN21InterfaceListControl27AddItemEP6LCWordiii   # InterfaceListControl2::AddItem(LCWord*, int, int, int)
  __ZN21InterfaceListControl27GetDataEiPi   # InterfaceListControl2::GetData(int, int*)
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
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
)
set(TPW_UNIT_UIListControl2_OWN_CALLLOG ON)
