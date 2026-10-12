#!/usr/bin/env python3
"""Package the latest completed study, including every original measured log."""
from pathlib import Path
import zipfile
root=Path(__file__).resolve().parents[1]
folders=sorted((root/'results').glob('followup_*'))
if not folders:raise SystemExit('No follow-up result folder exists')
latest=folders[-1]
if not (latest/'analysis.txt').is_file():raise SystemExit('Latest study is incomplete. Inspect the benchmark output before packaging it.')
target=root/'followup_results.zip'
with zipfile.ZipFile(target,'w',compression=zipfile.ZIP_DEFLATED) as z:
    for p in sorted(latest.rglob('*')):
        if p.is_file():z.write(p,p.relative_to(root))
print('Saved',target)
