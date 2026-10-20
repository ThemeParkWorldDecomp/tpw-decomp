# The main app's four effect-manager leaves (unit w38): the paused real-time clock and the table of sines. The advisor's state and the
# table are the real globals; the game clock's two reads are fakes in the unit file.
set(TPW_UNIT_HostLeaves src/host_leaves.cpp)
