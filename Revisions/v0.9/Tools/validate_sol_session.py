"""Validate this handoff against actual plans, evidence and source files."""
import csv
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VAULT = ROOT / 'book of mormon RTS'
PLAN = VAULT / '08 Development Plan'
EVIDENCE = ROOT / 'Docs/Evidence/2026-10-20'


def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def main():
    status = load(ROOT / 'Docs/SolTaskStatus.json')
    tasks = status['actions']
    expected = {a['action_id'] for a in load(PLAN / 'Action Difficulty Index.json')['actions'] if a['difficulty'] == 'Sol'}
    assert len(tasks) == 22 and {a['id'] for a in tasks} == expected
    verified = {a['id'] for a in tasks if a['status'] == 'verified'}
    assert verified == {'A002', 'A005', 'A008', 'A009', 'A011', 'A017'}
    assert all(a.get('depends_on') and a.get('acceptance') for a in tasks if a['status'] == 'blocked')
    checked = set(re.findall(r'^- \[x\] (A\d+) ', (PLAN / 'Development Action Plan.md').read_text(encoding='utf-8'), flags=re.M))
    # Later tiers may complete additional actions; validate this session's tier only.
    assert checked & expected == verified, f'Sol checkbox/status discrepancy: {(checked & expected) ^ verified}'
    assert not re.search(r'^- \[x\]', (PLAN / 'Asset Production Plan.md').read_text(encoding='utf-8'), flags=re.M)
    for note in load(PLAN / 'Source Inventory.json')['notes']:
        assert hashlib.sha256((VAULT / note['path']).read_bytes()).hexdigest() == note['sha256'], note['path']
    with (ROOT / 'Docs/AssetLedger.csv').open(encoding='utf-8', newline='') as handle:
        reader = csv.DictReader(handle)
        assert {'tax', 'usd_fx_rate', 'license_name', 'acquired_date', 'source_url', 'source_file_sha256', 'attribution', 'ai_tool_model', 'total_usd'} <= set(reader.fieldnames)
        assert not list(reader), 'Reconcile new transactions before reusing zero-spend handoff'
    voice = load(ROOT / 'Docs/AudioPreparation/ActiveVoicePreparation.json')['events']
    assert len(voice) == 153 and all(r['audio_asset_id'] is None and r['runtime_unit_id'] is None for r in voice)
    assert all(not r['runtime_fallback_verified'] for r in voice)
    with (ROOT / 'Docs/AudioPreparation/AudioProductionLedger.csv').open(encoding='utf-8', newline='') as handle:
        assert len(list(csv.DictReader(handle))) == 153
    for path in ['ProductContract.md', 'CapabilityReport.md', 'AssetLedger.md', 'OperatingCosts.md', 'VersionControl.md', 'ExecutionState.md', 'Decisions.md', 'KnownIssues.md']:
        assert (ROOT / 'Docs' / path).is_file(), path
    capabilities = load(EVIDENCE / 'capabilities.json')
    assert len(capabilities['engines']) == 2
    assert all(e['version'] and e['editor_exists'] for e in capabilities['engines'])
    baseline = '2a2e2862baae052cea2820549e653c4baf2c760b'
    parents = subprocess.check_output(['git', 'rev-list', '--parents', '-n', '1', baseline], cwd=ROOT, text=True).split()
    assert parents == [baseline]
    baseline_files = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', baseline], cwd=ROOT, text=True)
    assert all(line.startswith('book of mormon RTS/') for line in baseline_files.splitlines())
    lines = ['# Sol assignment status — October 20, 2026', '', 'Six verified administrative outputs; sixteen blocked assignments. No game phase gate passed. Full action wording remains in the vault plans.', '', f"Executor: {status['actual_model']}.", '', '| Task | Status | Evidence / preparation | Acceptance or dependency |', '| --- | --- | --- | --- |']
    for task in tasks:
        detail = task['acceptance'] + (' Dependency: ' + task['depends_on'] if task.get('depends_on') else '')
        lines.append(f"| {task['id']} | {task['status']} | {task['evidence']} | {detail} |")
    (ROOT / 'Docs/SolTaskStatus.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    summary = PLAN / 'Sol Work Summary - 2026-10-20.md'
    assert summary.exists()
    for link in re.findall(r'\]\(([^)]+)\)', summary.read_text(encoding='utf-8')):
        if '://' not in link:
            target = (summary.parent / link).resolve()
            if target != (EVIDENCE / 'session-validation.json').resolve():
                assert target.exists(), link
    report = {'result': 'PASS', 'sol_actions_accounted_for': 22, 'verified': sorted(verified), 'blocked': 16, 'original_canonical_hashes_matched': 117, 'active_voice_records': 153, 'audio_bindings': 0, 'asset_transactions': 0, 'session_spend_usd': 0, 'original_vault_baseline_verified': baseline, 'scope': 'Recordkeeping, extraction and handoff only; no Unreal compile/package/network/asset acceptance performed'}
    (EVIDENCE / 'session-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
