# SPDX-License-Identifier: MIT
"""Subprocess fixture: real guardian/child processes, fake sysfs files only."""
import contextlib
import os
from pathlib import Path
import subprocess
import sys
import time

mode, directory, source = sys.argv[1:]
runtime = Path(directory)
if mode == 'daemon':
    (runtime / 'daemon.pid').write_text(str(os.getpid()))
    time.sleep(60)
elif mode == 'controller':
    worker = subprocess.Popen([sys.executable, __file__, 'worker', directory, source], stdin=subprocess.PIPE)
    (runtime / 'guardian.pid').write_text(str(worker.pid))
    time.sleep(60)
else:
    sys.path.insert(0, source)
    import radio_guardian as guardian
    cap = guardian.cap_lora
    cap.POWER_LED = str(runtime / 'power')
    cap.MUX_LED = str(runtime / 'mux')
    cap.preflight = lambda: None
    @contextlib.contextmanager
    def powered():
        cap._write_text(cap.POWER_LED, '1')
        cap._write_text(cap.MUX_LED, '0')
        try: yield
        finally:
            cap._write_text(cap.MUX_LED, '1')
            cap._write_text(cap.POWER_LED, '0')
    cap.powered_cap = powered
    raise SystemExit(guardian.run([sys.executable, __file__, 'daemon', directory, source], runtime))
