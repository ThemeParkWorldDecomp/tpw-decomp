# The main app's four effect-manager leaves (unit w38): the paused real-time clock and the table of sines. The advisor's state and the
# table are the real globals; the game clock's two reads are fakes in the unit file.
set(TPW_UNIT_HostLeaves src/game/advisor/Advisor.cpp src/game/rides/BumperRide.cpp)
set(TPW_UNIT_HostLeaves_KEEP
  __Z18BMP_GenerateSinTabv   # BMP_GenerateSinTab()
  __Z21RealTimePauseAdjustedv   # RealTimePauseAdjusted()
  __Z8HOST_Cosi   # HOST_Cos(int)
  __Z8HOST_Sini   # HOST_Sin(int)
  _gHostSineTable
)
