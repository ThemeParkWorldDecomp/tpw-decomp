# Unit w107: the message centre's post (CMessageCentre::PostMessage), CMsgEvent and CSubscriptionHeader's constructors and destructors,
# CThing::ReceiveMessage and GetThingId, and the MSL red-black tree the subscriptions live in (find, erase, clear, destroy, increment,
# balance_erase and the rotations). Everything the original calls beyond that (GAME_ASSERTMSG, the eighteen ReceiveMessage targets)
# is a recording fake in the unit file; the unit file includes calllog.cpp itself (it needs more labels).
set(TPW_UNIT_MessageCentre src/MessageCentre_post.cpp src/Thing_receive.cpp src/msl_tree_subscription.cpp src/msl_red_black_tree.cpp)
set(TPW_UNIT_MessageCentre_OWN_CALLLOG ON)
