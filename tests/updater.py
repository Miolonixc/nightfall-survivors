#!/usr/bin/env python3
"""Exercise shipped shell scripts with deterministic HTTPS fixtures, no external writes."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
source = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    tools = root / 'tools'
    tools.mkdir()
    fixture = root / 'fixture'
    fixture.mkdir()
    (tools / 'curl').write_text('''#!/usr/bin/env python3
import os, pathlib, shutil, sys
args=sys.argv[1:]; dest=args[args.index('-o')+1]
if os.environ.get('FIXTURE_OFFLINE'): sys.exit(22)
url=next(a for a in args if a.startswith('https://'))
shutil.copyfile(pathlib.Path(os.environ['FIXTURE']) / ('manifest' if url.endswith('.txt') else 'binary'), dest)
''')
    (tools / 'sha256sum').write_text('''#!/usr/bin/env python3
import hashlib,sys
print(hashlib.sha256(open(sys.argv[1],'rb').read()).hexdigest()+'  '+sys.argv[1])
''')
    for path in tools.iterdir():
        path.chmod(0o755)
    env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ['PATH'], FIXTURE=str(fixture))
    binary = bytearray(80)
    binary[:5] = b'\x7fELF\x02'
    binary[18:20] = b'\xb7\0'
    binary = bytes(binary)
    digest = hashlib.sha256(binary).hexdigest()
    (fixture / 'binary').write_bytes(binary)
    def manifest(version='0.2.0', checksum=digest, url='https://fixture.invalid/game'):
        (fixture / 'manifest').write_text(f'{version} {checksum} {url}\n')
    def game(name):
        g = root / name / 'nightfall'
        g.mkdir(parents=True)
        (g / 'nightfall.aarch64').write_text('#!/bin/sh\necho restored > restored.txt\nexit 0\n')
        (g / 'nightfall.aarch64').chmod(0o755)
        (g / 'VERSION').write_text('0.1.0\n')
        shutil.copy(source / 'packaging/update.sh', g)
        return g
    def update(g, extra=None):
        p = subprocess.run(['sh', str(g/'update.sh'), str(g), 'https://fixture.invalid/latest.txt'], env={**env, **(extra or {})}, capture_output=True, text=True, timeout=5)
        assert p.returncode == 0, p.stderr
        assert not (g / '.update-lock').exists()
        return p.stdout
    manifest()
    g=game('success')
    update(g)
    assert (g/'nightfall.aarch64').read_bytes()==binary
    assert (g/'VERSION').read_text().strip()=='0.2.0'
    assert (g/'nightfall.previous').read_text().startswith('#!')
    assert (g/'update.pending').exists()
    for name, version, checksum, url in [
        ('checksum','0.2.0','0'*64,'https://fixture.invalid/game'),
        ('downgrade','0.0.1',digest,'https://fixture.invalid/game'),
        ('same','0.1.0',digest,'https://fixture.invalid/game'),
        ('injection','$(touch_PWNED)',digest,'https://fixture.invalid/game'),
        ('http','0.2.0',digest,'http://fixture.invalid/game'),
    ]:
        manifest(version,checksum,url)
        g=game(name);before=(g/'nightfall.aarch64').read_bytes();update(g)
        assert (g/'nightfall.aarch64').read_bytes()==before, name
    manifest()
    g=game('offline');update(g,{'FIXTURE_OFFLINE':'1'});assert not (g/'update.pending').exists()
    g=game('rollback')
    launcher=g.parent/'Nightfall Survivors.sh'
    shutil.copy(source/'packaging/Nightfall Survivors.sh',launcher)
    (g/'update-url.txt').write_text('https://fixture.invalid/latest.txt\n')
    p=subprocess.run(['bash',str(launcher)],env=env,capture_output=True,text=True,timeout=5)
    assert p.returncode==0,(p.stdout,p.stderr)
    assert (g/'restored.txt').exists() and (g/'VERSION').read_text().strip()=='0.1.0'
    assert (g/'failed-version').read_text().strip()=='0.2.0'
    assert not (g/'.launch-lock').exists()
    assert 'previously failed' in update(g)
    # Interrupted runs leave a PID lock that must not block future starts.
    (g/'.update-lock').mkdir();(g/'.update-lock/pid').write_text('999999999\n')
    update(g)
    print('PASS update install, HTTPS, checksum, version ordering, offline fallback, interrupted locks, startup rollback, failed-version quarantine')
