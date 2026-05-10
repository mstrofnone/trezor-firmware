#!/usr/bin/env python3
"""Start the prodtest emulator, run a command, then stop it.

This is the prodtest equivalent of `emu.py -c`. Usage:

    ./prodtest_emu.py pytest ../tests/prodtest_tests
    ./prodtest_emu.py -c "some command"
"""

import signal
import subprocess
import sys

from trezorlib.prodtest.prodtest_emulator import get_prodtest_emulator

emulator = get_prodtest_emulator()

with emulator:
    emulator.start()
    process = subprocess.Popen(sys.argv[1:])
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    sys.exit(process.wait())
