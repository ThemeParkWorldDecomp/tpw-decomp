# Unit v51041: CActionRecorder::ActivateActionRecording and the RecordAction / ProcessAction overloads taking a RecordedAction
# (src/game/ActionRecorder.cpp). The callees outside the
# unit (CTRACE, the file's virtual Write, CMemFile::Open) are recording fakes in tests/unit_CActionRecorder_ActivateActionRecording.cpp.
set(TPW_UNIT_CActionRecorder_ActivateActionRecording src/game/ActionRecorder.cpp platform/arena.cpp)
set(TPW_UNIT_CActionRecorder_ActivateActionRecording_KEEP
  __ZN15CActionRecorder12RecordActionE14RecordedAction   # CActionRecorder::RecordAction(RecordedAction)
  __ZN15CActionRecorder12RecordActionE14RecordedActionPh   # CActionRecorder::RecordAction(RecordedAction, unsigned char*)
  __ZN15CActionRecorder12RecordActionE14RecordedActionhh   # CActionRecorder::RecordAction(RecordedAction, unsigned char, unsigned char)
  __ZN15CActionRecorder12RecordActionE14RecordedActionhht   # CActionRecorder::RecordAction(RecordedAction, unsigned char, unsigned char, unsigned short)
  __ZN15CActionRecorder12RecordActionE14RecordedActiont   # CActionRecorder::RecordAction(RecordedAction, unsigned short)
  __ZN15CActionRecorder12RecordActionE14RecordedActionth   # CActionRecorder::RecordAction(RecordedAction, unsigned short, unsigned char)
  __ZN15CActionRecorder13ProcessActionE14RecordedActionPh   # CActionRecorder::ProcessAction(RecordedAction, unsigned char*)
  __ZN15CActionRecorder23ActivateActionRecordingENS_10RecordModeE   # CActionRecorder::ActivateActionRecording(CActionRecorder::RecordMode)
)
