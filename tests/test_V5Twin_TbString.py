"""Byte twins judged by test_TbString's tests (tests/v5twin.py).
Each row: the borrowed test, the translated instance D, the untranslated twin N run in its place, N's name."""
import test_TbString as borrowed
import v5twin

TWINS = [
    ('string list constructor', 0x10009ff8, 0x100148b8, 'TbTList<_LbSEFunction*,_LbSEFunction*>::TbTList'),
]
PROGRAM, SHIM_LIBRARY, setup, TESTS = v5twin.wrap(borrowed, TWINS)
