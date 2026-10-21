# Unit w107: the message centre's post (CMessageCentre::PostMessage), CMsgEvent and CSubscriptionHeader's constructors and destructors,
# CThing::ReceiveMessage and GetThingId, and the MSL red-black tree the subscriptions live in (find, erase, clear, destroy, increment,
# balance_erase and the rotations). Everything the original calls beyond that (GAME_ASSERTMSG, the eighteen ReceiveMessage targets)
# is a recording fake in the unit file; the unit file includes calllog.cpp itself (it needs more labels).
set(TPW_UNIT_MessageCentre src/game/advisor/Advisor.cpp src/game/park/BankAccount.cpp src/game/save/BlockSave.cpp
    src/game/ui/MessageCentre.cpp src/game/Thing.cpp src/game/MslSupport.cpp)
set(TPW_UNIT_MessageCentre_KEEP
  __ZN14CMessageCentre11PostMessageERK4CMsg   # CMessageCentre::PostMessage(CMsg const&)
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
  __ZN19CSubscriptionHeaderC1Ev   # CSubscriptionHeader::CSubscriptionHeader()
  __ZN19CSubscriptionHeaderD1Ev   # CSubscriptionHeader::~CSubscriptionHeader()
  __ZN21SubscriptionAllocator10deallocateEPN16SubscriptionTree4nodeEm   # SubscriptionAllocator::deallocate(SubscriptionTree::node*, unsigned long)
  __ZN21SubscriptionAllocator7destroyEP13CSubscription   # SubscriptionAllocator::destroy(CSubscription*)
  __ZN6CThing14ReceiveMessageERK4CMsg   # CThing::ReceiveMessage(CMsg const&)
  __ZN9CMsgEventC1E9EventType   # CMsgEvent::CMsgEvent(EventType)
  __ZN9CMsgEventD1Ev   # CMsgEvent::~CMsgEvent()
  __ZNK6CThing10GetThingIdEv   # CThing::GetThingId() const
  __ZNK9CMsgEvent7GetTypeEv   # CMsgEvent::GetType() const
)
set(TPW_UNIT_MessageCentre_OWN_CALLLOG ON)
