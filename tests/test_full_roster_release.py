import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FullRosterReleaseTests(unittest.TestCase):
    def test_all_26_fighters_and_104_specials(self):
        encounter = (ROOT / "mod/rogue_encounter.c").read_text(
            encoding="utf-8"
        )

        block = encounter.split(
            "static const CharacterKind enemies[] = {", 1
        )[1].split("};", 1)[0]

        fighters = re.findall(
            r"\bCKind_[A-Za-z0-9_]+\b",
            block
        )

        self.assertEqual(
            (len(fighters), len(set(fighters))),
            (26, 26)
        )

        registry = (
            ROOT / "mod/rogue_ability_registry.c"
        ).read_text(encoding="utf-8")

        sources = re.findall(
            r"^\s+FOUR\(Ft_Kind_",
            registry,
            re.MULTILINE
        )

        self.assertEqual(len(sources), 26)
        self.assertEqual(len(sources) * 4, 104)

    def test_expanded_elite_effects(self):
        encounter = (
            ROOT / "mod/rogue_encounter.c"
        ).read_text(encoding="utf-8")

        self.assertIn(
            "RogueRng_Bounded(rng, 16)",
            encounter
        )

        self.assertIn(
            "player->vs_invisible = enemy->invisible;",
            encounter
        )

        self.assertIn(
            "player->damage1 = enemy->start_damage;",
            encounter
        )

        self.assertIn(
            "enemy->start_damage = 150;",
            encounter
        )

        self.assertIn(
            "enemy->model_scale = 0.65f;",
            encounter
        )


if __name__ == "__main__":
    unittest.main()
