"""Byte twins judged by test_Lobby's tests (tests/v5twin.py).
Each row: the borrowed test, the translated instance D, the untranslated twin N run in its place, N's name."""
import test_Lobby as borrowed
import v5twin

TWINS = [
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x100acb58, 'TbPtrList<CLBGlobeCityFlag>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x100acbfc, 'TbPtrList<CLBGlobe>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x100b09b8, 'TbPtrList<CLBIsland>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x10147cb0, 'TbPtrList<TbDynamicStringTemplate<char>>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x101623cc, 'TbPtrList<CTPItemResource>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x1016fea8, 'TbPtrList<CFSOnlinePark>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x10170344, 'TbPtrList<CFSOnlineCity>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x101704b0, 'TbPtrList<TbDynamicStringTemplate<wchar_t>>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x10179c88, 'TbPtrList<CFSOnlineTheme>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x1017a864, 'TbPtrList<CFSOnlineServer>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x1017b404, 'TbPtrList<CFSOnlineRide>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x101fc2c8, 'TbPtrList<TFSLoadSaveSaveFileDetails>::~TbPtrList'),
    ('CLBLobby and CLBLobbySystem', 0x100a62ac, 0x102308d4, 'TbPtrList<CUI2DMapSprite>::~TbPtrList'),
]
PROGRAM, SHIM_LIBRARY, setup, TESTS = v5twin.wrap(borrowed, TWINS)
