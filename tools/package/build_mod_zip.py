"""Build the portable ISO patch ZIP; no emulator or installer is packaged."""
import hashlib
import subprocess
import zipfile
from delta import delta, ROOT, WORK

folder = ROOT / 'build/package'
folder.mkdir(parents=True, exist_ok=True)
original = (ROOT / 'orig/GALE01/sys/main.dol').read_bytes()
assert hashlib.sha1(original).hexdigest() == '08e0bf20134dfcb260699671004527b2d6bb1a45'
target = (ROOT / 'build/game/sys/main.dol').read_bytes()
(folder / 'rogue.delta').write_bytes(delta(original, target))
(folder / 'GRGE01.ini').write_bytes((ROOT / 'tools/package/GRGE01.ini').read_bytes())
(folder / 'READ ME.txt').write_bytes((ROOT / 'tools/package/ZIP-README.txt').read_bytes())
subprocess.run([
    'C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe', '/nologo',
    '/target:exe', '/main:PatchMod', '/platform:x64', '/optimize+',
    '/reference:System.Windows.Forms.dll', '/reference:System.Drawing.dll',
    '/reference:System.IO.Compression.dll', '/reference:System.IO.Compression.FileSystem.dll',
    '/out:' + str(folder / 'Apply Mod.exe'),
    str(ROOT / 'tools/package/PatchMod.cs'), str(ROOT / 'tools/package/PatchCore.cs')], check=True)
output = WORK / 'dist/RogueMelee-Mod.zip'
subprocess.run([
    'C:/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe', '/nologo',
    '/target:winexe', '/platform:x64', '/optimize+',
    '/reference:System.Windows.Forms.dll',
    '/out:' + str(folder / 'Play in Slippi.exe'),
    str(ROOT / 'tools/package/LaunchMod.cs')], check=True)
output.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name in ('Apply Mod.exe', 'Play in Slippi.exe', 'rogue.delta', 'READ ME.txt', 'GRGE01.ini'):
        archive.write(folder / name, name)
print(output, output.stat().st_size, 'bytes')
