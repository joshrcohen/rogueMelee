"""Fetch the pinned decomp, apply the mod overlay, and build locally."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = '11749c9ccbaf73bfc28a569650dfec5e18665a74'
DOL_SHA1 = '08e0bf20134dfcb260699671004527b2d6bb1a45'


def run(args, cwd=ROOT):
    subprocess.run([str(a) for a in args], cwd=cwd, check=True)


def prepare():
    patch = ROOT / 'patches/engine.patch'
    key = hashlib.sha256(patch.read_bytes()).hexdigest()[:12]
    checkout = ROOT / '.cache' / ('melee-' + key)
    ready = checkout / '.rogue-prepared'
    if not ready.exists():
        checkout.mkdir(parents=True, exist_ok=True)
        run(['git', 'init', checkout])
        run(['git', 'fetch', '--depth=1', 'https://github.com/doldecomp/melee.git', UPSTREAM], checkout)
        run(['git', 'checkout', '--detach', 'FETCH_HEAD'], checkout)
        run(['git', 'apply', '--check', patch], checkout)
        run(['git', 'apply', patch], checkout)
        ready.write_text(UPSTREAM + '\n')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=checkout, text=True).strip()
    if head != UPSTREAM:
        raise SystemExit('Unexpected upstream checkout revision: ' + str(checkout))
    run(['git', 'apply', '--reverse', '--check', patch], checkout)
    target = checkout / 'src/melee/rogue'
    target.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / 'mod').iterdir():
        if source.suffix in ('.c', '.h'):
            shutil.copy2(source, target / source.name)
    return checkout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--image', type=Path, help='Original US 1.02 ISO/RVZ, required for initial asset extraction')
    parser.add_argument('--dolphin', type=Path, help='Local Dolphin.exe; sibling DolphinTool.exe extracts assets')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument(
        '--qa-abilities', action='store_true',
        help='Build the host-driven borrowed-special compatibility harness')
    args = parser.parse_args()
    checkout = prepare()
    if args.prepare_only:
        print('Prepared modded upstream:', checkout)
        return
    game = ROOT / 'build/game'
    original = ROOT / 'orig/GALE01/sys/main.dol'
    if args.image:
        dolphin = args.dolphin or ROOT.parent / 'Dolphin-x64/Dolphin.exe'
        tool = dolphin.resolve().with_name('DolphinTool.exe')
        if not tool.is_file():
            parser.error('Use --dolphin with a Dolphin distribution containing DolphinTool.exe.')
        run([tool, 'extract', '-i', args.image.resolve(), '-o', game, '-q'])
        extracted = game / 'sys/main.dol'
        if hashlib.sha1(extracted.read_bytes()).hexdigest() != DOL_SHA1:
            parser.error('Game image is not unmodified US 1.02 Melee.')
        original.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(extracted, original)
    if not original.is_file() or hashlib.sha1(original.read_bytes()).hexdigest() != DOL_SHA1:
        parser.error('Supply the original game using --image and --dolphin.')
    if not (game / 'files').is_dir():
        parser.error('Extracted assets missing. Supply --image.')
    target_original = checkout / 'orig/GALE01/sys/main.dol'
    target_original.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(original, target_original)
    if args.qa_abilities:
        os.environ['ROGUE_QA'] = '3'
        print('QA mode: borrowed-special compatibility matrix')
    else:
        os.environ.pop('ROGUE_QA', None)
    run([sys.executable, 'configure.py', '--non-matching'], checkout)
    ninja = shutil.which('ninja') or ROOT.parent / '.tools/bin/ninja.exe'
    run([ninja, '-j', str(args.jobs)], checkout)
    shutil.copy2(checkout / 'build/GALE01/main.dol', game / 'sys/main.dol')
    shutil.copy2(checkout / 'build/GALE01/main.elf', game / 'sys/main.elf')
    print('Built:', game / 'sys/main.dol')


if __name__ == '__main__':
    main()
