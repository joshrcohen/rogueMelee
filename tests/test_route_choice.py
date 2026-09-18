import re
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

    def test_reward_flow(self):
        c = (ROOT / "mod/rogue_rewards.c").read_text(encoding="utf-8")
        self.assertIn("RogueRoute_Prepare", c)
        self.assertIn("RogueRoute_UseBoss", c)
        self.assertIn("ROGUE_PHASE_ROUTE", c)

    def test_progression_is_non_gameplay_menu_scene(self):
        c = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        self.assertIn("GS_TOU_BRACKET, NULL, NULL", c)
        self.assertIn("enterRouteMenu", c)
        self.assertIn("Rogue_RouteMenuSceneFrame", c)
        self.assertRegex(
            c,
            r"4,\s*lbDvdPreload_3,\s*0,\s*enterRouteMenu,\s*exitRouteMenu",
        )

    def test_rest_area_is_reserved_for_shop(self):
        c = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        menu = c.split("static void enterRouteMenu", 1)[1]
        menu = menu.split("static void encounterFrame", 1)[0]
        self.assertNotIn("St_Kind_Heal", menu)
        self.assertIn("Rogue_BeginCamp() ? 3 : 2", menu)
        camp = c.split("static void enterCamp", 1)[1]
        camp = camp.split("static void exitCamp", 1)[0]
        self.assertIn("St_Kind_Heal", camp)

    def test_mockup_composition(self):
        c = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        self.assertIn('"CHOOSE UPGRADE"', c)
        self.assertIn('"PICK %d OF 3"', c)
        self.assertIn('"CHOOSE NEXT MATCH     LEFT / RIGHT"', c)
        self.assertIn('"STAGE %d"', c)
        self.assertIn('"GOLD GAINED"', c)
        self.assertIn('"TOTAL GOLD"', c)
        self.assertIn('"TOTAL SCORE"', c)
        self.assertIn('"MOD: %s   %s"', c)


if __name__ == "__main__":
    unittest.main()
