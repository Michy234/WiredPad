#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
sample=Splashtop_Wired_XDisplay_Agent_v1.5.8.3.exe
printf '%s  %s\n' fb7d9a5cf6ae1b52786827ab9357062ec5a7a9b49d1faaaaebc4b2e2b1c5f182 "$sample" | sha256sum -c -
mkdir -p .research/extracted
7z x -y -o.research/extracted "$sample" Data1.cab >/dev/null
7z x -y -o.research/extracted/payload .research/extracted/Data1.cab >/dev/null
printf '%s\n' 'Estratti agent, librerie e driver; nessun eseguibile Windows è stato avviato.'
