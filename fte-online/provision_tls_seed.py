"""Provision one private Mbed TLS NV seed per Wii installation, never in a ZIP.

Use once per SD installation: python provision_tls_seed.py PATH_TO_ONLINE_FOLDER
Mbed TLS updates this seed before returning entropy. Never share it between Wiis.
"""
import os
from pathlib import Path
import sys

target = Path(sys.argv[1]).resolve()
if not target.is_dir():
    raise SystemExit('Choose the existing online installation directory.')
path = target / 'tls-seed.bin'
if path.exists():
    raise SystemExit('Existing private seed preserved; no change made.')
with path.open('xb') as seed:
    seed.write(os.urandom(64))
    seed.flush()
    os.fsync(seed.fileno())
print('Private TLS seed provisioned for this installation. Do not share this file.')
