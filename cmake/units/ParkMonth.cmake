# Unit w161: the park analyser's messages, month end, admissions, thoughts and cell save (src/ParkAnalyser_month.cpp: CParkAnalyser::ReceiveMessage,
# PersonPaidAdmission, UpdateEndOfMonth, GetThreeMostDominantThoughts, FinalDestroy, CParkAnalyserCell::Serialise and the start-up words) and the three
# messages with CBankAccount::GetBalance (src/ParkMessages.cpp). Every callee outside them is a recording fake in tests/unit_ParkMonth.cpp, which also
# defines the globals of other files. List only the sources the library must compile.
set(TPW_UNIT_ParkMonth src/ParkAnalyser_month.cpp src/ParkMessages.cpp platform/arena.cpp)
