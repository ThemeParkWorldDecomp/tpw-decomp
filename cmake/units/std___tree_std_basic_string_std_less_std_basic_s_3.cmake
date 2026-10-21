# Unit v5993: the set<wstring> tree's insert_node_at, erase and destroy (src/game/MslSupport.cpp). The callees
# outside the unit are recording fakes in tests/unit_std___tree_std_basic_string_std_less_std_basic_s_3.cpp.
set(TPW_UNIT_std___tree_std_basic_string_std_less_std_basic_s_3 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_std_basic_string_std_less_std_basic_s_3_KEEP
  __ZN11WStringTree14insert_node_atEPN15MslRedBlackTree9node_baseEbbPKN3IME13MslWideStringE   # WStringTree::insert_node_at(MslRedBlackTree::node_base*, bool, bool, IME::MslWideString const*)
  __ZN11WStringTree5eraseEPN15MslRedBlackTree9node_baseE   # WStringTree::erase(MslRedBlackTree::node_base*)
  __ZN11WStringTree7destroyEPN15MslRedBlackTree9node_baseE   # WStringTree::destroy(MslRedBlackTree::node_base*)
  __ZN19wstring_tree_callee14NodeDeallocateEPvP15WStringTreeNodem   # wstring_tree_callee::NodeDeallocate(void*, WStringTreeNode*, unsigned long)
)
