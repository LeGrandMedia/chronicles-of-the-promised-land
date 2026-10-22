from pathlib import Path
import json
root = Path(__file__).resolve().parents[1]
data = root / 'Revisions/v0.8/Game/Content/Data'
for path in data.rglob('*.json'):
    json.loads(path.read_text(encoding='utf-8-sig'))
manifest = json.loads((data / 'visuals.json').read_text(encoding='utf-8'))
art = {entry['texture'] for group in ('sprites', 'icons') for entry in manifest[group].values() if entry.get('texture')}
for relative in art:
    assert (data / manifest['art_root'] / relative).is_file(), relative
notes = list((root / 'book of mormon RTS').rglob('*.md'))
assert len(notes) == 27, len(notes)
assert (root / 'Revisions/v0.8/Tools/CookProject/Chronicles.uproject').is_file()
print(f'JSON parsed; {len(art)} referenced images and {len(notes)} vault notes present. Gameplay requires the Unreal tests.')
