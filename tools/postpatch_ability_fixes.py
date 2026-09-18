#!/usr/bin/env python3
"""
Post-engine-patch compatibility fixes for RogueMelee borrowed specials.
Runs after patches/engine.patch and is intentionally idempotent.
"""
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

def load(rel):
    path = root / rel
    if not path.is_file():
        raise SystemExit(f"Missing expected upstream file: {path}")
    return path, path.read_text(encoding="utf-8")

def save(path, text):
    path.write_text(text, encoding="utf-8", newline="\n")

def replace_required(rel, old, new, label, all_matches=False):
    path, text = load(rel)
    if new in text and old not in text:
        return False
    if old not in text:
        if new in text:
            return False
        raise SystemExit(f"{label}: expected source text was not found in {rel}")
    text = text.replace(old, new) if all_matches else text.replace(old, new, 1)
    save(path, text)
    print(f"postpatch: {label}")
    return True

def ensure_after(rel, anchor, addition, label):
    path, text = load(rel)
    if addition in text:
        return False
    if anchor not in text:
        raise SystemExit(f"{label}: anchor not found in {rel}")
    text = text.replace(anchor, anchor + addition, 1)
    save(path, text)
    print(f"postpatch: {label}")
    return True

ensure_after(
    "src/melee/ft/fighter.c",
    "    new_motion_state = Rogue_AbilityMotionState(fp, msid);\n",
    """    if (new_motion_state != NULL && Rogue_AbilitySourceKind(fp) != fp->kind) {
        flags |= Ft_MF_SkipMatAnim | Ft_MF_SkipModelPartVis |
                 Ft_MF_SkipModelFlags;
    }
""",
    "guard source-only model state",
)

replace_required(
    "src/melee/ft/kinds/ftPeach/ftpeachspecialn.c",
    "it_802BDE18(gobj, &pos, FtPart_109,",
    "it_802BDE18(gobj, &pos, Rogue_AbilityMapBone(fp, FtPart_109),",
    "map Toad attachment bone",
)
replace_required(
    "src/melee/ft/kinds/ftPeach/ftpeachspecialhi.c",
    "it_802BDA64(gobj, &pos, FtPart_109, fp->facing_dir)",
    "it_802BDA64(gobj, &pos, Rogue_AbilityMapBone(fp, FtPart_109), fp->facing_dir)",
    "map Parasol attachment bone",
)
replace_required(
    "src/melee/ft/kinds/ftPeach/ftpeachspeciallw.c",
    "it_802BD4AC(gobj, pos, Rogue_AbilityData(fp)->x8->x10, kind, fp->facing_dir)",
    "it_802BD4AC(gobj, pos, Rogue_AbilityMapBone(fp, Rogue_AbilityData(fp)->x8->x10), kind, fp->facing_dir)",
    "map Vegetable attachment bone",
)
replace_required(
    "src/melee/ft/kinds/ftLink/ftlinkspeciallw.c",
    "Fighter_Part part = da->x48;",
    "Fighter_Part part = Rogue_AbilityMapBone(fp, da->x48);",
    "map Link Bomb attachment bone",
)
replace_required(
    "src/melee/ft/kinds/ftCaptain/ftcaptainspecialn.c",
    "FighterKind kind = ftLib_GetKind(gobj);",
    "FighterKind kind = Rogue_AbilitySourceKind(fp);",
    "use borrowed source for Falcon/Ganon neutral-B",
)
replace_required(
    "src/melee/ft/kinds/ftCaptain/ftcaptainspecials.c",
    "switch (ftLib_GetKind(gobj))",
    "switch (Rogue_AbilitySourceKind(fp))",
    "use borrowed source for Raptor Boost/Gerudo Dragon",
    all_matches=True,
)

print("postpatch: ability compatibility fixes applied")
