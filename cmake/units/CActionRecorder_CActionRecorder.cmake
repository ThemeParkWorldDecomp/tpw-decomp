# Unit v5075: CActionRecorder constructor, destructor, Serialise and RecordAction (src/game/ActionRecorder.cpp). The callees
# outside the unit are recording fakes in tests/unit_CActionRecorder_CActionRecorder.cpp.
set(TPW_UNIT_CActionRecorder_CActionRecorder src/game/ActionRecorder.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_CActionRecorder_KEEP
  __ZN15CActionRecorder12RecordActionE14RecordedActiontt   # CActionRecorder::RecordAction(RecordedAction, unsigned short, unsigned short)
  __ZN15CActionRecorder8DestructEv   # CActionRecorder::Destruct()
  __ZN15CActionRecorder9ConstructEv   # CActionRecorder::Construct()
  __ZN15CActionRecorder9SerialiseEPviij   # CActionRecorder::Serialise(void*, int, int, unsigned int)
)
