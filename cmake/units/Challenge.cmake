# Unit w174: one challenge (CChallenge: constructor, Serialise, DidThisWinIt, the four message id getters; the ChallengeType and StaffType serialise instances). The callees outside the unit are recording fakes in tests/unit_Challenge.cpp; the serialise instances run real on both sides (the file is the fake).
set(TPW_UNIT_Challenge src/Challenge.cpp src/Serialise_basic.cpp platform/arena.cpp)
