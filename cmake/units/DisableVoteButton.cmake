# Unit v5766: DisableVoteButton and EarmuffsButtonDown (src/game/ui/ui_onlinePanel.cpp). The callees are fakes in tests/unit_DisableVoteButton.cpp.
set(TPW_UNIT_DisableVoteButton src/game/ui/ui_onlinePanel.cpp platform/arena.cpp)
set(TPW_UNIT_DisableVoteButton_KEEP
  __Z18EarmuffsButtonDownb   # EarmuffsButtonDown(bool)
  __ZN13online_callee17DisableVoteButtonEb   # online_callee::DisableVoteButton(bool)
)
