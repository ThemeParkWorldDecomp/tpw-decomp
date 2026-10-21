# Unit v5126: the theme map's tree (find, clear, insert_one; src/game/MslSupport.cpp).
# The callees are recording fakes in tests/unit_std___tree_std_pair_std_map_TbDynamicStringTempl.cpp.
set(TPW_UNIT_std___tree_std_pair_std_map_TbDynamicStringTempl src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_std_pair_std_map_TbDynamicStringTempl_KEEP
  __ZN13ParkThemeTree10insert_oneERKNSt3__14pairIK23TbDynamicStringTemplateIcEP17CFSGlobalParkDataEE   # ParkThemeTree::insert_one(std::__1::pair<TbDynamicStringTemplate<char> const, CFSGlobalParkData*> const&)
  __ZN13ParkThemeTree4findERK23TbDynamicStringTemplateIcE   # ParkThemeTree::find(TbDynamicStringTemplate<char> const&)
  __ZN13ParkThemeTree5clearEv   # ParkThemeTree::clear()
  _gParkTreeInserted
  _gParkTreeNotInserted
)
