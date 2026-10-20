"""Main app HoardingsBalance.cpp differential tests; shared protocol in balance_support."""
SHIM_LIBRARY = "legacy_testshim"
from balance_support import setup, tests_for

TESTS = tests_for("HoardingsBalance")
