# Unit v51025: the buddy map's tree (find, insert_one, destroy, erase; src/game/online/online.cpp). The callees
# outside the unit are recording fakes in tests/unit_std___tree_std_pair_std_map_nocase_string_BuddyI_2.cpp.
set(TPW_UNIT_std___tree_std_pair_std_map_nocase_string_BuddyI_2 src/game/online/online.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_std_pair_std_map_nocase_string_BuddyI_2_KEEP
  __Z13BuddyTreeFindP9BuddyTreePKv   # BuddyTreeFind(BuddyTree*, void const*)
  __Z14BuddyTreeEraseP9BuddyTreeP13BuddyTreeNode   # BuddyTreeErase(BuddyTree*, BuddyTreeNode*)
  __Z16BuddyTreeDestroyP9BuddyTreeP13BuddyTreeNode   # BuddyTreeDestroy(BuddyTree*, BuddyTreeNode*)
  __Z18BuddyTreeInsertOneP21BuddyTreeInsertResultP9BuddyTreePKv   # BuddyTreeInsertOne(BuddyTreeInsertResult*, BuddyTree*, void const*)
)
