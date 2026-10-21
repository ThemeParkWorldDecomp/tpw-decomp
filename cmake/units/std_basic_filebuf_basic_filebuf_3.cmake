# Unit v5944: std::basic_filebuf<wchar_t> constructor and open, __tree<pair<const nocase_string, BuddyInfo>>::insert_node_at
# (src/game/MslSupport.cpp). The callees outside the unit are recording fakes in
# tests/unit_std_basic_filebuf_basic_filebuf_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_std_basic_filebuf_basic_filebuf_3 src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_std_basic_filebuf_basic_filebuf_3_KEEP
  __Z16FilebufWcharOpenP17BasicFilebufWcharPKcj   # FilebufWcharOpen(BasicFilebufWchar*, char const*, unsigned int)
  __Z21BuddyTreeInsertNodeAtP9BuddyTreeP13BuddyTreeNodebbPKv   # BuddyTreeInsertNodeAt(BuddyTree*, BuddyTreeNode*, bool, bool, void const*)
  __Z21FilebufWcharConstructP17BasicFilebufWcharj   # FilebufWcharConstruct(BasicFilebufWchar*, unsigned int)
)
