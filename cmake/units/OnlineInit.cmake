# Unit w68: the online system's constructor, its Init* and Release*Connection members and TickLogin (src/FS_Online_init.cpp), and the
# theme, server and ride list constructors and the online park's default constructor (src/FS_OnlineList.cpp). The bullfrog strings are
# the real ones; the online library's connection classes, the clock, the debug log and the list family's destructors are fakes in the
# unit file, which includes tests/calllog.cpp itself to observe allocations.
set(TPW_UNIT_OnlineInit src/FS_Online_init.cpp src/FS_OnlineList.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
set(TPW_UNIT_OnlineInit_OWN_CALLLOG ON)
