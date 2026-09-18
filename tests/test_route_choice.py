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

    def test_native_rest_area_route_scene_is_unchanged(self):
        c = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        self.assertIn("St_Kind_Heal", c)
        self.assertRegex(
            c,
            r"4,\s*lbDvdPreload_2,\s*0,\s*enterRoute,\s*exitRoute",
        )

    def test_reward_row_is_between_route_and_match_choices(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        section = c.split("static void routeDraw(void)", 1)[1].split(
            "void RogueUI_OpenRoute(void)", 1
        )[0]
        self.assertIn("routeDrawRewards();", section)
        self.assertLess(
            section.index("routeDrawRewards();"),
            section.index('"CHOOSE NEXT MATCH"'),
        )

    def test_reward_must_be_selected_before_fight(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        self.assertIn("static bool route_reward_mode;", c)
        self.assertIn('"CHOOSE AN UPGRADE FIRST"', c)
        self.assertIn("Rogue_SelectReward(route_reward_cursor)", c)

    def test_bracket_ui_still_identifies_route_and_shop(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        self.assertIn('"ROGUE ROUTE"', c)
        self.assertIn('"ALL-STAR PROGRESSION"', c)
        self.assertIn('"AFTER MATCH 4: SHOP / REST AREA"', c)


if __name__ == "__main__":
    unittest.main()
