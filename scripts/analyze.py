#!/usr/bin/env python3
"""Ripete l'analisi statica via REA MCP stdio, senza registrare un server nell'agent."""
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
out = root / '.research' / 'reproduced'
out.mkdir(parents=True, exist_ok=True)
process = subprocess.Popen([os.environ.get('REA', 'rea'), '--mcp'], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, text=True)
request_id = 0


def rpc(method, params):
    global request_id
    request_id += 1
    process.stdin.write(json.dumps(dict(jsonrpc='2.0', id=request_id, method=method, params=params)) + '\n')
    process.stdin.flush()
    while True:
        line = process.stdout.readline()
        if not line:
            raise RuntimeError('REA MCP terminato')
        reply = json.loads(line)
        if reply.get('id') == request_id:
            if 'error' in reply:
                raise RuntimeError(reply['error'])
            return reply['result']


def call(name, args):
    result = rpc('tools/call', dict(name=name, arguments=args))
    if result.get('isError'):
        raise RuntimeError(result)
    return result.get('structuredContent', result)


def analyze(file_id, name, procedures):
    target = root / '.research' / 'extracted' / 'payload' / file_id
    if not target.is_file():
        raise RuntimeError('Prima esegui scripts/extract.sh')
    call('open_binary', dict(path=str(target), provider_id='ghidra'))
    evidence = call('batch_decompile', dict(addresses=procedures))
    (out / (name + '.json')).write_text(json.dumps(evidence, indent=2))
    call('close_binary', {})
    print(f'Evidenze: {out / (name + ".json")}', flush=True)


try:
    rpc('initialize', dict(protocolVersion='2024-11-05', capabilities={},
                           clientInfo=dict(name='xdisplaylinux-research', version='1')))
    process.stdin.write(json.dumps(dict(jsonrpc='2.0', method='notifications/initialized')) + '\n')
    process.stdin.flush()
    analyze('_8CFB2A6B0585CABF34C2023D32965904', 'agent',
            ['0x410020', '0x4303f0', '0x430bb0', '0x42f220', '0x42f580',
             '0x42f840', '0x440110', '0x440540', '0x43f360', '0x43bf80', '0x43c380'])
    analyze('_5EDB2B4992F4AAE04E10E90887CCBBB7', 'wrapper',
            ['SRx264Init', 'SRx264EncodeFrame', '0x10001020', '0x100011d0'])
    analyze('_F8BEF4EFB3F6E6706E1928DF27ADA457', 'x264', ['x264_param_default'])
except Exception as error:
    print(error, file=sys.stderr)
    sys.exit(1)
finally:
    try:
        call('close_binary', {})
    finally:
        process.terminate()
        process.wait(timeout=10)
