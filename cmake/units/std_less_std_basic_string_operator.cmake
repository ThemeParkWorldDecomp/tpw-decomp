# Unit v51044: small online, lobby and 2D map functions (src/game/online/online.cpp, src/game/ui/ui_2dmap.cpp, src/game/ui/ui_worldLobbyController.cpp). The callees outside the unit are recording
# fakes in tests/unit_std_less_std_basic_string_operator.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_less_std_basic_string_operator src/game/online/online.cpp src/game/ui/ui_2dmap.cpp
    src/game/ui/ui_worldLobbyController.cpp platform/arena.cpp)
set(TPW_UNIT_std_less_std_basic_string_operator_KEEP
  __Z14WideStringLessPvPK13MslWideStringS2_   # WideStringLess(void*, MslWideString const*, MslWideString const*)
  __Z15WStreambufUflowP13MslWStreambuf   # WStreambufUflow(MslWStreambuf*)
  __Z18HideWorldLobbySatsv   # HideWorldLobbySats()
  __Z18ShowWorldLobbySatsv   # ShowWorldLobbySats()
  __Z23WorldLobbySetZoomButtoni   # WorldLobbySetZoomButton(int)
  __ZN13nocase_callee12AllocDestroyEPvP13nocase_string   # nocase_callee::AllocDestroy(void*, nocase_string*)
  __ZN13wstring_alloc7DestroyEPvS0_   # wstring_alloc::Destroy(void*, void*)
  __ZN14CUI2DMapSpriteC1EPv16EUI2DMapSpriteID28TUI2DMapIndividualShowStatusf   # CUI2DMapSprite::CUI2DMapSprite(void*, EUI2DMapSpriteID, TUI2DMapIndividualShowStatus, float)
  __ZN17buddy_tree_callee12AllocDestroyEPvS0_   # buddy_tree_callee::AllocDestroy(void*, void*)
  __ZN17buddy_tree_callee8PairCtorEPvPKv   # buddy_tree_callee::PairCtor(void*, void const*)
)
