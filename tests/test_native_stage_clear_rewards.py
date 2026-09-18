import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeStageClearRewardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_stage_clear_is_not_the_reward_picker_anymore(self):
        post = self.rogue.split("bool Rogue_PostFight(void)", 1)[1]
        post = post.split("static void enterGameOver", 1)[0]
        self.assertIn("g_rogue_run.phase == ROGUE_PHASE_REWARD", post)
        self.assertIn("destination = 4;", post)
        self.assertIn("GS_TOU_BRACKET, NULL, NULL", self.rogue)
        menu = self.rogue.split("static void enterRouteMenu", 1)[1]
        menu = menu.split("static void encounterFrame", 1)[0]
        self.assertNotIn("St_Kind_Heal", menu)

    def test_progression_screen_contains_upgrade_and_fight_phases(self):
        self.assertIn("ROUTE_UI_UPGRADE", self.ui)
        self.assertIn("ROUTE_UI_FIGHT", self.ui)
        self.assertIn("ROUTE_UI_BOSS", self.ui)
        self.assertIn('"CHOOSE UPGRADE"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH     LEFT / RIGHT"', self.ui)

    def test_progression_screen_keeps_run_context_visible(self):
        self.assertIn('"GOLD GAINED"', self.ui)
        self.assertIn('"TOTAL GOLD"', self.ui)
        self.assertIn('"TOTAL SCORE"', self.ui)
        self.assertIn('"FLOOR %d / %d"', self.ui)

    def test_reward_confirmation_stays_in_route_scene(self):
        frame = self.ui.split("int RogueUI_RouteFrame(void)", 1)[1]
        frame = frame.split("/* Camp signs", 1)[0]
        self.assertIn("Rogue_SelectReward(route_reward_cursor)", frame)
        self.assertIn("ROUTE_UI_FIGHT", frame)
        self.assertIn("ROUTE_UI_BOSS", frame)

    def test_highlighted_fight_reveals_modifier(self):
        self.assertIn('"MOD: %s   %s"', self.ui)
        self.assertIn(
            "routeDrawMatchup(&current->choices[route_cursor], route_cursor);",
            self.ui,
        )


if __name__ == "__main__":
    unittest.main()
