# Unit v5760: CMessageCentre::Save, Load, UnsubscribeAll (src/game/ui/MessageCentre.cpp). The callees outside the unit are recording
# fakes in tests/unit_CMessageCentre_Save.cpp.
set(TPW_UNIT_CMessageCentre_Save src/game/ui/MessageCentre.cpp platform/arena.cpp)
set(TPW_UNIT_CMessageCentre_Save_KEEP
  __Z25PortSubscribeLoadedThingsR14CMessageCentre   # PortSubscribeLoadedThings(CMessageCentre&)
  __ZN14CMessageCentre14UnsubscribeAllERK6CThing   # CMessageCentre::UnsubscribeAll(CThing const&)
  __ZN14CMessageCentre4LoadERPv   # CMessageCentre::Load(void*&)
  __ZNK14CMessageCentre4SaveERPv   # CMessageCentre::Save(void*&) const
)
