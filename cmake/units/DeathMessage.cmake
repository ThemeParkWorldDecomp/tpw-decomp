# Unit w123c: the death message (CMsgDeath constructor, GetType and destructor), the message centre's post (CMessageCentre::PostMessage, w107,
# reviewed again here with every callee faked), CWorld::AddToPendingDeleteList and GetRand, and CParkTime::GetSeason. The unit file includes
# calllog.cpp itself (it needs more labels). The tree, the thing and the timestamp functions are recording fakes in the unit file.
set(TPW_UNIT_DeathMessage src/game/Thing.cpp src/game/ui/MessageCentre.cpp src/game/advisor/Advisor.cpp
    src/game/park/BankAccount.cpp src/game/save/BlockSave.cpp src/game/World.cpp src/game/people/ParkTime.cpp)
set(TPW_UNIT_DeathMessage_KEEP
  __ZN14CMessageCentre11PostMessageERK4CMsg   # CMessageCentre::PostMessage(CMsg const&)
  __ZN19CSubscriptionHeaderC1Ev   # CSubscriptionHeader::CSubscriptionHeader()
  __ZN19CSubscriptionHeaderD1Ev   # CSubscriptionHeader::~CSubscriptionHeader()
  __ZN6CWorld22AddToPendingDeleteListE12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CWorld::AddToPendingDeleteList(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN6CWorld7GetRandEv   # CWorld::GetRand()
  __ZN9CMsgDeathC1E12tag_IDX_TYPEI24IDXTYPEUniqueNameThingIdtE   # CMsgDeath::CMsgDeath(tag_IDX_TYPE<IDXTYPEUniqueNameThingId, unsigned short>)
  __ZN9CMsgDeathD1Ev   # CMsgDeath::~CMsgDeath()
  __ZN9CMsgEventC1E9EventType   # CMsgEvent::CMsgEvent(EventType)
  __ZN9CMsgEventD1Ev   # CMsgEvent::~CMsgEvent()
  __ZNK9CMsgDeath7GetTypeEv   # CMsgDeath::GetType() const
  __ZNK9CMsgEvent7GetTypeEv   # CMsgEvent::GetType() const
  __ZNK9CParkTime9GetSeasonEv   # CParkTime::GetSeason() const
)
set(TPW_UNIT_DeathMessage_OWN_CALLLOG ON)
