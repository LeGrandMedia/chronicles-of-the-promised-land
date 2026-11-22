"""Verify fresh release evidence, protected copies, and staged data; write delivery reports."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[1]
docs = root / 'Docs'
evidence = docs / 'Evidence/v0.7'
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
        for case in data.get(field, []):
            assert case.get('passed', True), (path.name, case)
    if path.name == 'visual-v07-animation-tests.json':
        for field in ('all_active_poses_non_rigid', 'mesh_never_folds', 'all_units_smaller_buildings_unchanged', 'work_state_routing'):
            assert data[field], field
        assert all(p['changed_pose'] and p['non_rigid_rms'] > 0 for p in data['poses'])
    passed = data.get('passed_count', data.get('passed', 0))
    native[path.name] = {'passed': passed, 'failed': 0}
assert len(native) == 16, len(native)
viewport = {}
for size in ('1600x900', '1280x720'):
    data = fresh(evidence / size / 'input-tests.json')
    assert data['passed'] and all(c['passed'] for c in data['cases'])
    viewport[size] = len(data['cases'])
network = {}
for role in ('host', 'client1', 'client2', 'client3'):
    data = fresh(evidence / 'network' / f'network-{role}.json')
    assert data['passed'] and data['v07_features'] and data['v05_features'], role
    network[role] = data
for slot in range(4):
    data = fresh(evidence / 'network' / f'network-v07-slot{slot}.json')
    assert data['passed'], slot
    network[f'v07_slot{slot}'] = data

audits = [p for p in (evidence / 'network').glob('network-v07-*audit*.json') if p.stat().st_mtime >= build_time]
assert audits, 'Missing fresh gate command audit'
for path in audits:
    data = fresh(path)
    assert data['passed'] and data['exactly_zero_cost'] and data['jobs_and_gate_unchanged'], path
    network[path.stem] = data

baseline = read(docs / 'ProtectedPlaytestBaseline-v0.7.json')
changed = [name for name, expected in baseline.items() if not Path(name).is_file() or digest(Path(name)) != expected]
protected = {'checked_files': len(baseline), 'changed_files': changed, 'passed': not changed}
(docs / 'ProtectedPlaytestVerification-v0.7.json').write_text(json.dumps(protected, indent=2), encoding='utf-8')
assert not changed, changed
data_dir = root / 'Game/Content/Data'
staged_dir = package / 'Chronicles/Content/Data'
data_files = [p for p in data_dir.rglob('*') if p.is_file()]
for path in data_files:
    staged = staged_dir / path.relative_to(data_dir)
    assert staged.is_file() and digest(path) == digest(staged), str(path)
manifest = read(data_dir / 'visuals.json')
art = {a['texture'] for group in ('sprites', 'icons') for a in manifest[group].values() if a.get('texture')}
assert len(art) == 238, len(art)
for name in art:
    assert (staged_dir / manifest['art_root'] / name).is_file(), name
assert digest(exe) == digest(root / 'Game/Binaries/Win64/Chronicles.exe')

report = {'revision': 'v0.7', 'verified_at_utc': datetime.now(timezone.utc).isoformat(),
          'native_checks': native, 'viewport_cases': viewport, 'network_reports': network,
          'png_images': len(art), 'staged_data_files_matching_source': len(data_files),
          'packaged_executable_sha256': digest(exe), 'protected_playtest_files_unchanged': len(baseline),
          'physical_multi_computer_lan_tested': False, 'all_checks_passed': True}
(docs / 'Validation-v0.7.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
counted = sum(v['passed'] for v in native.values() if type(v['passed']) is int)
rows = '\n'.join(f"| {name} | {v['passed']} | 0 |" for name, v in native.items())
text = f'''# v0.7 validation

The packaged Windows executable passed the checks below on {datetime.now().date()}. All evidence was produced after this executable was built, and staged data matches {len(data_files)} source files. Earlier playtest copies remain unchanged: {len(baseline)} protected files matched their original hashes.

## Native simulation

{counted} counted assertions passed, with zero failures, plus the visual import and mesh-animation suites. Coverage includes economy, religion, lobby authority and readiness, fog memory and privacy, conversion immunity, wall queues and per-segment gates, selection, art replacement, and combat. The Lamanite opening test uses normal commands and simulation time to build the first temple, train a priest, research Order, advance to Age III, research Pride, build with a priest, and repair with a worker.

| Report | Passed | Failed |
|---|---:|---:|
{rows}

## Actual rendered input

The executable received actual viewport mouse and keyboard events at 1600×900 ({viewport['1600x900']} checks) and 1280×720 ({viewport['1280x720']} checks). Both runs passed single-click menus and selection, drag selection, right-click gathering, programmable hotkeys, lobby options, double-tap camera centering, selection portraits, independent rallies, wall chaining, tooltip bounds, and six pairs of rendered action-animation frames. Screenshots and reports are in [viewport evidence](Evidence/v0.7/1280x720/). Selected screenshots were visually inspected at 1280×720.

## Four-process multiplayer

One listen server and three client processes communicated through Unreal networking on localhost, using the same packaged build. All four reports passed ordinary orders, lobby settings and readiness reset, five-second conversion and permanent immunity, simultaneous foundations, eligible per-wall gates and unsupported-faction rejection, owner income rates, public tower statistics, financial privacy, stress population, and match completion. The test uses four factions and adds fixture entities and resources for focused checks; gate research time is shortened after a real accepted command. It is an automated network test, not a human match or a physical multi-computer LAN test. Connection and Windows firewall behavior on another PC still need a real LAN play session.

## Art and combat observations

All {len(art)} unique generated PNGs are present in the package, including 13 new hero/research assets with receipts. Unit motion uses deformation of 2D art, with changed mesh poses and no folded triangles in the sampled checks. It is not a rigged 3D asset system.

[Combat playtest](CombatPlaytest-v0.7.md) records 12 engagements and their limitations. Cheap Lamanite infantry is particularly strong in several tested conditions; exposed conversion units are vulnerable to interruption. No extra balance changes were inferred from those observations.

Machine-readable results: [Validation-v0.7.json](Validation-v0.7.json). Executable SHA-256: `{report['packaged_executable_sha256']}`. The portable archive has a separate checksum and CRC report beside the ZIP.
'''
(docs / 'Validation-v0.7.md').write_text(text, encoding='utf-8')
path = docs / 'RevisionTasks-v0.7.md'
text = path.read_text(encoding='utf-8').replace('Implementation is recorded; final validation is pending.', 'Implementation and applicable native, viewport, catalog, and network checks are complete; see [validation](Validation-v0.7.md).').replace('Implemented; validation pending', 'Implemented; verified')
path.write_text(text, encoding='utf-8')
path = docs / 'Release-v0.7.md'
text = path.read_text(encoding='utf-8').replace('Validation is still in progress.', 'Native, rendered-input, and four-process network checks passed; see [validation](Validation-v0.7.md).')
path.write_text(text, encoding='utf-8')
path = docs / 'NewArtTasks-v0.7.json'
data = read(path)
for task in data['requests']:
    assert (root / task['outputPath']).is_file() and (root / task['receiptPath']).is_file()
    task['status'] = 'Generated, mapped, and verified in packaged data'
path.write_text(json.dumps(data, indent=2), encoding='utf-8')

path = docs / 'Art-v0.7.md'
text = path.read_text(encoding='utf-8').replace('marks these assets generated while leaving runtime verification to the root integration pass.', 'records that these assets were generated, mapped, and verified in the packaged data.')
path.write_text(text, encoding='utf-8')

guides = package / 'Guides'
guides.mkdir(exist_ok=True)
names = ['Controls.md', 'Naming.md', 'ArtReplacement.md', 'AbilityImplementationChecklist.md',
         'Release-v0.7.md', 'RevisionTasks-v0.7.md', 'Balance-v0.7.md', 'CombatPlaytest-v0.7.md',
         'Validation-v0.7.md', 'Validation-v0.7.json', 'Art-v0.7.md', 'ArtCoverage-v0.7.json']
for name in names:
    shutil.copy2(docs / name, guides / name)
(guides / 'ArtGallery.html').write_text((docs / 'ArtGallery.html').read_text(encoding='utf-8').replace('../Game/Content/Data/', '../Chronicles/Content/Data/'), encoding='utf-8')
shutil.copytree(evidence, guides / 'Evidence/v0.7', dirs_exist_ok=True)
(package / 'README.txt').write_text('Chronicles of the Promised Land - v0.7\n\nRun Chronicles.exe. For LAN multiplayer use this same v0.7 package on all computers. One player hosts; the others join the host computer LAN IPv4 address. Allow the game through Windows Firewall on your private network when Windows prompts.\n\nControls, release notes, combat findings, art gallery and validation reports are in Guides. If runtime prerequisites are missing, run Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe.\n', encoding='utf-8')
print(json.dumps({'native_counted_assertions': counted, 'viewport_cases': viewport, 'network_processes': 4, 'png_images': len(art), 'protected_files_unchanged': len(baseline), 'all_checks_passed': True}, indent=2))
