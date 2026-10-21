# Unit v5917: std::__tree<unsigned short> find / insert_one / insert_node_at, IPSystem::Startup / Shutdown, ccParkInfo::~ccParkInfo
# (src/game/MslSupport.cpp, src/game/online/FS_Online.cpp). The callees outside the unit are recording fakes in
# tests/unit_std___tree_insert_node_at_3.cpp, which also defines the globals of other files.
set(TPW_UNIT_std___tree_insert_node_at_3 src/game/MslSupport.cpp src/game/online/FS_Online.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_insert_node_at_3_KEEP
  __ZN10ccParkInfo8DestructEPS_s   # ccParkInfo::Destruct(ccParkInfo*, short)
  __ZN10ccParkInfoD1Ev   # ccParkInfo::~ccParkInfo()
  __ZN13UshortSetTree10insert_oneERKt   # UshortSetTree::insert_one(unsigned short const&)
  __ZN13UshortSetTree14insert_node_atEPNS_4nodeEbbRKt   # UshortSetTree::insert_node_at(UshortSetTree::node*, bool, bool, unsigned short const&)
  __ZN13UshortSetTree4findERKt   # UshortSetTree::find(unsigned short const&)
  __ZN8IPSystem7StartupEv   # IPSystem::Startup()
  __ZN8IPSystem8ShutdownEv   # IPSystem::Shutdown()
  _gTreeInsertedFalse
  _gTreeInsertedTrue
)
