"""Package an explicit v0.9 source/vault and player handoff; never uploads files."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def archive_verified(output, entries, extra=None):
    if output.exists():
        raise FileExistsError(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix + '.partial')
    manifest = {}
    with zipfile.ZipFile(temporary, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path, name in sorted(entries, key=lambda pair: pair[1]):
            if name in manifest or name.startswith('/') or '..' in Path(name).parts:
                raise ValueError(f'Invalid or repeated archive name: {name}')
            manifest[name] = {'bytes': path.stat().st_size, 'sha256': digest(path)}
            archive.write(path, name)
        for name, content in (extra or {}).items():
            data = content.encode('utf-8')
            if name in manifest:
                raise ValueError(name)
            manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
            archive.writestr(name, data)
        archive.writestr('CONTENTS-SHA256.json', json.dumps(manifest, indent=2) + '\n')
    with zipfile.ZipFile(temporary) as archive:
        if archive.testzip() is not None:
            raise ValueError('ZIP CRC verification failed')
        for name, record in manifest.items():
            info = archive.getinfo(name)
            if info.file_size != record['bytes']:
                raise ValueError(f'ZIP size differs: {name}')
    temporary.replace(output)
    return {'file': output.name, 'bytes': output.stat().st_size, 'sha256': digest(output),
            'files': len(manifest), 'crc_verified': True}

def build_portable(output):
    package = ROOT / 'Builds/Windows'
    required = ['Chronicles.exe', 'Chronicles/Binaries/Win64/Chronicles.exe',
                'Chronicles/Content/Data/catalog.json', 'Chronicles/Content/Data/Models/models.json',
                'Engine/Extras/Redist/en-us/UEPrereqSetup_x64.exe']
    for relative in required:
        if not (package / relative).is_file():
            raise FileNotFoundError(relative)
    entries = [(p, 'Chronicles-v0.9/' + p.relative_to(package).as_posix()) for p in package.rglob('*')
               if p.is_file() and 'Saved' not in p.relative_to(package).parts
               and p.suffix.lower() not in {'.pdb', '.obj', '.lib', '.exp', '.ilk'}
               and not p.name.startswith('Manifest_')]
    quickstart = (ROOT / 'Docs/PlayerQuickStart-v0.9.md').read_text(encoding='utf-8')
    return archive_verified(output, entries, {'Chronicles-v0.9/START-HERE.txt': quickstart})

def source_entries(vault):
    entries = []
    def add_tree(base, prefix, excluded=()):
        for p in base.rglob('*'):
            if not p.is_file():
                continue
            relative = p.relative_to(base)
            if any(part in excluded for part in relative.parts):
                continue
            if p.name.startswith('workspace') and '.obsidian' in relative.parts:
                continue
            if p.suffix.lower() in {'.key', '.pem', '.p12', '.pfx', '.keystore', '.jks', '.user', '.suo'} or p.name in {'.env', 'id_rsa', 'id_ed25519'}:
                raise ValueError(f'Unexpected sensitive file in source handoff: {p}')
            entries.append((p, prefix + relative.as_posix()))
    for relative in ['README.md', 'AGENTS.md', '.gitignore', 'Play.cmd', 'Game/Chronicles.uproject']:
        entries.append((ROOT / relative, 'Chronicles-v0.9/' + relative))
    for relative in ['Game/Source', 'Game/Config', 'Game/Content', 'Installer', 'Docs']:
        add_tree(ROOT / relative, 'Chronicles-v0.9/' + relative + '/', {'__pycache__', '.git'})
    add_tree(ROOT / 'Tools', 'Chronicles-v0.9/Tools/',
             {'Toolchain', 'Saved', 'Intermediate', 'Binaries', 'DerivedDataCache', '__pycache__', '.git'})
    add_tree(vault, 'Obsidian-Vault/', {'.git', '.trash', '.obsidian', '__pycache__'})
    return entries

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--vault', type=Path, required=True)
    parser.add_argument('--installer', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve(); vault = args.vault.resolve(); installer = args.installer.resolve()
    if not vault.is_dir() or not (vault / '00 Navigation/Start Here.md').is_file():
        raise ValueError('Canonical Obsidian vault entry point is missing')
    if not installer.is_file():
        raise FileNotFoundError(installer)
    output.mkdir(parents=True, exist_ok=True)
    items = []
    for source, name in [(installer, 'Chronicles-Setup-v0.9.0-Windows-x64.exe'),
                         (ROOT / 'Builds/Android/Chronicles-Android-arm64-ETC2-v0.9.apk', 'Chronicles-v0.9-Android-arm64.apk')]:
        target = output / name
        if target.exists():
            raise FileExistsError(target)
        shutil.copy2(source, target)
        if digest(source) != digest(target):
            raise ValueError(f'Copy verification failed: {name}')
        items.append({'file': name, 'bytes': target.stat().st_size, 'sha256': digest(target)})
    print('Copied and verified installer and Android APK', flush=True)
    items.append(build_portable(output / 'Chronicles-v0.9-Windows-Portable.zip'))
    print('Windows portable ZIP passed CRC verification', flush=True)
    items.append(archive_verified(output / 'Chronicles-v0.9-Source-and-Obsidian-Vault.zip', source_entries(vault)))
    print('Source and vault ZIP passed CRC verification', flush=True)
    for source, name in [(ROOT / 'Docs/Handoff-v0.9.md', 'START-HERE.md'),
                         (ROOT / 'Docs/PlayerQuickStart-v0.9.md', 'PLAYING.md')]:
        shutil.copy2(source, output / name)
    (output / 'DELIVERABLES.json').write_text(json.dumps({'version': '0.9.0', 'date': '2026-11-22', 'artifacts': items}, indent=2) + '\n', encoding='utf-8')
    (output / 'SHA256SUMS.txt').write_text(''.join(f"{item['sha256']}  {item['file']}\n" for item in items), encoding='utf-8')
    print(json.dumps(items, indent=2), flush=True)

if __name__ == '__main__':
    main()
