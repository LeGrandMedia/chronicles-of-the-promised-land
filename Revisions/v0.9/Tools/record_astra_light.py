"""Regenerate the bounded Astra Light handoff and verify source/checklist integrity."""
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VAULT = ROOT / 'book of mormon RTS'
PLAN = VAULT / '08 Development Plan'
DOCS = ROOT / 'Docs'
VERIFIED = {'A001', 'A003'}


def main():
    index = json.loads((PLAN / 'Action Difficulty Index.json').read_text(encoding='utf-8'))
    actions = [a for a in index['actions'] if a['difficulty'] == 'Astra Light']
    assert len(actions) == 131
    statuses = []
    for action in actions:
        ident = action['action_id']
        section = action['section']
        if ident in VERIFIED:
            dependency = 'Required sources read; user confirmed provisional target.'
        elif ident.startswith('A'):
            phase = int(re.search(r'Phase (\d+)', section).group(1))
            dependency = ('A004 (Astra Heavy): record immediate D01-D16 decisions before closing Phase 01. '
                          'A010 (Astra Medium): requirement register also remains open.')
            if phase > 1:
                dependency += f' Phase {phase:02d} also requires preceding phase gates and earlier actions within its phase.'
            if ident == 'A006':
                dependency += ' Compiler compatibility/incomplete VS2022 installation and required plugins are not verified; no engine pinned.'
            if ident == 'A007':
                dependency += ' RAM is unknown; performance budgets remain provisional until named hardware is recorded.'
            if phase >= 6:
                dependency += ' No packaged project or stable runtime catalog exists.'
        else:
            dependency = ('Art research/production waits for Phase 06 readiness under the Development Action Plan; '
                          'A004/A010 and the preceding packaged/network/representation/data gates have not passed.')
            explicit = re.search(r'After ([^,]+),', action['description'])
            if explicit:
                dependency += ' Explicit prerequisites: ' + explicit.group(1) + '.'
            if ident.startswith('U') and ident.endswith('M'):
                dependency += ' Reused Laborer source U18M and common rig A061 (Astra Heavy) are absent.'
            if ident.startswith('B') and ident.endswith('M'):
                dependency += ' Approved art baseline, building kit and D02 footprint/work-slot decisions are absent.'
            if ident.endswith('I'):
                dependency += ' Stable catalog IDs and packaged network integration acceptance are also unavailable.'
        statuses.append({**action, 'status': 'verified' if ident in VERIFIED else 'blocked',
                         'dependency_or_evidence': dependency,
                         'output': 'Docs/ScopeRecord.md' if ident in VERIFIED else None})
    for source in ('Development Action Plan.md', 'Asset Production Plan.md'):
        body = (PLAN / source).read_text(encoding='utf-8')
        for ident in VERIFIED:
            body = body.replace(f'- [ ] {ident} ', f'- [x] {ident} ')
        (PLAN / source).write_text(body, encoding='utf-8')
        for action in [a for a in statuses if a['source_file'] == source]:
            state = re.search(r'^- \[([ x])\] ' + action['action_id'] + r' ', body, re.M)
            assert state and (state.group(1) == 'x') == (action['status'] == 'verified')
    inventory = json.loads((PLAN / 'Source Inventory.json').read_text(encoding='utf-8'))['notes']
    for note in inventory:
        assert hashlib.sha256((VAULT / note['path']).read_bytes()).hexdigest() == note['sha256'], note['path']
    inputs = [n for n in inventory if n['path'].startswith(('01 Foundation/', '02 Systems/', '03 Civilizations/')) or n['title'] == 'Deferred Ideas']
    assert len(inputs) == 17
    data = {'date': '2026-10-20', 'timezone': 'America/Phoenix',
            'executor': 'Codex, GPT-6-based per session instructions; exact SKU/reasoning setting not exposed',
            'verified': 2, 'blocked': 129, 'actions': statuses}
    (DOCS / 'AstraLightTaskStatus.json').write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8')
    lines = ['# Astra Light assignment status — October 20, 2026', '',
             '2 verified; 129 prerequisite-blocked; all 131 assigned actions accounted for. No phase gate passed.', '',
             'Dependencies below distinguish the global phase gate from explicit local dependencies. This is a dependency audit, not a claim that each blocked task was attempted. Full acceptance wording is retained in the JSON and original plans.', '',
             '| Action | Phase / section | Status | Evidence or blocker |', '| --- | --- | --- | --- |']
    for a in statuses:
        lines.append(f"| {a['action_id']} | {a['section']} | {a['status']} | {a['output'] or ''} {a['dependency_or_evidence']} |")
    (DOCS / 'AstraLightTaskStatus.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    report = {'result': 'PASS', 'actions_accounted_for': len(statuses), 'verified': sorted(VERIFIED),
              'blocked': 129, 'canonical_hashes_matched': len(inventory),
              'scope_input_hashes': [{'path': n['path'], 'sha256': n['sha256']} for n in inputs],
              'scope': 'Documentation and dependency audit; no compile, package, network or asset acceptance tests',
              'session_spend_usd': 0}
    (DOCS / 'Evidence/2026-10-20/astra-light-validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'scope_input_hashes'}))


if __name__ == '__main__':
    main()
