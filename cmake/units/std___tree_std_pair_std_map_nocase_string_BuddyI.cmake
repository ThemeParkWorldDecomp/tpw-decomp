# Unit v51126: tree constructors, insert_one, destroyers, deque tear_down and four UI forwarders.
set(TPW_UNIT_std___tree_std_pair_std_map_nocase_string_BuddyI src/game/MslSupport.cpp src/game/ui/UIInterface.cpp
    src/game/ui/ui_FeatureStatus.cpp platform/arena.cpp)
set(TPW_UNIT_std___tree_std_pair_std_map_nocase_string_BuddyI_KEEP
  __ZN11UIInterface10InitOnlineEv   # UIInterface::InitOnline()
  __ZN15errdeque_callee19MakeIteratorMutableEP21MslErrorDequeIteratorP13MslErrorDequej   # errdeque_callee::MakeIteratorMutable(MslErrorDequeIterator*, MslErrorDeque*, unsigned int)
  __ZN18FeatureStatusPanel12DeInitWindowEv   # FeatureStatusPanel::DeInitWindow()
  __ZN18FeatureStatusPanel18DeInitStatusWindowEv   # FeatureStatusPanel::DeInitStatusWindow()
  __ZN18FeatureStatusPanel6RemoveEv   # FeatureStatusPanel::Remove()
  __ZN21filebuf_wclose_callee6GetlocEPvS0_   # filebuf_wclose_callee::Getloc(void*, void*)
  __ZN6v5112613BuddyTreeCtorEPNS_11TreeHeader4EPKh   # v51126::BuddyTreeCtor(v51126::TreeHeader4*, unsigned char const*)
  __ZN6v5112614NocaseTreeCtorEPNS_11TreeHeader3EPKh   # v51126::NocaseTreeCtor(v51126::TreeHeader3*, unsigned char const*)
  __ZN6v5112615ArgumentDestroyEP8Argument   # v51126::ArgumentDestroy(Argument*)
  __ZN6v5112615WStringTreeCtorEPNS_11TreeHeader3EPKh   # v51126::WStringTreeCtor(v51126::TreeHeader3*, unsigned char const*)
  __ZN6v5112615gBuddyTreeCountE   # v51126::gBuddyTreeCount
  __ZN6v5112616gNocaseTreeCountE   # v51126::gNocaseTreeCount
  __ZN6v5112617gWStringTreeCountE   # v51126::gWStringTreeCount
  __ZN6v5112619BuddyNodeDeallocateEPvS0_j   # v51126::BuddyNodeDeallocate(void*, void*, unsigned int)
  __ZN6v5112619NocaseTreeInsertOneEPNS_16TreeInsertResultEPNS_11TreeHeader3EPKv   # v51126::NocaseTreeInsertOne(v51126::TreeInsertResult*, v51126::TreeHeader3*, void const*)
  __ZN6v5112619gNocaseTreeInsertedE   # v51126::gNocaseTreeInserted
  __ZN6v5112620WStringTreeInsertOneEPNS_16TreeInsertResultEPNS_11TreeHeader3EPKv   # v51126::WStringTreeInsertOne(v51126::TreeInsertResult*, v51126::TreeHeader3*, void const*)
  __ZN6v5112620gWStringTreeInsertedE   # v51126::gWStringTreeInserted
  __ZN6v5112621ArgumentDequeTearDownEP16MslArgumentDeque   # v51126::ArgumentDequeTearDown(MslArgumentDeque*)
  __ZN6v5112621ArgumentDestroyRange2EP8ArgumentS1_   # v51126::ArgumentDestroyRange2(Argument*, Argument*)
  __ZN6v5112621ArgumentVectorDestroyEPv   # v51126::ArgumentVectorDestroy(void*)
  __ZN6v5112621NocaseWStringDestructEPjs   # v51126::NocaseWStringDestruct(unsigned int*, short)
  __ZN6v5112621WStringNodeDeallocateEPvS0_j   # v51126::WStringNodeDeallocate(void*, void*, unsigned int)
  __ZN6v5112622gNocaseTreeNotInsertedE   # v51126::gNocaseTreeNotInserted
  __ZN6v5112623gWStringTreeNotInsertedE   # v51126::gWStringTreeNotInserted
  __ZN6v5112626ArgVectorPtrCdequeDestructEPNS_18ArgVectorPtrCdequeEs   # v51126::ArgVectorPtrCdequeDestruct(v51126::ArgVectorPtrCdeque*, short)
  __ZN6v5112626ArgumentVectorDestroyRangeEPvS0_   # v51126::ArgumentVectorDestroyRange(void*, void*)
)
