# Unit v5899: std::__tree<nocase_string,...> erase, destroy, insert_node_at (src/game/MslSupport.cpp). The callees outside the unit are
# recording fakes in tests/unit_std___tree_erase_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_std___tree_erase_2 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_erase_2_KEEP
  __ZN13NocaseSetTree14insert_node_atEPNS_4nodeEbbRK13nocase_string   # NocaseSetTree::insert_node_at(NocaseSetTree::node*, bool, bool, nocase_string const&)
  __ZN13NocaseSetTree5eraseEPNS_4nodeE   # NocaseSetTree::erase(NocaseSetTree::node*)
  __ZN13NocaseSetTree7destroyEPNS_4nodeE   # NocaseSetTree::destroy(NocaseSetTree::node*)
)
