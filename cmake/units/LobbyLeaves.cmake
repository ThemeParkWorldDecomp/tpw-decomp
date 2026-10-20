# The lobby's remaining leaves (unit w40): the version number, the font table entry, text measuring, the mail icon's handler, the game
# type (main app) and the ltms string buffer with the converter it holds (two test modules, one per program: test_LobbyLeaves and
# test_LobbyLeavesLtms).
set(TPW_UNIT_LobbyLeaves src/ui_main_leaves.cpp src/ui_fonttable.cpp src/ui_measuretext.cpp src/lobby_mailicon.cpp src/FS_Online_gametype.cpp
    src/CStaticUnicodeBuffer.cpp src/TbIMBToUnicode.cpp src/UI_Base.cpp)
