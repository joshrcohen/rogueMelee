import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RouteChoiceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.progress = (ROOT / "mod/rogue_progression.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")
        cls.rewards = (ROOT / "mod/rogue_rewards.c").read_text(encoding="utf-8")
        cls.patch = (ROOT / "patches/engine.patch").read_text(encoding="utf-8")

    def test_route_source_is_compiled(self):
        self.assertIn('Object(Equivalent, "melee/rogue/rogue_route.c")', self.patch)
        self.assertIn('Object(Equivalent, "melee/rogue/rogue_progression.c")', self.patch)

    def test_shape(self):
        h = (ROOT / "mod/rogue_route.h").read_text(encoding="utf-8")
        self.assertIn("#define ROGUE_ROUTE_ROUNDS 4", h)
        self.assertIn("#define ROGUE_ROUTE_CHOICES 2", h)

    def test_opening_shop_precedes_first_route_choice(self):
        css = self.rogue.split("static void exitCharacterSelect", 1)[1]
        css = css.split("static void enterStageIntro", 1)[0]
        self.assertIn("Rogue_BeginOpeningCamp()", css)
        self.assertIn("gm_SetNextGameModeStateId(3);", css)
        self.assertIn("bool Rogue_BeginOpeningCamp(void)", self.rewards)

        camp_exit = self.rogue.split("static void exitCamp", 1)[1]
        camp_exit = camp_exit.split("static void routeFrame", 1)[0]
        self.assertIn("g_rogue_run.opening_camp", camp_exit)
        self.assertIn("ROGUE_STATE_PROGRESSION", camp_exit)

    def test_left_and_right_are_real_route_encounters(self):
        self.assertIn("left = &round->choices[0];", self.progress)
        self.assertIn("right = &round->choices[1];", self.progress)

    def test_after_upgrade_left_right_selects_fight(self):
        self.assertIn("fight_cursor ^= 1;", self.progress)
        self.assertIn("RogueRoute_Select(&g_rogue_run.route,", self.progress)

    def test_rest_area_remains_for_actual_camp(self):
        self.assertIn("Rogue_BeginCamp()", self.progress)
        self.assertIn("gm_SetNextGameModeStateId(3);", self.progress)


if __name__ == "__main__":
    unittest.main()
