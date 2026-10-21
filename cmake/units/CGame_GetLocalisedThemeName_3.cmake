# Unit v51048: eight small main-app functions (src/game/ActionRecorder.cpp, src/game/Game.cpp, src/game/people/ParkTime.cpp, src/game/ui/InterfaceBar.cpp, src/game/ui/UI_Base.cpp, src/game/ui/ui_visitorStatus.cpp). The callees outside the unit are recording fakes in
# tests/unit_CGame_GetLocalisedThemeName_3.cpp, which also defines the globals of other files and the operator delete that logs the node frees.
set(TPW_UNIT_CGame_GetLocalisedThemeName_3 src/game/ActionRecorder.cpp src/game/Game.cpp src/game/people/ParkTime.cpp
    src/game/ui/InterfaceBar.cpp src/game/ui/UI_Base.cpp src/game/ui/ui_visitorStatus.cpp)
set(TPW_UNIT_CGame_GetLocalisedThemeName_3_KEEP
  __Z21UI_PutDeferredMessageP15InterfaceWindowiiij   # UI_PutDeferredMessage(InterfaceWindow*, int, int, int, unsigned int)
  __Z26__sinit_ActionRecorder_cppv   # __sinit_ActionRecorder_cpp()
  __ZN12InterfaceBar8SetValueEi   # InterfaceBar::SetValue(int)
  __ZN12InterfaceBarmiER15InterfaceWindow   # InterfaceBar::operator-(InterfaceWindow&)
  __ZN5CGame11TogglePauseEb   # CGame::TogglePause(bool)
  __ZN7TbTListI12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtES2_E9RemoveAllEv   # TbTList<tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>>::RemoveAll()
  __ZNK5CGame21GetLocalisedThemeNameERK23TbDynamicStringTemplateIcERS0_IDsE   # CGame::GetLocalisedThemeName(TbDynamicStringTemplate<char> const&, TbDynamicStringTemplate<char16_t>&) const
  __ZNK9CParkTime24GetFunnyTimeForGameTurnsEj   # CParkTime::GetFunnyTimeForGameTurns(unsigned int) const
  _gActionRecorderDtor
  _gActionRecorderNode
  _gActionRecorderString
  _gActionRecorderStringNode
)
