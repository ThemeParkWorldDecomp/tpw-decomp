# Unit v51058: twenty small main-app functions (src/game/MslSupport.cpp, src/game/TPGlobals.cpp, src/game/online/OnlinePerson.cpp, src/game/ui/ui_2dmap.cpp, src/game/ui/ui_ErrorDisplay.cpp, src/game/ui/ui_publishParkPanel.cpp, src/game/ui/ui_worldLobbyController.cpp). The callees outside
# the unit are recording fakes in tests/unit_ShowWorldLobbyController.cpp, which also defines the globals of other files.
set(TPW_UNIT_ShowWorldLobbyController src/game/MslSupport.cpp src/game/TPGlobals.cpp src/game/online/OnlinePerson.cpp
    src/game/ui/ui_2dmap.cpp src/game/ui/ui_ErrorDisplay.cpp src/game/ui/ui_publishParkPanel.cpp
    src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp tests/standins/ShowWorldLobbyController.cpp)
set(TPW_UNIT_ShowWorldLobbyController_KEEP
  __Z19UI_2DMap_CallbackFnP15InterfaceWindowiii   # UI_2DMap_CallbackFn(InterfaceWindow*, int, int, int)
  __Z20ErrorMessageCallbackP15InterfaceWindowiii   # ErrorMessageCallback(InterfaceWindow*, int, int, int)
  __Z21__sinit_TPGlobals_cppv   # __sinit_TPGlobals_cpp()
  __Z22RemovePublishParkPanelv   # RemovePublishParkPanel()
  __Z24HideWorldLobbyControllerv   # HideWorldLobbyController()
  __Z24ShowWorldLobbyControllerv   # ShowWorldLobbyController()
  __Z24__sinit_OnlinePerson_cppv   # __sinit_OnlinePerson_cpp()
  __Z26MslArgumentVectorCursorAddP19MslErrorDequeCursori   # MslArgumentVectorCursorAdd(MslErrorDequeCursor*, int)
  __Z26RemoveWorldLobbyControllerv   # RemoveWorldLobbyController()
  __Z28BuddyMapValueCompare_KeyPairPKvS0_S0_   # BuddyMapValueCompare_KeyPair(void const*, void const*, void const*)
  __Z28BuddyMapValueCompare_PairKeyPKvS0_S0_   # BuddyMapValueCompare_PairKey(void const*, void const*, void const*)
  __Z31__sinit_ui_publishParkPanel_cppv   # __sinit_ui_publishParkPanel_cpp()
  __Z33PostRequestUploadCallbackFunctioniPKv   # PostRequestUploadCallbackFunction(int, void const*)
  __Z35__sinit_ui_worldLobbyController_cppv   # __sinit_ui_worldLobbyController_cpp()
  __Z41FindParkOnlineParkInfoPanelCancelCallbackv   # FindParkOnlineParkInfoPanelCancelCallback()
  __ZN15argdeque_callee11CdequeIndexEPvj   # argdeque_callee::CdequeIndex(void*, unsigned int)
  __ZN15argdeque_callee12IterNotEqualEPK20MslArgumentDequeIterS2_   # argdeque_callee::IterNotEqual(MslArgumentDequeIter const*, MslArgumentDequeIter const*)
  __ZN15argdeque_callee9CursorAddEP17MslArgumentCursori   # argdeque_callee::CursorAdd(MslArgumentCursor*, int)
  __ZN15errdeque_callee11CdequeIndexEPvj   # errdeque_callee::CdequeIndex(void*, unsigned int)
  __ZN15errdeque_callee15CdequeDecrementEP19MslErrorDequeCursor   # errdeque_callee::CdequeDecrement(MslErrorDequeCursor*)
  __ZN15errdeque_callee9CursorAddEP19MslErrorDequeCursori   # errdeque_callee::CursorAdd(MslErrorDequeCursor*, int)
  __ZN19wstring_less_callee2LtEPK13MslWideStringS2_   # wstring_less_callee::Lt(MslWideString const*, MslWideString const*)
  _gOnlinePersonWord20
  _gOnlinePersonWord24
  _gPublishParkPanelText
  _gPublishParkPanelTextNode
  _gTPGlobalsDtorVector
  _gTPGlobalsNode
  _gWorldLobbyControllerText
  _gWorldLobbyControllerTextNode
)
