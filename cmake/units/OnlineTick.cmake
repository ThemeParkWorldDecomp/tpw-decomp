# Unit w69: the online system's four state tickers, CompleteLogin and AreWeLoggedIn (src/FS_Online_tick.cpp). The unit file includes
# unit_OnlineInit.cpp (the object harness of unit w68, with its own call log) and links the same sources, so the constructor builds the
# nine lists; the online library's connection classes, the job completions, the list requests and the interface calls are fakes.
set(TPW_UNIT_OnlineTick src/FS_Online_tick.cpp src/FS_Online_init.cpp src/FS_OnlineList.cpp src/TbString.cpp src/TbStringBuffer.cpp platform/msl.cpp)
set(TPW_UNIT_OnlineTick_OWN_CALLLOG ON)
