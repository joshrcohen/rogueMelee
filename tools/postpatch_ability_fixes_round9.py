#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
path = root / "src/melee/ft/kinds/ftPeach/ftpeachspecialhi.c"
if not path.is_file():
    raise SystemExit(f"Missing expected file: {path}")

text = path.read_text(encoding="utf-8")

needle = '''void ftPe_SpecialHi_8011D424(HSD_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);
'''

insert = '''void ftPe_SpecialHi_8011D424(HSD_GObj* gobj)
{
    Fighter* fp = GET_FIGHTER(gobj);

    /*
     * Peach's parasol item has its own per-frame owner/controller path.
     * Cross-character borrowed Up-B can finish all fighter-script work and
     * still stall in that Peach-owned article runtime.
     *
     * Keep the source move's movement, collision, and fighter hitboxes, but do
     * not create Peach's attached parasol item for a non-Peach recipient.
     */
    if (Rogue_IsAbilityState(fp) &&
        Rogue_AbilitySourceKind(fp) == Ft_Kind_Peach &&
        fp->kind != Ft_Kind_Peach)
    {
        if (fp->mv.pe.specialhi.kind == It_Kind_Capsule)
            fp->mv.pe.specialhi.kind = It_Kind_Peach_Parasol;

        fp->accessory4_cb = NULL;
        fp->pre_hitlag_cb = NULL;
        fp->post_hitlag_cb = NULL;
        return;
    }
'''

if insert in text:
    print("round9: borrowed Peach Parasol runtime guard already installed")
elif needle in text:
    text = text.replace(needle, insert, 1)
    path.write_text(text, encoding="utf-8", newline="\n")
    print("round9: disable Peach-owned Parasol article on non-Peach recipients")
else:
    raise SystemExit("round9: ftPe_SpecialHi_8011D424 function header not found")

print("round9: postpatch applied")
