# Unit v5017: the online outbox panel's helpers (FillOutboxList, SetCurrentEmailInfo, CalcEmailAddressScrollBar, UpdatePreview;
# src/game/ui/ui_onlineOutbox.cpp). The callees outside the unit are recording fakes in
# tests/unit_CalcEmailAddressScrollBar.cpp, which also defines the globals of other files. CoolRect is the program's (UI_Base.cpp).
set(TPW_UNIT_CalcEmailAddressScrollBar src/game/ui/ui_onlineOutbox.cpp src/game/ui/CoolRect.cpp
    src/game/ui/CoolRegion.cpp src/game/ui/UI_Base.cpp platform/arena.cpp)
set(TPW_UNIT_CalcEmailAddressScrollBar_KEEP
  __Z13UI_hash_cakesPKcj   # UI_hash_cakes(char const*, unsigned int)
  __Z13UpdatePreviewv   # UpdatePreview()
  __Z14FillOutboxListv   # FillOutboxList()
  __Z19SetCurrentEmailInfoi   # SetCurrentEmailInfo(int)
  __Z19UI_SetRealScreenRezss   # UI_SetRealScreenRez(short, short)
  __Z25CalcEmailAddressScrollBarv   # CalcEmailAddressScrollBar()
  __ZN10CoolRegion5ShiftEss   # CoolRegion::Shift(short, short)
  __ZN10CoolRegion6AdjustEssss   # CoolRegion::Adjust(short, short, short, short)
  __ZN10CoolRegion6MiddleEPsS0_   # CoolRegion::Middle(short*, short*)
  __ZN10CoolRegion7PointInEss   # CoolRegion::PointIn(short, short)
  __ZN10CoolRegionD1Ev   # CoolRegion::~CoolRegion()
  __ZN10CoolRegioneqERS_   # CoolRegion::operator==(CoolRegion&)
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
  _gEmailSeparators
  _gOutboxCursor
  _gOutboxPreview
  _gOutboxWindow
  _gUIHighPresets
  _gUIRealScreenHeight
  _gUIRealScreenWidth
  _gUIUnitWidth
  _gUIWideAspect
  __Z16UI_InterfaceSizev   # UI_InterfaceSize()
  _gUIInterfaceSizeOn
  _gUIScale
  _gUIUnitHeight
)
