# Unit v5029: the research panel bar messages (src/game/ui/ui_researchPanel.cpp). TankButton and AnimatingPumpMsg are recording fakes in
# tests/unit_RideMsg.cpp.
set(TPW_UNIT_RideMsg src/game/ui/ui_researchPanel.cpp platform/arena.cpp)
set(TPW_UNIT_RideMsg_KEEP
  __Z10FeatureMsgP15InterfaceWindowiii   # FeatureMsg(InterfaceWindow*, int, int, int)
  __Z6UpgMsgP15InterfaceWindowiii   # UpgMsg(InterfaceWindow*, int, int, int)
  __Z7RideMsgP15InterfaceWindowiii   # RideMsg(InterfaceWindow*, int, int, int)
  __Z7ShopMsgP15InterfaceWindowiii   # ShopMsg(InterfaceWindow*, int, int, int)
  __Z7ShowMsgP15InterfaceWindowiii   # ShowMsg(InterfaceWindow*, int, int, int)
)
