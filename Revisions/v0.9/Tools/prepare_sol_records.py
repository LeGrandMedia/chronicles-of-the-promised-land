"""Reproducible source audit and audio preproduction, not an Unreal catalog import.

Run from any directory: python Tools/prepare_sol_records.py
Outputs are restricted to Docs/Evidence/2026-10-20 and Docs/AudioPreparation.
Canonical notes and their inventories are never written.
"""
import csv
import hashlib
import json
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VAULT = ROOT / 'book of mormon RTS'
PLAN = VAULT / '08 Development Plan'
OUT = ROOT / 'Docs' / 'AudioPreparation'
EVIDENCE = ROOT / 'Docs' / 'Evidence' / '2026-10-20'


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def table_rows(text, heading):
    section = text.split(heading, 1)[1].split('\n## ', 1)[0]
    return [[cell.strip() for cell in line.strip().strip('|').split('|')]
            for line in section.splitlines() if line.startswith('| ')
            and not line.startswith('| ---')][1:]


def main():
    inventory = read_json(PLAN / 'Source Inventory.json')['notes']
    audit = []
    for note in inventory:
        path = VAULT / note['path']
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        normalized = hashlib.sha256(data.replace(b'\r\n', b'\n')).hexdigest()
        audit.append({'path': note['path'], 'expected_sha256': note['sha256'],
                      'actual_sha256': digest, 'lf_sha256': normalized,
                      'matches_inventory': digest == note['sha256'],
                      'matches_after_lf_normalization': normalized == note['sha256']})
    mismatches = [a for a in audit if not a['matches_inventory']]
    write_json(EVIDENCE / 'source-audit.json', {
        'notes_checked': len(audit), 'exact_hash_mismatches': len(mismatches),
        'notes': audit})
    # Stop on real content drift. LF-only differences remain visible in evidence.
    assert not [a for a in mismatches if not a['matches_after_lf_normalization']], 'Canonical content changed: reread sources before regenerating'
    units = {e['name']: e for e in read_json(PLAN / 'Asset Task Inventory.json')['entries'] if e['kind'] == 'unit'}
    records = []
    for path in sorted((VAULT / '06 Voice Sheets').glob('Voice - *.md')):
        name = path.stem.removeprefix('Voice - ')
        unit = units[name]
        unit_path = VAULT / unit['source_note']
        unit_text = unit_path.read_text(encoding='utf-8-sig')
        stats = dict(table_rows(unit_text, '## Base statistics'))
        noncombatant = stats['Attack damage'] == '0'
        behavior = unit_text.split('## Behavior and abilities', 1)[1].split('\n## ', 1)[0].strip()
        text = path.read_text(encoding='utf-8-sig')
        for event, cue, reference in table_rows(text, '## Voice implementation prompt'):
            creature = name in {'Cumelom', 'Curelom', 'Elephant'}
            records.append({
                'production_unit_id': unit['id'], 'runtime_unit_id': None,
                'unit_name': name, 'event_id': event,
                'recording_filename': f"{unit['id']}_{event}_01.wav",
                'text_or_cue': cue, 'subtitle': cue,
                'scriptural_reference': reference,
                'basis_type': 'nonverbal' if creature else None,
                'basis_review': 'Nonverbal cue verified against creature sheet' if creature else 'Paraphrase versus comic_allusion requires editorial review; do not assert direct quotation',
                'noncombatant': noncombatant,
                'attack_response_direction': behavior if noncombatant and event == 'attack' else None,
                'pronunciation_direction': 'Nonverbal; do not speak the bracketed cue' if creature else 'Director approves and records roster-name pronunciation before takes; restrained theatrical speech, no leader imitation or stereotyped accent',
                'pronunciation_approved': False,
                'source_note': path.relative_to(VAULT).as_posix(),
                'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'unit_source_note': unit['source_note'],
                'audio_asset_id': None, 'audio_file': None,
                'license_reference': None, 'performer_release': None,
                'source_route': 'Original performance planned; not yet recorded/cleared',
                'status': 'unrecorded', 'fallback_text_prepared': True,
                'runtime_fallback_verified': False})
    assert len(records) == 153
    assert len({(r['production_unit_id'], r['event_id']) for r in records}) == 153
    assert len({r['recording_filename'] for r in records}) == 153
    assert len(units) == 39
    expected = read_json(PLAN / 'Voice Event Inventory.json')['events']
    fields = ('unit_name', 'event_id', 'text_or_cue', 'scriptural_reference', 'source_note')
    assert sorted(tuple(r[k] for k in fields) for r in records) == sorted(tuple(r[k] for k in fields) for r in expected), 'Individual sheets disagree with prior inventory'
    for name in units:
        events = {r['event_id'] for r in records if r['unit_name'] == name}
        assert {'select', 'move', 'attack'} <= events, name
    for name in ['Industrious Laborer', 'Tiller of the Earth', 'Keeper of Flocks', 'Worker in Ore', 'Ammonite Covenant Keeper']:
        assert {'gather_food', 'gather_timber', 'gather_wealth', 'return_cargo'} <= {r['event_id'] for r in records if r['unit_name'] == name}
    write_json(OUT / 'ActiveVoicePreparation.json', {
        'status': 'preproduction_only',
        'warning': 'Uxx IDs are existing asset production IDs. Runtime schema A044 and import A051 remain blocked. No audio or runtime fallback exists.',
        'active_event_count': len(records), 'events': records})
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / 'AudioProductionLedger.csv').open('w', encoding='utf-8', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=list(records[0]))
        writer.writeheader()
        writer.writerows(records)
    script = ['# Recording script — A186 preparation', '',
              '153 active rows from 39 individual sheets. Optional campaign scripts are excluded. No recordings made. Filenames use existing Uxx production IDs, not an approved runtime schema.', '',
              'All speech is original paraphrase or comic allusion. Scripture is internal basis metadata, never a claim of verbatim quotation. Use clear, restrained theatrical delivery; let the situation carry the joke. Do not imitate a church leader or ethnic stereotype. Approve name pronunciations with the director and retain the approved guide before recording. No authoritative phonetic guide is supplied by the vault.', '',
              'For each filename, record one approved take; later alternatives may increment the take suffix without renaming the event. Creature cues are nonverbal. Noncombatant attack responses never imply damage. No sample-rate/import specification is frozen before A187.', '']
    for name in units:
        script += [f'## {name}', '']
        for r in [r for r in records if r['unit_name'] == name]:
            script += [f"- `{r['recording_filename']}` — **{r['event_id']}**: {r['text_or_cue']}",
                       f"  Basis metadata: {r['scriptural_reference']}. Source: `{r['source_note']}`."]
            if r['attack_response_direction']:
                script += [f"  Noncombatant direction: {r['attack_response_direction']}"]
        script += ['']
    (OUT / 'RecordingScript.md').write_text('\n'.join(script) + '\n', encoding='utf-8')
    master = (VAULT / '06 Voice Sheets' / 'Voice Lines.md').read_text(encoding='utf-8-sig')
    extras = []
    for event, cue, basis, scope in table_rows(master, '## Retained contextual lines'):
        extras.append({'event_id': event, 'text_or_cue': cue, 'scriptural_reference': basis,
                       'scope': scope, 'optional_campaign': 'Future' in scope,
                       'status': 'unrecorded_unbound', 'license_reference': None,
                       'source_note': '06 Voice Sheets/Voice Lines.md',
                       'deduplication': 'Review overlap with routine event before recording; excluded from 153 denominator'})
    write_json(OUT / 'ContextualExtras.json', extras)
    report = {'result': 'PASS — source extraction only', 'units': len(units),
              'active_events': len(records), 'inventory_comparison': 'exact match on all five canonical fields',
              'unique_filenames': 153, 'creature_cues': sum(r['basis_type'] == 'nonverbal' for r in records),
              'playable_audio': 0, 'cleared_audio_licenses': 0,
              'text_prepared': 153, 'runtime_fallbacks_verified': 0,
              'contextual_extras': len(extras), 'dormant_contextual_rows': sum(e['optional_campaign'] for e in extras),
              'basis_type_review_pending': sum(r['basis_type'] is None for r in records),
              'pronunciation_approval_pending': 39,
              'action_acceptance': 'Does not pass A051, A185, A186 or A194; no runtime schema, approved pronunciations, recordings or fallback dispatch'}
    write_json(EVIDENCE / 'audio-preparation-validation.json', report)
    (OUT / 'CoverageAndLicenses.md').write_text(
        '# Audio coverage and license gap report — A194 preparation\n\n'
        '153/153 active event texts prepared from individual sheets; 0/153 playable audio bindings; '
        '0/153 runtime text fallbacks verified; 0 cleared audio licenses. No event is runtime-ready. '
        'Each of the 153 ledger rows explicitly tracks its unrecorded state and missing license/performer release. '
        'Creature cues are inside the active denominator. Contextual extras are separate and may duplicate routine lines.\n\n'
        'A185 remains incomplete until the chosen recordings/cues resolve to licensed sources or original performers. '
        'UI, music and world-effect requirements need an approved cue list from working gameplay; see AudioScopeGaps.md. '
        'A186 needs a director-approved pronunciation guide and the prior phase gates. '
        'A194 needs actual runtime coverage and licenses before release polish closes. '
        'Fallback strings in preparation files do not prove that a player can see subtitles.\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
