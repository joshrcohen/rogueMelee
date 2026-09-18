import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class NativeOnePlayerUiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ui = (ROOT / "mod/rogue_ui.c").read_text(encoding="utf-8")
        cls.rogue = (ROOT / "mod/rogue.c").read_text(encoding="utf-8")

    def test_upgrade_picker_is_on_route_screen(self):
        route = self.ui.split("/* Rogue Bracket:", 1)[1].split(
            "/* Camp signs are projected", 1
        )[0]
        self.assertIn('"CHOOSE UPGRADE"', route)
        self.assertIn("routeRewardBox", route)
        self.assertIn("g_rogue_run.current_rewards[i]", route)
        self.assertIn(
            "Rogue_SelectReward(route_reward_cursor)",
            route,
        )
        self.assertNotIn(
            "ROGUE_UI_ROUTE_REWARD_BASE",
            route,
        )
        self.assertNotIn("static void drawStageClear(void)", self.ui)

    def test_upgrade_row_precedes_next_match(self):
        section = self.ui.split("static void routeDraw(void)", 1)[1].split(
            "void RogueUI_OpenRoute(void)", 1
        )[0]
        self.assertLess(
            section.index("routeDrawRewards();"),
            section.index('"CHOOSE NEXT MATCH"'),
        )

    def test_native_classic_matchup_intro(self):
        self.assertIn("GS_INTRO_EASY, &stage_intro", self.rogue)
        self.assertIn("stage_intro.allies[0] = g_rogue_run.player_kind", self.rogue)
        self.assertIn("stage_intro.enemies[i] = encounter->enemies[i].kind", self.rogue)

    def test_all_star_route_language(self):
        self.assertIn('"ALL-STAR PROGRESSION"', self.ui)
        self.assertIn('"CHOOSE NEXT MATCH"', self.ui)
        self.assertIn('"FINAL: %s"', self.ui)

    def test_native_continue_screen(self):
        self.assertIn("GS_GAMEOVER, &game_over_data, &game_over_data", self.rogue)

    def test_hud_is_simplified(self):
        self.assertIn('"ACT %d-%d   G%d   %s"', self.ui)


if __name__ == "__main__":
    unittest.main()
