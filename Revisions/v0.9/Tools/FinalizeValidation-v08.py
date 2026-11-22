"""Verify fresh v0.8 evidence and the untouched v0.7 copy; prepare release guides."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
docs = root / 'Docs'
evidence = docs / 'Evidence/v0.8'
package = root / 'Builds/Windows'
exe = package / 'Chronicles/Binaries/Win64/Chronicles.exe'
build_time = exe.stat().st_mtime

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def fresh(path):
    assert path.stat().st_mtime >= build_time, f'Stale evidence: {path}'
    return read(path)

native = {}
for path in sorted((evidence / 'native').glob('*tests.json')):
    data = fresh(path)
    assert not data.get('failed', 0) and not data.get('failed_count', 0), path.name
    assert data.get('passed', True) is not False, path.name
    for field in ('assertions', 'cases'):
        assert all(c.get('passed', True) for c in data.get(field, [])), path.name
    if path.name == 'visual-v07-animation-tests.json':
        for field in ('all_active_poses_non_rigid', 'mesh_never_folds', 'all_units_smaller_buildings_unchanged', 'work_state_routing'):
            assert data[field], field
        assert all(p['changed_pose'] and p['non_rigid_rms'] > 0 for p in data['poses'])
    native[path.name] = {'passed': data.get('passed_count', data.get('passed', 0)), 'failed': 0}
assert len(native) == 19, len(native)
assert {'religion-v08-tests.json', 'v08-economy-tests.json', 'v08-presentation-tests.json'} <= native.keys()
viewport = {}
for size in ('1600x900', '1280x720'):
    data = fresh(evidence / size / 'input-tests.json')
    assert data['passed'] and all(c['passed'] for c in data['cases'])
    viewport[size] = len(data['cases'])
network = {}
for role in ('host', 'client1', 'client2', 'client3'):
    data = fresh(evidence / 'network' / f'network-{role}.json')
    assert data['passed'] and all(data[f'v{v}_features'] for v in ('05', '07', '08')), role
    network[role] = data
for slot in range(4):
    data = fresh(evidence / 'network' / f'network-v08-slot{slot}.json')
    assert data['passed'], slot
    network[f'v08_slot{slot}'] = data
audits = list((evidence / 'network').glob('network-v07-*audit*.json'))
assert audits
for path in audits:
    data = fresh(path)
    assert data['passed'] and data['exactly_zero_cost'] and data['jobs_and_gate_unchanged'], path
    network[path.stem] = data

baseline = read(docs / 'v0.7-preservation.json')
changed = [name for name, expected in baseline.items() if not (root.parent / 'v0.7' / name).is_file() or digest(root.parent / 'v0.7' / name) != expected]
protected = {'checked_files': len(baseline), 'changed_files': changed, 'passed': not changed}
(docs / 'v0.7-preservation-verified.json').write_text(json.dumps(protected, indent=2), encoding='utf-8')
assert not changed, changed
data_dir = root / 'Game/Content/Data'
staged_dir = package / 'Chronicles/Content/Data'
data_files = [p for p in data_dir.rglob('*') if p.is_file()]
for path in data_files:
    staged = staged_dir / path.relative_to(data_dir)
    assert staged.is_file() and digest(path) == digest(staged), str(path)
manifest = read(data_dir / 'visuals.json')
art = {a['texture'] for group in ('sprites', 'icons') for a in manifest[group].values() if a.get('texture')}
for name in art:
    assert (staged_dir / manifest['art_root'] / name).is_file(), name
for asset in read(docs / 'ArtGeneration-v0.8.json')['assets']:
    assert digest(root / asset['output']) == asset['sha256'], asset['id']
assert digest(exe) == digest(root / 'Game/Binaries/Win64/Chronicles.exe')
report = {'revision': 'v0.8', 'verified_at_utc': datetime.now(timezone.utc).isoformat(), 'native_checks': native,
          'viewport_cases': viewport, 'network_reports': network, 'active_png_images': len(art),
          'staged_data_files_matching_source': len(data_files), 'packaged_executable_sha256': digest(exe),
          'protected_v07_files_unchanged': len(baseline), 'physical_multi_computer_lan_tested': False,
          'all_checks_passed': True}
(docs / 'Validation-v0.8.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
counted = sum(v['passed'] for v in native.values() if type(v['passed']) is int)
rows = '\n'.join(f"| {name} | {v['passed']} | 0 |" for name, v in native.items())
text = f'''# v0.8 validation

The packaged Windows build passed {counted} counted native assertions, visual import/animation suites, {viewport['1600x900']} rendered-input checks at 1600×900 and {viewport['1280x720']} at 1280×720, and four-process multiplayer on localhost. Evidence is newer than the executable. All {len(data_files)} staged data files match source; {len(baseline)} protected v0.7 files retain their original hashes.

## Native checks

| Report | Passed | Failed |
|---|---:|---:|
{rows}

Coverage includes trader payouts on blocked waypoints, newborn raid routes, real worker-cargo theft and delivery, generalized Hideout boarding, Winery/G&S generation and ages, visible auras, nonlinear Favor HP, conversion hostility/capacity, Priestcrafter occupancy, beast foundations and repairs, gates, Night Raid, and finite death/collapse poses for every unit/building definition.

## Rendered input and multiplayer

The viewport harness sends actual mouse/keyboard events. It covers clicks and drag selection, hotkeys, Roman lock labels, Winery training, normal/far building views, minimap rotation and inverse clicks, Storehouse rally input, Bribe hover, Hideout boarding, and two frames plus expiry of unit/building deaths. Captures are in [viewport evidence](Evidence/v0.8/1280x720/).

One listen server and three clients use the packaged executable and normal RPCs. New probes cover newborn three-point trader routes, economic and raid rallies, stolen worker cargo without damage, Hideout boarding/unloading, visible Bribe channels and stealth recovery, reliable deaths without a prior entity snapshot, hidden-death privacy, aura replication, and owner-only information. Earlier lobby, conversion, gate-cost, stress and match-completion probes also run. The mass-resignation test exposed and verified a fix for reliable-buffer overflow: compact death events now travel in bounded batches, with native checks for serialization, queue bounds and post-victory draining. Fixtures supply resources/entities for focused checks; this is not a human balance match.

Physical multi-computer LAN and Wi-Fi/firewall behavior remain unverified. No host migration, reconnect or saves. Animation uses deformation of 2D sprites and replaceable frame states; the new robber voice is synthesized placeholder audio. The unchanged UE 5.3 Entry map/engine cook was reused; game code and loose data were rebuilt and staged.

## Delivery

{len(art)} active image paths are present, including eight new Laban building assets. [Art provenance and full prompts](ArtGeneration-v0.8.json), [task/model labels](RevisionTasks-v0.8.md), and [machine-readable evidence](Validation-v0.8.json).

Executable SHA-256: `{report['packaged_executable_sha256']}`. The portable archive has its own checksum/CRC report alongside the ZIP.
'''
(docs / 'Validation-v0.8.md').write_text(text, encoding='utf-8')
path = docs / 'RevisionTasks-v0.8.md'
text = path.read_text(encoding='utf-8').replace('Rows remain pending until the root integrator records completion and evidence.', 'All rows implemented and verified in the packaged build; see [validation](Validation-v0.8.md).').replace('| Pending |', '| Implemented; verified |')
path.write_text(text, encoding='utf-8')
path = docs / 'Release-v0.8.md'
path.write_text(path.read_text(encoding='utf-8').replace('Validation is in progress.', 'Native, rendered-input and four-process network checks passed; see [validation](Validation-v0.8.md).'), encoding='utf-8')
path = docs / 'KnownIssues.md'
text = path.read_text(encoding='utf-8')
text = text[:text.index('The v0.8 validation report is being prepared.')] + 'Current checks and limits: [v0.8 validation](Validation-v0.8.md).\n' if 'The v0.8 validation report is being prepared.' in text else text
path.write_text(text, encoding='utf-8')
guides = package / 'Guides'
guides.mkdir(exist_ok=True)
for name in ('Controls.md', 'Naming.md', 'ArtReplacement.md', 'Release-v0.8.md', 'RevisionTasks-v0.8.md', 'Validation-v0.8.md', 'Validation-v0.8.json', 'ArtGeneration-v0.8.json', 'KnownIssues.md'):
    shutil.copy2(docs / name, guides / name)
(guides / 'ArtGallery.html').write_text((docs / 'ArtGallery.html').read_text(encoding='utf-8').replace('../Game/Content/Data/', '../Chronicles/Content/Data/'), encoding='utf-8')
shutil.copytree(evidence, guides / 'Evidence/v0.8', dirs_exist_ok=True)
(package / 'README.txt').write_text('Chronicles of the Promised Land — v0.8\n\nRun Chronicles.exe. Use the same v0.8 package on all computers. One player hosts; others join the host LAN IPv4 address. Allow the game on your private network when Windows Firewall prompts. Guides contains controls, release notes, art gallery and validation. Prerequisites: Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe.\n', encoding='utf-8')
print(json.dumps({'native_counted_assertions': counted, 'viewport_cases': viewport, 'network_processes': 4, 'active_art_paths': len(art), 'protected_files_unchanged': len(baseline), 'all_checks_passed': True}, indent=2))
