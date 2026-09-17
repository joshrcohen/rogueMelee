"""Build/stage a modded extracted disc without modifying the source game image."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT.parent
UPSTREAM = '11749c9ccbaf73bfc28a569650dfec5e18665a74'
DOL_SHA1 = '08e0bf20134dfcb260699671004527b2d6bb1a45'


def run(args):
    subprocess.run([str(x) for x in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, help='US 1.02 ISO/RVZ; required on first build')
    parser.add_argument('--launch', action='store_true')
    parser.add_argument('--diagnostic', choices=['camp', 'route'], help='Build an isolated diagnostic DOL; never replace the launcher DOL')
    parser.add_argument('--jobs', type=int, default=8)
    args = parser.parse_args()
    if subprocess.run(['git', 'merge-base', '--is-ancestor', UPSTREAM, 'HEAD'],
                      cwd=ROOT).returncode:
        parser.error('This checkout does not descend from the supported upstream revision.')
    game = ROOT / 'build/game'
    original = ROOT / 'orig/GALE01/sys/main.dol'
    dolphin = WORK / 'Dolphin-x64/Dolphin.exe'
    if args.image:
        source = args.image.resolve()
        if not source.is_file():
            parser.error('Game image not found.')
        run([dolphin.with_name('DolphinTool.exe'), 'extract', '-i', source, '-o', game, '-q'])
        extracted = game / 'sys/main.dol'
        if hashlib.sha1(extracted.read_bytes()).hexdigest() != DOL_SHA1:
            parser.error('Image is not the supported US 1.02 revision.')
        original.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(extracted, original)
    if not original.is_file() or hashlib.sha1(original.read_bytes()).hexdigest() != DOL_SHA1:
        parser.error('Provide --image with the original US 1.02 game.')
    if not (game / 'files').is_dir():
        parser.error('Extracted assets missing; provide --image.')
    if args.diagnostic:
        os.environ['ROGUE_QA'] = '1' if args.diagnostic == 'camp' else '2'
    else:
        os.environ.pop('ROGUE_QA', None)
    run([sys.executable, 'configure.py', '--non-matching'])
    ninja = WORK / '.tools/bin/ninja.exe'
    run([ninja if ninja.exists() else 'ninja', '-j', str(args.jobs)])
    output = (ROOT / 'build/game-qa/sys/main.dol') if args.diagnostic else (game / 'sys/main.dol')
    if args.diagnostic:
        # Dolphin recognizes an extracted disc only at sys/main.dol, with
        # the boot metadata and sibling files directory in place.
        output.parent.mkdir(parents=True, exist_ok=True)
        for name in ('boot.bin', 'bi2.bin', 'apploader.img'):
            shutil.copy2(game / 'sys' / name, output.parent / name)
        assets = output.parent.parent / 'files'
        if not assets.exists():
            if os.name == 'nt':
                subprocess.run(['powershell', '-NoProfile', '-Command',
                                'New-Item -ItemType Junction -Path $env:ROGUE_QA_ASSETS '
                                '-Target $env:ROGUE_GAME_ASSETS'], check=True,
                               env={**os.environ, 'ROGUE_QA_ASSETS': str(assets),
                                    'ROGUE_GAME_ASSETS': str(game / 'files')})
            else:
                assets.symlink_to(game / 'files', target_is_directory=True)
    shutil.copy2(ROOT / 'build/GALE01/main.dol', output)
    shutil.copy2(ROOT / 'build/GALE01/main.elf', output.with_suffix('.elf'))
    if not args.diagnostic:
        shutil.copy2(ROOT / 'tools/Launch Rogue Melee.cmd', WORK / 'Launch Rogue Melee.cmd')
    print('Staged Rogue Melee:', output)
    if args.launch:
        subprocess.Popen([str(dolphin), '-u', str(ROOT / ('build/dolphin-ui-test' if args.diagnostic else 'build/dolphin-user')),
                          '-e', str(output)], cwd=WORK)


if __name__ == '__main__':
    main()
