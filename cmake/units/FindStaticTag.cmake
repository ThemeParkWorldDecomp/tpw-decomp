# Unit v5249: FindStaticTag and AddTag (src/game/ui/ui_tags.cpp). AddATag and ShowTagPanel are recording fakes in
# tests/unit_FindStaticTag.cpp, which also defines gStaticTags.
set(TPW_UNIT_FindStaticTag src/game/ui/ui_tags.cpp platform/arena.cpp)
set(TPW_UNIT_FindStaticTag_KEEP
  __Z13FindStaticTagP15InterfaceWindow   # FindStaticTag(InterfaceWindow*)
  __Z6AddTagR10CTagParams   # AddTag(CTagParams&)
)
