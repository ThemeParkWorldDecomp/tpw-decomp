# Unit v5362: CActionRecorder::ProcessAction (0x10145e38, 0x101465a8) and CActionRecorder::RecordAction (0x1014445c;
# src/game/ActionRecorder.cpp). The callees outside
# the unit are recording fakes in tests/unit_CActionRecorder_ProcessAction_2.cpp, which also defines the globals of other files.
set(TPW_UNIT_CActionRecorder_ProcessAction_2 src/game/ActionRecorder.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_ProcessAction_2_KEEP
  __ZN15CActionRecorder12RecordActionEittt   # CActionRecorder::RecordAction(int, unsigned short, unsigned short, unsigned short)
  __ZN15CActionRecorder13ProcessActionEit   # CActionRecorder::ProcessAction(int, unsigned short)
  __ZN15CActionRecorder13ProcessActionEithPDs   # CActionRecorder::ProcessAction(int, unsigned short, unsigned char, char16_t*)
  _gReplayChainEndA
  _gReplayChainEndB
)
