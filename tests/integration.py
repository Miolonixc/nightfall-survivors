#!/usr/bin/env python3
"""Real processes exchange the production protocol over loopback TCP."""
import json
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time

binary = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    processes = []
    def port():
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            return s.getsockname()[1]
    def run(name, *args):
        p = subprocess.Popen([binary, '--headless', '--demo', '--report', str(root / (name + '.json')), *args], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        processes.append(p)
        return p
    def finish(p, expected=0):
        out, err = p.communicate(timeout=25)
        assert p.returncode == expected, (p.returncode, out, err)
    def report(name):
        return json.loads((root / (name + '.json')).read_text())
    try:
        n = port()
        host = run('host', '--host', '--port', str(n), '--frames', '1300')
        time.sleep(.3)
        # Reject malformed and wrong-version clients without taking the server down.
        for payload in (struct.pack('<III', 0, 0, 999), b'x' * 8):
            with socket.create_connection(('127.0.0.1', n), timeout=2) as s:
                s.sendall(struct.pack('<I', len(payload)) + payload)
            time.sleep(.05)
        clients = [run('client' + str(i), '--join', '127.0.0.1', '--port', str(n), '--frames', '650') for i in range(3)]
        for p in clients:
            finish(p)
        finish(host)
        h = report('host')
        assert h['joins'] == 3 and h['ticks'] > 60 and not h['network_error'], h
        ids = set()
        for i in range(3):
            c = report('client' + str(i))
            ids.add(c['local'])
            assert c['states'] > 10 and c['ticks'] > 60 and not c['network_error'], c
            assert c['max_party'] == 4, c
        assert ids == {1, 2, 3}, ids
        # A host disappearing is surfaced as an error, rather than a frozen game.
        n = port()
        host = run('short-host', '--host', '--port', str(n), '--frames', '450')
        time.sleep(.3)
        client = run('orphan', '--join', '127.0.0.1', '--port', str(n), '--frames', '850')
        finish(host)
        finish(client, 1)
        assert report('orphan')['network_error']
        print('PASS four-player sessions, distinct slots, authoritative snapshots, malformed clients, host disconnect')
    finally:
        for p in processes:
            if p.poll() is None:
                p.kill()
                p.communicate()
