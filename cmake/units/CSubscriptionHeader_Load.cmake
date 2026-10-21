# Unit v5729: CSubscriptionHeader::Save and ::Load (src/game/ui/MessageCentre.cpp). The callees outside the unit are
# recording fakes in tests/unit_CSubscriptionHeader_Load.cpp. The tree walk is the program's own code.
set(TPW_UNIT_CSubscriptionHeader_Load src/game/ui/MessageCentre.cpp src/game/MslSupport.cpp platform/arena.cpp)
set(TPW_UNIT_CSubscriptionHeader_Load_KEEP
  __ZN15MslRedBlackTree11rotate_leftEPNS_9node_baseERS1_   # MslRedBlackTree::rotate_left(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*&)
  __ZN15MslRedBlackTree12rotate_rightEPNS_9node_baseERS1_   # MslRedBlackTree::rotate_right(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*&)
  __ZN15MslRedBlackTree13balance_eraseEPNS_9node_baseES1_   # MslRedBlackTree::balance_erase(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*)
  __ZN15MslRedBlackTree9incrementERPNS_9node_baseE   # MslRedBlackTree::increment(MslRedBlackTree::node_base*&)
  __ZN16SubscriptionTree4findERK13CSubscription   # SubscriptionTree::find(CSubscription const&)
  __ZN16SubscriptionTree5beginEv   # SubscriptionTree::begin()
  __ZN16SubscriptionTree5clearEv   # SubscriptionTree::clear()
  __ZN16SubscriptionTree5eraseEPN15MslRedBlackTree9node_baseE   # SubscriptionTree::erase(MslRedBlackTree::node_base*)
  __ZN16SubscriptionTree7destroyEPNS_4nodeE   # SubscriptionTree::destroy(SubscriptionTree::node*)
  __ZN16SubscriptionTree9ConstructEv   # SubscriptionTree::Construct()
  __ZN19CSubscriptionHeader4LoadERPv   # CSubscriptionHeader::Load(void*&)
  __ZN21SubscriptionAllocator10deallocateEPN16SubscriptionTree4nodeEm   # SubscriptionAllocator::deallocate(SubscriptionTree::node*, unsigned long)
  __ZN21SubscriptionAllocator7destroyEP13CSubscription   # SubscriptionAllocator::destroy(CSubscription*)
  __ZNK19CSubscriptionHeader4SaveERPv   # CSubscriptionHeader::Save(void*&) const
)
