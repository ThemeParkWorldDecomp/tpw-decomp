# Unit v51041: CActionRecorder::ActivateActionRecording and the RecordAction / ProcessAction overloads taking a RecordedAction
# (src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ActivateActionRecording.cpp). The callees outside the
# unit (CTRACE, the file's virtual Write, CMemFile::Open) are recording fakes in tests/unit_CActionRecorder_ActivateActionRecording.cpp.
set(TPW_UNIT_CActionRecorder_ActivateActionRecording src/ActionRecorder_CActionRecorder_CActionRecorder_CActionRecorder_ProcessAction_CActionRecorder_ActivateActionRecording.cpp platform/arena.cpp)
