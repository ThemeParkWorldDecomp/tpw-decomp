# Unit v5240: SetButton and DoButton (src/game/ui/ui_tags.cpp). The callees outside the unit are recording fakes in
# tests/unit_SetButton.cpp, which also defines the globals and InterfaceWindow's virtuals.
set(TPW_UNIT_SetButton src/game/ui/ui_tags.cpp platform/arena.cpp)
set(TPW_UNIT_SetButton_KEEP
  __Z8DoButton15TagButtonAction12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEi   # DoButton(TagButtonAction, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, int)
  __Z9SetButtonP15InterfaceWindow15TagButtonAction   # SetButton(InterfaceWindow*, TagButtonAction)
)
