# Unit v5001: the tag functions AddATag, SaveTags, LoadTags (src/game/ui/ui_tags.cpp). The callees outside the unit are recording fakes in
# tests/unit_SaveTags.cpp, which also defines the globals of other files.
set(TPW_UNIT_SaveTags src/game/ui/ui_tags.cpp platform/msl.cpp platform/arena.cpp)
set(TPW_UNIT_SaveTags_KEEP
  __Z7AddATagR10CTagParams   # AddATag(CTagParams&)
  __Z8LoadTagsPv   # LoadTags(void*)
  __Z8SaveTagsPv   # SaveTags(void*)
  _gMaxTags
  _gTagCount
  _gTagFirstWindow
)
