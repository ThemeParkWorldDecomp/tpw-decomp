# Unit v5942: balance_insert, insert_node_at and CSubscription::Load (src/game/ui/MessageCentre.cpp). The callees outside the unit
# are recording fakes in tests/unit_std___red_black_tree_balance_insert.cpp; the rotations are the real ones.
set(TPW_UNIT_std___red_black_tree_balance_insert src/game/ui/MessageCentre.cpp src/game/MslSupport.cpp
    platform/arena.cpp)
set(TPW_UNIT_std___red_black_tree_balance_insert_KEEP
  __ZN13CSubscription10LoadResultERPv   # CSubscription::LoadResult(void*&)
  __ZN13CSubscription4LoadERPv   # CSubscription::Load(void*&)
  __ZN15MslRedBlackTree11rotate_leftEPNS_9node_baseERS1_   # MslRedBlackTree::rotate_left(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*&)
  __ZN15MslRedBlackTree12rotate_rightEPNS_9node_baseERS1_   # MslRedBlackTree::rotate_right(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*&)
  __ZN15MslRedBlackTree13balance_eraseEPNS_9node_baseES1_   # MslRedBlackTree::balance_erase(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*)
  __ZN15MslRedBlackTree14balance_insertEPNS_9node_baseES1_   # MslRedBlackTree::balance_insert(MslRedBlackTree::node_base*, MslRedBlackTree::node_base*)
  __ZN15MslRedBlackTree9incrementERPNS_9node_baseE   # MslRedBlackTree::increment(MslRedBlackTree::node_base*&)
  __ZN16SubscriptionTree14insert_node_atEPN15MslRedBlackTree9node_baseEbbRK13CSubscription   # SubscriptionTree::insert_node_at(MslRedBlackTree::node_base*, bool, bool, CSubscription const&)
)
