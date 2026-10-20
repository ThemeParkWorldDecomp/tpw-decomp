"""Combined module for branch coverage over both original balance address ranges."""
from balance_support import setup, tests_for
from test_TPDialog import TESTS as DIALOG_TESTS

TESTS = sum((tests_for(file) for file in ("HoardingsBalance", "MainBalance", "OnlineBalance", "OptionsBalance",
                                       "SoundBalance", "ResolutionBalance")), []) + DIALOG_TESTS
