import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RouteChoiceTests(unittest.TestCase):
    def test_route_source_is_compiled(self):
        p = (ROOT / "patches/engine.patch").read_text(encoding="utf-8")
        self.assertIn('Object(Equivalent, "melee/rogue/rogue_route.c")', p)

    def test_shape(self):
        h = (ROOT / "mod/rogue_route.h").read_text(encoding="utf-8")
        self.assertIn("#define ROGUE_ROUTE_ROUNDS 4", h)
        self.assertIn("#define ROGUE_ROUTE_CHOICES 2", h)

    def test_initial_route_uses_existing_rest_area_scene(self):
        c = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        self.assertIn("St_Kind_Heal", c)
        self.assertRegex(
            c,
            r"4,\s*lbDvdPreload_2,\s*0,\s*enterRoute,\s*exitRoute",
        )

    def test_rest_area_route_has_no_reward_input(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        section = c.split("/* Rogue Bracket:", 1)[1].split(
            "/* Camp signs are projected", 1
        )[0]
        self.assertNotIn("Rogue_SelectReward", section)
        self.assertNotIn("CHOOSE UPGRADE", section)
        self.assertIn('"CHOOSE NEXT MATCH"', section)

    def test_postfight_route_choice_is_on_stage_clear(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        stage = c.split("static void drawStageClear(void)", 1)[1].split(
            "static void drawRunEnd(void)", 1
        )[0]
        self.assertIn('"CHOOSE NEXT FIGHT"', stage)
        self.assertIn("stageFightBox", stage)

    def test_shop_route_language_remains(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        self.assertIn('"AFTER MATCH 4: SHOP / REST AREA"', c)


if __name__ == "__main__":
    unittest.main()
