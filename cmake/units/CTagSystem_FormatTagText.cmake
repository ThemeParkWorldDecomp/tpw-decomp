# Unit v5626: CTagSystem::FormatTagText (src/game/Thing.cpp). The callees outside the unit are recording fakes in
# tests/unit_CTagSystem_FormatTagText.cpp, which also defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_CTagSystem_FormatTagText src/game/Thing.cpp platform/arena.cpp)
set(TPW_UNIT_CTagSystem_FormatTagText_KEEP
  __ZN10CTagSystem13FormatTagTextEPK15TagTableElement12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtEjPK13CMsgChallenge   # CTagSystem::FormatTagText(TagTableElement const*, tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>, unsigned int, CMsgChallenge const*)
  _gTagNoRideType
  _gTagNoThingId
)
