# Unit v51029: seven small main-app functions (src/game/keyboard.cpp, src/game/park/SkyObjects.cpp, src/game/rides/RideDB.cpp, src/game/ui/InterfaceFont.cpp, src/game/ui/MessageCentre.cpp). The callees outside the unit are
# recording fakes in tests/unit_CSkyObjects_InitSkyObjects_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CSkyObjects_InitSkyObjects_2 src/game/keyboard.cpp src/game/park/SkyObjects.cpp src/game/rides/RideDB.cpp
    src/game/ui/InterfaceFont.cpp src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_CSkyObjects_InitSkyObjects_2_KEEP
  __Z13CText_DestroyP5CTexts   # CText_Destroy(CText*, short)
  __Z18IosBaseInitDestroyPv   # IosBaseInitDestroy(void*)
  __Z18__sinit_RideDB_cppv   # __sinit_RideDB_cpp()
  __Z19RideDatabaseDestroyPv   # RideDatabaseDestroy(void*)
  __Z20CText_OperatorDeletePv   # CText_OperatorDelete(void*)
  __Z20IosBaseInitConstructPv   # IosBaseInitConstruct(void*)
  __Z20__sinit_keyboard_cppv   # __sinit_keyboard_cpp()
  __Z24FontWrapperVectorDestroyPv   # FontWrapperVectorDestroy(void*)
  __ZN11CSkyObjects14InitSkyObjectsEv   # CSkyObjects::InitSkyObjects()
  __ZN20InterfaceFontWrapper10LastCharInEPKDsss   # InterfaceFontWrapper::LastCharIn(char16_t const*, short, short)
  __ZN20InterfaceFontWrapper8GetWidthEPKDsss   # InterfaceFontWrapper::GetWidth(char16_t const*, short, short)
  __ZNK13CSubscription10SaveResultERPv   # CSubscription::SaveResult(void*&) const
  __ZNK13CSubscription4SaveERPv   # CSubscription::Save(void*&) const
  _gKeyboardF288
  _gKeyboardF28C
  _gKeyboardQuickNode
  _gKeyboardWideNode
  _gKeyboardWideString
  _gRideDBIosInit
  _gRideDBIosNode
  _gRideDBNode
)
