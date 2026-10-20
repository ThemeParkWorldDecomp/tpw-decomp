# Unit w123c: the death message (CMsgDeath constructor, GetType and destructor), the message centre's post (CMessageCentre::PostMessage, w107,
# reviewed again here with every callee faked), CWorld::AddToPendingDeleteList and GetRand, and CParkTime::GetSeason. The unit file includes
# calllog.cpp itself (it needs more labels). The tree, the thing and the timestamp functions are recording fakes in the unit file.
set(TPW_UNIT_DeathMessage src/MsgDeath.cpp src/MessageCentre_post.cpp src/World_death.cpp src/ParkTime_season.cpp)
set(TPW_UNIT_DeathMessage_OWN_CALLLOG ON)
