# Unit v51001: GetNextStaffMember and ShowStatusPanel::InitWindow (src/game/ui/ui_showStatus.cpp, src/game/ui/ui_staffPanel.cpp). The callees
# outside the unit are recording fakes in tests/unit_GetNextStaffMember.cpp, which also defines the globals of other files.
set(TPW_UNIT_GetNextStaffMember src/game/ui/ui_showStatus.cpp src/game/ui/ui_staffPanel.cpp platform/arena.cpp)
set(TPW_UNIT_GetNextStaffMember_KEEP
  __Z18GetNextStaffMemberv   # GetNextStaffMember()
  __ZN15ShowStatusPanel10InitWindowEv   # ShowStatusPanel::InitWindow()
  __ZN17showstatus_callee11NewCashTextEv   # showstatus_callee::NewCashText()
  __ZN17showstatus_callee13NewTextSpriteEv   # showstatus_callee::NewTextSprite()
)
