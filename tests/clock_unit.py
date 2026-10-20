"""Combined coverage entry point for the assigned 18-function address range."""
from clock_support import setup
from test_Clock import TESTS as CLOCK_TESTS
from test_Cheat import TESTS as CHEAT_TESTS
from test_MsgCheatMode import TESTS as MESSAGE_TESTS

TESTS = CHEAT_TESTS + MESSAGE_TESTS + CLOCK_TESTS
