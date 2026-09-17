"""Static coverage checks for the 26 playable fighters' four special slots."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "mod" / "rogue_ability_registry.c"

EXPECTED = {
    "CKind_Captain", "CKind_Donkey", "CKind_Fox", "CKind_GameWatch",
    "CKind_Kirby", "CKind_Koopa", "CKind_Link", "CKind_Luigi",
    "CKind_Mario", "CKind_Mars", "CKind_Mewtwo", "CKind_Ness",
    "CKind_Peach", "CKind_Pikachu", "CKind_PopoNana", "CKind_Purin",
    "CKind_Samus", "CKind_Yoshi", "CKind_Zelda", "CKind_Seak",
    "CKind_Falco", "CKind_CLink", "CKind_DrMario", "CKind_Emblem",
    "CKind_Pichu", "CKind_Ganon",
}


class AbilityRegistryCoverageTests(unittest.TestCase):
    def test_every_playable_character_has_four_registered_specials(self):
        text = REGISTRY.read_text(encoding="utf-8")
        entries = re.findall(
            r"^\s*FOUR\(\s*Ft_Kind_[A-Za-z0-9_]+\s*,\s*"
            r"(CKind_[A-Za-z0-9_]+)\s*,",
            text,
            re.MULTILINE,
        )
        self.assertEqual(len(entries), 26, "Expected 26 FOUR(...) registry rows = 104 specials")
        self.assertEqual(set(entries), EXPECTED)
        self.assertEqual(len(set(entries)), len(entries), "Duplicate character in ability registry")


if __name__ == "__main__":
    unittest.main()
