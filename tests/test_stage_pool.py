import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXPECTED_STAGES = [
    "St_Kind_Battle",
    "St_Kind_Last",
    "St_Kind_OldPupupu",
    "St_Kind_Story",
    "St_Kind_PStadium",
    "St_Kind_Izumi",
    "St_Kind_RCruise",
    "St_Kind_Kongo",
    "St_Kind_Garden",
    "St_Kind_Greens",
    "St_Kind_Corneria",
    "St_Kind_Zebes",
    "St_Kind_MuteCity",
    "St_Kind_Pura",
    "St_Kind_OldKongo",
]

EXPECTED_NAMES = {
    "St_Kind_Battle": "BATTLEFIELD",
    "St_Kind_Last": "FINAL DESTINATION",
    "St_Kind_OldPupupu": "DREAM LAND 64",
    "St_Kind_Story": "YOSHI'S STORY",
    "St_Kind_PStadium": "POKEMON STADIUM",
    "St_Kind_Izumi": "FOUNTAIN OF DREAMS",
    "St_Kind_RCruise": "RAINBOW CRUISE",
    "St_Kind_Kongo": "KONGO JUNGLE",
    "St_Kind_Garden": "JUNGLE JAPES",
    "St_Kind_Greens": "GREEN GREENS",
    "St_Kind_Corneria": "CORNERIA",
    "St_Kind_Zebes": "BRINSTAR",
    "St_Kind_MuteCity": "MUTE CITY",
    "St_Kind_Pura": "POKE FLOATS",
    "St_Kind_OldKongo": "KONGO JUNGLE 64",
}


class StagePoolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.encounter = (ROOT / "mod/rogue_encounter.c").read_text(encoding="utf-8")
        cls.route = (ROOT / "mod/rogue_route.c").read_text(encoding="utf-8")

    def test_stage_pool_is_exactly_requested_15(self):
        match = re.search(
            r"static const StKind stages\[\]\s*=\s*\{(?P<body>.*?)\};",
            self.encounter,
            re.S,
        )
        self.assertIsNotNone(match, "Could not find Rogue stage pool")
        found = re.findall(r"\bSt_Kind_[A-Za-z0-9_]+\b", match.group("body"))
        self.assertEqual(found, EXPECTED_STAGES)

    def test_all_stage_names_are_displayable(self):
        for stage, name in EXPECTED_NAMES.items():
            self.assertIn(f'case {stage}: return "{name}";', self.route)

    def test_random_stage_selection_uses_whole_pool(self):
        self.assertIn(
            "stages[RogueRng_Bounded(rng, sizeof(stages) / sizeof(stages[0]))]",
            self.encounter,
        )


if __name__ == "__main__":
    unittest.main()
