# Unit v5224: CActionRecorder::ReplayRecording, EndReplay and three RecordAction overloads
# (src/game/ActionRecorder.cpp). The callees outside the unit are recording
# fakes in tests/unit_CActionRecorder_ReplayRecording.cpp, which also defines the globals of other files.
set(TPW_UNIT_CActionRecorder_ReplayRecording src/game/ActionRecorder.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_ReplayRecording_KEEP
  __ZN15CActionRecorder12RecordActionE14RecordedActionfft   # CActionRecorder::RecordAction(RecordedAction, float, float, unsigned short)
  __ZN15CActionRecorder12RecordActionE14RecordedActionft   # CActionRecorder::RecordAction(RecordedAction, float, unsigned short)
  __ZN15CActionRecorder12RecordActionE14RecordedActionthPKDs   # CActionRecorder::RecordAction(RecordedAction, unsigned short, unsigned char, char16_t const*)
  __ZN15CActionRecorder15ReplayRecordingE10ReplayType   # CActionRecorder::ReplayRecording(ReplayType)
  __ZN15CActionRecorder9EndReplayEv   # CActionRecorder::EndReplay()
)
