# Unit v5015: CGame::GetLocalisedThemeName (src/game/Game.cpp). The callees outside the unit are recording fakes in
# tests/unit_CGame_GetLocalisedThemeName.cpp, which also defines the globals of other files.
set(TPW_UNIT_CGame_GetLocalisedThemeName src/game/Game.cpp platform/arena.cpp)
set(TPW_UNIT_CGame_GetLocalisedThemeName_KEEP
  __ZNK5CGame21GetLocalisedThemeNameERK23TbDynamicStringTemplateIDsERS1_   # CGame::GetLocalisedThemeName(TbDynamicStringTemplate<char16_t> const&, TbDynamicStringTemplate<char16_t>&) const
)
