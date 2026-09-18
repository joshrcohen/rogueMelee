#!/usr/bin/env python3
from pathlib import Path
import re
import sys

root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()

targets = [
    "src/melee/ft/fighter.c",
    "src/melee/ft/ftaction.c",
    "src/melee/ft/ftanim.c",
    "src/melee/ft/ftparts.c",
    "src/melee/ft/ftafterimage.c",
    "src/melee/ft/kinds/ftLink/ftlinkspecialn.c",
    "src/melee/it/kinds/itlinkbow.c",
    "src/melee/it/kinds/itlinkarrow.c",
]

for rel in targets:
    path = root / rel
    if not path.is_file():
        continue

    text = path.read_text(encoding="utf-8")

    text = text.replace(
        "#include <melee/rogue/rogue_debug.h>\n",
        ""
    )

    text = re.sub(
        r'^[ \t]*Rogue_DebugCheckpoint\([^;]*\);[ \t]*\r?\n',
        '',
        text,
        flags=re.MULTILINE,
    )

    text = re.sub(
        r'^[ \t]*Rogue_DebugCheckpointEvent\([^;]*\);[ \t]*\r?\n',
        '',
        text,
        flags=re.MULTILINE,
    )

    path.write_text(text, encoding="utf-8", newline="\n")

print("production: removed ability-QA checkpoint instrumentation")
