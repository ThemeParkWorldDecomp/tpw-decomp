# Unit w175: the challenge hall's progress (CChallengeHQ::FinalDestroy, Serialise, AddToCounter, GetCurrentChallenge, ReceiveMessage, OfferFollowup,
# TellPlayerChallengeOffered, ChallengeAccepted, ChallengeDeclined, EndCurrentChallenge, WonCurrentChallenge and LostCurrentChallenge; src/ChallengeHQ_progress.cpp).
# Every callee outside them (the base thing's and the challenges' serialise, the challenges' ids and win test, the save-file templates, the clock, the
# world's getters, the analyser's queries, the bank deposit, the message centre, the trace and the assert) is a fake in the unit file; gMainBalance is the
# real storage, and the message classes come from src/ThingMessages.cpp and src/ParkMessages.cpp.
set(TPW_UNIT_ChallengeHQ src/ChallengeHQ_progress.cpp src/ThingMessages.cpp src/ParkMessages.cpp src/MainBalance.cpp platform/arena.cpp)
