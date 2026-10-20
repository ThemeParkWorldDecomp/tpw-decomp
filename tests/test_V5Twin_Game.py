"""Byte twins judged by test_Game's tests (tests/v5twin.py).
Each row: the borrowed test, the translated instance D, the untranslated twin N run in its place, N's name."""
import test_Game as borrowed
import v5twin

TWINS = [
    ('CGame::AddThemeName, ClearThemeNames', 0x1014ad34, 0x100b51b0, 'TbTList<CLBIsland*,CLBIsland*>::InsertBefore'),
    ('FS_ScanThemes', 0x1014ad34, 0x100b51b0, 'TbTList<CLBIsland*,CLBIsland*>::InsertBefore'),
]
PROGRAM, SHIM_LIBRARY, setup, TESTS = v5twin.wrap(borrowed, TWINS)
