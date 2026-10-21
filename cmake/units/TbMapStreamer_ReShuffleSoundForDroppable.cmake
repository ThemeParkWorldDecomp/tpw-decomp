# Unit v5047: TbMapStreamer::ReShuffleSoundForDroppable and SaveCatgoryFixupTable (src/sound/TbMapStreamer.cpp).
# The callees outside the unit are recording fakes in tests/unit_TbMapStreamer_ReShuffleSoundForDroppable.cpp.
set(TPW_UNIT_TbMapStreamer_ReShuffleSoundForDroppable src/sound/TbMapStreamer.cpp platform/arena.cpp
    tests/standins/TbMapStreamer_ReShuffleSoundForDroppable.cpp)
set(TPW_UNIT_TbMapStreamer_ReShuffleSoundForDroppable_KEEP
  __ZN13TbMapStreamer21SaveCatgoryFixupTableEPN3bfl10FileSystem5CFileE   # TbMapStreamer::SaveCatgoryFixupTable(bfl::FileSystem::CFile*)
  __ZN13TbMapStreamer26ReShuffleSoundForDroppableEP10CSoundData   # TbMapStreamer::ReShuffleSoundForDroppable(CSoundData*)
  __ZN18TbMapStreamerFixup10NameLengthERK23TbDynamicStringTemplateIcE   # TbMapStreamerFixup::NameLength(TbDynamicStringTemplate<char> const&)
  __ZN18TbMapStreamerFixup10StringInitEPvPKc   # TbMapStreamerFixup::StringInit(void*, char const*)
  __ZN18TbMapStreamerFixup13StringDestroyEPvi   # TbMapStreamerFixup::StringDestroy(void*, int)
  __ZN18TbMapStreamerFixup13StringReplaceEPviii   # TbMapStreamerFixup::StringReplace(void*, int, int, int)
  __ZN18TbMapStreamerFixup8NameTextERK23TbDynamicStringTemplateIcE   # TbMapStreamerFixup::NameText(TbDynamicStringTemplate<char> const&)
)
