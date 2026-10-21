# Unit v51049: std::locale::~locale, basic_filebuf<char>::imbue / pbackfail, DisableRideHighlight, ESP_GetRandomSpritesFrom,
# ToggleProceduralTexturing, DestroyDynamicMeshSetInstance, CAdvisor::SetPlayingFor (src/game/HLSprite.cpp, src/game/TPInstance.cpp, src/game/TPPointer.cpp, src/game/advisor/Advisor.cpp).
# The callees outside the unit are recording fakes in tests/unit_std_locale_Dtor_locale.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_locale_Dtor_locale src/game/HLSprite.cpp src/game/TPInstance.cpp src/game/TPPointer.cpp
    src/game/advisor/Advisor.cpp platform/arena.cpp)
set(TPW_UNIT_std_locale_Dtor_locale_KEEP
  __Z16LocaleStdDestroyP14StdLocaleWordss   # LocaleStdDestroy(StdLocaleWords*, short)
  __Z20DisableRideHighlighti   # DisableRideHighlight(int)
  __Z24ESP_GetRandomSpritesFrom11ESP_Sprites   # ESP_GetRandomSpritesFrom(ESP_Sprites)
  __Z25ToggleProceduralTexturingv   # ToggleProceduralTexturing()
  __Z29DestroyDynamicMeshSetInstanceP13tag_sDMeshSetP9sHeapInfoj   # DestroyDynamicMeshSetInstance(tag_sDMeshSet*, sHeapInfo*, unsigned int)
  __ZN16BasicFilebufChar5imbueEPKv   # BasicFilebufChar::imbue(void const*)
  __ZN16BasicFilebufChar9pbackfailEi   # BasicFilebufChar::pbackfail(int)
  __ZN8CAdvisor13SetPlayingForEj   # CAdvisor::SetPlayingFor(unsigned int)
  _gRideHighlightDisabled
)
