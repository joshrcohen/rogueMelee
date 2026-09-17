"Build the portable ISO patch ZIP and the smaller friend update ZIP."
import hashlib
import os
import subprocess
import zipfile
from delta import delta, ROOT

folder = ROOT / 'build/package'
folder.mkdir(parents=True, exist_ok=True)
original = (ROOT / 'orig/GALE01/sys/main.dol').read_bytes()
assert hashlib.sha1(original).hexdigest() == '08e0bf20134dfcb260699671004527b2d6bb1a45'
target = (ROOT / 'build/game/sys/main.dol').read_bytes()
(folder / 'rogue.delta').write_bytes(delta(original, target))
(folder / 'GRGE01.ini').write_bytes((ROOT / 'tools/package/GRGE01.ini').read_bytes())
(folder / 'READ ME.txt').write_bytes((ROOT / 'tools/package/ZIP-README.txt').read_bytes())
(folder / 'UPDATE ME.txt').write_bytes((ROOT / 'tools/package/UPDATE-README.txt').read_bytes())

version = os.environ.get('ROGUEMELEE_VERSION', '').strip()
if not version:
    try:
        version = subprocess.check_output(
            ['git', 'rev-parse', '--short=8', 'HEAD'], cwd=ROOT, text=True
        ).strip()
    except Exception:
        version = 'dev'
(folder / 'version.txt').write_text(version + '\n', encoding='ascii')

compiler = 'C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe'
common_refs = [
    '/reference:System.Windows.Forms.dll',
    '/reference:System.Drawing.dll',
    '/reference:System.IO.Compression.dll',
    '/reference:System.IO.Compression.FileSystem.dll',
]
subprocess.run([
    compiler, '/nologo', '/target:exe', '/main:PatchMod', '/platform:x64', '/optimize+',
    *common_refs,
    '/out:' + str(folder / 'Apply Mod.exe'),
    str(ROOT / 'tools/package/PatchMod.cs'), str(ROOT / 'tools/package/PatchCore.cs')
], check=True)
subprocess.run([
    compiler, '/nologo', '/target:winexe', '/main:UpdateMod', '/platform:x64', '/optimize+',
    *common_refs,
    '/out:' + str(folder / 'Update RogueMelee.exe'),
    str(ROOT / 'tools/package/UpdateMod.cs'), str(ROOT / 'tools/package/PatchCore.cs')
], check=True)
subprocess.run([
    compiler, '/nologo', '/target:winexe', '/platform:x64', '/optimize+',
    '/reference:System.Windows.Forms.dll',
    '/out:' + str(folder / 'Play in Slippi.exe'),
    str(ROOT / 'tools/package/LaunchMod.cs')
], check=True)
subprocess.run([
    compiler, '/nologo', '/target:winexe', '/main:RogueMelee', '/platform:x64', '/optimize+',
    *common_refs,
    '/out:' + str(folder / 'RogueMelee.exe'),
    str(ROOT / 'tools/package/RogueMelee.cs'), str(ROOT / 'tools/package/PatchCore.cs')
], check=True)

dist = ROOT / 'dist'
dist.mkdir(exist_ok=True)
full_output = dist / 'RogueMelee-Mod.zip'
update_output = dist / 'RogueMelee-Update.zip'

with zipfile.ZipFile(full_output, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name in (
        'Apply Mod.exe', 'RogueMelee.exe', 'Update RogueMelee.exe',
        'Play in Slippi.exe', 'rogue.delta', 'version.txt',
        'READ ME.txt', 'UPDATE ME.txt', 'GRGE01.ini'
    ):
        archive.write(folder / name, name)

with zipfile.ZipFile(update_output, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name in (
        'RogueMelee.exe', 'Update RogueMelee.exe', 'Play in Slippi.exe',
        'rogue.delta', 'version.txt', 'UPDATE ME.txt', 'GRGE01.ini'
    ):
        archive.write(folder / name, name)

print(full_output, full_output.stat().st_size, 'bytes')
print(update_output, update_output.stat().st_size, 'bytes')
print('version', version)
